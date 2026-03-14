#include "lopcore/prov/ble_data_framer.hpp"
#include "lopcore/logging/logger.hpp"
#include <cstring>
#include <algorithm>

namespace lopcore {
namespace prov {

BleDataFramer::BleDataFramer(size_t maxPayloadSize)
    : maxPayloadSize_(maxPayloadSize)
    , buffer_()
    , expectedPayloadSize_(0)
    , receivedBytes_(0)
    , lengthReceived_(false)
    , complete_(false)
    , error_(false)
{
    // Pre-allocate buffer to avoid reallocations
    buffer_.reserve(maxPayloadSize_ + 4);  // +4 for length prefix
    
    LOPCORE_LOGD("BleDataFramer", "Initialized with max payload: %zu bytes", maxPayloadSize_);
}

FrameResult BleDataFramer::onData(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) {
        LOPCORE_LOGW("BleDataFramer", "Received null or zero-length data");
        return FrameResult::ERROR_INVALID;
    }

    if (error_) {
        LOPCORE_LOGW("BleDataFramer", "Framer in error state, call reset() first");
        return FrameResult::ERROR_INVALID;
    }

    if (complete_) {
        LOPCORE_LOGW("BleDataFramer", "Frame already complete, call reset() first");
        return FrameResult::ERROR_INVALID;
    }

    // Append to buffer
    buffer_.insert(buffer_.end(), data, data + length);
    receivedBytes_ += length;

    LOPCORE_LOGV("BleDataFramer", "Received chunk: %zu bytes (total: %zu)", length, receivedBytes_);

    // Parse length prefix if not yet done
    if (!lengthReceived_) {
        if (buffer_.size() < 4) {
            // Need at least 4 bytes for length prefix
            LOPCORE_LOGV("BleDataFramer", "Waiting for length prefix (have %zu/4 bytes)", buffer_.size());
            return FrameResult::NEED_MORE;
        }

        FrameResult parseResult = parseLength();
        if (parseResult != FrameResult::COMPLETE) {
            error_ = true;
            return parseResult; // ERROR_INVALID (zero length) or ERROR_TOO_LARGE (oversized)
        }

        lengthReceived_ = true;
        LOPCORE_LOGD("BleDataFramer", "Payload length parsed: %zu bytes", expectedPayloadSize_);
    }

    // Check if we have complete payload
    // Total size = 4 bytes (length) + payload
    size_t expectedTotalSize = 4 + expectedPayloadSize_;
    
    if (buffer_.size() >= expectedTotalSize) {
        // Complete!
        complete_ = true;
        LOPCORE_LOGI("BleDataFramer", "Frame complete: %zu bytes payload", expectedPayloadSize_);
        return FrameResult::COMPLETE;
    }

    // More chunks needed
    LOPCORE_LOGV("BleDataFramer", "Progress: %zu / %zu bytes", buffer_.size(), expectedTotalSize);
    return FrameResult::NEED_MORE;
}

const uint8_t* BleDataFramer::payload() const {
    if (!complete_ || buffer_.size() < 4) {
        return nullptr;
    }
    // Skip first 4 bytes (length prefix)
    return buffer_.data() + 4;
}

size_t BleDataFramer::payloadSize() const {
    if (!complete_) {
        return 0;
    }
    return expectedPayloadSize_;
}

bool BleDataFramer::isComplete() const {
    return complete_;
}

bool BleDataFramer::hasError() const {
    return error_;
}

void BleDataFramer::reset() {
    buffer_.clear();
    expectedPayloadSize_ = 0;
    receivedBytes_ = 0;
    lengthReceived_ = false;
    complete_ = false;
    error_ = false;
    
    LOPCORE_LOGV("BleDataFramer", "Reset");
}

FrameResult BleDataFramer::parseLength() {
    if (buffer_.size() < 4) {
        return FrameResult::ERROR_INVALID;
    }

    // Read first 4 bytes as little-endian uint32_t
    expectedPayloadSize_ =
        static_cast<uint32_t>(buffer_[0]) |
        (static_cast<uint32_t>(buffer_[1]) << 8) |
        (static_cast<uint32_t>(buffer_[2]) << 16) |
        (static_cast<uint32_t>(buffer_[3]) << 24);

    if (expectedPayloadSize_ == 0) {
        LOPCORE_LOGE("BleDataFramer", "Invalid payload length: 0");
        return FrameResult::ERROR_INVALID;
    }

    if (expectedPayloadSize_ > maxPayloadSize_) {
        LOPCORE_LOGE("BleDataFramer", "Payload too large: %zu > %zu (max)",
                     expectedPayloadSize_, maxPayloadSize_);
        return FrameResult::ERROR_TOO_LARGE;
    }

    return FrameResult::COMPLETE; // Sentinel: success
}

} // namespace prov
} // namespace lopcore
