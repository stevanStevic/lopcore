/**
 * @file core_pkcs11.h
 * @brief Host-side stub for corePKCS11 C API
 *
 * Provides minimal type definitions and stub functions for compiling
 * PKCS#11-dependent code on the host without a real PKCS #11 module.
 *
 * Only functions referenced by certificate_manager.cpp are stubbed.
 * All stubs return CKR_FUNCTION_NOT_SUPPORTED.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

// ----------------------------------------------------------------
// PKCS#11 Basic Types
// ----------------------------------------------------------------

typedef unsigned long CK_ULONG;
typedef unsigned long CK_RV;
typedef unsigned long CK_FLAGS;
typedef unsigned long CK_OBJECT_CLASS;
typedef unsigned long CK_KEY_TYPE;
typedef unsigned long CK_CERTIFICATE_TYPE;
typedef unsigned char CK_BYTE;
typedef unsigned char CK_BBOOL;
typedef void *CK_VOID_PTR;
typedef CK_BYTE *CK_BYTE_PTR;
typedef CK_ULONG *CK_ULONG_PTR;

typedef CK_ULONG CK_SESSION_HANDLE;
typedef CK_ULONG CK_OBJECT_HANDLE;
typedef CK_ULONG CK_SLOT_ID;

typedef CK_OBJECT_HANDLE *CK_OBJECT_HANDLE_PTR;
typedef CK_SESSION_HANDLE *CK_SESSION_HANDLE_PTR;

// ----------------------------------------------------------------
// Constants
// ----------------------------------------------------------------

#define CK_TRUE ((CK_BBOOL) 1)
#define CK_FALSE ((CK_BBOOL) 0)

#define CK_INVALID_HANDLE ((CK_OBJECT_HANDLE) 0)

// Return codes
#define CKR_OK ((CK_RV) 0x00000000UL)
#define CKR_CANCEL ((CK_RV) 0x00000001UL)
#define CKR_HOST_MEMORY ((CK_RV) 0x00000002UL)
#define CKR_ARGUMENTS_BAD ((CK_RV) 0x00000007UL)
#define CKR_FUNCTION_NOT_SUPPORTED ((CK_RV) 0x00000054UL)
#define CKR_FUNCTION_FAILED ((CK_RV) 0x00000006UL)
#define CKR_ATTRIBUTE_VALUE_INVALID ((CK_RV) 0x00000013UL)
#define CKR_CURVE_NOT_SUPPORTED ((CK_RV) 0x00000140UL)
#define CKR_BUFFER_TOO_SMALL ((CK_RV) 0x00000150UL)

// Object classes
#define CKO_DATA ((CK_OBJECT_CLASS) 0x00000000UL)
#define CKO_CERTIFICATE ((CK_OBJECT_CLASS) 0x00000001UL)
#define CKO_PUBLIC_KEY ((CK_OBJECT_CLASS) 0x00000002UL)
#define CKO_PRIVATE_KEY ((CK_OBJECT_CLASS) 0x00000003UL)
#define CKO_SECRET_KEY ((CK_OBJECT_CLASS) 0x00000004UL)

// Key types
#define CKK_RSA ((CK_KEY_TYPE) 0x00000000UL)
#define CKK_DSA ((CK_KEY_TYPE) 0x00000001UL)
#define CKK_EC ((CK_KEY_TYPE) 0x00000003UL)

// Attribute types
#define CKA_CLASS ((CK_ULONG) 0x00000000UL)
#define CKA_TOKEN ((CK_ULONG) 0x00000001UL)
#define CKA_PRIVATE ((CK_ULONG) 0x00000002UL)
#define CKA_LABEL ((CK_ULONG) 0x00000003UL)
#define CKA_VALUE ((CK_ULONG) 0x00000011UL)
#define CKA_CERTIFICATE_TYPE ((CK_ULONG) 0x00000080UL)
#define CKA_SUBJECT ((CK_ULONG) 0x00000101UL)
#define CKA_KEY_TYPE ((CK_ULONG) 0x00000100UL)
#define CKA_SIGN ((CK_ULONG) 0x00000108UL)
#define CKA_VERIFY ((CK_ULONG) 0x0000010AUL)
#define CKA_EC_PARAMS ((CK_ULONG) 0x00000180UL)
#define CKA_EC_POINT ((CK_ULONG) 0x00000181UL)
#define CKA_MODULUS ((CK_ULONG) 0x00000120UL)
#define CKA_PUBLIC_EXPONENT ((CK_ULONG) 0x00000122UL)
#define CKA_PRIVATE_EXPONENT ((CK_ULONG) 0x00000123UL)
#define CKA_PRIME_1 ((CK_ULONG) 0x00000124UL)
#define CKA_PRIME_2 ((CK_ULONG) 0x00000125UL)
#define CKA_EXPONENT_1 ((CK_ULONG) 0x00000126UL)
#define CKA_EXPONENT_2 ((CK_ULONG) 0x00000127UL)
#define CKA_COEFFICIENT ((CK_ULONG) 0x00000128UL)

// Mechanism types
#define CKM_RSA_PKCS ((CK_ULONG) 0x00000001UL)
#define CKM_EC_KEY_PAIR_GEN ((CK_ULONG) 0x00001040UL)
#define CKM_ECDSA ((CK_ULONG) 0x00001041UL)

// Certificate types
#define CKC_X_509 ((CK_CERTIFICATE_TYPE) 0x00000000UL)

// ----------------------------------------------------------------
// Attribute struct
// ----------------------------------------------------------------

typedef struct CK_ATTRIBUTE
{
    CK_ULONG type;
    CK_VOID_PTR pValue;
    CK_ULONG ulValueLen;
} CK_ATTRIBUTE;
typedef CK_ATTRIBUTE *CK_ATTRIBUTE_PTR;

// ----------------------------------------------------------------
// Mechanism struct
// ----------------------------------------------------------------

typedef struct CK_MECHANISM
{
    CK_ULONG mechanism;
    CK_VOID_PTR pParameter;
    CK_ULONG ulParameterLen;
} CK_MECHANISM;
typedef CK_MECHANISM *CK_MECHANISM_PTR;

// ----------------------------------------------------------------
// Function list (minimal subset)
// ----------------------------------------------------------------

typedef CK_RV (*CK_C_CloseSession)(CK_SESSION_HANDLE);
typedef CK_RV (*CK_C_DestroyObject)(CK_SESSION_HANDLE, CK_OBJECT_HANDLE);
typedef CK_RV (*CK_C_CreateObject)(CK_SESSION_HANDLE, CK_ATTRIBUTE_PTR, CK_ULONG, CK_OBJECT_HANDLE_PTR);
typedef CK_RV (*CK_C_GetAttributeValue)(CK_SESSION_HANDLE, CK_OBJECT_HANDLE, CK_ATTRIBUTE_PTR, CK_ULONG);
typedef CK_RV (*CK_C_GenerateKeyPair)(CK_SESSION_HANDLE,
                                      CK_MECHANISM_PTR,
                                      CK_ATTRIBUTE_PTR,
                                      CK_ULONG,
                                      CK_ATTRIBUTE_PTR,
                                      CK_ULONG,
                                      CK_OBJECT_HANDLE_PTR,
                                      CK_OBJECT_HANDLE_PTR);
typedef CK_RV (*CK_C_Sign)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR);
typedef CK_RV (*CK_C_SignInit)(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE);
typedef CK_RV (*CK_C_GenerateRandom)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG);
typedef CK_RV (*CK_C_Finalize)(CK_VOID_PTR);

typedef struct CK_FUNCTION_LIST
{
    CK_C_CloseSession C_CloseSession;
    CK_C_DestroyObject C_DestroyObject;
    CK_C_CreateObject C_CreateObject;
    CK_C_GetAttributeValue C_GetAttributeValue;
    CK_C_GenerateKeyPair C_GenerateKeyPair;
    CK_C_Sign C_Sign;
    CK_C_SignInit C_SignInit;
    CK_C_GenerateRandom C_GenerateRandom;
    CK_C_Finalize C_Finalize;
} CK_FUNCTION_LIST;
typedef CK_FUNCTION_LIST *CK_FUNCTION_LIST_PTR;
typedef CK_FUNCTION_LIST_PTR *CK_FUNCTION_LIST_PTR_PTR;

// ----------------------------------------------------------------
// API stubs
// ----------------------------------------------------------------

static inline CK_RV C_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR ppFunctionList)
{
    if (ppFunctionList)
        *ppFunctionList = NULL;
    return CKR_FUNCTION_NOT_SUPPORTED;
}

// corePKCS11 helper stubs
static inline CK_RV xInitializePkcs11Session(CK_SESSION_HANDLE *pxSession)
{
    if (pxSession)
        *pxSession = CK_INVALID_HANDLE;
    return CKR_FUNCTION_NOT_SUPPORTED;
}

static inline CK_RV xFindObjectWithLabelAndClass(CK_SESSION_HANDLE session,
                                                 const char *label,
                                                 size_t labelLen,
                                                 CK_OBJECT_CLASS objectClass,
                                                 CK_OBJECT_HANDLE *pHandle)
{
    (void) session;
    (void) label;
    (void) labelLen;
    (void) objectClass;
    if (pHandle)
        *pHandle = CK_INVALID_HANDLE;
    return CKR_OK; // object not found (handle stays CK_INVALID_HANDLE)
}

// PKI utility stubs
#define pkcs11ECDSA_P256_SIGNATURE_LENGTH 64

static inline void PKI_pkcs11SignatureTombedTLSSignature(unsigned char *pRawSig, size_t *pSigLen)
{
    (void) pRawSig;
    (void) pSigLen;
}

// DER-encoded OID for P-256 (prime256v1): 30 13 06 07 2a 86 ...
#define pkcs11DER_ENCODED_OID_P256                                                                           \
    {0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,                                       \
     0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07}

// PKCS#11 label constants (matching corePKCS11_config.h)
#ifndef pkcs11configLABEL_CLAIM_CERTIFICATE
#define pkcs11configLABEL_CLAIM_CERTIFICATE "claim_cert"
#endif
#ifndef pkcs11configLABEL_CLAIM_PRIVATE_KEY
#define pkcs11configLABEL_CLAIM_PRIVATE_KEY "claim_key"
#endif
#ifndef pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS
#define pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS "device_cert"
#endif
#ifndef pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS
#define pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS "device_priv"
#endif
#ifndef pkcs11configLABEL_DEVICE_PUBLIC_KEY_FOR_TLS
#define pkcs11configLABEL_DEVICE_PUBLIC_KEY_FOR_TLS "device_pub"
#endif

#ifdef __cplusplus
}
#endif
