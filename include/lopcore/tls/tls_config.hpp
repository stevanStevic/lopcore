/**
 * @file tls_config.hpp
 * @brief TLS connection configuration and builder
 *
 * Provides a type-safe configuration structure and fluent builder API
 * for TLS connection parameters.
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
    std::string caCertPath;      ///< Path to CA certificate file (e.g., "/spiffs/certs/AmazonRootCA1.crt")
    std::string clientCertLabel; ///< PKCS#11 label for client certificate
    std::string clientKeyLabel;  ///< PKCS#11 label for client private key

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
    std::chrono::milliseconds recvTimeout{10000};       ///< Receive timeout (default 10s)

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
            if (clientCertLabel.empty())
            {
                LOPCORE_LOGE(TAG,
                             "Validation failed: client certificate label is required when verifyPeer=true");
                hasError = true;
            }

            if (clientKeyLabel.empty())
            {
                LOPCORE_LOGE(TAG, "Validation failed: private key label is required when verifyPeer=true");
                hasError = true;
            }

            // When using PKCS#11 (cert/key labels), CA certificate is required for server verification
            // This prevents the cryptic "Arguments cannot be NULL" error from mbedtls-pkcs11
            if (!clientCertLabel.empty() && caCertPath.empty())
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
