/**
 * @file transport_interface.h
 * @brief Mock transport interface for coreMQTT testing
 */

#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
// Note: NetworkContext is defined by the application (core mqtt_client.hpp)
struct NetworkContext;

/**
 * @brief Transport send function pointer
 */
typedef int32_t (*TransportSend_t)(struct NetworkContext *pNetworkContext,
                                   const void *pBuffer,
                                   size_t bytesToSend);

/**
 * @brief Transport receive function pointer
 */
typedef int32_t (*TransportRecv_t)(struct NetworkContext *pNetworkContext, void *pBuffer, size_t bytesToRecv);

// NetworkContext_t is just an alias - actual definition comes from application
typedef struct NetworkContext NetworkContext_t;

/** One piece of an outgoing packet (coreMQTT's TransportOutVector_t). */
typedef struct TransportOutVector
{
    const void *iov_base;
    size_t iov_len;
} TransportOutVector_t;

/** Optional vectored send: bytes written, or negative on error (0 = retry). */
typedef int32_t (*TransportWritev_t)(struct NetworkContext *pNetworkContext,
                                     TransportOutVector_t *pIoVec,
                                     size_t ioVecCount);

/**
 * @brief Transport interface
 */
typedef struct TransportInterface
{
    TransportRecv_t recv;
    TransportSend_t send;
    TransportWritev_t writev;
    NetworkContext_t *pNetworkContext;
} TransportInterface_t;

#ifdef __cplusplus
}
#endif
