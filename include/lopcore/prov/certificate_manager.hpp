#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lopcore
{
namespace prov
{

/**
 * Certificate Manager - Centralized certificate lifecycle management
 *
 * Handles:
 * - Importing claim certificates (temporary, from BLE or pre-flashed)
 * - Generating EC P-256 key pairs in PKCS11
 * - Generating Certificate Signing Requests (CSR) with PKCS11-backed signing
 * - Storing final device certificates (permanent, in PKCS11)
 * - Deleting claim/device credentials (for factory reset)
 *
 * Backend Selection:
 * - CONFIG_LOPCORE_PROV_CERT_COREP11: Use AWS corePKCS11 (compile-time)
 * - CONFIG_LOPCORE_PROV_CERT_MBEDTLS_ONLY: Use mbedTLS only (software keys)
 */
class CertificateManager
{
public:
    /**
     * PKCS11 backend selection
     */
    enum class Pkcs11Backend
    {
        AWS_CORE_PKCS11, ///< AWS corePKCS11 backed by NVS (recommended for production)
        ESP_IDF_PKCS11   ///< ESP-IDF native PKCS11 (future support)
    };

    /**
     * Certificate Manager configuration
     */
    struct Config
    {
        // Buffer sizes for operations
        size_t csrBufferSize = 2048;  ///< CSR buffer size (bytes)
        size_t certBufferSize = 2048; ///< Certificate buffer size (bytes)

        /// Use PKCS11 for key generation and certificate storage.
        /// When false, mbedTLS software keys are used (development mode).
        bool usePkcs11 = false;

        /// PKCS11 backend to use when usePkcs11 == true.
        Pkcs11Backend pkcs11Backend = Pkcs11Backend::AWS_CORE_PKCS11;

        // PKCS11 labels (optional customization)
        std::optional<std::string> pkcs11TokenLabel; ///< PKCS11 token label (e.g., "MyDevice")
        std::optional<std::string> pkcs11Pin;        ///< PKCS11 PIN (if required)
    };

    /**
     * Construct Certificate Manager
     *
     * Initializes PKCS11 session if CONFIG_LOPCORE_PROV_CERT_COREP11 enabled.
     *
     * @param config Configuration
     */
    explicit CertificateManager(const Config &config);

    /**
     * Destructor
     *
     * Closes PKCS11 session and cleans up resources.
     */
    ~CertificateManager();

    // Delete copy constructor and assignment
    CertificateManager(const CertificateManager &) = delete;
    CertificateManager &operator=(const CertificateManager &) = delete;

    // ========== Claim Certificate Management ==========

    /**
     * Import claim certificate and private key (temporary credentials)
     *
     * Stores in PKCS11 with claim-specific labels:
     * - pkcs11configLABEL_CLAIM_CERTIFICATE
     * - pkcs11configLABEL_CLAIM_PRIVATE_KEY
     *
     * @param certPem Certificate PEM string
     * @param keyPem Private key PEM string
     * @return true if import succeeded
     */
    bool importClaimCertificate(const std::string &certPem, const std::string &keyPem);

    /**
     * Import claim certificate and private key from raw byte vectors.
     *
     * Convenience overload — vectors are treated as PEM data.
     *
     * @param certData Certificate data (PEM bytes)
     * @param keyData  Private key data (PEM bytes)
     * @return true if import succeeded
     */
    bool importClaimCertificate(const std::vector<uint8_t> &certData, const std::vector<uint8_t> &keyData);

    /**
     * Check if claim certificate is loaded
     *
     * @return true if claim cert and key are in PKCS11
     */
    bool hasClaimCertificate() const;

    /**
     * Get claim certificate PEM (for MQTT connection with claim creds)
     *
     * @return Claim certificate PEM, or std::nullopt if not loaded
     */
    std::optional<std::string> getClaimCertPem() const;

    /**
     * Get claim private key PEM (for MQTT connection with claim creds)
     *
     * Note: This is typically not needed as PKCS11 handles signing.
     * Included for compatibility with certain MQTT client implementations.
     *
     * @return Claim private key PEM, or std::nullopt if not loaded
     */
    std::optional<std::string> getClaimKeyPem() const;

    /**
     * Delete claim credentials (after successful provisioning)
     *
     * Removes claim cert and key from PKCS11.
     *
     * @return true if deletion succeeded
     */
    bool deleteClaimCredentials();

    // ========== Device Key Pair and CSR Generation ==========

    /**
     * Generate EC P-256 key pair and CSR (PKCS11-backed)
     *
     * This is the primary method for fleet provisioning:
     * 1. Generates EC P-256 key pair in PKCS11
     * 2. Extracts public key for CSR
     * 3. Creates CSR signed via PKCS11 private key
     *
     * Labels used:
     * - pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS
     * - pkcs11configLABEL_DEVICE_PUBLIC_KEY_FOR_TLS
     *
     * @param commonName X.509 subject common name (e.g., "CN=MyDevice")
     * @param csrOut Output buffer for CSR PEM
     * @return true if generation succeeded
     */
    bool generateKeyPairAndCsr(const std::string &commonName, std::string &csrOut);

    /**
     * Generate CSR from existing key pair (mbedTLS path, for development)
     *
     * Legacy method: generates software key pair with mbedTLS.
     * Used when CONFIG_LOPCORE_PROV_CERT_MBEDTLS_ONLY is enabled.
     *
     * @param commonName X.509 subject common name
     * @param csrOut Output buffer for CSR PEM
     * @return true if generation succeeded
     */
    bool generateCsr(const std::string &commonName, std::string &csrOut);

    // ========== Final Certificate Storage ==========

    /**
     * Store final device certificate (permanent credentials)
     *
     * Converts PEM to DER and stores in PKCS11.
     * Default label: pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS
     *
     * @param label PKCS11 label for the certificate
     * @param certPem Certificate PEM string
     * @return true if storage succeeded
     */
    bool storeFinalCertificate(const std::string &label, const std::string &certPem);

    /**
     * Store final device certificate (raw bytes overload).
     *
     * @param label    PKCS11 label for the certificate
     * @param certData Certificate data (PEM bytes)
     * @return true if storage succeeded
     */
    bool storeFinalCertificate(const std::string &label, const std::vector<uint8_t> &certData);

    /**
     * Store final device private key (if needed for mbedTLS path)
     *
     * Note: When using PKCS11, the private key is already in secure storage
     * after generateKeyPairAndCsr(). This method is for mbedTLS-only mode.
     *
     * @param label PKCS11 label for the key
     * @param keyPem Private key PEM string
     * @return true if storage succeeded
     */
    bool storeFinalPrivateKey(const std::string &label, const std::string &keyPem);

    /**
     * Store final device private key (raw bytes overload).
     *
     * @param label   PKCS11 / storage label
     * @param keyData Private key data (PEM bytes)
     * @return true if storage succeeded
     */
    bool storeFinalPrivateKey(const std::string &label, const std::vector<uint8_t> &keyData);

    /**
     * Check if final certificate is stored in PKCS11
     *
     * @param label PKCS11 label to check
     * @return true if certificate exists
     */
    bool hasFinalCertificate(const std::string &label) const;

    // ========== Factory Reset ==========

    /**
     * Delete device credentials (for factory reset)
     *
     * Removes device cert and key from PKCS11.
     *
     * @return true if deletion succeeded
     */
    bool deleteDeviceCredentials();

    /**
     * Get configuration
     *
     * @return Current configuration
     */
    const Config &getConfig() const
    {
        return config_;
    }

private:
    Config config_;

    // Internal state
    struct Impl;
    std::unique_ptr<Impl> pImpl_; // PIMPL pattern for PKCS11 handle hiding

    // PKCS11 operations (compile-time backend selection)
    bool pkcs11StoreObject(const std::string &label, const std::vector<uint8_t> &data, uint32_t objectClass);
    bool pkcs11GetObject(const std::string &label, std::vector<uint8_t> &data, uint32_t objectClass) const;
    bool pkcs11ObjectExists(const std::string &label, uint32_t objectClass) const;
    bool pkcs11DeleteObject(const std::string &label, uint32_t objectClass);

    // Helper: Convert PEM to DER
    int pemToDer(const std::string &pem, std::vector<uint8_t> &der) const;

#if CONFIG_LOPCORE_PROV_CERT_COREP11
    // PKCS#11 path for key pair generation + CSR signing
    bool generateKeyPairAndCsrPkcs11_(const std::string &commonName, std::string &csrOut);
#endif
};

} // namespace prov
} // namespace lopcore
