#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "certificate_manager.hpp"
#include "cloud_provisioner.hpp"
#include "fleet_provisioning_serializer.hpp"
#include "provisioning_config.hpp"

// For cJSON-based JSON parsing in MQTT callbacks
#include <cJSON.h>
// For vTaskDelay in retry loop
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace lopcore
{
namespace prov
{

/**
 * AWS Fleet Provisioning Status
 *
 * Detailed status tracking for AWS provisioning workflow.
 */
enum class AwsProvisioningStep
{
    NOT_STARTED,
    LOADING_CLAIM_CREDS,
    CONNECTING_CLAIM,
    SUBSCRIBING_TOPICS,
    GENERATING_KEYPAIR,
    SENDING_CSR,
    RECEIVING_CERT,
    REGISTERING_THING,
    STORING_CREDS,
    COMPLETE,
    FAILED
};

/**
 * AWS Fleet Provisioning Result
 *
 * Contains result of provisioning operation.
 */
struct AwsProvisioningResult
{
    bool success = false;
    AwsProvisioningStep lastStep = AwsProvisioningStep::NOT_STARTED;
    std::string errorMessage;
    std::string deviceId;       // AWS Thing Name
    std::string certificatePem; // Final device certificate
};

/**
 * AWS Fleet Provisioner (Template-based)
 *
 * Implements AWS IoT Fleet Provisioning workflow with claim certificates.
 * Uses template parameter for MQTT client dependency injection (duck typing).
 *
 * Template Requirements (TMqttClient):
 * The MQTT client must provide these methods:
 * ```cpp
 * bool connect(const char* endpoint, uint16_t port,
 *              const char* clientId,
 *              const char* certPem, const char* keyPem,
 *              const char* rootCaPem);
 * bool disconnect();
 * bool subscribe(const char* topic,
 *                std::function<void(const char*, size_t)> callback);
 * bool unsubscribe(const char* topic);
 * bool publish(const char* topic, const char* payload, size_t length);
 * bool isConnected() const;
 * void processEvents(uint32_t timeoutMs);  // Pump MQTT loop
 * ```
 *
 * Provisioning Workflow:
 * 1. Load claim certificate + private key from storage
 * 2. Connect to AWS IoT using claim credentials
 * 3. Subscribe to CreateCertificateFromCsr response topics
 * 4. Subscribe to RegisterThing response topics
 * 5. Generate EC P-256 keypair locally
 * 6. Generate CSR (Certificate Signing Request)
 * 7. Publish CSR to CreateCertificateFromCsr topic
 * 8. Receive final certificate from AWS
 * 9. Publish to RegisterThing topic with template parameters
 * 10. Receive Thing Name (device ID) from AWS
 * 11. Store final certificate + private key in PKCS11
 * 12. Store Thing Name and endpoint in config storage
 * 13. Disconnect claim connection
 *
 * MQTT Topics (Fleet Provisioning API):
 * - Publish CSR: `$aws/certificates/create-from-csr/json`
 * - Accept response: `$aws/certificates/create-from-csr/json/accepted`
 * - Reject response: `$aws/certificates/create-from-csr/json/rejected`
 * - Publish registration: `$aws/provisioning-templates/{templateName}/provision/json`
 * - Accept response: `$aws/provisioning-templates/{templateName}/provision/json/accepted`
 * - Reject response: `$aws/provisioning-templates/{templateName}/provision/json/rejected`
 *
 * Example Usage:
 * ```cpp
 * // Setup
 * auto nvsStorage = std::make_shared<lopcore::NvsStorage>("aws", false);
 * nvsStorage->init();
 *
 * auto certManager = std::make_shared<CertificateManager>();
 *
 * AwsProvisioningConfig config;
 * config.addConfigStorage("aws_endpoint", makeNvsStorage(nvsStorage, "aws_endpoint"))
 *       .addConfigStorage("provisioning_template", makeNvsStorage(nvsStorage, "template"))
 *       .addConfigStorage("thing_name", makeNvsStorage(nvsStorage, "thing_name"))
 *       .setCertificateManager(certManager)
 *       .setCsrSubjectName("CN=GrowBorgDevice")
 *       .setDeviceIdProvider([]() { return getMacAddress(); });
 *
 * // Inject MQTT client (coreMQTT, ESP-MQTT, or mock)
 * auto mqttClient = std::make_shared<CoreMqttClient>();
 *
 * AwsFleetProvisioner<CoreMqttClient> provisioner(config, mqttClient);
 *
 * // Provision (blocking call, 30-60 seconds typical)
 * if (provisioner.provision()) {
 *     auto result = provisioner.getLastResult();
 *     ESP_LOGI(TAG, "Provisioned! Thing Name: %s", result.deviceId.c_str());
 * } else {
 *     ESP_LOGE(TAG, "Provisioning failed: %s",
 *              provisioner.getLastResult().errorMessage.c_str());
 * }
 * ```
 */
template<typename TMqttClient>
class AwsFleetProvisioner : public ICloudProvisioner
{
public:
    /**
     * Constructor
     *
     * @param config AWS provisioning configuration (endpoints, template, storage)
     * @param mqttClient Injected MQTT client (coreMQTT, ESP-MQTT, mock, etc.)
     */
    AwsFleetProvisioner(const AwsProvisioningConfig &config, std::shared_ptr<TMqttClient> mqttClient)
        : config_(config), mqttClient_(std::move(mqttClient)), status_(ProvisioningStatus::NOT_PROVISIONED)
    {
    }

    // ---------- ICloudProvisioner Implementation ----------

    /**
     * Check if device is already provisioned
     *
     * Reads storage for device certificate and Thing Name.
     *
     * @return true if credentials exist, false otherwise
     */
    bool isProvisioned() const override
    {
        // Check for Thing Name in storage
        auto thingName = config_.getThingName();
        if (!thingName.has_value() || thingName->empty())
        {
            return false;
        }

        // Check for device certificate in CertificateManager
        auto certManager = config_.certificateManager();
        if (!certManager)
        {
            return false;
        }

        // If Thing Name exists and we have cert manager, assume provisioned
        // (PKCS11 cert existence check would require additional API)
        return true;
    }

    /**
     * Execute AWS Fleet Provisioning workflow
     *
     * BLOCKING CALL: Typically takes 30-60 seconds.
     *
     * Steps:
     * 1. Load claim credentials
     * 2. Connect to AWS IoT with claim cert
     * 3. Subscribe to response topics
     * 4. Generate keypair + CSR
     * 5. Request certificate from AWS
     * 6. Register Thing with template
     * 7. Store final credentials
     *
     * @return true on success, false on failure (check getLastResult())
     */
    bool provision() override
    {
        status_ = ProvisioningStatus::PROVISIONING_IN_PROGRESS;
        int maxRetries = config_.maxRetries();
        int retryDelaySec = config_.retryDelaySeconds();

        for (int attempt = 0; attempt <= maxRetries; ++attempt)
        {
            if (attempt > 0)
            {
                ESP_LOGW("AwsFleetProv", "Provisioning retry %d/%d (delay %ds)...", attempt, maxRetries,
                         retryDelaySec);
                vTaskDelay(pdMS_TO_TICKS(static_cast<uint32_t>(retryDelaySec) * 1000U));
            }
            if (provisionOnce_())
            {
                return true;
            }
        }
        return false;
    }

    ProvisioningStatus getStatus() const override
    {
        return status_;
    }

    const char *getDeviceId() const override
    {
        return lastResult_.deviceId.c_str();
    }

    bool resetProvisioning() override
    {
        auto certManager = config_.certificateManager();
        if (!certManager)
        {
            return false;
        }

        // Delete device credentials from PKCS11
        certManager->deleteDeviceCredentials();

        // Delete claim credentials (if present)
        certManager->deleteClaimCredentials();

        // Clear Thing Name from config storage
        config_.writeConfig(config_.thingNameKey(), "");

        status_ = ProvisioningStatus::NOT_PROVISIONED;
        lastResult_ = AwsProvisioningResult{};
        return true;
    }

    /**
     * Get detailed result from last provisioning attempt
     */
    const AwsProvisioningResult &getLastResult() const
    {
        return lastResult_;
    }

private:
    // ---------- Private Implementation ----------

    bool loadClaimCredentials()
    {
        auto certManager = config_.certificateManager();
        if (!certManager)
        {
            return false;
        }

        // Try to load from storage (BLE-transferred mode)
        auto claimCert = config_.getClaimCert();
        auto claimKey = config_.getClaimKey();

        if (claimCert.has_value() && claimKey.has_value())
        {
            // Import claim credentials into PKCS11
            if (!certManager->importClaimCertificate(claimCert.value(), claimKey.value()))
            {
                return false;
            }
        }

        // Get root CA
        claimRootCa_ = config_.getRootCa().value_or("");
        if (claimRootCa_.empty())
        {
            return false;
        }

        return true;
    }

    bool connectWithClaimCredentials()
    {
        auto endpoint = config_.getEndpoint();
        if (!endpoint.has_value())
        {
            return false;
        }

        auto certManager = config_.certificateManager();
        if (!certManager)
        {
            return false;
        }

        // Get claim cert + key PEM from CertificateManager
        auto claimCertPem = certManager->getClaimCertPem();
        auto claimKeyPem = certManager->getClaimKeyPem();

        if (!claimCertPem.has_value() || !claimKeyPem.has_value() || claimRootCa_.empty())
        {
            return false;
        }

        std::string clientId = "claim-";
        auto deviceIdProvider = config_.deviceIdProvider();
        if (deviceIdProvider)
        {
            clientId += deviceIdProvider();
        }
        else
        {
            clientId += "device";
        }

        return mqttClient_->connect(endpoint->c_str(), 8883, clientId.c_str(), claimCertPem->c_str(),
                                    claimKeyPem->c_str(), claimRootCa_.c_str());
    }

    bool subscribeToResponseTopics()
    {
#ifdef CONFIG_LOPCORE_PROV_AWS_CBOR
        const char *format = "cbor";
#else
        const char *format = "json";
#endif

        // Subscribe to CreateCertificateFromCsr responses
        csrAcceptTopic_ = std::string("$aws/certificates/create-from-csr/") + format + "/accepted";
        csrRejectTopic_ = std::string("$aws/certificates/create-from-csr/") + format + "/rejected";

        bool success = mqttClient_->subscribe(csrAcceptTopic_.c_str(),
                                              [this](const char *payload, size_t length) {
                                                  onCertificateAccepted(payload, length);
                                              });

        success &= mqttClient_->subscribe(csrRejectTopic_.c_str(),
                                          [this](const char *payload, size_t length) {
                                              onCertificateRejected(payload, length);
                                          });

        // Subscribe to RegisterThing responses
        std::string templateName = config_.getTemplateName().value_or("default");
        regAcceptTopic_ = "$aws/provisioning-templates/" + templateName + "/provision/" + format +
                          "/accepted";
        regRejectTopic_ = "$aws/provisioning-templates/" + templateName + "/provision/" + format +
                          "/rejected";

        success &= mqttClient_->subscribe(regAcceptTopic_.c_str(),
                                          [this](const char *payload, size_t length) {
                                              onRegisterThingAccepted(payload, length);
                                          });

        success &= mqttClient_->subscribe(regRejectTopic_.c_str(),
                                          [this](const char *payload, size_t length) {
                                              onRegisterThingRejected(payload, length);
                                          });

        return success;
    }

    bool generateKeyPair()
    {
        auto certManager = config_.certificateManager();
        if (!certManager)
        {
            return false;
        }

        // Generate EC P-256 keypair in PKCS11 and create CSR
        std::string subjectName = config_.csrSubjectName();
        return certManager->generateKeyPairAndCsr(subjectName, csrPem_);
    }

    bool requestCertificateFromCsr()
    {
        if (csrPem_.empty())
        {
            return false;
        }

#ifdef CONFIG_LOPCORE_PROV_AWS_CBOR
        const char *topic = "$aws/certificates/create-from-csr/cbor";

        // Generate CBOR payload
        auto payload = FleetProvisioningSerializer::generateCsrRequest(csrPem_);
        if (!payload.has_value())
        {
            return false;
        }

        return mqttClient_->publish(topic, reinterpret_cast<const char *>(payload->data()), payload->size());
#else
        const char *topic = "$aws/certificates/create-from-csr/json";

        // Build JSON payload
        std::string payload = "{\"certificateSigningRequest\":\"";
        payload += csrPem_;
        payload += "\"}";

        return mqttClient_->publish(topic, payload.c_str(), payload.length());
#endif
    }

    bool waitForCertificate()
    {
        // Poll MQTT events for up to 30 seconds
        for (int i = 0; i < 300; ++i)
        { // 30 seconds / 100ms
            mqttClient_->processEvents(100);
            if (!certificateReceived_.empty())
            {
                return true;
            }
        }
        return false; // Timeout
    }

    bool waitForRegisterThingResponse_()
    {
        // Poll MQTT events for up to 30 seconds
        for (int i = 0; i < 300; ++i)
        {
            mqttClient_->processEvents(100);
            if (!lastResult_.deviceId.empty())
            {
                return true;
            }
        }
        ESP_LOGE("AwsFleetProv", "waitForRegisterThingResponse_: timed out");
        return false;
    }

    void unsubscribeCsrTopics_()
    {
        if (!csrAcceptTopic_.empty())
        {
            mqttClient_->unsubscribe(csrAcceptTopic_.c_str());
        }
        if (!csrRejectTopic_.empty())
        {
            mqttClient_->unsubscribe(csrRejectTopic_.c_str());
        }
    }

    void unsubscribeRegisterTopics_()
    {
        if (!regAcceptTopic_.empty())
        {
            mqttClient_->unsubscribe(regAcceptTopic_.c_str());
        }
        if (!regRejectTopic_.empty())
        {
            mqttClient_->unsubscribe(regRejectTopic_.c_str());
        }
    }

    bool registerThing()
    {
        std::string templateName = config_.getTemplateName().value_or("default");

        // Get device ID from provider
        std::string deviceId;
        auto deviceIdProvider = config_.deviceIdProvider();
        if (deviceIdProvider)
        {
            deviceId = deviceIdProvider();
        }

#ifdef CONFIG_LOPCORE_PROV_AWS_CBOR
        std::string topic = "$aws/provisioning-templates/" + templateName + "/provision/cbor";

        // Generate CBOR payload with ownership token and template parameters
        auto payload = FleetProvisioningSerializer::generateRegisterThingRequest(certificateOwnershipToken_,
                                                                                 deviceId);
        if (!payload.has_value())
        {
            return false;
        }

        return mqttClient_->publish(topic.c_str(), reinterpret_cast<const char *>(payload->data()),
                                    payload->size());
#else
        std::string topic = "$aws/provisioning-templates/" + templateName + "/provision/json";

        // Build JSON payload including mandatory certificateOwnershipToken
        std::string payload = "{\"certificateOwnershipToken\":\"" + certificateOwnershipToken_ +
                              "\",\"parameters\":{";
        if (!deviceId.empty())
        {
            payload += "\"SerialNumber\":\"" + deviceId + "\"";
        }
        payload += "}}";

        return mqttClient_->publish(topic.c_str(), payload.c_str(), payload.length());
#endif
    }

    bool storeFinalCredentials()
    {
        auto certManager = config_.certificateManager();
        if (!certManager)
        {
            return false;
        }

        // Store device certificate in PKCS11
        std::string deviceCertLabel = config_.deviceCertLabel();
        if (!certManager->storeFinalCertificate(deviceCertLabel, lastResult_.certificatePem))
        {
            return false;
        }

        // Store Thing Name in config storage
        bool success = config_.writeConfig(config_.thingNameKey(), lastResult_.deviceId);

        // Store endpoint (if not already stored)
        if (config_.getEndpoint().has_value())
        {
            success &= config_.writeConfig(config_.endpointKey(), config_.getEndpoint().value());
        }

        return success;
    }

    bool failProvisioning(AwsProvisioningStep step, const std::string &error)
    {
        lastResult_.lastStep = step;
        lastResult_.errorMessage = error;
        lastResult_.success = false;
        status_ = ProvisioningStatus::PROVISIONING_FAILED;
        return false;
    }

    // ---------- One provisioning attempt (called by provision() retry loop) ----------

    bool provisionOnce_()
    {
        lastResult_ = AwsProvisioningResult{};
        lastResult_.lastStep = AwsProvisioningStep::LOADING_CLAIM_CREDS;

        // Reset per-attempt state
        certificateReceived_.clear();
        csrPem_.clear();
        claimRootCa_.clear();
        certificateId_.clear();
        certificateOwnershipToken_.clear();
        csrAcceptTopic_.clear();
        csrRejectTopic_.clear();
        regAcceptTopic_.clear();
        regRejectTopic_.clear();

        // Step 1: Load claim credentials
        if (!loadClaimCredentials())
        {
            return failProvisioning(AwsProvisioningStep::LOADING_CLAIM_CREDS,
                                    "Failed to load claim certificate");
        }

        // Step 2: Connect to AWS IoT using claim credentials
        lastResult_.lastStep = AwsProvisioningStep::CONNECTING_CLAIM;
        if (!connectWithClaimCredentials())
        {
            return failProvisioning(AwsProvisioningStep::CONNECTING_CLAIM,
                                    "Failed to connect with claim credentials");
        }

        // Step 3: Subscribe to response topics
        lastResult_.lastStep = AwsProvisioningStep::SUBSCRIBING_TOPICS;
        if (!subscribeToResponseTopics())
        {
            mqttClient_->disconnect();
            return failProvisioning(AwsProvisioningStep::SUBSCRIBING_TOPICS,
                                    "Failed to subscribe to response topics");
        }

        // Step 4: Generate keypair + CSR
        lastResult_.lastStep = AwsProvisioningStep::GENERATING_KEYPAIR;
        if (!generateKeyPair())
        {
            mqttClient_->disconnect();
            return failProvisioning(AwsProvisioningStep::GENERATING_KEYPAIR, "Failed to generate keypair");
        }

        // Step 5: Request certificate from CSR
        lastResult_.lastStep = AwsProvisioningStep::SENDING_CSR;
        if (!requestCertificateFromCsr())
        {
            mqttClient_->disconnect();
            return failProvisioning(AwsProvisioningStep::SENDING_CSR, "Failed to send CSR to AWS");
        }

        // Wait for certificate response (poll MQTT events)
        lastResult_.lastStep = AwsProvisioningStep::RECEIVING_CERT;
        if (!waitForCertificate())
        {
            unsubscribeCsrTopics_();
            unsubscribeRegisterTopics_();
            mqttClient_->disconnect();
            return failProvisioning(AwsProvisioningStep::RECEIVING_CERT,
                                    "Failed to receive certificate from AWS");
        }

        // Unsubscribe from CSR topics — no longer needed
        unsubscribeCsrTopics_();

        // Step 6: Register Thing with template
        lastResult_.lastStep = AwsProvisioningStep::REGISTERING_THING;
        if (!registerThing())
        {
            unsubscribeRegisterTopics_();
            mqttClient_->disconnect();
            return failProvisioning(AwsProvisioningStep::REGISTERING_THING,
                                    "Failed to register Thing with template");
        }

        // Wait for RegisterThing response
        if (!waitForRegisterThingResponse_())
        {
            unsubscribeRegisterTopics_();
            mqttClient_->disconnect();
            return failProvisioning(AwsProvisioningStep::REGISTERING_THING,
                                    "Timed out waiting for RegisterThing response from AWS");
        }

        // Unsubscribe from RegisterThing topics — done
        unsubscribeRegisterTopics_();

        // Step 7: Store final credentials
        lastResult_.lastStep = AwsProvisioningStep::STORING_CREDS;
        if (!storeFinalCredentials())
        {
            mqttClient_->disconnect();
            return failProvisioning(AwsProvisioningStep::STORING_CREDS, "Failed to store final credentials");
        }

        // Success!
        mqttClient_->disconnect();
        lastResult_.lastStep = AwsProvisioningStep::COMPLETE;
        lastResult_.success = true;
        status_ = ProvisioningStatus::PROVISIONED;
        return true;
    }

    // ---------- MQTT Callbacks ----------

    void onCertificateAccepted(const char *payload, size_t length)
    {
#ifdef CONFIG_LOPCORE_PROV_AWS_CBOR
        // Parse CBOR response
        std::vector<uint8_t> data(payload, payload + length);
        auto response = FleetProvisioningSerializer::parseCsrResponse(data);
        if (response.has_value())
        {
            certificateReceived_ = response->certificatePem;
            certificateId_ = response->certificateId;
            certificateOwnershipToken_ = response->ownershipToken;
            lastResult_.certificatePem = certificateReceived_;
        }
        else
        {
            certificateReceived_ = ""; // Parse failed
        }
#else
        // Parse JSON response using cJSON
        // Expected: {"certificatePem":"...","certificateId":"...","certificateOwnershipToken":"..."}
        cJSON *root = cJSON_ParseWithLength(payload, length);
        if (root)
        {
            const cJSON *pemItem = cJSON_GetObjectItemCaseSensitive(root, "certificatePem");
            const cJSON *idItem = cJSON_GetObjectItemCaseSensitive(root, "certificateId");
            const cJSON *tokItem = cJSON_GetObjectItemCaseSensitive(root, "certificateOwnershipToken");
            if (cJSON_IsString(pemItem) && pemItem->valuestring)
            {
                certificateReceived_ = pemItem->valuestring;
                lastResult_.certificatePem = certificateReceived_;
            }
            if (cJSON_IsString(idItem) && idItem->valuestring)
            {
                certificateId_ = idItem->valuestring;
            }
            if (cJSON_IsString(tokItem) && tokItem->valuestring)
            {
                certificateOwnershipToken_ = tokItem->valuestring;
            }
            cJSON_Delete(root);
        }
        if (certificateReceived_.empty())
        {
            ESP_LOGE("AwsFleetProv", "onCertificateAccepted: failed to parse response");
        }
#endif
    }

    void onCertificateRejected(const char *payload, size_t length)
    {
        certificateReceived_ = ""; // Mark as failed
        lastResult_.errorMessage = "Certificate request rejected by AWS";
    }

    void onRegisterThingAccepted(const char *payload, size_t length)
    {
#ifdef CONFIG_LOPCORE_PROV_AWS_CBOR
        // Parse CBOR response
        std::vector<uint8_t> data(payload, payload + length);
        auto thingName = FleetProvisioningSerializer::parseRegisterThingResponse(data);
        if (thingName.has_value())
        {
            lastResult_.deviceId = *thingName;
        }
        else
        {
            lastResult_.deviceId = "";
        }
#else
        // Parse JSON response using cJSON
        // Expected: {"thingName":"...","deviceConfiguration":{...}}
        cJSON *root = cJSON_ParseWithLength(payload, length);
        if (root)
        {
            const cJSON *nameItem = cJSON_GetObjectItemCaseSensitive(root, "thingName");
            if (cJSON_IsString(nameItem) && nameItem->valuestring)
            {
                lastResult_.deviceId = nameItem->valuestring;
            }
            else
            {
                lastResult_.deviceId = "";
                ESP_LOGE("AwsFleetProv", "onRegisterThingAccepted: thingName missing in response");
            }
            cJSON_Delete(root);
        }
        else
        {
            lastResult_.deviceId = "";
            ESP_LOGE("AwsFleetProv", "onRegisterThingAccepted: JSON parse failed");
        }
#endif
    }

    void onRegisterThingRejected(const char *payload, size_t length)
    {
        lastResult_.deviceId = "";
        lastResult_.errorMessage = "RegisterThing rejected by AWS";
    }

    // ---------- Member Variables ----------

    AwsProvisioningConfig config_;
    std::shared_ptr<TMqttClient> mqttClient_;
    ProvisioningStatus status_;
    AwsProvisioningResult lastResult_;

    // State tracking
    std::string certificateReceived_;
    std::string csrPem_;
    std::string claimRootCa_;
    std::string certificateId_;
    std::string certificateOwnershipToken_;

    // Subscribed topic strings (stored for unsubscription after each phase)
    std::string csrAcceptTopic_;
    std::string csrRejectTopic_;
    std::string regAcceptTopic_;
    std::string regRejectTopic_;
};

} // namespace prov
} // namespace lopcore
