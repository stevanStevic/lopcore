#pragma once

#include <map>
#include <string>

#include "custom_endpoint_handler.hpp"
#include "storage_types.hpp"

namespace lopcore
{
namespace prov
{

/**
 * AWS Data Endpoint Handler Configuration
 */
struct AwsDataEndpointConfig
{
    /**
     * Storage callback map for AWS credential fields
     *
     * Maps field names to storage backends:
     * - "claim_cert" → Claim certificate PEM
     * - "claim_key" → Claim private key PEM
     * - "aws_endpoint" → AWS IoT endpoint URL
     * - "root_ca" → Root CA certificate PEM
     * - "provisioning_template" → Fleet provisioning template name
     */
    StorageCallbackMap storageMap;

    /**
     * Endpoint name (BLE characteristic name)
     * Default: "aws-data"
     */
    std::string endpointName = "aws-data";
};

/**
 * AWS Data Endpoint Handler
 *
 * Custom BLE endpoint handler for receiving AWS IoT credentials.
 *
 * Expected JSON Payload Format:
 * ```json
 * {
 *   "certificate": "<claim cert PEM>",
 *   "private_key": "<claim key PEM>",
 *   "aws_endpoint": "<AWS IoT endpoint URL>",
 *   "root_ca": "<root CA PEM>",
 *   "provisioning_template": "<template name>"
 * }
 * ```
 *
 * Protocol: JSON with length-prefixed framing (NO EOF markers!)
 *
 * The WiFiProvisioning layer handles:
 * - BLE chunk reassembly via BleDataFramer
 * - Length-prefix parsing
 *
 * This handler receives the complete JSON payload and:
 * 1. Validates all 5 fields are present
 * 2. Validates PEM fields start with "-----BEGIN"
 * 3. Writes each field to storage via StorageCallbacks
 * 4. Returns "SUCCESS" or "ERROR: <reason>"
 */
class AwsDataEndpointHandler : public ICustomEndpointHandler
{
public:
    /**
     * Constructor
     *
     * @param config Endpoint configuration (storage mapping)
     */
    explicit AwsDataEndpointHandler(const AwsDataEndpointConfig &config);

    /**
     * Destructor
     */
    ~AwsDataEndpointHandler() override = default;

    // ---------- ICustomEndpointHandler Implementation ----------

    /**
     * Handle received data chunk
     *
     * Note: WiFiProvisioning layer already reassembles chunks via BleDataFramer.
     * This method receives the complete JSON payload in a single call.
     *
     * @param sessionId BLE session ID
     * @param data Complete JSON payload
     * @param length Payload length
     * @param isComplete Should always be true (complete payload)
     * @return true on success, false on error
     */
    bool onDataReceived(uint32_t sessionId, const uint8_t *data, size_t length, bool isComplete) override;

    /**
     * Generate response to send back to mobile app
     *
     * Returns "SUCCESS" or "ERROR: <reason>".
     *
     * @param sessionId BLE session ID
     * @param response Buffer to write response
     * @param maxLength Maximum response buffer size
     * @return Number of bytes written
     */
    size_t getResponse(uint32_t sessionId, uint8_t *response, size_t maxLength) override;

    /**
     * Reset handler state
     */
    void reset() override;

    /**
     * Get protocol
     *
     * @return EndpointProtocol::JSON_LENGTH_PREFIXED
     */
    EndpointProtocol getProtocol() const override;

private:
    AwsDataEndpointConfig config_;
    std::string lastResponse_; // Last response message
    bool lastSuccess_;         // Last operation success status

    /**
     * Parse and validate JSON payload
     *
     * Extracts all 5 required fields and validates them.
     *
     * @param data Raw JSON bytes (not null-terminated)
     * @param length Length of data
     * @return true on success, false on error
     */
    bool parseAndStoreJson(const uint8_t *data, size_t length);

    /**
     * Validate PEM-formatted field
     *
     * Checks that field starts with "-----BEGIN".
     *
     * @param fieldName Field name (for error reporting)
     * @param pemStr PEM string to validate
     * @return true if valid, false otherwise
     */
    bool validatePem(const std::string &fieldName, const std::string &pemStr);

    /**
     * Store field to storage
     *
     * Uses config_.storageMap to find appropriate storage backend.
     *
     * @param key Storage key
     * @param value Value to store
     * @return true on success, false on error
     */
    bool storeField(const std::string &key, const std::string &value);
};

} // namespace prov
} // namespace lopcore
