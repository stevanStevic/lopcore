#include "lopcore/prov/certificate_manager.hpp"

#include <cstring>
#include <vector>

#include "lopcore/logging/logger.hpp"

// mbedTLS — MBEDTLS_ALLOW_PRIVATE_ACCESS must be defined before any mbedTLS struct headers
#include <mbedtls/version.h>
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
#define MBEDTLS_ALLOW_PRIVATE_ACCESS
#endif
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/oid.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_csr.h>

#if CONFIG_LOPCORE_PROV_CERT_COREP11
// corePKCS11
#include <core_pkcs11.h>
#include <core_pkcs11_config.h>
#include <core_pki_utils.h>
#include <mbedtls_utils.h>
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
#include <pk_wrap.h>
#else
#include <mbedtls/pk_internal.h>
#endif
#endif // CONFIG_LOPCORE_PROV_CERT_COREP11

static const char *TAG_CM = "CertManager";

// PKCS#11 object class aliases (avoid pulling in pkcs11.h in the header)
#define LOPCORE_CKO_CERTIFICATE (0x00000001UL)
#define LOPCORE_CKO_PRIVATE_KEY (0x00000003UL)
#define LOPCORE_CKO_PUBLIC_KEY (0x00000002UL)

namespace lopcore
{
namespace prov
{

// ---------- PIMPL ----------

struct CertificateManager::Impl
{
    // Claim certificate storage (memory, non-PKCS#11 path)
    std::string claimCertPem;
    std::string claimKeyPem;
    bool hasClaimCert = false;

    // mbedTLS contexts for CSR generation (software path)
    mbedtls_pk_context deviceKey;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctrDrbg;
    bool mbedtlsInitialized = false;

#if CONFIG_LOPCORE_PROV_CERT_COREP11
    CK_SESSION_HANDLE p11Session = CK_INVALID_HANDLE;
    bool p11SessionOpen = false;
#endif

    Impl()
    {
        mbedtls_pk_init(&deviceKey);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctrDrbg);
    }

    ~Impl()
    {
#if CONFIG_LOPCORE_PROV_CERT_COREP11
        if (p11SessionOpen)
        {
            CK_FUNCTION_LIST_PTR fnList = nullptr;
            if (C_GetFunctionList(&fnList) == CKR_OK && fnList)
            {
                fnList->C_CloseSession(p11Session);
            }
            p11SessionOpen = false;
        }
#endif
        mbedtls_pk_free(&deviceKey);
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&ctrDrbg);
    }

    bool initMbedTls()
    {
        if (mbedtlsInitialized)
        {
            return true;
        }
        const char *pers = "lopcore_cert_manager";
        int ret = mbedtls_ctr_drbg_seed(&ctrDrbg, mbedtls_entropy_func, &entropy,
                                        reinterpret_cast<const unsigned char *>(pers), strlen(pers));
        if (ret != 0)
        {
            LOPCORE_LOGE(TAG_CM, "Failed to seed RNG: -0x%04x", -ret);
            return false;
        }
        mbedtlsInitialized = true;
        return true;
    }
};

// ================================================================
// Constructor / Destructor
// ================================================================

CertificateManager::CertificateManager(const Config &config)
    : config_(config), pImpl_(std::make_unique<Impl>())
{
    LOPCORE_LOGI(TAG_CM, "Initializing Certificate Manager");
    LOPCORE_LOGI(TAG_CM, "  usePkcs11 : %s", config_.usePkcs11 ? "true" : "false");

#if CONFIG_LOPCORE_PROV_CERT_COREP11
    if (config_.usePkcs11)
    {
        CK_RV rv = xInitializePkcs11Session(&pImpl_->p11Session);
        if (rv == CKR_OK)
        {
            pImpl_->p11SessionOpen = true;
            LOPCORE_LOGI(TAG_CM, "PKCS#11 session opened");
        }
        else
        {
            LOPCORE_LOGE(TAG_CM, "xInitializePkcs11Session failed: 0x%lx", rv);
        }
    }
#else
    if (config_.usePkcs11)
    {
        LOPCORE_LOGW(TAG_CM, "CONFIG_LOPCORE_PROV_CERT_COREP11 not set — usePkcs11 ignored");
    }
#endif
}

CertificateManager::~CertificateManager()
{
    LOPCORE_LOGI(TAG_CM, "Shutting down Certificate Manager");
}

// ================================================================
// Claim Certificate
// ================================================================

bool CertificateManager::importClaimCertificate(const std::vector<uint8_t> &certData,
                                                const std::vector<uint8_t> &keyData)
{
    std::string certPem(certData.begin(), certData.end());
    std::string keyPem(keyData.begin(), keyData.end());
    return importClaimCertificate(certPem, keyPem);
}

bool CertificateManager::importClaimCertificate(const std::string &certPem, const std::string &keyPem)
{
    LOPCORE_LOGI(TAG_CM, "Importing claim certificate (%zu bytes) and key (%zu bytes)", certPem.size(),
                 keyPem.size());

    // Always keep in memory so getClaimCertPem/getClaimKeyPem work
    pImpl_->claimCertPem = certPem;
    pImpl_->claimKeyPem = keyPem;
    pImpl_->hasClaimCert = true;

#if CONFIG_LOPCORE_PROV_CERT_COREP11
    if (config_.usePkcs11 && pImpl_->p11SessionOpen)
    {
        // Store cert
        std::vector<uint8_t> certBytes(certPem.begin(), certPem.end());
        certBytes.push_back(0);
        bool certOk = pkcs11StoreObject(pkcs11configLABEL_CLAIM_CERTIFICATE, certBytes,
                                        LOPCORE_CKO_CERTIFICATE);

        // Store key — best-effort only. AWS CreateProvisioningClaim returns RSA keys which
        // the PKCS#11 backend does not support. The in-memory PEM (set above) is sufficient
        // for the TLS connection during Fleet Provisioning.
        std::vector<uint8_t> keyBytes(keyPem.begin(), keyPem.end());
        keyBytes.push_back(0); // null-terminate for MbedTLS
        bool keyOk = pkcs11StoreObject(pkcs11configLABEL_CLAIM_PRIVATE_KEY, keyBytes,
                                       LOPCORE_CKO_PRIVATE_KEY);

        if (!certOk)
        {
            LOPCORE_LOGE(TAG_CM, "Failed to store claim certificate in PKCS#11");
            return false;
        }
        if (!keyOk)
        {
            LOPCORE_LOGW(TAG_CM, "Claim key not stored in PKCS#11 (RSA keys unsupported) — "
                                 "using in-memory PEM for provisioning TLS");
        }
        else
        {
            LOPCORE_LOGI(TAG_CM, "Claim credentials stored in PKCS#11");
        }
    }
#endif

    return true;
}

bool CertificateManager::hasClaimCertificate() const
{
    // In-memory copy takes priority — claim keys may be RSA (not stored in PKCS#11)
    if (pImpl_->hasClaimCert)
    {
        return true;
    }
#if CONFIG_LOPCORE_PROV_CERT_COREP11
    if (config_.usePkcs11 && pImpl_->p11SessionOpen)
    {
        return pkcs11ObjectExists(pkcs11configLABEL_CLAIM_CERTIFICATE, LOPCORE_CKO_CERTIFICATE) &&
               pkcs11ObjectExists(pkcs11configLABEL_CLAIM_PRIVATE_KEY, LOPCORE_CKO_PRIVATE_KEY);
    }
#endif
    return false;
}

std::optional<std::string> CertificateManager::getClaimCertPem() const
{
    if (pImpl_->hasClaimCert && !pImpl_->claimCertPem.empty())
    {
        return pImpl_->claimCertPem;
    }
    return std::nullopt;
}

std::optional<std::string> CertificateManager::getClaimKeyPem() const
{
    if (pImpl_->hasClaimCert && !pImpl_->claimKeyPem.empty())
    {
        return pImpl_->claimKeyPem;
    }
    return std::nullopt;
}

bool CertificateManager::deleteClaimCredentials()
{
    LOPCORE_LOGI(TAG_CM, "Deleting claim credentials");

    pImpl_->claimCertPem.clear();
    pImpl_->claimKeyPem.clear();
    pImpl_->hasClaimCert = false;

#if CONFIG_LOPCORE_PROV_CERT_COREP11
    if (config_.usePkcs11 && pImpl_->p11SessionOpen)
    {
        bool certOk = pkcs11DeleteObject(pkcs11configLABEL_CLAIM_CERTIFICATE, LOPCORE_CKO_CERTIFICATE);
        bool keyOk = pkcs11DeleteObject(pkcs11configLABEL_CLAIM_PRIVATE_KEY, LOPCORE_CKO_PRIVATE_KEY);
        if (!certOk || !keyOk)
        {
            LOPCORE_LOGW(TAG_CM, "One or more claim objects could not be deleted from PKCS#11");
        }
    }
#endif

    return true;
}

// ================================================================
// Device Key Pair and CSR
// ================================================================

bool CertificateManager::generateCsr(const std::string &commonName, std::string &csrOut)
{
    LOPCORE_LOGI(TAG_CM, "generateCsr (mbedTLS SW path) CN='%s'", commonName.c_str());

    if (!pImpl_->initMbedTls())
    {
        return false;
    }

    mbedtls_pk_free(&pImpl_->deviceKey);
    mbedtls_pk_init(&pImpl_->deviceKey);

    int ret = mbedtls_pk_setup(&pImpl_->deviceKey, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0)
    {
        LOPCORE_LOGE(TAG_CM, "mbedtls_pk_setup failed: -0x%04x", -ret);
        return false;
    }

    ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(pImpl_->deviceKey),
                              mbedtls_ctr_drbg_random, &pImpl_->ctrDrbg);
    if (ret != 0)
    {
        LOPCORE_LOGE(TAG_CM, "mbedtls_ecp_gen_key failed: -0x%04x", -ret);
        return false;
    }

    mbedtls_x509write_csr csrCtx;
    mbedtls_x509write_csr_init(&csrCtx);
    mbedtls_x509write_csr_set_md_alg(&csrCtx, MBEDTLS_MD_SHA256);
    mbedtls_x509write_csr_set_key(&csrCtx, &pImpl_->deviceKey);

    // Accept either bare CN value or full "CN=..." subject string
    std::string subject = (commonName.find('=') != std::string::npos) ? commonName : ("CN=" + commonName);
    ret = mbedtls_x509write_csr_set_subject_name(&csrCtx, subject.c_str());
    if (ret != 0)
    {
        LOPCORE_LOGE(TAG_CM, "mbedtls_x509write_csr_set_subject_name failed: -0x%04x", -ret);
        mbedtls_x509write_csr_free(&csrCtx);
        return false;
    }

    std::vector<uint8_t> buf(config_.csrBufferSize, 0);
    ret = mbedtls_x509write_csr_pem(&csrCtx, buf.data(), buf.size(), mbedtls_ctr_drbg_random,
                                    &pImpl_->ctrDrbg);
    mbedtls_x509write_csr_free(&csrCtx);

    if (ret != 0)
    {
        LOPCORE_LOGE(TAG_CM, "mbedtls_x509write_csr_pem failed: -0x%04x", -ret);
        return false;
    }

    csrOut = std::string(reinterpret_cast<char *>(buf.data()));
    LOPCORE_LOGI(TAG_CM, "CSR generated (SW) — %zu bytes", csrOut.size());
    return true;
}

bool CertificateManager::generateKeyPairAndCsr(const std::string &commonName, std::string &csrOut)
{
#if CONFIG_LOPCORE_PROV_CERT_COREP11
    if (config_.usePkcs11 && pImpl_->p11SessionOpen)
    {
        LOPCORE_LOGI(TAG_CM, "generateKeyPairAndCsr (PKCS#11 path) CN='%s'", commonName.c_str());
        return generateKeyPairAndCsrPkcs11_(commonName, csrOut);
    }
#endif
    // Fallback: software key pair
    return generateCsr(commonName, csrOut);
}

// ================================================================
// Final Certificate Storage
// ================================================================

bool CertificateManager::storeFinalCertificate(const std::string &label, const std::vector<uint8_t> &certData)
{
    std::string certPem(certData.begin(), certData.end());
    return storeFinalCertificate(label, certPem);
}

bool CertificateManager::storeFinalCertificate(const std::string &label, const std::string &certPem)
{
    LOPCORE_LOGI(TAG_CM, "Storing final certificate — label='%s'", label.c_str());

#if CONFIG_LOPCORE_PROV_CERT_COREP11
    if (config_.usePkcs11 && pImpl_->p11SessionOpen)
    {
        std::vector<uint8_t> certBytes(certPem.begin(), certPem.end());
        certBytes.push_back(0); // null-terminate for MbedTLS PEM
        return pkcs11StoreObject(label, certBytes, LOPCORE_CKO_CERTIFICATE);
    }
#endif

    LOPCORE_LOGW(TAG_CM, "storeFinalCertificate: no persistent storage backend (usePkcs11=false) — "
                         "certificate retained in memory only");
    return true; // non-fatal for dev mode
}

bool CertificateManager::storeFinalPrivateKey(const std::string &label, const std::vector<uint8_t> &keyData)
{
    std::string keyPem(keyData.begin(), keyData.end());
    return storeFinalPrivateKey(label, keyPem);
}

bool CertificateManager::storeFinalPrivateKey(const std::string &label, const std::string &keyPem)
{
    LOPCORE_LOGI(TAG_CM, "Storing final private key — label='%s'", label.c_str());

#if CONFIG_LOPCORE_PROV_CERT_COREP11
    if (config_.usePkcs11 && pImpl_->p11SessionOpen)
    {
        std::vector<uint8_t> keyBytes(keyPem.begin(), keyPem.end());
        keyBytes.push_back(0);
        return pkcs11StoreObject(label, keyBytes, LOPCORE_CKO_PRIVATE_KEY);
    }
#endif

    LOPCORE_LOGW(TAG_CM, "storeFinalPrivateKey: no persistent storage backend — key in memory only");
    return true; // non-fatal for dev mode
}

bool CertificateManager::hasFinalCertificate(const std::string &label) const
{
#if CONFIG_LOPCORE_PROV_CERT_COREP11
    if (config_.usePkcs11 && pImpl_->p11SessionOpen)
    {
        return pkcs11ObjectExists(label, LOPCORE_CKO_CERTIFICATE);
    }
#endif
    return false;
}

// ================================================================
// Factory Reset
// ================================================================

bool CertificateManager::deleteDeviceCredentials()
{
    LOPCORE_LOGI(TAG_CM, "Deleting device credentials (factory reset)");

#if CONFIG_LOPCORE_PROV_CERT_COREP11
    if (config_.usePkcs11 && pImpl_->p11SessionOpen)
    {
        bool certOk = pkcs11DeleteObject(pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS,
                                         LOPCORE_CKO_CERTIFICATE);
        bool privOk = pkcs11DeleteObject(pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS,
                                         LOPCORE_CKO_PRIVATE_KEY);
        bool pubOk = pkcs11DeleteObject(pkcs11configLABEL_DEVICE_PUBLIC_KEY_FOR_TLS, LOPCORE_CKO_PUBLIC_KEY);
        (void) pubOk; // public key deletion is best-effort
        if (!certOk || !privOk)
        {
            LOPCORE_LOGW(TAG_CM, "Some device PKCS#11 objects could not be deleted");
        }
        return certOk && privOk;
    }
#endif

    LOPCORE_LOGW(TAG_CM, "deleteDeviceCredentials: no PKCS#11 backend — nothing to delete");
    return true;
}

// ================================================================
// PEM → DER helper
// ================================================================

int CertificateManager::pemToDer(const std::string &pem, std::vector<uint8_t> &der) const
{
    // Allocate a buffer the same size as the PEM (DER is always smaller)
    der.resize(pem.size(), 0);
    size_t outLen = der.size();

    int ret = convert_pem_to_der(reinterpret_cast<const unsigned char *>(pem.c_str()), pem.size() + 1,
                                 der.data(), &outLen);

    if (ret != 0)
    {
        LOPCORE_LOGE(TAG_CM, "pemToDer failed: %d", ret);
        der.clear();
        return ret;
    }

    der.resize(outLen);
    return 0;
}

// ================================================================
// PKCS#11 primitives
// ================================================================

bool CertificateManager::pkcs11ObjectExists(const std::string &label, uint32_t objectClass) const
{
#if CONFIG_LOPCORE_PROV_CERT_COREP11
    if (!pImpl_->p11SessionOpen)
    {
        return false;
    }
    CK_OBJECT_HANDLE handle = CK_INVALID_HANDLE;
    CK_OBJECT_CLASS ckClass = static_cast<CK_OBJECT_CLASS>(objectClass);
    CK_RV rv = xFindObjectWithLabelAndClass(pImpl_->p11Session, const_cast<char *>(label.c_str()),
                                            label.size(), ckClass, &handle);
    return (rv == CKR_OK) && (handle != CK_INVALID_HANDLE);
#else
    (void) label;
    (void) objectClass;
    return false;
#endif
}

bool CertificateManager::pkcs11DeleteObject(const std::string &label, uint32_t objectClass)
{
#if CONFIG_LOPCORE_PROV_CERT_COREP11
    if (!pImpl_->p11SessionOpen)
    {
        return false;
    }

    CK_FUNCTION_LIST_PTR fnList = nullptr;
    CK_RV rv = C_GetFunctionList(&fnList);
    if (rv != CKR_OK || fnList == nullptr)
    {
        LOPCORE_LOGE(TAG_CM, "pkcs11DeleteObject: C_GetFunctionList failed: 0x%lx", rv);
        return false;
    }

    CK_OBJECT_CLASS ckClass = static_cast<CK_OBJECT_CLASS>(objectClass);
    CK_OBJECT_HANDLE handle = CK_INVALID_HANDLE;

    rv = xFindObjectWithLabelAndClass(pImpl_->p11Session, const_cast<char *>(label.c_str()), label.size(),
                                      ckClass, &handle);
    bool anyDeleted = false;
    while (rv == CKR_OK && handle != CK_INVALID_HANDLE)
    {
        rv = fnList->C_DestroyObject(pImpl_->p11Session, handle);
        if (rv != CKR_OK)
        {
            break;
        }
        anyDeleted = true;
        // Find the next object with same label/class
        rv = xFindObjectWithLabelAndClass(pImpl_->p11Session, const_cast<char *>(label.c_str()), label.size(),
                                          ckClass, &handle);
    }

    (void) anyDeleted;
    return (rv == CKR_OK);
#else
    (void) label;
    (void) objectClass;
    LOPCORE_LOGW(TAG_CM, "pkcs11DeleteObject: CONFIG_LOPCORE_PROV_CERT_COREP11 not set");
    return false;
#endif
}

bool CertificateManager::pkcs11GetObject(const std::string &label,
                                         std::vector<uint8_t> &data,
                                         uint32_t objectClass) const
{
#if CONFIG_LOPCORE_PROV_CERT_COREP11
    if (!pImpl_->p11SessionOpen)
    {
        return false;
    }

    CK_FUNCTION_LIST_PTR fnList = nullptr;
    CK_RV rv = C_GetFunctionList(&fnList);
    if (rv != CKR_OK || fnList == nullptr)
    {
        return false;
    }

    CK_OBJECT_CLASS ckClass = static_cast<CK_OBJECT_CLASS>(objectClass);
    CK_OBJECT_HANDLE handle = CK_INVALID_HANDLE;
    rv = xFindObjectWithLabelAndClass(pImpl_->p11Session, const_cast<char *>(label.c_str()), label.size(),
                                      ckClass, &handle);
    if (rv != CKR_OK || handle == CK_INVALID_HANDLE)
    {
        return false;
    }

    // First call: get required buffer size
    CK_ATTRIBUTE valueAttr = {CKA_VALUE, nullptr, 0};
    rv = fnList->C_GetAttributeValue(pImpl_->p11Session, handle, &valueAttr, 1);
    if (rv != CKR_OK || valueAttr.ulValueLen == 0)
    {
        return false;
    }

    data.resize(valueAttr.ulValueLen);
    valueAttr.pValue = data.data();

    rv = fnList->C_GetAttributeValue(pImpl_->p11Session, handle, &valueAttr, 1);
    if (rv != CKR_OK)
    {
        data.clear();
        return false;
    }

    data.resize(valueAttr.ulValueLen);
    return true;
#else
    (void) label;
    (void) data;
    (void) objectClass;
    LOPCORE_LOGW(TAG_CM, "pkcs11GetObject: CONFIG_LOPCORE_PROV_CERT_COREP11 not set");
    return false;
#endif
}

#if CONFIG_LOPCORE_PROV_CERT_COREP11
/**
 * Import a PEM-encoded EC P-256 private key into PKCS#11 persistent storage.
 * Equivalent to the static provisionPrivateKey() in the legacy pkcs11_operations.c,
 * restricted to EC P-256 which is the only key type generated by CertificateManager.
 */
static CK_RV pkcs11ImportPrivateKey(CK_SESSION_HANDLE session,
                                    const char *keyPem,
                                    size_t keyPemLen,
                                    const char *label,
                                    mbedtls_ctr_drbg_context *ctrDrbg)
{
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
    int ret = mbedtls_pk_parse_key(&pk, reinterpret_cast<const unsigned char *>(keyPem), keyPemLen, nullptr,
                                   0, mbedtls_ctr_drbg_random, ctrDrbg);
#else
    (void) ctrDrbg;
    int ret = mbedtls_pk_parse_key(&pk, reinterpret_cast<const unsigned char *>(keyPem), keyPemLen, nullptr,
                                   0);
#endif
    if (ret != 0)
    {
        LOPCORE_LOGE(TAG_CM, "pkcs11ImportPrivateKey: PEM parse failed: -0x%04x", -ret);
        mbedtls_pk_free(&pk);
        return CKR_ARGUMENTS_BAD;
    }

    mbedtls_pk_type_t pkType = mbedtls_pk_get_type(&pk);
    if (pkType != MBEDTLS_PK_ECDSA && pkType != MBEDTLS_PK_ECKEY && pkType != MBEDTLS_PK_ECKEY_DH)
    {
        LOPCORE_LOGE(TAG_CM, "pkcs11ImportPrivateKey: unsupported key type %d (only EC P-256)", pkType);
        mbedtls_pk_free(&pk);
        return CKR_ARGUMENTS_BAD;
    }

    mbedtls_ecp_keypair *keyPair = reinterpret_cast<mbedtls_ecp_keypair *>(pk.pk_ctx);
    if (keyPair->grp.id != MBEDTLS_ECP_DP_SECP256R1)
    {
        LOPCORE_LOGE(TAG_CM, "pkcs11ImportPrivateKey: only P-256 curve is supported");
        mbedtls_pk_free(&pk);
        return CKR_CURVE_NOT_SUPPORTED;
    }

    // DER encoding of secp256r1 OID: 0x06 0x08 <8-byte OID>
    static const uint8_t kEcParamsP256[] = "\x06\x08" MBEDTLS_OID_EC_GRP_SECP256R1;
    static constexpr size_t kEcParamsLen = 10; // tag(1) + length(1) + 8-byte OID
    static constexpr size_t kDLen = 32;        // P-256 private scalar size

    CK_BYTE dBuf[kDLen] = {};
    int mret = mbedtls_mpi_write_binary(&keyPair->d, dBuf, kDLen);
    mbedtls_pk_free(&pk);
    if (mret != 0)
    {
        LOPCORE_LOGE(TAG_CM, "pkcs11ImportPrivateKey: failed to extract D: -0x%04x", -mret);
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    CK_FUNCTION_LIST_PTR fnList = nullptr;
    CK_RV rv = C_GetFunctionList(&fnList);
    if (rv != CKR_OK || fnList == nullptr)
    {
        return rv;
    }

    CK_BBOOL trueVal = CK_TRUE;
    CK_KEY_TYPE keyTypeVal = CKK_EC;
    CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
    CK_OBJECT_HANDLE objHandle = CK_INVALID_HANDLE;

    CK_ATTRIBUTE tmpl[] = {
        {CKA_CLASS, &keyClass, sizeof(keyClass)},
        {CKA_KEY_TYPE, &keyTypeVal, sizeof(keyTypeVal)},
        {CKA_LABEL, const_cast<char *>(label), strlen(label)},
        {CKA_TOKEN, &trueVal, sizeof(trueVal)},
        {CKA_SIGN, &trueVal, sizeof(trueVal)},
        {CKA_EC_PARAMS, const_cast<uint8_t *>(kEcParamsP256), kEcParamsLen},
        {CKA_VALUE, dBuf, kDLen},
    };

    rv = fnList->C_CreateObject(session, tmpl, sizeof(tmpl) / sizeof(CK_ATTRIBUTE), &objHandle);
    if (rv != CKR_OK)
    {
        LOPCORE_LOGE(TAG_CM, "pkcs11ImportPrivateKey: C_CreateObject failed: 0x%lx", rv);
    }
    return rv;
}
#endif // CONFIG_LOPCORE_PROV_CERT_COREP11

bool CertificateManager::pkcs11StoreObject(const std::string &label,
                                           const std::vector<uint8_t> &data,
                                           uint32_t objectClass)
{
#if CONFIG_LOPCORE_PROV_CERT_COREP11
    if (!pImpl_->p11SessionOpen)
    {
        LOPCORE_LOGE(TAG_CM, "pkcs11StoreObject: session not open");
        return false;
    }

    CK_FUNCTION_LIST_PTR fnList = nullptr;
    CK_RV rv = C_GetFunctionList(&fnList);
    if (rv != CKR_OK || fnList == nullptr)
    {
        LOPCORE_LOGE(TAG_CM, "pkcs11StoreObject: C_GetFunctionList failed: 0x%lx", rv);
        return false;
    }

    CK_OBJECT_CLASS ckClass = static_cast<CK_OBJECT_CLASS>(objectClass);
    CK_OBJECT_HANDLE objHandle = CK_INVALID_HANDLE;

    if (ckClass == CKO_CERTIFICATE)
    {
        // Convert PEM → DER before creating the PKCS#11 object
        std::string pemStr(reinterpret_cast<const char *>(data.data()), data.size());
        std::vector<uint8_t> der;
        if (pemToDer(pemStr, der) != 0)
        {
            LOPCORE_LOGE(TAG_CM, "pkcs11StoreObject: PEM→DER conversion failed for '%s'", label.c_str());
            return false;
        }

        CK_CERTIFICATE_TYPE certType = CKC_X_509;
        CK_BBOOL tokenStorage = CK_TRUE;
        CK_BYTE subject[] = "lopcore";

        CK_ATTRIBUTE tmpl[] = {{CKA_CLASS, &ckClass, sizeof(ckClass)},
                               {CKA_CERTIFICATE_TYPE, &certType, sizeof(certType)},
                               {CKA_TOKEN, &tokenStorage, sizeof(tokenStorage)},
                               {CKA_LABEL, const_cast<char *>(label.c_str()), label.size()},
                               {CKA_SUBJECT, subject, sizeof(subject)},
                               {CKA_VALUE, der.data(), der.size()}};

        // Remove old object first (best-effort)
        pkcs11DeleteObject(label, objectClass);

        rv = fnList->C_CreateObject(pImpl_->p11Session, tmpl, sizeof(tmpl) / sizeof(CK_ATTRIBUTE),
                                    &objHandle);
    }
    else
    {
        // Private key — EC P-256 PEM import into PKCS#11.
        // Seed the RNG if not already done (required by mbedtls_pk_parse_key on mbedTLS 3.x).
        if (!pImpl_->initMbedTls())
        {
            LOPCORE_LOGE(TAG_CM, "pkcs11StoreObject: failed to init mbedTLS RNG for key import");
            return false;
        }
        // Remove old key first so re-provisioning doesn't leave stale objects.
        pkcs11DeleteObject(label, objectClass);
        CK_RV pkRv = pkcs11ImportPrivateKey(pImpl_->p11Session, reinterpret_cast<const char *>(data.data()),
                                            data.size(), label.c_str(), &pImpl_->ctrDrbg);
        rv = pkRv;
    }

    if (rv != CKR_OK)
    {
        LOPCORE_LOGE(TAG_CM, "pkcs11StoreObject failed for '%s': 0x%lx", label.c_str(), rv);
        return false;
    }

    LOPCORE_LOGI(TAG_CM, "PKCS#11 object stored — label='%s' class=0x%lx", label.c_str(), objectClass);
    return true;
#else
    (void) label;
    (void) data;
    (void) objectClass;
    LOPCORE_LOGW(TAG_CM, "pkcs11StoreObject: CONFIG_LOPCORE_PROV_CERT_COREP11 not set");
    return false;
#endif
}

// ================================================================
// PKCS#11 key-pair + CSR generation (private helper)
// ================================================================

#if CONFIG_LOPCORE_PROV_CERT_COREP11

namespace
{

/** Signing callback context (mirrors GrowBorg pkcs11_operations.c) */
struct P11SignContext
{
    CK_SESSION_HANDLE session;
    CK_OBJECT_HANDLE privKey;
};

static P11SignContext g_signCtx = {};

/** Extract EC public key from PKCS#11 into an mbedTLS ecdsa context */
static int extractEcPublicKey(CK_SESSION_HANDLE session,
                              mbedtls_ecdsa_context *ecCtx,
                              CK_OBJECT_HANDLE pubKey)
{
    CK_FUNCTION_LIST_PTR fnList = nullptr;
    CK_RV rv = C_GetFunctionList(&fnList);
    if (rv != CKR_OK || fnList == nullptr)
    {
        return -1;
    }

    CK_BYTE ecPoint[67] = {0};
    CK_ATTRIBUTE attr = {CKA_EC_POINT, ecPoint, sizeof(ecPoint)};
    rv = fnList->C_GetAttributeValue(session, pubKey, &attr, 1);
    if (rv != CKR_OK)
    {
        return -1;
    }

    mbedtls_ecdsa_init(ecCtx);
    int ret = mbedtls_ecp_group_load(&ecCtx->grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0)
    {
        return ret;
    }

    // EC_POINT is DER-encoded OCTET STRING — skip 2-byte prefix
    ret = mbedtls_ecp_point_read_binary(&ecCtx->grp, &ecCtx->Q, &ecPoint[2], attr.ulValueLen - 2);
    return ret;
}

/** mbedTLS signing callback that uses PKCS#11 C_Sign */
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
static int p11SignCallback(mbedtls_pk_context * /*ctx*/,
                           mbedtls_md_type_t /*mdAlg*/,
                           const unsigned char *pHash,
                           size_t hashLen,
                           unsigned char *pSig,
                           size_t sigSize,
                           size_t *pSigLen,
                           int (* /*pRng*/)(void *, unsigned char *, size_t),
                           void * /*pRngCtx*/)
#else
static int p11SignCallback(void * /*ctx*/,
                           mbedtls_md_type_t /*mdAlg*/,
                           const unsigned char *pHash,
                           size_t hashLen,
                           unsigned char *pSig,
                           size_t *pSigLen,
                           int (* /*pRng*/)(void *, unsigned char *, size_t),
                           void * /*pRngCtx*/)
#endif
{
    CK_FUNCTION_LIST_PTR fnList = nullptr;
    C_GetFunctionList(&fnList);
    if (fnList == nullptr)
    {
        return -1;
    }

    CK_MECHANISM mech = {CKM_ECDSA, nullptr, 0};
    CK_RV rv = fnList->C_SignInit(g_signCtx.session, &mech, g_signCtx.privKey);
    if (rv != CKR_OK)
    {
        return -1;
    }

    CK_BYTE toBeSigned[256];
    if (hashLen > sizeof(toBeSigned))
    {
        return -1;
    }
    memcpy(toBeSigned, pHash, hashLen);

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
    *pSigLen = sigSize;
#else
    *pSigLen = 256;
#endif
    rv = fnList->C_Sign(g_signCtx.session, toBeSigned, static_cast<CK_ULONG>(hashLen), pSig,
                        reinterpret_cast<CK_ULONG_PTR>(pSigLen));
    if (rv != CKR_OK)
    {
        return -1;
    }

    // Convert raw ECDSA (R||S) to DER ASN.1
    if (*pSigLen == pkcs11ECDSA_P256_SIGNATURE_LENGTH)
    {
        PKI_pkcs11SignatureTombedTLSSignature(pSig, pSigLen);
    }

    return 0;
}

/** RNG callback backed by PKCS#11 C_GenerateRandom */
static int p11RngCallback(void *pCtx, unsigned char *pRandom, size_t len)
{
    CK_SESSION_HANDLE *session = static_cast<CK_SESSION_HANDLE *>(pCtx);
    CK_FUNCTION_LIST_PTR fnList = nullptr;
    C_GetFunctionList(&fnList);
    if (fnList == nullptr)
    {
        return -1;
    }
    CK_RV rv = fnList->C_GenerateRandom(*session, pRandom, static_cast<CK_ULONG>(len));
    return (rv == CKR_OK) ? 0 : -1;
}

} // anonymous namespace

bool CertificateManager::generateKeyPairAndCsrPkcs11_(const std::string &commonName, std::string &csrOut)
{
    CK_FUNCTION_LIST_PTR fnList = nullptr;
    CK_RV rv = C_GetFunctionList(&fnList);
    if (rv != CKR_OK || fnList == nullptr)
    {
        LOPCORE_LOGE(TAG_CM, "generateKeyPairAndCsrPkcs11_: cannot get function list");
        return false;
    }

    // ---- Generate EC P-256 key pair ----
    CK_MECHANISM mech = {CKM_EC_KEY_PAIR_GEN, nullptr, 0};
    CK_BYTE ecOid[] = pkcs11DER_ENCODED_OID_P256;
    CK_KEY_TYPE keyType = CKK_EC;
    CK_BBOOL trueVal = CK_TRUE;

    const char *pubLabel = pkcs11configLABEL_DEVICE_PUBLIC_KEY_FOR_TLS;
    const char *privLabel = pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS;

    CK_ATTRIBUTE pubTemplate[] = {{CKA_KEY_TYPE, &keyType, sizeof(keyType)},
                                  {CKA_VERIFY, &trueVal, sizeof(trueVal)},
                                  {CKA_EC_PARAMS, ecOid, sizeof(ecOid)},
                                  {CKA_LABEL, const_cast<char *>(pubLabel), strlen(pubLabel)}};
    CK_ATTRIBUTE privTemplate[] = {{CKA_KEY_TYPE, &keyType, sizeof(keyType)},
                                   {CKA_TOKEN, &trueVal, sizeof(trueVal)},
                                   {CKA_PRIVATE, &trueVal, sizeof(trueVal)},
                                   {CKA_SIGN, &trueVal, sizeof(trueVal)},
                                   {CKA_LABEL, const_cast<char *>(privLabel), strlen(privLabel)}};

    CK_OBJECT_HANDLE privHandle = CK_INVALID_HANDLE;
    CK_OBJECT_HANDLE pubHandle = CK_INVALID_HANDLE;

    rv = fnList->C_GenerateKeyPair(pImpl_->p11Session, &mech, pubTemplate,
                                   sizeof(pubTemplate) / sizeof(CK_ATTRIBUTE), privTemplate,
                                   sizeof(privTemplate) / sizeof(CK_ATTRIBUTE), &pubHandle, &privHandle);
    if (rv != CKR_OK)
    {
        LOPCORE_LOGE(TAG_CM, "C_GenerateKeyPair failed: 0x%lx", rv);
        return false;
    }

    // ---- Extract EC public key into mbedTLS ecdsa context ----
    mbedtls_ecdsa_context ecCtx;
    int mbedRet = extractEcPublicKey(pImpl_->p11Session, &ecCtx, pubHandle);
    if (mbedRet != 0)
    {
        LOPCORE_LOGE(TAG_CM, "extractEcPublicKey failed: %d", mbedRet);
        mbedtls_ecdsa_free(&ecCtx);
        return false;
    }

    // ---- Build a fake mbedtls_pk_context backed by the PKCS#11 private key ----
    const mbedtls_pk_info_t *ecInfo = mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY);
    mbedtls_pk_info_t customInfo;
    memcpy(&customInfo, ecInfo, sizeof(mbedtls_pk_info_t));
    customInfo.sign_func = p11SignCallback;

    mbedtls_pk_context fakePk;
    fakePk.pk_info = &customInfo;
    fakePk.pk_ctx = &ecCtx;

    // Set signing context (global, matches GrowBorg pattern)
    g_signCtx.session = pImpl_->p11Session;
    g_signCtx.privKey = privHandle;

    // ---- Write the CSR ----
    mbedtls_x509write_csr csrCtx;
    mbedtls_x509write_csr_init(&csrCtx);
    mbedtls_x509write_csr_set_md_alg(&csrCtx, MBEDTLS_MD_SHA256);
    mbedtls_x509write_csr_set_key(&csrCtx, &fakePk);

    std::string subject = (commonName.find('=') != std::string::npos) ? commonName : ("CN=" + commonName);

    mbedRet = mbedtls_x509write_csr_set_subject_name(&csrCtx, subject.c_str());
    if (mbedRet == 0)
    {
        mbedRet |= mbedtls_x509write_csr_set_key_usage(&csrCtx, MBEDTLS_X509_KU_DIGITAL_SIGNATURE);
        mbedRet |= mbedtls_x509write_csr_set_ns_cert_type(&csrCtx, MBEDTLS_X509_NS_CERT_TYPE_SSL_CLIENT);
    }

    std::vector<uint8_t> buf(config_.csrBufferSize, 0);
    if (mbedRet == 0)
    {
        CK_SESSION_HANDLE sessionForRng = pImpl_->p11Session;
        mbedRet = mbedtls_x509write_csr_pem(&csrCtx, buf.data(), buf.size(), p11RngCallback, &sessionForRng);
    }

    mbedtls_x509write_csr_free(&csrCtx);
    mbedtls_ecdsa_free(&ecCtx);

    if (mbedRet != 0)
    {
        LOPCORE_LOGE(TAG_CM, "mbedtls_x509write_csr_pem (PKCS#11) failed: %d", mbedRet);
        return false;
    }

    csrOut = std::string(reinterpret_cast<char *>(buf.data()));
    LOPCORE_LOGI(TAG_CM, "CSR generated (PKCS#11) — %zu bytes", csrOut.size());
    return true;
}

#endif // CONFIG_LOPCORE_PROV_CERT_COREP11

} // namespace prov
} // namespace lopcore
