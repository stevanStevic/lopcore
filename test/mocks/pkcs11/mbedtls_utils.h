/**
 * @file mbedtls_utils.h
 * @brief Host-side stub for mbedtls_utils (corePKCS11 dependency)
 *
 * Stubs the `mbedtls_utils.h` utilities used by corePKCS11 PKCS#11 operations
 * when building certificate_manager.cpp on the host.
 *
 * In device builds this is provided by aws-iot-device-sdk-embedded-C component.
 */

#pragma once

#include <mbedtls/pk.h>
#include <mbedtls/x509_csr.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert a PEM private-key blob to an mbedtls_pk_context.
 * Stub — returns -1 on host.
 */
static inline int convert_pem_to_der_pk(const char *pem, size_t pemLen, unsigned char *der, size_t *derLen)
{
    (void) pem;
    (void) pemLen;
    (void) der;
    if (derLen)
        *derLen = 0;
    return -1;
}

#ifdef __cplusplus
}
#endif
