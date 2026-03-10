/**
 * @file core_pki_utils.h
 * @brief Host-side stub for corePKCS11 PKI utility functions
 *
 * Stubs for `convert_pem_to_der` and `provisionPrivateKey`.
 * Used only when compiling certificate_manager.cpp on the host
 * with CONFIG_LOPCORE_PROV_CERT_COREP11 enabled.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "core_pkcs11.h"

/**
 * @brief Stub for convert_pem_to_der.
 *
 * On host, we cannot perform real PEM→DER conversion here.
 * Returns -1 (failure) so PKCS#11 store path is exercised only in device builds.
 */
static inline int
convert_pem_to_der(const unsigned char *pucInput, size_t xLen, unsigned char *pucOutput, size_t *pxOlen)
{
    (void) pucInput;
    (void) xLen;
    (void) pucOutput;
    if (pxOlen)
        *pxOlen = 0;
    return -1;
}

/**
 * @brief Stub for provisionPrivateKey (corePKCS11 provisioning helper).
 *
 * Returns CKR_FUNCTION_NOT_SUPPORTED on host.
 */
static inline CK_RV provisionPrivateKey(CK_SESSION_HANDLE session,
                                        const uint8_t *pKeyData,
                                        size_t keyDataLen,
                                        const uint8_t *pLabel,
                                        CK_OBJECT_HANDLE *pHandle)
{
    (void) session;
    (void) pKeyData;
    (void) keyDataLen;
    (void) pLabel;
    if (pHandle)
        *pHandle = CK_INVALID_HANDLE;
    return CKR_FUNCTION_NOT_SUPPORTED;
}

#ifdef __cplusplus
}
#endif
