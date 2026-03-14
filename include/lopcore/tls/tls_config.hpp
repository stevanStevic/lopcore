/**
 * @file tls_config.hpp
 * @brief TLS connection configuration and builder
 *
 * Provides a type-safe configuration structure and fluent builder API
 * for TLS connection parameters.
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <esp_err.h>
#include <lopcore/logging/logger.hpp>

namespace lopcore
{
namespace tls
{

/**
 * @brief Unified TLS connection configuration
 *
 * This configuration is used across all components that need TLS:
 * - MQTT clients (CoreMqttClient, EspMqttClient)
 * - HTTP clients
 * - Other secure transport protocols
 *
 * Contains all parameters needed to establish a TLS connection including
 * server details, certificate labels, timeouts, and protocol options.
 *
 * @note This replaces the previous separate mqtt::TlsConfig
 */
struct TlsConfig
{
    // ========================================================================
    // Server details
    // ========================================================================
    std::string hostname; ///< Remote hostname or IP address
    uint16_t port{0};     ///< Remote port number

    // ========================================================================
    // Certificate/key configuration (PKCS#11)
    // ========================================================================
    std::string caCertPath;      ///< Path to CA certificate file or inline PEM string
    std::string clientCertLabel; ///< PKCS#11 label for client certificate
    std::string clientKeyLabel;  ///< PKCS#11 label for client private key

    // ========================================================================
    // Certificate/key configuration (in-memory PEM — alternative to PKCS#11)
    // Use these when credentials are available as PEM strings at runtime,
    // e.g. during provisioning before they are imported into PKCS#11 storage.
    // ========================================================================
    std::string clientCertPem; ///< In-memory client certificate PEM (mutually exclusive with clientCertLabel)
    std::string clientKeyPem;  ///< In-memory client private key PEM (mutually exclusive with clientKeyLabel)

    // ========================================================================
    // TLS protocol options
    // ========================================================================
    std::vector<std::string> alpnProtocols; ///< ALPN protocol list (e.g., "x-amzn-mqtt-ca" for AWS IoT)
    bool enableSni{true};                   ///< Enable Server Name Indication
    bool verifyPeer{true};                  ///< Verify server certificate
    bool skipCommonNameCheck{false};        ///< Skip CN verification (for testing/debugging only)

    // ========================================================================
    // Timeout configuration
    // ========================================================================
    std::chrono::milliseconds connectionTimeout{30000}; ///< Connection timeout (default 30s)
    std::chrono::milliseconds sendTimeout{10000};       ///< Send timeout (default 10s)
    /// Receive timeout (default 500ms).
    ///
    /// IMPORTANT — mutex contention: CoreMqttClient's processLoopTask holds
    /// mutex_ for the entire duration of each MQTT_ProcessLoop call, which in
    /// turn blocks in mbedtls_ssl_read for up to recvTimeout waiting for data.
    /// Any concurrent publish()/subscribe() must wait for that mutex.  With
    /// the old 10 s default this caused ~10 s stalls; keep recvTimeout short
    /// (≤ a few hundred ms) so the background task yields the mutex quickly.
    /// Timeout on recv returns MQTTNeedMoreBytes to coreMQTT — not an error.
    std::chrono::milliseconds recvTimeout{500};

    // Legacy timeout (for backwards compatibility with milliseconds-based APIs)
    uint32_t timeoutMs{10000}; ///< TLS handshake timeout (ms) - deprecated, use connectionTimeout

    // ========================================================================
    // Retry configuration
    // ========================================================================
    uint32_t maxRetries{5};                        ///< Maximum connection retry attempts
    std::chrono::milliseconds retryBaseDelay{500}; ///< Base retry delay (exponential backoff)
    std::chrono::milliseconds retryMaxDelay{5000}; ///< Maximum retry delay

    /**
     * @brief Validate configuration
     * @return ESP_OK if valid, error code otherwise
     */
    esp_err_t validate() const
    {
        static const char *TAG = "TlsConfig";
        bool hasError = false;

        if (hostname.empty())
        {
            LOPCORE_LOGE(TAG, "Validation failed: hostname is required");
            hasError = true;
        }

        if (port == 0)
        {
            LOPCORE_LOGE(TAG, "Validation failed: port must be non-zero");
            hasError = true;
        }

        // Certificate validation only if peer verification enabled
        if (verifyPeer)
        {
            bool hasPkcs11Cert = !clientCertLabel.empty();
            bool hasInMemoryCert = !clientCertPem.empty();

            if (!hasPkcs11Cert && !hasInMemoryCert)
            {
                LOPCORE_LOGE(TAG, "Validation failed: client certificate required (set clientCertLabel for "
                                  "PKCS#11 or clientCertPem for in-memory PEM)");
                hasError = true;
            }

            if (hasPkcs11Cert && clientKeyLabel.empty())
            {
                LOPCORE_LOGE(TAG, "Validation failed: private key label is required when using PKCS#11");
                hasError = true;
            }

            if (hasInMemoryCert && clientKeyPem.empty())
            {
                LOPCORE_LOGE(TAG, "Validation failed: clientKeyPem is required when clientCertPem is set");
                hasError = true;
            }

            // When using PKCS#11, CA certificate is required for server verification
            if (hasPkcs11Cert && caCertPath.empty())
            {
                LOPCORE_LOGE(TAG, "Validation failed: CA certificate path is required when using PKCS#11");
                hasError = true;
            }
        }

        if (hasError)
        {
            LOPCORE_LOGE(TAG, "TLS configuration is invalid");
            return ESP_ERR_INVALID_ARG;
        }

        return ESP_OK;
    }
};

/**
 * @brief Fluent builder for TlsConfig
 *
 * Provides a convenient API for constructing TLS configurations with
 * method chaining. Works for MQTT, HTTP, and any protocol requiring TLS.
 *
 * Example (MQTT):
 * @code
 * auto config = TlsConfigBuilder()
 *     .hostname("mqtt.example.com")
 *     .port(8883)
 *     .caCertificate("/spiffs/certs/root-ca.crt")
 *     .clientCertificate("device-cert")
 *     .privateKey("device-key")
 *     .alpn("x-amzn-mqtt-ca")
 *     .connectionTimeout(std::chrono::seconds(30))
 *     .build();
 * @endcode
 *
 * Example (HTTP):
 * @code
 * auto config = TlsConfigBuilder()
 *     .hostname("api.example.com")
 *     .port(443)
 *     .caCertificate("/spiffs/certs/root-ca.crt")
 *     .verifyPeer(true)
 *     .build();
 * @endcode
 */
class TlsConfigBuilder
{
public:
    TlsConfigBuilder() = default;

    /**
     * @brief Set remote hostname
     */
    TlsConfigBuilder &hostname(const std::string &host)
    {
        config_.hostname = host;
        return *this;
    }

    /**
     * @brief Set remote port
     */
    TlsConfigBuilder &port(uint16_t p)
    {
        config_.port = p;
        return *this;
    }

    /**
     * @brief Set CA certificate file path
     */
    TlsConfigBuilder &caCertificate(const std::string &path)
    {
        config_.caCertPath = path;
        return *this;
    }

    /**
     * @brief Set client certificate PKCS#11 label
     */
    TlsConfigBuilder &clientCertificate(const std::string &label)
    {
        config_.clientCertLabel = label;
        return *this;
    }

    /**
     * @brief Set private key PKCS#11 label
     */
    TlsConfigBuilder &privateKey(const std::string &label)
    {
        config_.clientKeyLabel = label;
        return *this;
    }

    /**
     * @brief Set in-memory client certificate PEM (use instead of PKCS#11 label)
     *
     * Intended for provisioning or testing scenarios where credentials are
     * available as PEM strings before PKCS#11 import.
     */
    TlsConfigBuilder &clientCertificatePem(const std::string &pem)
    {
        config_.clientCertPem = pem;
        return *this;
    }

    /**
     * @brief Set in-memory client private key PEM (use instead of PKCS#11 label)
     */
    TlsConfigBuilder &privateKeyPem(const std::string &pem)
    {
        config_.clientKeyPem = pem;
        return *this;
    }

    /**
     * @brief Add single ALPN protocol
     */
    TlsConfigBuilder &alpn(const std::string &protocol)
    {
        config_.alpnProtocols.push_back(protocol);
        return *this;
    }

    /**
     * @brief Set multiple ALPN protocols
     */
    TlsConfigBuilder &alpnProtocols(const std::vector<std::string> &protocols)
    {
        config_.alpnProtocols = protocols;
        return *this;
    }

    /**
     * @brief Enable/disable Server Name Indication
     */
    TlsConfigBuilder &sni(bool enable)
    {
        config_.enableSni = enable;
        return *this;
    }

    /**
     * @brief Enable/disable peer certificate verification
     */
    TlsConfigBuilder &verifyPeer(bool verify)
    {
        config_.verifyPeer = verify;
        return *this;
    }

    /**
     * @brief Skip common name check (for testing only)
     */
    TlsConfigBuilder &skipCommonNameCheck(bool skip)
    {
        config_.skipCommonNameCheck = skip;
        return *this;
    }

    /**
     * @brief Set connection timeout
     */
    TlsConfigBuilder &connectionTimeout(std::chrono::milliseconds timeout)
    {
        config_.connectionTimeout = timeout;
        return *this;
    }

    /**
     * @brief Set send timeout
     */
    TlsConfigBuilder &sendTimeout(std::chrono::milliseconds timeout)
    {
        config_.sendTimeout = timeout;
        return *this;
    }

    /**
     * @brief Set receive timeout
     */
    TlsConfigBuilder &recvTimeout(std::chrono::milliseconds timeout)
    {
        config_.recvTimeout = timeout;
        return *this;
    }

    /**
     * @brief Set maximum retry attempts
     */
    TlsConfigBuilder &maxRetries(uint32_t retries)
    {
        config_.maxRetries = retries;
        return *this;
    }

    /**
     * @brief Set retry base delay (for exponential backoff)
     */
    TlsConfigBuilder &retryBaseDelay(std::chrono::milliseconds delay)
    {
        config_.retryBaseDelay = delay;
        return *this;
    }

    /**
     * @brief Set maximum retry delay
     */
    TlsConfigBuilder &retryMaxDelay(std::chrono::milliseconds delay)
    {
        config_.retryMaxDelay = delay;
        return *this;
    }

    /**
     * @brief Build and return the configuration
     *
     * @return Constructed TlsConfig object
     */
    TlsConfig build() const
    {
        return config_;
    }

private:
    TlsConfig config_;
};

} // namespace tls
} // namespace lopcore
