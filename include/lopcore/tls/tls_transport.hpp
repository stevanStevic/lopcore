/**
 * @file tls_transport.hpp
 * @brief Abstract interface for TLS transport implementations
 *
 * Defines the contract for TLS transport layers that can be used with
 * MQTT clients and other network protocols. Uses esp_err_t for error
 * handling (no exceptions).
 */

#pragma once

#include <cstddef>

#include <esp_err.h>

namespace lopcore
{
namespace tls
{

// Forward declare config type
struct TlsConfig;

/**
 * @brief Abstract interface for TLS transport operations
 *
 * This interface provides a protocol-agnostic way to establish secure
 * connections, send/receive data, and manage connection lifecycle.
 *
 * All operations use esp_err_t for error reporting (no exceptions).
 *
 * Implementations must be thread-safe if accessed from multiple tasks.
 */
class ITlsTransport
{
public:
    virtual ~ITlsTransport() = default;

    /**
     * @brief Establish TLS connection to remote server
     *
     * Performs TCP connection, TLS handshake, and certificate verification.
     * May retry with exponential backoff based on configuration.
     *
     * @param[in] config TLS connection configuration (hostname, port, certs, etc.)
     * @return ESP_OK on success
     *         ESP_ERR_INVALID_ARG if config is invalid
     *         ESP_ERR_INVALID_STATE if already connected
     *         ESP_ERR_TIMEOUT if connection times out
     *         ESP_FAIL on other connection errors
     */
    virtual esp_err_t connect(const TlsConfig &config) = 0;

    /**
     * @brief Disconnect from remote server
     *
     * Closes the TLS session and underlying TCP connection.
     * Safe to call even if not connected (no-op).
     */
    virtual void disconnect() noexcept = 0;

    /**
     * @brief Send data over TLS connection
     *
     * Blocks until all data is sent or timeout occurs.
     *
     * @param[in] data Pointer to data buffer to send
     * @param[in] size Number of bytes to send
     * @param[out] bytesSent Number of bytes actually sent (may be less than size on error)
     * @return ESP_OK on success (all data sent)
     *         ESP_ERR_INVALID_ARG if data is null or bytesSent is null
     *         ESP_ERR_INVALID_STATE if not connected
     *         ESP_ERR_TIMEOUT if send times out
     *         ESP_FAIL on other send errors
     */
    virtual esp_err_t send(const void *data, size_t size, size_t *bytesSent) = 0;

    /**
     * @brief Receive data from TLS connection
     *
     * Blocks until data is available or timeout occurs.
     *
     * @param[out] buffer Buffer to store received data
     * @param[in] size Maximum number of bytes to receive
     * @param[out] bytesReceived Number of bytes actually received
     * @return ESP_OK on success
     *         ESP_ERR_INVALID_ARG if buffer is null or bytesReceived is null
     *         ESP_ERR_INVALID_STATE if not connected
     *         ESP_ERR_TIMEOUT if receive times out
     *         ESP_FAIL on other receive errors
     */
    virtual esp_err_t recv(void *buffer, size_t size, size_t *bytesReceived) = 0;

    /**
     * @brief Check if transport is connected
     *
     * @return true if TLS connection is established and active
     */
    virtual bool isConnected() const noexcept = 0;

    /**
     * @brief Get opaque network context pointer
     *
     * Returns a pointer to the underlying network context that can be
     * passed to coreMQTT or other protocol libraries.
     *
     * For MbedTLS implementations, this returns NetworkContext_t*.
     *
     * @return Pointer to network context, or nullptr if not connected
     */
    virtual void *getNetworkContext() noexcept = 0;
};

} // namespace tls
} // namespace lopcore
