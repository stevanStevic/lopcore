/**
 * @file test_coremqtt_client_publish_confirmed.cpp
 * @brief publishConfirmed(): a QoS 1 publish that reports the broker's PUBACK
 *
 * Drives the real coremqtt_client.cpp against the host mocks with a manual
 * pump (the mock task runner spawns no threads), so publishConfirmed() runs
 * processLoop() itself while it waits. The mock MQTT_ProcessLoop delivers
 * queued PUBACKs to the event callback like the real library.
 */

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "mqtt/coremqtt_client.hpp"
#include "mqtt/mqtt_config.hpp"
#include "tls/mock_tls_transport.hpp"

using namespace lopcore::mqtt;

namespace
{
constexpr const char *kTopic = "dt/test/records";
const std::vector<uint8_t> kPayload{0x01, 0x02, 0x03};
} // namespace

class CoreMqttClientPublishConfirmedTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MockCoreMqtt::clearIncomingPublishes();
        MockCoreMqtt::resetAcks();

        config = MqttConfig::builder()
                     .broker("test-broker.example.com")
                     .port(8883)
                     .clientId("confirmed-test")
                     .autoStartProcessLoop(false)
                     .build();

        transport = std::make_shared<lopcore::test::MockTlsTransport>();
        transport->connect(lopcore::tls::TlsConfig{});

        client = std::make_unique<CoreMqttClient>(config, transport);
        ASSERT_EQ(client->connect(), ESP_OK);
    }

    void TearDown() override
    {
        MockCoreMqtt::clearIncomingPublishes();
        MockCoreMqtt::resetAcks();
    }

    MqttConfig config;
    std::shared_ptr<lopcore::test::MockTlsTransport> transport;
    std::unique_ptr<CoreMqttClient> client;
};

TEST_F(CoreMqttClientPublishConfirmedTest, PubAckReturnsOk)
{
    MockCoreMqtt::setAutoPubAck(true);
    EXPECT_EQ(client->publishConfirmed(kTopic, kPayload, 500), ESP_OK);
}

// The mock clock advances 1 ms per read, so the wait ends in mock time, not wall time.
TEST_F(CoreMqttClientPublishConfirmedTest, NoPubAckTimesOut)
{
    EXPECT_EQ(client->publishConfirmed(kTopic, kPayload, 100), ESP_ERR_TIMEOUT);
}

TEST_F(CoreMqttClientPublishConfirmedTest, PubAckForAnotherPacketDoesNotConfirm)
{
    // An earlier QoS 1 publish whose PUBACK arrives while the confirmed one waits.
    ASSERT_EQ(client->publish("dt/test/other", kPayload, MqttQos::AT_LEAST_ONCE), ESP_OK);
    MockCoreMqtt::enqueuePubAck(MockCoreMqtt::lastPublishedPacketId());
    EXPECT_EQ(client->publishConfirmed(kTopic, kPayload, 100), ESP_ERR_TIMEOUT);
}

TEST_F(CoreMqttClientPublishConfirmedTest, NotConnectedIsInvalidState)
{
    ASSERT_EQ(client->disconnect(), ESP_OK);
    EXPECT_EQ(client->publishConfirmed(kTopic, kPayload, 100), ESP_ERR_INVALID_STATE);
}

TEST_F(CoreMqttClientPublishConfirmedTest, FromMessageCallbackIsRejected)
{
    // Waiting inside a callback would need the processLoop() that is running it.
    esp_err_t result = ESP_OK;
    ASSERT_EQ(client->subscribe("dt/test/in",
                                [&](const MqttMessage &) {
                                    result = client->publishConfirmed(kTopic, kPayload, 100);
                                }),
              ESP_OK);
    MockCoreMqtt::enqueueIncomingPublish("dt/test/in", "x");
    EXPECT_EQ(client->processLoop(20), ESP_OK);
    EXPECT_EQ(result, ESP_ERR_INVALID_STATE);
}

TEST_F(CoreMqttClientPublishConfirmedTest, SecondConfirmedPublishAfterFirstSucceeds)
{
    MockCoreMqtt::setAutoPubAck(true);
    EXPECT_EQ(client->publishConfirmed(kTopic, kPayload, 500), ESP_OK);
    EXPECT_EQ(client->publishConfirmed(kTopic, kPayload, 500), ESP_OK);
}

TEST_F(CoreMqttClientPublishConfirmedTest, LateAckOfTimedOutPublishDoesNotConfirmTheNext)
{
    ASSERT_EQ(client->publishConfirmed(kTopic, kPayload, 50), ESP_ERR_TIMEOUT);
    MockCoreMqtt::enqueuePubAck(MockCoreMqtt::lastPublishedPacketId()); // the first one's late PUBACK
    EXPECT_EQ(client->publishConfirmed(kTopic, kPayload, 100), ESP_ERR_TIMEOUT);
}

TEST_F(CoreMqttClientPublishConfirmedTest, DisconnectWhileWaitingIsInvalidState)
{
    // Something seen during the wait drops the connection.
    ASSERT_EQ(client->subscribe("dt/test/in", [&](const MqttMessage &) { client->disconnect(); }), ESP_OK);
    MockCoreMqtt::enqueueIncomingPublish("dt/test/in", "x");
    EXPECT_EQ(client->publishConfirmed(kTopic, kPayload, 500), ESP_ERR_INVALID_STATE);
}

TEST_F(CoreMqttClientPublishConfirmedTest, PubAckFromANewSessionDoesNotConfirm)
{
    // The session the packet went out on ends and a new one starts between two
    // checks; a PUBACK carrying the same id then belongs to the new session.
    ASSERT_EQ(client->subscribe("dt/test/in",
                                [&](const MqttMessage &) {
                                    const uint16_t awaited = MockCoreMqtt::lastPublishedPacketId();
                                    client->disconnect();
                                    transport->connect(lopcore::tls::TlsConfig{});
                                    client->connect();
                                    MockCoreMqtt::enqueuePubAck(awaited);
                                }),
              ESP_OK);
    MockCoreMqtt::enqueueIncomingPublish("dt/test/in", "x");
    EXPECT_EQ(client->publishConfirmed(kTopic, kPayload, 500), ESP_ERR_INVALID_STATE);
}

TEST_F(CoreMqttClientPublishConfirmedTest, PumpsAgainAfterAForceDeletedTask)
{
    // The mock never runs a created task, so disconnect() times out waiting for
    // it and force-deletes it. A later session without a task must pump itself.
    ASSERT_EQ(client->disconnect(), ESP_OK);
    ASSERT_EQ(client->setAutoProcessing(true), ESP_ERR_INVALID_STATE); // not connected: no task yet
    transport->connect(lopcore::tls::TlsConfig{});
    ASSERT_EQ(client->connect(), ESP_OK);
    ASSERT_EQ(client->startProcessLoopTask(), ESP_OK);
    ASSERT_EQ(client->disconnect(), ESP_OK); // force-deletes the never-run task
    transport->connect(lopcore::tls::TlsConfig{});
    ASSERT_EQ(client->connect(), ESP_OK); // autoStartProcessLoop is false: no task
    MockCoreMqtt::setAutoPubAck(true);
    EXPECT_EQ(client->publishConfirmed(kTopic, kPayload, 500), ESP_OK);
}
