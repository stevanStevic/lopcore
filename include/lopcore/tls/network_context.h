/**
 * @file network_context.h
 * @brief Network context structure for transport layer abstraction
 *
 * This file defines the NetworkContext structure used by transport implementations.
 * The structure uses an opaque pointer to allow different TLS implementations
 * (MbedTLS, BearSSL, WolfSSL, etc.) without creating hard dependencies.
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 */

#ifndef NETWORK_CONTEXT_H
#define NETWORK_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration for C++ class (must use fully qualified name)
#ifdef __cplusplus
namespace lopcore
{
namespace mqtt
{
class CoreMqttClient;
}
} // namespace lopcore
#endif

/**
 * @brief Network context structure for transport layer
 *
 * This structure is used by transport implementations to maintain connection
 * state and provide back-reference to the MQTT client for callbacks.
 *
 * The pParams field is an opaque pointer that can point to any TLS
 * implementation-specific context (e.g., MbedtlsPkcs11Context_t for MbedTLS).
 * This design allows the MQTT layer to remain agnostic of the specific TLS
 * implementation being used.
 */
struct NetworkContext
{
    /**
     * @brief Opaque pointer to TLS implementation-specific context
     *
     * For MbedTLS: Cast to MbedtlsPkcs11Context_t*
     * For BearSSL: Cast to br_ssl_client_context*
     * For WolfSSL: Cast to WOLFSSL*
     * etc.
     */
    void *pParams;

#ifdef __cplusplus
    /**
     * @brief Back-reference to CoreMqttClient for callbacks
     *
     * This pointer allows transport layer callbacks to access the MQTT
     * client instance without using global state or maps.
     */
    lopcore::mqtt::CoreMqttClient *client;
#else
    /**
     * @brief Back-reference to client context (C version)
     *
     * In C code, this is a void pointer that can be cast as needed.
     */
    void *client;
#endif
};

/**
 * @brief Type alias for NetworkContext (CoreMQTT compatibility)
 */
typedef struct NetworkContext NetworkContext_t;

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_CONTEXT_H */
