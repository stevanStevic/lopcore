#pragma once

#include <cstdint>
#include <cstddef>

namespace lopcore {
namespace prov {

/**
 * Endpoint protocol enumeration
 * 
 * Defines the protocol used for data exchange over BLE endpoints.
 */
enum class EndpointProtocol {
    JSON_LENGTH_PREFIXED,  // JSON with 4-byte length prefix (NO EOF markers!)
    PROTOBUF,              // Protocol Buffers (binary, type-safe)
    CBOR,                  // Concise Binary Object Representation
    RAW                    // Raw bytes (application-defined format)
};

/**
 * Custom Endpoint Handler Interface
 * 
 * Implement this interface to handle custom BLE characteristics during
 * WiFi provisioning. Allows receiving arbitrary data from mobile app
 * (e.g., AWS credentials, mesh config, OTA URLs).
 * 
 * Characteristics:
 * - Stateful: Can accumulate chunks across multiple BLE writes
 * - Protocol-aware: Supports JSON, Protobuf, CBOR, or raw bytes
 * - Response generation: Can send acknowledgment back to mobile app
 * 
 * Example use cases:
 * - AWS credentials (claim cert, endpoint, template name)
 * - GCP service account keys
 * - Mesh network configuration
 * - OTA firmware URLs
 * - Application-specific metadata
 */
class ICustomEndpointHandler {
public:
    virtual ~ICustomEndpointHandler() = default;

    /**
     * Handle received data chunk
     * 
     * Called when BLE characteristic receives data. May be called multiple
     * times for chunked data (BLE MTU is typically 512 bytes).
     * 
     * Implementation should:
     * 1. Accumulate chunks if protocol requires (e.g., length-prefixed JSON)
     * 2. Parse complete message when ready
     * 3. Store received data using appropriate storage backend
     * 4. Return true to indicate success
     * 
     * @param sessionId BLE session ID (for tracking)
     * @param data Received data chunk
     * @param length Data length in bytes
     * @param isComplete True if this is the final chunk (protocol-dependent)
     * @return true if handling succeeded, false on error
     */
    virtual bool onDataReceived(uint32_t sessionId, 
                               const uint8_t* data, 
                               size_t length,
                               bool isComplete) = 0;

    /**
     * Generate response to send back to mobile app
     * 
     * Called after onDataReceived() returns true. Allows sending
     * acknowledgment or error message back to mobile app.
     * 
     * @param sessionId BLE session ID
     * @param response Buffer to write response data
     * @param maxLength Maximum response buffer size
     * @return Number of bytes written to response buffer, 0 if no response
     */
    virtual size_t getResponse(uint32_t sessionId, 
                              uint8_t* response, 
                              size_t maxLength) = 0;

    /**
     * Reset handler state
     * 
     * Called when BLE session ends or new session starts.
     * Clear any accumulated data or internal state.
     */
    virtual void reset() = 0;

    /**
     * Get protocol used by this endpoint
     * 
     * Used by WiFi provisioning layer to determine how to frame data.
     * 
     * @return Endpoint protocol
     */
    virtual EndpointProtocol getProtocol() const = 0;
};

} // namespace prov
} // namespace lopcore
