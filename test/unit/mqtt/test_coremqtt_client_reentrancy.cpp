/**
 * @file test_coremqtt_client_reentrancy.cpp
 * @brief Regression tests for the processLoop callback re-entrancy deadlock
 *
 * Background: CoreMqttClient::processLoop() fires subscription callbacks from
 * inside MQTT_ProcessLoop() while the client mutex is held. With the original
 * non-recursive std::mutex, any callback that called publish()/subscribe()
 * (directly, or transitively by logging through an MQTT log sink) re-locked
 * the owned mutex on the same thread: undefined behavior, in practice a
 * device wedge. The fix makes the mutex recursive and rejects re-entrant
 * processLoop() calls.
 *
 * These tests drive the real coremqtt_client.cpp against the host mocks; the
 * mock MQTT_ProcessLoop delivers queued synthetic PUBLISH packets to the
 * registered event callback synchronously, exactly like the real library.
 * A regression re-introducing the deadlock shows up as a test-suite hang
 * (caught by the CTest timeout).
 */

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "logging/logger.hpp"
#include "logging/mqtt_log_sink.hpp"
#include "logging/queued_sink.hpp"
#include "mqtt/coremqtt_client.hpp"
#include "mqtt/mqtt_config.hpp"
#include "tls/mock_tls_transport.hpp"

using namespace lopcore::mqtt;

namespace
{
constexpr const char *kTopic = "dt/test/response";
constexpr uint32_t kPumpMs = 20;
} // namespace

class CoreMqttClientReentrancyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MockCoreMqtt::clearIncomingPublishes();

        config = MqttConfig::builder()
                     .broker("test-broker.example.com")
                     .port(8883)
                     .clientId("reentrancy-test")
                     .autoStartProcessLoop(false) // manual pump: the mock task runner spawns no threads
                     .build();

        transport = std::make_shared<lopcore::test::MockTlsTransport>();
        transport->connect(lopcore::tls::TlsConfig{});

        client = std::make_unique<CoreMqttClient>(config, transport);
        ASSERT_EQ(client->connect(), ESP_OK);
        ASSERT_TRUE(client->isConnected());
    }

    void TearDown() override
    {
        MockCoreMqtt::clearIncomingPublishes();
    }

    MqttConfig config;
    std::shared_ptr<lopcore::test::MockTlsTransport> transport;
    std::unique_ptr<CoreMqttClient> client;
};

// The original bug: a subscription callback that publishes. Before the fix
// this re-locked the non-recursive client mutex on the same thread.
TEST_F(CoreMqttClientReentrancyTest, CallbackPublish_NoDeadlock)
{
    std::atomic<bool> callbackRan{false};
    esp_err_t publishResult = ESP_FAIL;

    ASSERT_EQ(client->subscribe(kTopic,
                                [&](const MqttMessage &msg) {
                                    callbackRan = true;
                                    publishResult = client->publishString("dt/test/ack",
                                                                          msg.getPayloadAsString());
                                }),
              ESP_OK);

    MockCoreMqtt::enqueueIncomingPublish(kTopic, "hello");
    EXPECT_EQ(client->processLoop(kPumpMs), ESP_OK);

    EXPECT_TRUE(callbackRan);
    EXPECT_EQ(publishResult, ESP_OK);
}

TEST_F(CoreMqttClientReentrancyTest, CallbackSubscribeUnsubscribe_NoDeadlock)
{
    std::atomic<bool> callbackRan{false};
    esp_err_t subscribeResult = ESP_FAIL;
    esp_err_t unsubscribeResult = ESP_FAIL;

    ASSERT_EQ(client->subscribe(kTopic,
                                [&](const MqttMessage &) {
                                    callbackRan = true;
                                    subscribeResult = client->subscribe("dt/test/other",
                                                                        [](const MqttMessage &) {});
                                    unsubscribeResult = client->unsubscribe("dt/test/other");
                                }),
              ESP_OK);

    MockCoreMqtt::enqueueIncomingPublish(kTopic, "x");
    EXPECT_EQ(client->processLoop(kPumpMs), ESP_OK);

    EXPECT_TRUE(callbackRan);
    EXPECT_EQ(subscribeResult, ESP_OK);
    EXPECT_EQ(unsubscribeResult, ESP_OK);
}

// The production trigger: streaming installs an MQTT-backed log sink, then a
// callback logs. The QueuedSink makes the in-callback write enqueue-only; the
// actual publish happens from poll() outside every lock.
TEST_F(CoreMqttClientReentrancyTest, CallbackLogsThroughQueuedMqttSink_NoDeadlock)
{
    std::atomic<int> published{0};

    auto queued = std::make_unique<lopcore::QueuedSink>(
        std::make_unique<lopcore::MqttLogSink>([&](const std::string &line) {
            published++;
            client->publishString("dt/test/logs", line);
        }));
    lopcore::QueuedSink *sink = queued.get();
    lopcore::Logger::getInstance().addSink(std::move(queued));

    std::atomic<bool> callbackRan{false};
    ASSERT_EQ(client->subscribe(kTopic,
                                [&](const MqttMessage &) {
                                    callbackRan = true;
                                    // Logger mutex + client mutex are both held on
                                    // this thread here; write() must only enqueue.
                                    LOPCORE_LOGI("reentrancy_test", "log line from inside a callback");
                                }),
              ESP_OK);

    MockCoreMqtt::enqueueIncomingPublish(kTopic, "x");
    EXPECT_EQ(client->processLoop(kPumpMs), ESP_OK);
    EXPECT_TRUE(callbackRan);
    EXPECT_EQ(published.load(), 0); // nothing published synchronously

    // Drain outside the locks: the publish goes through the client normally
    EXPECT_GE(sink->poll(), static_cast<size_t>(1));
    EXPECT_GE(published.load(), 1);

    EXPECT_TRUE(lopcore::Logger::getInstance().removeSink(sink));
}

TEST_F(CoreMqttClientReentrancyTest, ReentrantProcessLoop_Rejected)
{
    esp_err_t nestedResult = ESP_OK;

    ASSERT_EQ(client->subscribe(kTopic, [&](const MqttMessage &) { nestedResult = client->processLoop(5); }),
              ESP_OK);

    MockCoreMqtt::enqueueIncomingPublish(kTopic, "x");
    EXPECT_EQ(client->processLoop(kPumpMs), ESP_OK);

    // Re-entering MQTT_ProcessLoop would reuse the RX buffer mid-parse; the
    // client must reject it instead of recursing.
    EXPECT_EQ(nestedResult, ESP_ERR_INVALID_STATE);
}

// The manual-poll request-response contract from mqtt_traits.hpp: a flag set
// inside the callback is visible when processLoop() returns.
TEST_F(CoreMqttClientReentrancyTest, RequestResponse_FlagVisibleAfterProcessLoop)
{
    bool responseReceived = false;

    ASSERT_EQ(client->subscribe(kTopic, [&](const MqttMessage &) { responseReceived = true; }), ESP_OK);

    MockCoreMqtt::enqueueIncomingPublish(kTopic, "response");
    ASSERT_EQ(client->processLoop(kPumpMs), ESP_OK);

    EXPECT_TRUE(responseReceived);
}

// Cross-thread sanity: one thread pumps processLoop while another publishes.
// The recursive mutex still serializes coreMQTT access across threads; a
// lock-ordering regression shows up as a hang (CTest timeout).
TEST_F(CoreMqttClientReentrancyTest, ConcurrentPumpAndPublish_NoDeadlock)
{
    std::atomic<int> received{0};
    std::atomic<bool> stop{false};

    ASSERT_EQ(client->subscribe(kTopic,
                                [&](const MqttMessage &) {
                                    received++;
                                    client->publishString("dt/test/ack", "ok");
                                }),
              ESP_OK);

    std::thread pumper([&] {
        while (!stop)
        {
            client->processLoop(5);
        }
    });

    std::thread publisher([&] {
        for (int i = 0; i < 50; i++)
        {
            client->publishString("dt/test/out", "payload");
        }
    });

    for (int i = 0; i < 20; i++)
    {
        MockCoreMqtt::enqueueIncomingPublish(kTopic, "in");
    }

    publisher.join();

    // Give the pumper time to drain the queued messages, then stop it
    for (int i = 0; i < 200 && received.load() < 20; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    stop = true;
    pumper.join();

    EXPECT_EQ(received.load(), 20);
}

// disconnect() from inside a callback is tolerated: teardown proceeds and the
// in-flight processLoop() returns an error instead of hanging or self-deleting.
TEST_F(CoreMqttClientReentrancyTest, DisconnectFromCallback_NoHang)
{
    std::atomic<bool> callbackRan{false};

    ASSERT_EQ(client->subscribe(kTopic,
                                [&](const MqttMessage &) {
                                    callbackRan = true;
                                    client->disconnect();
                                }),
              ESP_OK);

    MockCoreMqtt::enqueueIncomingPublish(kTopic, "x");
    const esp_err_t result = client->processLoop(kPumpMs);

    EXPECT_TRUE(callbackRan);
    EXPECT_NE(result, ESP_OK); // connection is gone mid-loop
    EXPECT_FALSE(client->isConnected());
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
