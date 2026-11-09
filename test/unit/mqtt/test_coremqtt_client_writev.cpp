/**
 * @file test_coremqtt_client_writev.cpp
 * @brief CoreMqttClient gives coreMQTT a writev that joins one packet's pieces into one
 *        transport write. Without it every piece is its own TLS record and TCP segment,
 *        and Nagle holds the later ones until the first is acknowledged (issue #15).
 *
 * The tests play coreMQTT's part: sendMessageVector() advances its vectors by the count
 * a call returns (whole vectors dropped, a partial one moved forward) and calls again.
 */

#include <cstring>
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

/** The three pieces coreMQTT sends for a CONNECT: fixed header, client-id length, client id. */
struct ConnectPieces
{
    std::vector<uint8_t> header{0x10, 0x16, 0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x02, 0x00, 0x3C};
    std::vector<uint8_t> idLength{0x00, 0x0A};
    std::string clientId{"device-001"};

    std::vector<TransportOutVector_t> vectors()
    {
        return {{header.data(), header.size()},
                {idLength.data(), idLength.size()},
                {clientId.data(), clientId.size()}};
    }

    std::vector<uint8_t> joined() const
    {
        std::vector<uint8_t> all(header);
        all.insert(all.end(), idLength.begin(), idLength.end());
        all.insert(all.end(), clientId.begin(), clientId.end());
        return all;
    }
};

std::vector<uint8_t> tail(const std::vector<uint8_t> &data, size_t count)
{
    return std::vector<uint8_t>(data.end() - static_cast<std::ptrdiff_t>(count), data.end());
}

} // namespace

class CoreMqttClientWritevTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        transport = std::make_shared<lopcore::test::MockTlsTransport>();
        transport->connect(lopcore::tls::TlsConfig{});
        ASSERT_TRUE(transport->isConnected());
    }

    MqttConfigBuilder baseConfig()
    {
        return MqttConfig::builder().broker("test-broker.example.com").port(8883).clientId("device-001");
    }

    /** Build a client; MockCoreMqtt::lastInitTransport() then holds the interface it installed. */
    std::unique_ptr<CoreMqttClient> makeClient(const MqttConfig &config)
    {
        MockCoreMqtt::lastInitTransport() = TransportInterface_t{};
        return std::make_unique<CoreMqttClient>(config, transport);
    }

    std::shared_ptr<lopcore::test::MockTlsTransport> transport;
};

TEST_F(CoreMqttClientWritevTest, InstallsWritevNextToSendAndRecv)
{
    auto client = makeClient(baseConfig().build());

    const TransportInterface_t &iface = MockCoreMqtt::lastInitTransport();
    EXPECT_NE(iface.send, nullptr);
    EXPECT_NE(iface.recv, nullptr);
    EXPECT_NE(iface.writev, nullptr);
}

TEST_F(CoreMqttClientWritevTest, JoinsThePiecesOfOnePacketIntoOneWrite)
{
    auto client = makeClient(baseConfig().build());
    const TransportInterface_t &iface = MockCoreMqtt::lastInitTransport();
    ASSERT_NE(iface.writev, nullptr);

    ConnectPieces connect;
    std::vector<TransportOutVector_t> vectors = connect.vectors();
    const int32_t written = iface.writev(iface.pNetworkContext, vectors.data(), vectors.size());

    EXPECT_EQ(written, static_cast<int32_t>(connect.joined().size()));
    EXPECT_EQ(transport->getSendCallCount(), 1);
    EXPECT_EQ(transport->getSentData(), connect.joined());
}

TEST_F(CoreMqttClientWritevTest, StopsBeforeAPieceThatDoesNotFitAndLetsCoreMqttContinue)
{
    // 13 bytes hold the 12-byte header; the 2-byte id length would not fit any more.
    auto client = makeClient(baseConfig().sendCoalesceBufferSize(13).build());
    const TransportInterface_t &iface = MockCoreMqtt::lastInitTransport();
    ASSERT_NE(iface.writev, nullptr);

    ConnectPieces connect;
    std::vector<TransportOutVector_t> vectors = connect.vectors();
    const int32_t first = iface.writev(iface.pNetworkContext, vectors.data(), vectors.size());
    EXPECT_EQ(first, static_cast<int32_t>(connect.header.size()));

    // coreMQTT drops the header vector and calls again with the two pieces left.
    const int32_t second = iface.writev(iface.pNetworkContext, &vectors[1], 2);
    EXPECT_EQ(second, static_cast<int32_t>(connect.idLength.size() + connect.clientId.size()));

    EXPECT_EQ(transport->getSendCallCount(), 2);
    EXPECT_EQ(transport->getSentData(), connect.joined());
}

TEST_F(CoreMqttClientWritevTest, HandsAPieceTheBufferCannotHoldOverWhole)
{
    auto client = makeClient(baseConfig().sendCoalesceBufferSize(8).build());
    const TransportInterface_t &iface = MockCoreMqtt::lastInitTransport();
    ASSERT_NE(iface.writev, nullptr);

    ConnectPieces connect; // the 12-byte header alone exceeds the 8-byte buffer
    std::vector<TransportOutVector_t> vectors = connect.vectors();
    const int32_t written = iface.writev(iface.pNetworkContext, vectors.data(), vectors.size());

    // Uncut and uncopied: the transport sees the piece itself, mbedTLS splits it if it must.
    EXPECT_EQ(written, static_cast<int32_t>(connect.header.size()));
    EXPECT_EQ(transport->getSendCallCount(), 1);
    EXPECT_EQ(transport->getSentData(), connect.header);
}

TEST_F(CoreMqttClientWritevTest, ASinglePieceGoesOutWholeBeyondTheBuffer)
{
    auto client = makeClient(baseConfig().sendCoalesceBufferSize(16).build());
    const TransportInterface_t &iface = MockCoreMqtt::lastInitTransport();
    ASSERT_NE(iface.writev, nullptr);

    const std::vector<uint8_t> payload(1000, 0xA5);
    TransportOutVector_t vector{payload.data(), payload.size()};

    EXPECT_EQ(iface.writev(iface.pNetworkContext, &vector, 1), 1000);
    EXPECT_EQ(transport->getSendCallCount(), 1);
    EXPECT_EQ(transport->getSentData(), payload);
}

TEST_F(CoreMqttClientWritevTest, ContinuesInsideAPieceAfterAShortTlsWrite)
{
    auto client = makeClient(baseConfig().build());
    const TransportInterface_t &iface = MockCoreMqtt::lastInitTransport();
    ASSERT_NE(iface.writev, nullptr);

    ConnectPieces connect;
    const std::vector<uint8_t> all = connect.joined();
    std::vector<TransportOutVector_t> vectors = connect.vectors();
    transport->enqueueSendResult(ESP_OK, 5); // mbedTLS wrote only the first 5 bytes

    EXPECT_EQ(iface.writev(iface.pNetworkContext, vectors.data(), vectors.size()), 5);

    // coreMQTT moves the header vector 5 bytes forward and calls again.
    vectors[0].iov_base = connect.header.data() + 5;
    vectors[0].iov_len = connect.header.size() - 5;
    const int32_t rest = iface.writev(iface.pNetworkContext, vectors.data(), vectors.size());

    EXPECT_EQ(rest, static_cast<int32_t>(all.size() - 5));
    EXPECT_EQ(tail(transport->getSentData(), all.size() - 5), tail(all, all.size() - 5));
}

TEST_F(CoreMqttClientWritevTest, ZeroBytesFromTheTransportMeansRetry)
{
    auto client = makeClient(baseConfig().build());
    const TransportInterface_t &iface = MockCoreMqtt::lastInitTransport();
    ASSERT_NE(iface.writev, nullptr);

    ConnectPieces connect;
    std::vector<TransportOutVector_t> vectors = connect.vectors();
    transport->enqueueSendResult(ESP_OK, 0); // WANT_WRITE: nothing went out yet

    EXPECT_EQ(iface.writev(iface.pNetworkContext, vectors.data(), vectors.size()), 0);

    // The retry rebuilds the same bytes, as mbedTLS requires after WANT_WRITE.
    EXPECT_EQ(iface.writev(iface.pNetworkContext, vectors.data(), vectors.size()),
              static_cast<int32_t>(connect.joined().size()));
    EXPECT_EQ(tail(transport->getSentData(), connect.joined().size()), connect.joined());
}

TEST_F(CoreMqttClientWritevTest, ReportsATransportFailureAsNegative)
{
    auto client = makeClient(baseConfig().build());
    const TransportInterface_t &iface = MockCoreMqtt::lastInitTransport();
    ASSERT_NE(iface.writev, nullptr);

    ConnectPieces connect;
    std::vector<TransportOutVector_t> vectors = connect.vectors();
    transport->enqueueSendResult(ESP_FAIL, 0);

    EXPECT_LT(iface.writev(iface.pNetworkContext, vectors.data(), vectors.size()), 0);
}

TEST_F(CoreMqttClientWritevTest, SizeZeroLeavesCoreMqttSendingEachPieceOnItsOwn)
{
    auto client = makeClient(baseConfig().sendCoalesceBufferSize(0).build());

    const TransportInterface_t &iface = MockCoreMqtt::lastInitTransport();
    EXPECT_NE(iface.send, nullptr);
    EXPECT_EQ(iface.writev, nullptr);
}
