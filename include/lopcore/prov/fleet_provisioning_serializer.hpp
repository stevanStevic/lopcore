#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <optional>

namespace lopcore {
namespace prov {

/**
 * CSR Response Data Structure
 * 
 * Data extracted from CreateCertificateFromCsr response payload.
 */
struct CsrResponseData {
    std::string certificatePem;           // Device certificate (PEM format)
    std::string certificateId;            // Certificate ID from AWS IoT
    std::string ownershipToken;           // Ownership token for RegisterThing
};

/**
 * Fleet Provisioning Serializer
 * 
 * CBOR encoding/decoding for AWS IoT Fleet Provisioning MQTT payloads.
 * Uses tinyCBOR for efficient binary serialization.
 * 
 * Supports:
 * - CreateCertificateFromCsr request/response
 * - RegisterThing request/response
 * 
 * Reference:
 * https://docs.aws.amazon.com/iot/latest/developerguide/fleet-provision-api.html
 */
class FleetProvisioningSerializer {
public:
    /**
     * Generate CreateCertificateFromCsr request payload (CBOR)
     * 
     * Payload format:
     * {
     *   "certificateSigningRequest": "<PEM CSR>"
     * }
     * 
     * @param csrPem Certificate Signing Request in PEM format
     * @return CBOR-encoded payload if successful, std::nullopt on error
     */
    static std::optional<std::vector<uint8_t>> generateCsrRequest(const std::string& csrPem);

    /**
     * Generate RegisterThing request payload (CBOR)
     * 
     * Payload format:
     * {
     *   "certificateOwnershipToken": "<token>",
     *   "parameters": {
     *     "SerialNumber": "<device_id>"
     *   }
     * }
     * 
     * @param ownershipToken Certificate ownership token from CSR response
     * @param serialNumber Device serial number or unique identifier
     * @return CBOR-encoded payload if successful, std::nullopt on error
     */
    static std::optional<std::vector<uint8_t>> generateRegisterThingRequest(
        const std::string& ownershipToken,
        const std::string& serialNumber);

    /**
     * Parse CreateCertificateFromCsr response payload (CBOR)
     * 
     * Extracts:
     * - certificatePem: Device certificate (PEM)
     * - certificateId: AWS certificate ID
     * - certificateOwnershipToken: Token for RegisterThing
     * 
     * @param payload CBOR-encoded response
     * @return Parsed response data if successful, std::nullopt on error
     */
    static std::optional<CsrResponseData> parseCsrResponse(const std::vector<uint8_t>& payload);

    /**
     * Parse CreateCertificateFromCsr response payload (CBOR)
     * 
     * Overload for raw buffer input.
     * 
     * @param payload CBOR-encoded response buffer
     * @param length Payload length
     * @return Parsed response data if successful, std::nullopt on error
     */
    static std::optional<CsrResponseData> parseCsrResponse(const uint8_t* payload, size_t length);

    /**
     * Parse RegisterThing response payload (CBOR)
     * 
     * Extracts:
     * - thingName: AWS IoT Thing name assigned to the device
     * 
     * @param payload CBOR-encoded response
     * @return Thing name if successful, std::nullopt on error
     */
    static std::optional<std::string> parseRegisterThingResponse(const std::vector<uint8_t>& payload);

    /**
     * Parse RegisterThing response payload (CBOR)
     * 
     * Overload for raw buffer input.
     * 
     * @param payload CBOR-encoded response buffer
     * @param length Payload length
     * @return Thing name if successful, std::nullopt on error
     */
    static std::optional<std::string> parseRegisterThingResponse(const uint8_t* payload, size_t length);

    /**
     * Convert CBOR payload to human-readable string (for debugging)
     * 
     * @param payload CBOR-encoded data
     * @return Pretty-printed CBOR string
     */
    static std::string getStringFromCbor(const std::vector<uint8_t>& payload);

    /**
     * Convert CBOR payload to human-readable string (for debugging)
     * 
     * Overload for raw buffer input.
     * 
     * @param payload CBOR-encoded data buffer
     * @param length Payload length
     * @return Pretty-printed CBOR string
     */
    static std::string getStringFromCbor(const uint8_t* payload, size_t length);
};

} // namespace prov
} // namespace lopcore
