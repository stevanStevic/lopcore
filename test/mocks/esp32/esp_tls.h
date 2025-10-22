/**
 * @file esp_tls.h
 * @brief Mock ESP-TLS API for host testing
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct esp_tls;
struct esp_tls_cfg;

/**
 * @brief ESP-TLS context (opaque mock structure)
 */
typedef struct esp_tls
{
    int sockfd;      // Mock socket descriptor
    bool connected;  // Mock connection state
    void *user_data; // User data pointer
} esp_tls_t;

/**
 * @brief ESP-TLS configuration
 */
typedef struct esp_tls_cfg
{
    const unsigned char *cacert_buf;
    size_t cacert_bytes;
    const unsigned char *clientcert_buf;
    size_t clientcert_bytes;
    const unsigned char *clientkey_buf;
    size_t clientkey_bytes;
    const char *alpn_protos;
    bool use_secure_element;
    int timeout_ms;
} esp_tls_cfg_t;

/**
 * @brief Initialize ESP-TLS context (mock)
 */
inline esp_tls_t *esp_tls_init(void)
{
    esp_tls_t *tls = new esp_tls_t();
    tls->sockfd = -1;
    tls->connected = false;
    tls->user_data = nullptr;
    return tls;
}

/**
 * @brief Create new TLS connection (mock - always succeeds)
 */
inline int
esp_tls_conn_new_sync(const char *hostname, int hostlen, int port, const esp_tls_cfg_t *cfg, esp_tls_t *tls)
{
    (void) hostname;
    (void) hostlen;
    (void) port;
    (void) cfg;

    if (tls)
    {
        tls->sockfd = 42; // Mock socket
        tls->connected = true;
        return 1; // Success
    }
    return -1;
}

/**
 * @brief Write data over TLS connection (mock)
 */
inline ssize_t esp_tls_conn_write(esp_tls_t *tls, const void *data, size_t datalen)
{
    (void) tls;
    (void) data;

    // Mock: pretend all data was written
    return datalen;
}

/**
 * @brief Read data from TLS connection (mock)
 */
inline ssize_t esp_tls_conn_read(esp_tls_t *tls, void *data, size_t datalen)
{
    (void) tls;
    (void) data;
    (void) datalen;

    // Mock: return 0 (no data available)
    return 0;
}

/**
 * @brief Destroy TLS connection (mock)
 */
inline int esp_tls_conn_destroy(esp_tls_t *tls)
{
    if (tls)
    {
        tls->connected = false;
        tls->sockfd = -1;
        delete tls;
    }
    return 0;
}

/**
 * @brief Get TLS error (mock)
 */
inline int esp_tls_get_error_handle(esp_tls_t *tls)
{
    (void) tls;
    return 0; // No error
}

#ifdef __cplusplus
}
#endif
