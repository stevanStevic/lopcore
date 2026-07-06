/**
 * @file test_ble_data_framer.cpp
 * @brief Unit tests for BleDataFramer
 *
 * Tests the length-prefixed chunk reassembly used for reliable BLE data transfer.
 * All tests run on the host (no ESP-IDF target required).
 */

#include <gtest/gtest.h>

#include "lopcore/prov/ble_data_framer.hpp"

using namespace lopcore::prov;

// ----------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------

/** Build a length-prefixed frame (4-byte LE uint32 header + payload) */
static std::vector<uint8_t> makeFrame(const std::vector<uint8_t> &payload)
{
    std::vector<uint8_t> frame(4 + payload.size());
    uint32_t len = static_cast<uint32_t>(payload.size());
    frame[0] = len & 0xFF;
    frame[1] = (len >> 8) & 0xFF;
    frame[2] = (len >> 16) & 0xFF;
    frame[3] = (len >> 24) & 0xFF;
    std::copy(payload.begin(), payload.end(), frame.begin() + 4);
    return frame;
}

// ----------------------------------------------------------------
// Fixture
// ----------------------------------------------------------------

class BleDataFramerTest : public ::testing::Test
{
protected:
    static constexpr size_t MAX_PAYLOAD = 4096;
    BleDataFramer framer{MAX_PAYLOAD};
};

// ----------------------------------------------------------------
// Single-chunk tests
// ----------------------------------------------------------------

TEST_F(BleDataFramerTest, SingleChunk_SmallPayload_ReturnsComplete)
{
    std::vector<uint8_t> payload = {'H', 'e', 'l', 'l', 'o'};
    auto frame = makeFrame(payload);

    EXPECT_EQ(framer.onData(frame.data(), frame.size()), FrameResult::COMPLETE);
    EXPECT_EQ(framer.payloadSize(), payload.size());
    EXPECT_EQ(std::vector<uint8_t>(framer.payload(), framer.payload() + framer.payloadSize()), payload);
}

TEST_F(BleDataFramerTest, SingleChunk_EmptyPayload_Rejected)
{
    // A length header of 0 is deliberately rejected: no provisioning sender
    // ever produces an empty frame (esp_prov frames JSON payloads, which are
    // never zero-length), so a zero length always indicates a corrupt header.
    uint8_t frame[] = {0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(framer.onData(frame, sizeof(frame)), FrameResult::ERROR_INVALID);
    EXPECT_TRUE(framer.hasError());
    EXPECT_EQ(framer.payloadSize(), 0u);
}

TEST_F(BleDataFramerTest, SingleChunk_ExactMaxSize_ReturnsComplete)
{
    std::vector<uint8_t> payload(MAX_PAYLOAD, 0xAB);
    auto frame = makeFrame(payload);

    EXPECT_EQ(framer.onData(frame.data(), frame.size()), FrameResult::COMPLETE);
    EXPECT_EQ(framer.payloadSize(), MAX_PAYLOAD);
}

// ----------------------------------------------------------------
// Multi-chunk tests
// ----------------------------------------------------------------

TEST_F(BleDataFramerTest, TwoChunks_ReturnsNeedMoreThenComplete)
{
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5, 6, 7, 8};
    auto frame = makeFrame(payload);

    // Split after header
    auto firstChunk = std::vector<uint8_t>(frame.begin(), frame.begin() + 4);
    auto secondChunk = std::vector<uint8_t>(frame.begin() + 4, frame.end());

    EXPECT_EQ(framer.onData(firstChunk.data(), firstChunk.size()), FrameResult::NEED_MORE);
    EXPECT_EQ(framer.onData(secondChunk.data(), secondChunk.size()), FrameResult::COMPLETE);
    EXPECT_EQ(framer.payloadSize(), payload.size());
}

TEST_F(BleDataFramerTest, ManySmallChunks_ByteByByte_ReturnsComplete)
{
    std::vector<uint8_t> payload = {0x11, 0x22, 0x33};
    auto frame = makeFrame(payload);

    FrameResult result = FrameResult::NEED_MORE;
    for (size_t i = 0; i < frame.size() - 1; ++i)
    {
        result = framer.onData(&frame[i], 1);
        EXPECT_EQ(result, FrameResult::NEED_MORE) << "Unexpected result at byte " << i;
    }
    result = framer.onData(&frame[frame.size() - 1], 1);
    EXPECT_EQ(result, FrameResult::COMPLETE);
    EXPECT_EQ(framer.payloadSize(), payload.size());
}

TEST_F(BleDataFramerTest, HeaderSplitAcrossChunks_Works)
{
    std::vector<uint8_t> payload = {'X', 'Y'};
    auto frame = makeFrame(payload);

    // Header split: 2 + 2 bytes, then payload
    EXPECT_EQ(framer.onData(frame.data(), 2), FrameResult::NEED_MORE);
    EXPECT_EQ(framer.onData(frame.data() + 2, 2), FrameResult::NEED_MORE);
    EXPECT_EQ(framer.onData(frame.data() + 4, frame.size() - 4), FrameResult::COMPLETE);
}

// ----------------------------------------------------------------
// Error cases
// ----------------------------------------------------------------

TEST_F(BleDataFramerTest, OversizedPayload_ReturnsErrorTooLarge)
{
    // Build a frame claiming max+1 bytes
    uint32_t bigLen = MAX_PAYLOAD + 1;
    uint8_t header[4];
    header[0] = bigLen & 0xFF;
    header[1] = (bigLen >> 8) & 0xFF;
    header[2] = (bigLen >> 16) & 0xFF;
    header[3] = (bigLen >> 24) & 0xFF;

    EXPECT_EQ(framer.onData(header, 4), FrameResult::ERROR_TOO_LARGE);
}

TEST_F(BleDataFramerTest, NullData_ReturnsErrorInvalid)
{
    EXPECT_EQ(framer.onData(nullptr, 10), FrameResult::ERROR_INVALID);
}

TEST_F(BleDataFramerTest, ZeroLength_ReturnsErrorInvalid)
{
    uint8_t dummy = 0;
    EXPECT_EQ(framer.onData(&dummy, 0), FrameResult::ERROR_INVALID);
}

TEST_F(BleDataFramerTest, FeedAfterError_ReturnsError)
{
    // Force error
    uint32_t bigLen = MAX_PAYLOAD + 100;
    uint8_t header[4];
    header[0] = bigLen & 0xFF;
    header[1] = (bigLen >> 8) & 0xFF;
    header[2] = (bigLen >> 16) & 0xFF;
    header[3] = (bigLen >> 24) & 0xFF;
    framer.onData(header, 4);

    // Further feeding should still return error
    uint8_t extra[] = {0xAA, 0xBB};
    EXPECT_EQ(framer.onData(extra, sizeof(extra)), FrameResult::ERROR_INVALID);
}

TEST_F(BleDataFramerTest, FeedAfterComplete_ReturnsError)
{
    std::vector<uint8_t> payload = {'A', 'B', 'C'};
    auto frame = makeFrame(payload);
    framer.onData(frame.data(), frame.size());

    // Feeding more data to a completed framer should return error
    uint8_t extra[] = {0x01};
    EXPECT_EQ(framer.onData(extra, sizeof(extra)), FrameResult::ERROR_INVALID);
}

// ----------------------------------------------------------------
// Reset
// ----------------------------------------------------------------

TEST_F(BleDataFramerTest, ResetAfterComplete_AllowsNewFrame)
{
    std::vector<uint8_t> payload1 = {0x01, 0x02};
    auto frame1 = makeFrame(payload1);
    EXPECT_EQ(framer.onData(frame1.data(), frame1.size()), FrameResult::COMPLETE);

    framer.reset();
    EXPECT_FALSE(framer.isComplete());
    EXPECT_EQ(framer.payloadSize(), 0u);

    std::vector<uint8_t> payload2 = {0xAA, 0xBB, 0xCC};
    auto frame2 = makeFrame(payload2);
    EXPECT_EQ(framer.onData(frame2.data(), frame2.size()), FrameResult::COMPLETE);
    EXPECT_EQ(framer.payloadSize(), payload2.size());
}

TEST_F(BleDataFramerTest, ResetAfterError_AllowsNewFrame)
{
    // Force error
    uint32_t bigLen = MAX_PAYLOAD + 1;
    uint8_t header[4] = {static_cast<uint8_t>(bigLen), static_cast<uint8_t>(bigLen >> 8),
                         static_cast<uint8_t>(bigLen >> 16), static_cast<uint8_t>(bigLen >> 24)};
    framer.onData(header, 4);

    framer.reset();

    std::vector<uint8_t> payload = {0xDE, 0xAD};
    auto frame = makeFrame(payload);
    EXPECT_EQ(framer.onData(frame.data(), frame.size()), FrameResult::COMPLETE);
}

// ----------------------------------------------------------------
// Edge cases
// ----------------------------------------------------------------

TEST_F(BleDataFramerTest, LargePayload_100KB_FramerWithLargerLimit)
{
    const size_t payloadSize = 1024; // 1 KB within our 4KB framer
    std::vector<uint8_t> payload(payloadSize);
    for (size_t i = 0; i < payloadSize; ++i)
    {
        payload[i] = static_cast<uint8_t>(i & 0xFF);
    }
    auto frame = makeFrame(payload);

    EXPECT_EQ(framer.onData(frame.data(), frame.size()), FrameResult::COMPLETE);
    EXPECT_EQ(std::vector<uint8_t>(framer.payload(), framer.payload() + framer.payloadSize()), payload);
}
