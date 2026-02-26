#include "lopcore/prov/aws_data_endpoint_handler.hpp"
#include "lopcore/logging/logger.hpp"
#include <cJSON.h>
#include <cstring>

namespace lopcore {
namespace prov {

AwsDataEndpointHandler::AwsDataEndpointHandler(const AwsDataEndpointConfig& config)
    : config_(config)
    , lastResponse_("SUCCESS")
    , lastSuccess_(true)
{
    LOPCORE_LOGI("AwsDataHandler", "Initialized endpoint: %s", config_.endpointName.c_str());
}

bool AwsDataEndpointHandler::onDataReceived(uint32_t sessionId, 
                                            const uint8_t* data, 
                                            size_t length,
                                            bool isComplete) {
    (void)sessionId;  // Unused
    
    if (!isComplete) {
        LOPCORE_LOGW("AwsDataHandler", "Received incomplete data (should not happen with BleDataFramer)");
        lastResponse_ = "ERROR: Incomplete data";
        lastSuccess_ = false;
        return false;
    }

    if (data == nullptr || length == 0) {
        LOPCORE_LOGE("AwsDataHandler", "Received null or zero-length data");
        lastResponse_ = "ERROR: Invalid data";
        lastSuccess_ = false;
        return false;
    }

    // Convert to string
    std::string jsonStr(reinterpret_cast<const char*>(data), length);
    
    LOPCORE_LOGI("AwsDataHandler", "Received AWS credentials: %zu bytes", length);
    LOPCORE_LOGD("AwsDataHandler", "JSON payload: %s", jsonStr.c_str());

    // Parse and store
    if (!parseAndStoreJson(jsonStr)) {
        LOPCORE_LOGE("AwsDataHandler", "Failed to parse/store AWS credentials");
        lastSuccess_ = false;
        return false;
    }

    LOPCORE_LOGI("AwsDataHandler", "AWS credentials stored successfully");
    lastResponse_ = "SUCCESS";
    lastSuccess_ = true;
    return true;
}

size_t AwsDataEndpointHandler::getResponse(uint32_t sessionId, 
                                           uint8_t* response, 
                                           size_t maxLength) {
    (void)sessionId;  // Unused

    if (response == nullptr || maxLength == 0) {
        return 0;
    }

    size_t responseLen = std::min(lastResponse_.length(), maxLength - 1);
    memcpy(response, lastResponse_.c_str(), responseLen);
    response[responseLen] = '\0';

    return responseLen;
}

void AwsDataEndpointHandler::reset() {
    lastResponse_ = "SUCCESS";
    lastSuccess_ = true;
    LOPCORE_LOGD("AwsDataHandler", "Reset");
}

EndpointProtocol AwsDataEndpointHandler::getProtocol() const {
    return EndpointProtocol::JSON_LENGTH_PREFIXED;
}

bool AwsDataEndpointHandler::parseAndStoreJson(const std::string& jsonStr) {
    // Parse JSON
    cJSON* root = cJSON_Parse(jsonStr.c_str());
    if (root == nullptr) {
        const char* error = cJSON_GetErrorPtr();
        LOPCORE_LOGE("AwsDataHandler", "JSON parse error: %s", error ? error : "unknown");
        lastResponse_ = "ERROR: Invalid JSON";
        return false;
    }

    bool success = true;

    // Extract and validate required fields
    const char* requiredFields[] = {
        "certificate",
        "private_key",
        "aws_endpoint",
        "root_ca",
        "provisioning_template"
    };

    const char* storageKeys[] = {
        "claim_cert",
        "claim_key",
        "aws_endpoint",
        "root_ca",
        "provisioning_template"
    };

    for (size_t i = 0; i < 5; ++i) {
        cJSON* field = cJSON_GetObjectItemCaseSensitive(root, requiredFields[i]);
        
        if (field == nullptr || !cJSON_IsString(field)) {
            LOPCORE_LOGE("AwsDataHandler", "Missing or invalid field: %s", requiredFields[i]);
            lastResponse_ = std::string("ERROR: Missing field ") + requiredFields[i];
            success = false;
            break;
        }

        std::string value = field->valuestring;

        // Validate non-empty
        if (value.empty()) {
            LOPCORE_LOGE("AwsDataHandler", "Empty field: %s", requiredFields[i]);
            lastResponse_ = std::string("ERROR: Empty field ") + requiredFields[i];
            success = false;
            break;
        }

        // Validate PEM fields (certificate, private_key, root_ca)
        if (i == 0 || i == 1 || i == 3) {
            if (!validatePem(requiredFields[i], value)) {
                success = false;
                break;
            }
        }

        // Store to storage
        if (!storeField(storageKeys[i], value)) {
            LOPCORE_LOGE("AwsDataHandler", "Failed to store field: %s", storageKeys[i]);
            lastResponse_ = std::string("ERROR: Storage failed for ") + storageKeys[i];
            success = false;
            break;
        }

        LOPCORE_LOGD("AwsDataHandler", "Stored %s: %zu bytes", storageKeys[i], value.length());
    }

    cJSON_Delete(root);
    return success;
}

bool AwsDataEndpointHandler::validatePem(const std::string& fieldName, const std::string& pemStr) {
    if (pemStr.find("-----BEGIN") != 0) {
        LOPCORE_LOGE("AwsDataHandler", "Invalid PEM format for %s (missing -----BEGIN)", fieldName.c_str());
        lastResponse_ = std::string("ERROR: Invalid PEM for ") + fieldName;
        return false;
    }

    LOPCORE_LOGV("AwsDataHandler", "PEM validation passed for %s", fieldName.c_str());
    return true;
}

bool AwsDataEndpointHandler::storeField(const std::string& key, const std::string& value) {
    // Find storage callbacks for this key
    auto it = config_.storageMap.find(key);
    if (it == config_.storageMap.end()) {
        LOPCORE_LOGW("AwsDataHandler", "No storage mapping for key: %s", key.c_str());
        return false;
    }

    const StorageCallbacks& callbacks = it->second;

    // Write to storage
    if (!callbacks.write(key, value)) {
        LOPCORE_LOGE("AwsDataHandler", "Storage write failed for key: %s", key.c_str());
        return false;
    }

    return true;
}

} // namespace prov
} // namespace lopcore
