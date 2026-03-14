#include "lopcore/prov/fleet_provisioning_serializer.hpp"

#include "lopcore/logging/logger.hpp"

// Only compile if CBOR is enabled
#ifdef CONFIG_LOPCORE_PROV_AWS_CBOR

#include <cstring>
#include <map>
#include <memory>

#include <cbor.h>

namespace lopcore
{
namespace prov
{

// =============================================================================
// CSR Request Generation
// =============================================================================

std::optional<std::vector<uint8_t>> FleetProvisioningSerializer::generateCsrRequest(const std::string &csrPem)
{
    if (csrPem.empty())
    {
        LOPCORE_LOGE("FleetProvSerializer", "CSR PEM is empty");
        return std::nullopt;
    }

    // Allocate buffer for CBOR encoding (CSR + overhead)
    std::vector<uint8_t> buffer(csrPem.size() + 128);

    CborEncoder encoder, mapEncoder;
    CborError cborRet;

    // Initialize CBOR encoder
    cbor_encoder_init(&encoder, buffer.data(), buffer.size(), 0);

    // Create map with 1 key-value pair
    cborRet = cbor_encoder_create_map(&encoder, &mapEncoder, 1);

    if (cborRet == CborNoError)
    {
        cborRet = cbor_encode_text_stringz(&mapEncoder, "certificateSigningRequest");
    }

    if (cborRet == CborNoError)
    {
        cborRet = cbor_encode_text_string(&mapEncoder, csrPem.c_str(), csrPem.length());
    }

    if (cborRet == CborNoError)
    {
        cborRet = cbor_encoder_close_container(&encoder, &mapEncoder);
    }

    if (cborRet != CborNoError)
    {
        LOPCORE_LOGE("FleetProvSerializer", "CBOR encoding error: %s", cbor_error_string(cborRet));

        if ((cborRet & CborErrorOutOfMemory) != 0)
        {
            LOPCORE_LOGE("FleetProvSerializer", "Buffer too small for CSR request");
        }
        return std::nullopt;
    }

    // Get actual encoded length
    size_t encodedLength = cbor_encoder_get_buffer_size(&encoder, buffer.data());
    buffer.resize(encodedLength);

    LOPCORE_LOGD("FleetProvSerializer", "CSR request generated: %zu bytes", encodedLength);
    return buffer;
}

// =============================================================================
// RegisterThing Request Generation
// =============================================================================

std::optional<std::vector<uint8_t>> FleetProvisioningSerializer::generateRegisterThingRequest(
    const std::string &ownershipToken,
    const std::map<std::string, std::string> &parameters)
{
    if (ownershipToken.empty())
    {
        LOPCORE_LOGE("FleetProvSerializer", "Ownership token is empty");
        return std::nullopt;
    }

    if (parameters.empty())
    {
        LOPCORE_LOGE("FleetProvSerializer", "Parameters map is empty");
        return std::nullopt;
    }

    // Allocate buffer: ownership token + all key/value sizes + overhead
    size_t paramsSize = 0;
    for (const auto &[k, v] : parameters)
    {
        paramsSize += k.size() + v.size();
    }
    std::vector<uint8_t> buffer(ownershipToken.size() + paramsSize * 2 + 256);

    CborEncoder encoder, mapEncoder, parametersEncoder;
    CborError cborRet;

    cbor_encoder_init(&encoder, buffer.data(), buffer.size(), 0);

    // Create outer map with 2 key-value pairs
    cborRet = cbor_encoder_create_map(&encoder, &mapEncoder, 2);

    if (cborRet == CborNoError)
    {
        cborRet = cbor_encode_text_stringz(&mapEncoder, "certificateOwnershipToken");
    }

    if (cborRet == CborNoError)
    {
        cborRet = cbor_encode_text_string(&mapEncoder, ownershipToken.c_str(), ownershipToken.length());
    }

    if (cborRet == CborNoError)
    {
        cborRet = cbor_encode_text_stringz(&mapEncoder, "parameters");
    }

    if (cborRet == CborNoError)
    {
        cborRet = cbor_encoder_create_map(&mapEncoder, &parametersEncoder, parameters.size());
    }

    for (const auto &[k, v] : parameters)
    {
        if (cborRet == CborNoError)
        {
            cborRet = cbor_encode_text_string(&parametersEncoder, k.c_str(), k.length());
        }
        if (cborRet == CborNoError)
        {
            cborRet = cbor_encode_text_string(&parametersEncoder, v.c_str(), v.length());
        }
    }

    if (cborRet == CborNoError)
    {
        cborRet = cbor_encoder_close_container(&mapEncoder, &parametersEncoder);
    }

    if (cborRet == CborNoError)
    {
        cborRet = cbor_encoder_close_container(&encoder, &mapEncoder);
    }

    if (cborRet != CborNoError)
    {
        LOPCORE_LOGE("FleetProvSerializer", "CBOR encoding error: %s", cbor_error_string(cborRet));

        if ((cborRet & CborErrorOutOfMemory) != 0)
        {
            LOPCORE_LOGE("FleetProvSerializer", "Buffer too small for RegisterThing request");
        }
        return std::nullopt;
    }

    // Get actual encoded length
    size_t encodedLength = cbor_encoder_get_buffer_size(&encoder, buffer.data());
    buffer.resize(encodedLength);

    LOPCORE_LOGD("FleetProvSerializer", "RegisterThing request generated: %zu bytes", encodedLength);
    return buffer;
}

// =============================================================================
// CSR Response Parsing
// =============================================================================

std::optional<CsrResponseData>
FleetProvisioningSerializer::parseCsrResponse(const std::vector<uint8_t> &payload)
{
    return parseCsrResponse(payload.data(), payload.size());
}

std::optional<CsrResponseData> FleetProvisioningSerializer::parseCsrResponse(const uint8_t *payload,
                                                                             size_t length)
{
    if (payload == nullptr || length == 0)
    {
        LOPCORE_LOGE("FleetProvSerializer", "Invalid CSR response payload");
        return std::nullopt;
    }

    CborParser parser;
    CborValue map, value;
    CborError cborRet;

    cborRet = cbor_parser_init(payload, length, 0, &parser, &map);

    if (cborRet != CborNoError)
    {
        LOPCORE_LOGE("FleetProvSerializer", "CBOR parser init error: %s", cbor_error_string(cborRet));
        return std::nullopt;
    }

    if (!cbor_value_is_map(&map))
    {
        LOPCORE_LOGE("FleetProvSerializer", "CSR response is not a map");
        return std::nullopt;
    }

    CsrResponseData result;

    // Parse certificatePem
    cborRet = cbor_value_map_find_value(&map, "certificatePem", &value);
    if (cborRet != CborNoError || value.type == CborInvalidType)
    {
        LOPCORE_LOGE("FleetProvSerializer", "certificatePem not found in response");
        return std::nullopt;
    }

    if (value.type != CborTextStringType)
    {
        LOPCORE_LOGE("FleetProvSerializer", "certificatePem is not a string");
        return std::nullopt;
    }

    size_t certLen = 0;
    cborRet = cbor_value_calculate_string_length(&value, &certLen);
    if (cborRet != CborNoError)
    {
        LOPCORE_LOGE("FleetProvSerializer", "Failed to get certificatePem length");
        return std::nullopt;
    }

    result.certificatePem.resize(certLen + 1); // +1 for null terminator
    cborRet = cbor_value_copy_text_string(&value, &result.certificatePem[0], &certLen, nullptr);
    if (cborRet != CborNoError)
    {
        LOPCORE_LOGE("FleetProvSerializer", "Failed to copy certificatePem");
        return std::nullopt;
    }
    result.certificatePem.resize(certLen); // Remove null terminator

    // Parse certificateId
    cborRet = cbor_value_map_find_value(&map, "certificateId", &value);
    if (cborRet != CborNoError || value.type == CborInvalidType)
    {
        LOPCORE_LOGE("FleetProvSerializer", "certificateId not found in response");
        return std::nullopt;
    }

    if (value.type != CborTextStringType)
    {
        LOPCORE_LOGE("FleetProvSerializer", "certificateId is not a string");
        return std::nullopt;
    }

    size_t idLen = 0;
    cborRet = cbor_value_calculate_string_length(&value, &idLen);
    if (cborRet != CborNoError)
    {
        LOPCORE_LOGE("FleetProvSerializer", "Failed to get certificateId length");
        return std::nullopt;
    }

    result.certificateId.resize(idLen + 1);
    cborRet = cbor_value_copy_text_string(&value, &result.certificateId[0], &idLen, nullptr);
    if (cborRet != CborNoError)
    {
        LOPCORE_LOGE("FleetProvSerializer", "Failed to copy certificateId");
        return std::nullopt;
    }
    result.certificateId.resize(idLen);

    // Parse certificateOwnershipToken
    cborRet = cbor_value_map_find_value(&map, "certificateOwnershipToken", &value);
    if (cborRet != CborNoError || value.type == CborInvalidType)
    {
        LOPCORE_LOGE("FleetProvSerializer", "certificateOwnershipToken not found in response");
        return std::nullopt;
    }

    if (value.type != CborTextStringType)
    {
        LOPCORE_LOGE("FleetProvSerializer", "certificateOwnershipToken is not a string");
        return std::nullopt;
    }

    size_t tokenLen = 0;
    cborRet = cbor_value_calculate_string_length(&value, &tokenLen);
    if (cborRet != CborNoError)
    {
        LOPCORE_LOGE("FleetProvSerializer", "Failed to get certificateOwnershipToken length");
        return std::nullopt;
    }

    result.ownershipToken.resize(tokenLen + 1);
    cborRet = cbor_value_copy_text_string(&value, &result.ownershipToken[0], &tokenLen, nullptr);
    if (cborRet != CborNoError)
    {
        LOPCORE_LOGE("FleetProvSerializer", "Failed to copy certificateOwnershipToken");
        return std::nullopt;
    }
    result.ownershipToken.resize(tokenLen);

    LOPCORE_LOGD("FleetProvSerializer", "CSR response parsed successfully");
    LOPCORE_LOGD("FleetProvSerializer", "  Certificate ID: %s", result.certificateId.c_str());
    LOPCORE_LOGD("FleetProvSerializer", "  Certificate length: %zu bytes", result.certificatePem.length());
    LOPCORE_LOGD("FleetProvSerializer", "  Ownership token length: %zu bytes",
                 result.ownershipToken.length());

    return result;
}

// =============================================================================
// RegisterThing Response Parsing
// =============================================================================

std::optional<std::string>
FleetProvisioningSerializer::parseRegisterThingResponse(const std::vector<uint8_t> &payload)
{
    return parseRegisterThingResponse(payload.data(), payload.size());
}

std::optional<std::string> FleetProvisioningSerializer::parseRegisterThingResponse(const uint8_t *payload,
                                                                                   size_t length)
{
    if (payload == nullptr || length == 0)
    {
        LOPCORE_LOGE("FleetProvSerializer", "Invalid RegisterThing response payload");
        return std::nullopt;
    }

    CborParser parser;
    CborValue map, value;
    CborError cborRet;

    cborRet = cbor_parser_init(payload, length, 0, &parser, &map);

    if (cborRet != CborNoError)
    {
        LOPCORE_LOGE("FleetProvSerializer", "CBOR parser init error: %s", cbor_error_string(cborRet));
        return std::nullopt;
    }

    if (!cbor_value_is_map(&map))
    {
        LOPCORE_LOGE("FleetProvSerializer", "RegisterThing response is not a map");
        return std::nullopt;
    }

    // Parse thingName
    cborRet = cbor_value_map_find_value(&map, "thingName", &value);
    if (cborRet != CborNoError || value.type == CborInvalidType)
    {
        LOPCORE_LOGE("FleetProvSerializer", "thingName not found in response");
        return std::nullopt;
    }

    if (value.type != CborTextStringType)
    {
        LOPCORE_LOGE("FleetProvSerializer", "thingName is not a string");
        return std::nullopt;
    }

    size_t nameLen = 0;
    cborRet = cbor_value_calculate_string_length(&value, &nameLen);
    if (cborRet != CborNoError)
    {
        LOPCORE_LOGE("FleetProvSerializer", "Failed to get thingName length");
        return std::nullopt;
    }

    std::string thingName;
    thingName.resize(nameLen + 1);
    cborRet = cbor_value_copy_text_string(&value, &thingName[0], &nameLen, nullptr);
    if (cborRet != CborNoError)
    {
        LOPCORE_LOGE("FleetProvSerializer", "Failed to copy thingName");
        return std::nullopt;
    }
    thingName.resize(nameLen);

    LOPCORE_LOGD("FleetProvSerializer", "RegisterThing response parsed: Thing Name = %s", thingName.c_str());

    return thingName;
}

// =============================================================================
// Debug Helper
// =============================================================================

std::string FleetProvisioningSerializer::getStringFromCbor(const std::vector<uint8_t> &payload)
{
    return getStringFromCbor(payload.data(), payload.size());
}

std::string FleetProvisioningSerializer::getStringFromCbor(const uint8_t *payload, size_t length)
{
    if (payload == nullptr || length == 0)
    {
        return "<empty>";
    }

    CborParser parser;
    CborValue value;
    CborError error;

    error = cbor_parser_init(payload, length, 0, &parser, &value);
    if (error != CborNoError)
    {
        return "<invalid CBOR>";
    }

    // Simple string representation (could use cbor_value_to_pretty if needed)
    // For now, just return a basic representation
    return "<CBOR data: " + std::to_string(length) + " bytes>";
}

} // namespace prov
} // namespace lopcore

#else // !CONFIG_LOPCORE_PROV_AWS_CBOR

// Stub implementations when CBOR is disabled
#include <map>

#include "lopcore/logging/logger.hpp"

namespace lopcore
{
namespace prov
{

std::optional<std::vector<uint8_t>> FleetProvisioningSerializer::generateCsrRequest(const std::string &)
{
    LOPCORE_LOGE("FleetProvSerializer", "CBOR support not enabled (CONFIG_LOPCORE_PROV_AWS_CBOR)");
    return std::nullopt;
}

std::optional<std::vector<uint8_t>>
FleetProvisioningSerializer::generateRegisterThingRequest(const std::string &,
                                                          const std::map<std::string, std::string> &)
{
    LOPCORE_LOGE("FleetProvSerializer", "CBOR support not enabled (CONFIG_LOPCORE_PROV_AWS_CBOR)");
    return std::nullopt;
}

std::optional<CsrResponseData> FleetProvisioningSerializer::parseCsrResponse(const std::vector<uint8_t> &)
{
    LOPCORE_LOGE("FleetProvSerializer", "CBOR support not enabled (CONFIG_LOPCORE_PROV_AWS_CBOR)");
    return std::nullopt;
}

std::optional<CsrResponseData> FleetProvisioningSerializer::parseCsrResponse(const uint8_t *, size_t)
{
    LOPCORE_LOGE("FleetProvSerializer", "CBOR support not enabled (CONFIG_LOPCORE_PROV_AWS_CBOR)");
    return std::nullopt;
}

std::optional<std::string>
FleetProvisioningSerializer::parseRegisterThingResponse(const std::vector<uint8_t> &)
{
    LOPCORE_LOGE("FleetProvSerializer", "CBOR support not enabled (CONFIG_LOPCORE_PROV_AWS_CBOR)");
    return std::nullopt;
}

std::optional<std::string> FleetProvisioningSerializer::parseRegisterThingResponse(const uint8_t *, size_t)
{
    LOPCORE_LOGE("FleetProvSerializer", "CBOR support not enabled (CONFIG_LOPCORE_PROV_AWS_CBOR)");
    return std::nullopt;
}

std::string FleetProvisioningSerializer::getStringFromCbor(const std::vector<uint8_t> &)
{
    return "<CBOR not enabled>";
}

std::string FleetProvisioningSerializer::getStringFromCbor(const uint8_t *, size_t)
{
    return "<CBOR not enabled>";
}

} // namespace prov
} // namespace lopcore

#endif // CONFIG_LOPCORE_PROV_AWS_CBOR
