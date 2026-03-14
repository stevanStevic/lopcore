#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include "storage_types.hpp"
#include "certificate_manager.hpp"

// Claim certificate PKCS#11 label defaults (may be overridden by core_pkcs11_config.h)
#ifdef CONFIG_LOPCORE_PROV_CERT_COREP11
#include <core_pkcs11_config.h>
#endif
#ifndef pkcs11configLABEL_CLAIM_CERTIFICATE
#define pkcs11configLABEL_CLAIM_CERTIFICATE "Claim Cert"
#endif
#ifndef pkcs11configLABEL_CLAIM_PRIVATE_KEY
#define pkcs11configLABEL_CLAIM_PRIVATE_KEY "Claim Key"
#endif

namespace lopcore {
namespace prov {

/**
 * Cloud provider enumeration
 */
enum class CloudProvider {
    AWS,
    GCP,
    AZURE,
    CUSTOM
};

/**
 * Base cloud provisioning configuration
 * 
 * Provides common configuration for cloud-specific provisioning flows.
 * Uses lambda-based storage callbacks for maximum flexibility.
 */
class CloudProvisioningConfig {
public:
    CloudProvisioningConfig()
        : provider_(CloudProvider::AWS)
        , maxRetries_(3)
        , retryDelaySeconds_(5)
    {}
    
    virtual ~CloudProvisioningConfig() = default;

    /**
     * Set cloud provider
     */
    CloudProvisioningConfig& setProvider(CloudProvider provider) {
        provider_ = provider;
        return *this;
    }

    /**
     * Set certificate manager for cert operations
     * 
     * @param certMgr Shared pointer to CertificateManager instance
     */
    CloudProvisioningConfig& setCertificateManager(std::shared_ptr<CertificateManager> certMgr) {
        certificateManager_ = certMgr;
        return *this;
    }

    /**
     * Add storage callbacks for a specific config key
     * 
     * Allows different config keys to use different storage backends:
     * - Critical configs (endpoint, thing_name) -> Encrypted NVS
     * - Large files (root_ca) -> SPIFFS
     * - Metadata (template_name) -> Plain NVS
     * 
     * Example:
     *   config.addConfigStorage("aws_endpoint",
     *       storage::makeNvsStorage(nvsEncrypted))
     *         .addConfigStorage("root_ca",
     *       storage::makeSpiffsStorage(spiffs));
     * 
     * @param key Configuration key
     * @param callbacks Storage callbacks (lambda-based)
     */
    CloudProvisioningConfig& addConfigStorage(const std::string& key,
                                              StorageCallbacks callbacks) {
        configStorages_[key] = callbacks;
        return *this;
    }

    /**
     * Set default storage callbacks for unmapped keys
     * 
     * If a config key doesn't have specific storage, this fallback will be used.
     * 
     * @param callbacks Default storage callbacks
     */
    CloudProvisioningConfig& setDefaultConfigStorage(StorageCallbacks callbacks) {
        defaultConfigStorage_ = callbacks;
        return *this;
    }

    /**
     * Set retry configuration
     * 
     * @param maxRetries Maximum number of retry attempts
     * @param delaySeconds Delay between retries (seconds)
     */
    CloudProvisioningConfig& setRetries(int maxRetries, int delaySeconds) {
        maxRetries_ = maxRetries;
        retryDelaySeconds_ = delaySeconds;
        return *this;
    }

    // Getters
    CloudProvider provider() const { return provider_; }
    std::shared_ptr<CertificateManager> certificateManager() const { return certificateManager_; }
    const StorageCallbackMap& configStorages() const { return configStorages_; }
    std::optional<StorageCallbacks> defaultConfigStorage() const { return defaultConfigStorage_; }
    int maxRetries() const { return maxRetries_; }
    int retryDelaySeconds() const { return retryDelaySeconds_; }

    /**
     * Get storage callbacks for a specific config key
     * 
     * Returns specific callbacks if found, otherwise default callbacks.
     * Returns std::nullopt if neither exist.
     * 
     * @param key Configuration key
     * @return Storage callbacks, or std::nullopt if not found
     */
    std::optional<StorageCallbacks> getStorageForKey(const std::string& key) const {
        auto it = configStorages_.find(key);
        if (it != configStorages_.end()) {
            return it->second;
        }
        return defaultConfigStorage_;
    }

    /**
     * Read configuration value from storage
     * 
     * Uses getStorageForKey() to find appropriate storage backend.
     * 
     * @param key Configuration key
     * @return Value if found, std::nullopt otherwise
     */
    std::optional<std::string> readConfig(const std::string& key) const {
        auto storage = getStorageForKey(key);
        if (storage.has_value()) {
            return storage->read(key);
        }
        return std::nullopt;
    }

    /**
     * Write configuration value to storage
     * 
     * Uses getStorageForKey() to find appropriate storage backend.
     * 
     * @param key Configuration key
     * @param value Configuration value
     * @return true if write succeeded
     */
    bool writeConfig(const std::string& key, const std::string& value) {
        auto storage = getStorageForKey(key);
        if (storage.has_value()) {
            return storage->write(key, value);
        }
        return false;
    }

protected:
    CloudProvider provider_;
    std::shared_ptr<CertificateManager> certificateManager_;
    StorageCallbackMap configStorages_;  // Map of config key -> storage callbacks
    std::optional<StorageCallbacks> defaultConfigStorage_;  // Fallback storage
    int maxRetries_;
    int retryDelaySeconds_;
};

/**
 * AWS Fleet Provisioning configuration
 * 
 * AWS-specific configuration for IoT Fleet Provisioning with
 * "provisioning by claim" flow using temporary claim certificates.
 */
class AwsProvisioningConfig : public CloudProvisioningConfig {
public:
    AwsProvisioningConfig()
        : csrBufferSize_(2048)
        , certBufferSize_(2048)
        , endpointKey_("aws_endpoint")
        , templateKey_("prov_template")
        , thingNameKey_("thing_name")
        , rootCaKey_("root_ca")
        , claimCertKey_("claim_cert")
        , claimKeyKey_("claim_key")
        , deviceCertLabel_("device_cert")
        , deviceKeyLabel_("device_key")
        , claimCertLabel_(pkcs11configLABEL_CLAIM_CERTIFICATE)
        , claimKeyLabel_(pkcs11configLABEL_CLAIM_PRIVATE_KEY)
        , csrSubjectName_("CN=LopCore Device")
        , deviceIdProvider_(nullptr)
    {
        provider_ = CloudProvider::AWS;
        
        // Set defaults from Kconfig if available
#ifdef CONFIG_LOPCORE_PROV_AWS_CSR_BUFFER_SIZE
        csrBufferSize_ = CONFIG_LOPCORE_PROV_AWS_CSR_BUFFER_SIZE;
#endif
#ifdef CONFIG_LOPCORE_PROV_AWS_CERT_BUFFER_SIZE
        certBufferSize_ = CONFIG_LOPCORE_PROV_AWS_CERT_BUFFER_SIZE;
#endif
#ifdef CONFIG_LOPCORE_PROV_AWS_MAX_RETRIES
        maxRetries_ = CONFIG_LOPCORE_PROV_AWS_MAX_RETRIES;
#endif
#ifdef CONFIG_LOPCORE_PROV_AWS_RETRY_DELAY_SEC
        retryDelaySeconds_ = CONFIG_LOPCORE_PROV_AWS_RETRY_DELAY_SEC;
#endif
    }

    /**
     * Set configuration key names
     * 
     * Defines which keys are used for AWS-specific configs.
     * These keys are used with the storage callbacks set via addConfigStorage().
     * 
     * @param endpointKey AWS IoT endpoint key (default: "aws_endpoint")
     * @param templateKey Provisioning template name key (default: "provisioning_template")
     * @param thingNameKey Thing name key (default: "thing_name")
     * @param rootCaKey Root CA certificate key (default: "root_ca")
     */
    AwsProvisioningConfig& setConfigKeys(const std::string& endpointKey,
                                        const std::string& templateKey,
                                        const std::string& thingNameKey,
                                        const std::string& rootCaKey = "root_ca") {
        endpointKey_ = endpointKey;
        templateKey_ = templateKey;
        thingNameKey_ = thingNameKey;
        rootCaKey_ = rootCaKey;
        return *this;
    }

    /**
     * Set storage keys for claim credentials (for BLE or pre-flashed mode)
     * 
     * @param certKey Storage key for claim certificate PEM
     * @param keyKey Storage key for claim private key PEM
     * @param rootCaKey Storage key for root CA (optional, default: "root_ca")
     */
    AwsProvisioningConfig& setClaimCertKeys(const std::string& certKey,
                                           const std::string& keyKey,
                                           const std::string& rootCaKey = "root_ca") {
        claimCertKey_ = certKey;
        claimKeyKey_ = keyKey;
        rootCaKey_ = rootCaKey;
        return *this;
    }

    /**
     * Set PKCS#11 labels for claim credentials
     * 
     * @param certLabel PKCS11 label for claim certificate
     * @param keyLabel PKCS11 label for claim private key
     */
    AwsProvisioningConfig& setClaimCertLabels(const std::string& certLabel,
                                             const std::string& keyLabel) {
        claimCertLabel_ = certLabel;
        claimKeyLabel_ = keyLabel;
        return *this;
    }

    /**
     * Set PKCS#11 labels for final device certificates
     * 
     * @param certLabel Certificate label (default: "device_cert")
     * @param keyLabel Private key label (default: "device_key")
     */
    AwsProvisioningConfig& setCertificateLabels(const std::string& certLabel,
                                               const std::string& keyLabel) {
        deviceCertLabel_ = certLabel;
        deviceKeyLabel_ = keyLabel;
        return *this;
    }

    /**
     * Set CSR subject name (X.509 CN field)
     * 
     * @param subjectName Subject common name (e.g., "CN=MyDevice", default: "CN=LopCore Device")
     */
    AwsProvisioningConfig& setCsrSubjectName(const std::string& subjectName) {
        csrSubjectName_ = subjectName;
        return *this;
    }

    /**
     * Set device ID provider callback
     * 
     * This callback is invoked to get the device  serial number or unique ID
     * for RegisterThing parameters. Common implementations:
     * - MAC address-based
     * - UUID
     * - Custom factory serial number
     * 
     * @param provider Function that returns device ID string
     */
    AwsProvisioningConfig& setDeviceIdProvider(std::function<std::string()> provider) {
        deviceIdProvider_ = provider;
        return *this;
    }

    /**
     * Set additional RegisterThing parameters
     *
     * These are merged into the "parameters" map in the RegisterThing payload.
     * The device serial number from deviceIdProvider() is added automatically
     * as "SerialNumber" unless overridden here.
     *
     * @param params Key-value pairs sent as RegisterThing parameters
     */
    AwsProvisioningConfig& setRegisterThingParameters(std::map<std::string, std::string> params) {
        registerThingParameters_ = std::move(params);
        return *this;
    }

    /**
     * Set CSR and certificate buffer sizes
     * 
     * @param csrSize CSR buffer size (default: 2048)
     * @param certSize Certificate buffer size (default: 2048)
     */
    AwsProvisioningConfig& setBufferSizes(size_t csrSize, size_t certSize) {
        csrBufferSize_ = csrSize;
        certBufferSize_ = certSize;
        return *this;
    }

    // Getters
    const std::string& endpointKey() const { return endpointKey_; }
    const std::string& templateKey() const { return templateKey_; }
    const std::string& thingNameKey() const { return thingNameKey_; }
    const std::string& rootCaKey() const { return rootCaKey_; }
    const std::string& claimCertKey() const { return claimCertKey_; }
    const std::string& claimKeyKey() const { return claimKeyKey_; }
    const std::string& claimCertLabel() const { return claimCertLabel_; }
    const std::string& claimKeyLabel() const { return claimKeyLabel_; }
    const std::string& deviceCertLabel() const { return deviceCertLabel_; }
    const std::string& deviceKeyLabel() const { return deviceKeyLabel_; }
    const std::string& csrSubjectName() const { return csrSubjectName_; }
    const std::function<std::string()>& deviceIdProvider() const { return deviceIdProvider_; }
    const std::map<std::string, std::string>& registerThingParameters() const { return registerThingParameters_; }
    size_t csrBufferSize() const { return csrBufferSize_; }
    size_t certBufferSize() const { return certBufferSize_; }

    // Helper methods to read AWS-specific configs
    std::optional<std::string> getEndpoint() const { return readConfig(endpointKey_); }
    std::optional<std::string> getTemplateName() const { return readConfig(templateKey_); }
    std::optional<std::string> getThingName() const { return readConfig(thingNameKey_); }
    std::optional<std::string> getRootCa() const { return readConfig(rootCaKey_); }
    std::optional<std::string> getClaimCert() const { return readConfig(claimCertKey_); }
    std::optional<std::string> getClaimKey() const { return readConfig(claimKeyKey_); }

private:
    size_t csrBufferSize_;
    size_t certBufferSize_;
    std::string endpointKey_;
    std::string templateKey_;
    std::string thingNameKey_;
    std::string rootCaKey_;
    std::string claimCertKey_;
    std::string claimKeyKey_;
    std::string deviceCertLabel_;
    std::string deviceKeyLabel_;
    std::string claimCertLabel_;
    std::string claimKeyLabel_;
    std::string csrSubjectName_;
    std::function<std::string()> deviceIdProvider_;
    std::map<std::string, std::string> registerThingParameters_;
};

} // namespace prov
} // namespace lopcore
