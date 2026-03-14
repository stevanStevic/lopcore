#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>

namespace lopcore {
namespace prov {

/**
 * Frame Result Enumeration
 * 
 * Status returned after feeding data to the framer.
 */
enum class FrameResult {
    NEED_MORE,          // Need more chunks to complete the frame
    COMPLETE,           // Frame complete, payload ready
    ERROR_TOO_LARGE,    // Payload exceeds maximum allowed size
    ERROR_INVALID       // Invalid frame structure or data
};

/**
 * BLE Data Framer
 * 
 * Length-prefixed framing for reliable BLE data transfer.
 * 
 * Framing Protocol:
 * - First 4 bytes: uint32_t total payload length (little-endian)
 * - Remaining bytes: payload data (chunked across multiple BLE writes)
 * 
 * Example:
 *   Chunk 1: [0x00][0x04][0x00][0x00] [A][B][C][D] ... (length=1024)
 *   Chunk 2: [E][F][G][H] ...
 *   ...
 *   Chunk N: ... [Y][Z]
 * 
 * Usage:
 * ```cpp
 * BleDataFramer framer(8192);  // Max 8KB payload
 * 
 * // Feed chunks as they arrive
 * FrameResult result = framer.onData(chunk1, chunk1_len);
 * if (result == FrameResult::NEED_MORE) {
 *     result = framer.onData(chunk2, chunk2_len);
 * }
 * 
 * if (result == FrameResult::COMPLETE) {
 *     const uint8_t* payload = framer.payload();
 *     size_t len = framer.payloadSize();
 *     // Process complete payload
 *     framer.reset();  // Ready for next frame
 * }
 * ```
 */
class BleDataFramer {
public:
    /**
     * Constructor
     * 
     * @param maxPayloadSize Maximum allowed payload size (bytes)
     */
    explicit BleDataFramer(size_t maxPayloadSize);

    /**
     * Destructor
     */
    ~BleDataFramer() = default;

    /**
     * Feed a data chunk
     * 
     * Accumulates chunks and checks for completion.
     * 
     * @param data Received data chunk
     * @param length Chunk length
     * @return Frame status (NEED_MORE, COMPLETE, ERROR_*)
     */
    FrameResult onData(const uint8_t* data, size_t length);

    /**
     * Get complete payload pointer
     * 
     * Only valid when isComplete() == true.
     * 
     * @return Pointer to complete payload
     */
    const uint8_t* payload() const;

    /**
     * Get complete payload size
     * 
     * Only valid when isComplete() == true.
     * 
     * @return Payload size in bytes
     */
    size_t payloadSize() const;

    /**
     * Check if frame is complete
     * 
     * @return true if complete payload received
     */
    bool isComplete() const;

    /**
     * Check if error occurred
     * 
     * @return true if error state
     */
    bool hasError() const;

    /**
     * Reset framer state
     * 
     * Clears accumulated data and prepares for next frame.
     */
    void reset();

private:
    size_t maxPayloadSize_;                 // Maximum allowed payload size
    std::vector<uint8_t> buffer_;           // Accumulated data buffer
    size_t expectedPayloadSize_;            // Declared total payload size
    size_t receivedBytes_;                  // Bytes received so far
    bool lengthReceived_;                   // Has length prefix been parsed?
    bool complete_;                         // Is frame complete?
    bool error_;                            // Is framer in error state?

    /**
     * Parse length prefix from buffer
     *
     * Reads first 4 bytes as little-endian uint32_t.
     *
     * @return COMPLETE on success, ERROR_INVALID for zero-length,
     *         ERROR_TOO_LARGE if payload exceeds maxPayloadSize_
     */
    FrameResult parseLength();
};

} // namespace prov
} // namespace lopcore
