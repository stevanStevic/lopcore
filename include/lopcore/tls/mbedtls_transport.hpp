#pragma once

#include "lopcore/tls/network_context.h"
#include "lopcore/tls/pkcs11_session.hpp"
#include "lopcore/tls/tls_config.hpp"
#include "lopcore/tls/tls_transport.hpp"

// C wrapper includes - only mbedtls_pkcs11_posix needed for Mbedtls_Pkcs11_* functions
#include "lopcore/tls/mbedtls_pkcs11_posix.h"

// FreeRTOS includes
#include <memory>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace lopcore
{
namespace tls
{

/**
 * @brief Concrete implementation of ITlsTransport using MbedTLS and PKCS#11
 *
 * This class wraps the existing C TLS implementation while providing a modern
 * C++ interface. It preserves all original functionality including:
 * - PKCS#11 integration for secure credential management
 * - Exponential backoff retry logic
 * - ALPN protocol support
 * - Server Name Indication (SNI)
 *
 * The implementation is thread-safe and uses esp_err_t for error handling
 * to maintain compatibility with ESP-IDF's -fno-exceptions build configuration.
 *
 * Example usage:
 * @code
 * auto config = TlsConfigBuilder()
 *     .hostname("mqtt.example.com")
 *     .port(8883)
 *     .clientCertificate("device-cert")
 *     .privateKey("device-key")
 *     .build();
 *
 * MbedtlsTransport transport;
 * esp_err_t err = transport.connect(config);
 * if (err == ESP_OK) {
 *     size_t bytesSent;
 *     transport.send(data, size, &bytesSent);
 * }
 * @endcode
 *
 * @note This class is NOT thread-safe for concurrent operations on the same
 *       instance. Use separate instances or external synchronization for
 *       concurrent connections.
 */
class MbedtlsTransport : public ITlsTransport
{
public:
    /**
     * @brief Construct a new MbedtlsTransport object
     *
     * Initializes the transport in a disconnected state. Call connect() to
     * establish a TLS connection.
     */
    MbedtlsTransport();

    /**
     * @brief Destroy the MbedtlsTransport object
     *
     * Automatically disconnects if still connected.
     */
    ~MbedtlsTransport() override;

    // Delete copy constructor and assignment operator
    MbedtlsTransport(const MbedtlsTransport &) = delete;
    MbedtlsTransport &operator=(const MbedtlsTransport &) = delete;

    // Allow move semantics
    MbedtlsTransport(MbedtlsTransport &&) noexcept;
    MbedtlsTransport &operator=(MbedtlsTransport &&) noexcept;

    /**
     * @brief Establish a TLS connection to the specified server
     *
     * This method:
     * 1. Validates the configuration
     * 2. Acquires a PKCS#11 session from Pkcs11Provider
     * 3. Establishes TLS connection with retry logic
     * 4. Sets up transport interface callbacks
     *
     * The connection uses exponential backoff retry logic (default: 500ms to 5000ms,
     * 5 attempts) as configured in TlsConfig.
     *
     * @param[in] config TLS configuration including server details, certificates, and options
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_ARG if config is invalid
     * @return ESP_ERR_NO_MEM if memory allocation fails
     * @return ESP_FAIL if connection fails after all retry attempts
     * @return ESP_ERR_INVALID_STATE if already connected
     *
     * @note This operation is synchronous and may take several seconds if retries
     *       are needed.
     */
    esp_err_t connect(const TlsConfig &config) override;

    /**
     * @brief Gracefully close the TLS connection
     *
     * Sends a close_notify alert to the server and releases all resources.
     * This method is safe to call multiple times.
     *
     * @note This method never throws and is safe to call from destructors
     */
    void disconnect() noexcept override;

    /**
     * @brief Send data over the established TLS connection
     *
     * @param[in] data Pointer to data buffer to send
     * @param[in] size Number of bytes to send
     * @param[out] bytesSent Number of bytes actually sent (may be NULL)
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_ARG if data is NULL or size is 0
     * @return ESP_ERR_INVALID_STATE if not connected
     * @return ESP_FAIL if send operation fails
     *
     * @note This function does not retry on failure. The caller should
     *       implement retry logic if needed.
     */
    esp_err_t send(const void *data, size_t size, size_t *bytesSent) override;

    /**
     * @brief Receive data from the established TLS connection
     *
     * This function blocks until at least one byte is received or a timeout occurs.
     * The timeout is configured in TlsConfig (default: 3000ms).
     *
     * @param[out] buffer Pointer to buffer to store received data
     * @param[in] size Maximum number of bytes to receive
     * @param[out] bytesReceived Number of bytes actually received (may be NULL)
     * @return ESP_OK on success (at least one byte received)
     * @return ESP_ERR_INVALID_ARG if buffer is NULL or size is 0
     * @return ESP_ERR_INVALID_STATE if not connected
     * @return ESP_ERR_TIMEOUT if no data received within timeout period
     * @return ESP_FAIL if receive operation fails
     *
     * @note A return value of ESP_ERR_TIMEOUT can be retried
     */
    esp_err_t recv(void *buffer, size_t size, size_t *bytesReceived) override;

    /**
     * @brief Check if currently connected to server
     *
     * @return true if connected, false otherwise
     */
    bool isConnected() const noexcept override;

    /**
     * @brief Get the underlying network context
     *
     * Returns a pointer to the NetworkContext structure used by coreMQTT and
     * other AWS IoT SDK libraries.
     *
     * @return Pointer to NetworkContext, or nullptr if not connected
     *
     * @note The returned pointer is valid until disconnect() is called
     */
    void *getNetworkContext() noexcept override;

private:
    /**
     * @brief Convert MbedTLS PKCS11 status to esp_err_t
     *
     * @param status MbedTLS PKCS11 status code
     * @return Corresponding esp_err_t value
     */
    static esp_err_t convertMbedtlsError(MbedtlsPkcs11Status_t status);

    /**
     * @brief Internal helper to establish connection with retry logic
     *
     * @param config TLS configuration
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t connectWithRetries(const TlsConfig &config);

    /**
     * @brief Setup ALPN protocol list if needed
     *
     * @param port Server port number
     * @return Pointer to ALPN protocol array, or nullptr if not needed
     */
    const char **setupAlpnProtocols(uint16_t port);

private:
    // Connection state
    bool connected_; ///< Connection state flag

    // MbedTLS and network contexts (allocated on heap)
    std::unique_ptr<MbedtlsPkcs11Context_t> tlsContext_; ///< MbedTLS context
    std::unique_ptr<NetworkContext_t> networkContext_;   ///< Network context

    // PKCS#11 session (RAII wrapper)
    Pkcs11Session pkcs11Session_; ///< PKCS#11 session wrapper

    // ALPN protocol storage (must persist for connection lifetime)
    const char *alpnProtos_[2]; ///< ALPN protocol list (NULL-terminated)

    // Thread safety mutex
    SemaphoreHandle_t mutex_; ///< Mutex for thread-safe operations
};

} // namespace tls
} // namespace lopcore
