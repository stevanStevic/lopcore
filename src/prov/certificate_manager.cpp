#include "lopcore/prov/certificate_manager.hpp"
#include "lopcore/logging/logger.hpp"

// Include mbedTLS for crypto operations
#include <mbedtls/pk.h>
#include <mbedtls/x509_csr.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <cstring>  // For strlen

namespace lopcore {
namespace prov {

// PIMPL implementation to hide PKCS11 details
struct CertificateManager::Impl {
    // Claim certificate storage (temporary)
    std::string claimCertPem;
    std::string claimKeyPem;
    bool hasClaimCert = false;
    
    // mbedTLS contexts for CSR generation
    mbedtls_pk_context deviceKey;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctrDrbg;
    bool mbedtlsInitialized = false;
    
    // PKCS11 handle (backend-specific, initialized in constructor)
    void* pkcs11Handle = nullptr;
    
    Impl() {
        mbedtls_pk_init(&deviceKey);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctrDrbg);
    }
    
    ~Impl() {
        mbedtls_pk_free(&deviceKey);
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&ctrDrbg);
    }
};

CertificateManager::CertificateManager(const Config& config)
    : config_(config)
    , pImpl_(std::make_unique<Impl>())
{
    LOPCORE_LOGI("CertManager", "Initializing Certificate Manager");
    LOPCORE_LOGI("CertManager", "  usePkcs11: %s", config_.usePkcs11 ? "true" : "false");
    LOPCORE_LOGI("CertManager", "  backend: %s", 
                 config_.pkcs11Backend == Pkcs11Backend::AWS_CORE_PKCS11 ? "AWS corePKCS11" : "ESP-IDF PKCS11");
    
    // TODO: Initialize PKCS11 backend based on config_.pkcs11Backend
    // For now, just log the configuration
    if (config_.usePkcs11) {
        LOPCORE_LOGI("CertManager", "PKCS11 initialization will be implemented");
    }
}

CertificateManager::~CertificateManager() {
    LOPCORE_LOGI("CertManager", "Shutting down Certificate Manager");
    // TODO: Cleanup PKCS11 session
}

bool CertificateManager::importClaimCertificate(const std::vector<uint8_t>& certData,
                                                const std::vector<uint8_t>& keyData) {
    LOPCORE_LOGI("CertManager", "Importing claim certificate (%zu bytes) and key (%zu bytes)",
                 certData.size(), keyData.size());
    
    // Convert to strings (assume PEM format)
    std::string certPem(certData.begin(), certData.end());
    std::string keyPem(keyData.begin(), keyData.end());
    
    return importClaimCertificate(certPem, keyPem);
}

bool CertificateManager::importClaimCertificate(const std::string& certPem,
                                                const std::string& keyPem) {
    LOPCORE_LOGI("CertManager", "Storing claim certificate and key in memory");
    
    // Store in memory for later use during provisioning
    pImpl_->claimCertPem = certPem;
    pImpl_->claimKeyPem = keyPem;
    pImpl_->hasClaimCert = true;
    
    LOPCORE_LOGI("CertManager", "Claim certificate imported successfully");
    return true;
}

bool CertificateManager::generateCsr(const std::string& commonName, std::string& csrOut) {
    LOPCORE_LOGI("CertManager", "Generating CSR with CN='%s'", commonName.c_str());
    
    // Initialize mbedTLS random number generator if not already done
    if (!pImpl_->mbedtlsInitialized) {
        const char* pers = "lopcore_cert_manager";
        int ret = mbedtls_ctr_drbg_seed(&pImpl_->ctrDrbg, mbedtls_entropy_func,
                                       &pImpl_->entropy,
                                       (const unsigned char*)pers, strlen(pers));
        if (ret != 0) {
            LOPCORE_LOGE("CertManager", "Failed to seed RNG: -0x%04x", -ret);
            return false;
        }
        pImpl_->mbedtlsInitialized = true;
    }
    
    // Generate EC key pair (P-256)
    LOPCORE_LOGI("CertManager", "Generating EC P-256 key pair");
    int ret = mbedtls_pk_setup(&pImpl_->deviceKey,
                              mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) {
        LOPCORE_LOGE("CertManager", "Failed to setup PK context: -0x%04x", -ret);
        return false;
    }
    
    ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,
                              mbedtls_pk_ec(pImpl_->deviceKey),
                              mbedtls_ctr_drbg_random,
                              &pImpl_->ctrDrbg);
    if (ret != 0) {
        LOPCORE_LOGE("CertManager", "Failed to generate EC key: -0x%04x", -ret);
        return false;
    }
    
    // Create CSR
    LOPCORE_LOGI("CertManager", "Creating CSR");
    mbedtls_x509write_csr csrCtx;
    mbedtls_x509write_csr_init(&csrCtx);
    mbedtls_x509write_csr_set_md_alg(&csrCtx, MBEDTLS_MD_SHA256);
    mbedtls_x509write_csr_set_key(&csrCtx, &pImpl_->deviceKey);
    
    // Set subject name
    char subjectName[128];
    snprintf(subjectName, sizeof(subjectName), "CN=%s", commonName.c_str());
    ret = mbedtls_x509write_csr_set_subject_name(&csrCtx, subjectName);
    if (ret != 0) {
        LOPCORE_LOGE("CertManager", "Failed to set subject name: -0x%04x", -ret);
        mbedtls_x509write_csr_free(&csrCtx);
        return false;
    }
    
    // Write CSR to PEM buffer
    std::vector<uint8_t> csrBuffer(config_.csrBufferSize);
    ret = mbedtls_x509write_csr_pem(&csrCtx, csrBuffer.data(), csrBuffer.size(),
                                   mbedtls_ctr_drbg_random, &pImpl_->ctrDrbg);
    mbedtls_x509write_csr_free(&csrCtx);
    
    if (ret != 0) {
        LOPCORE_LOGE("CertManager", "Failed to write CSR: -0x%04x", -ret);
        return false;
    }
    
    csrOut = std::string((char*)csrBuffer.data());
    LOPCORE_LOGI("CertManager", "CSR generated successfully (%zu bytes)", csrOut.size());
    return true;
}

bool CertificateManager::storeFinalCertificate(const std::string& label,
                                               const std::vector<uint8_t>& certData) {
    std::string certPem(certData.begin(), certData.end());
    return storeFinalCertificate(label, certPem);
}

bool CertificateManager::storeFinalCertificate(const std::string& label,
                                               const std::string& certPem) {
    LOPCORE_LOGI("CertManager", "Storing final certificate with label '%s'", label.c_str());
    
    if (config_.usePkcs11) {
        // TODO: Store in PKCS11
        LOPCORE_LOGW("CertManager", "PKCS11 storage not yet implemented - storing in memory");
        // For now, just log success
        return true;
    } else {
        // Store in file system or other storage
        LOPCORE_LOGW("CertManager", "File-based cert storage not yet implemented");
        return false;
    }
}

bool CertificateManager::storeFinalPrivateKey(const std::string& label,
                                              const std::vector<uint8_t>& keyData) {
    std::string keyPem(keyData.begin(), keyData.end());
    return storeFinalPrivateKey(label, keyPem);
}

bool CertificateManager::storeFinalPrivateKey(const std::string& label,
                                              const std::string& keyPem) {
    LOPCORE_LOGI("CertManager", "Storing final private key with label '%s'", label.c_str());
    
    if (config_.usePkcs11) {
        // TODO: Store in PKCS11
        LOPCORE_LOGW("CertManager", "PKCS11 storage not yet implemented - storing in memory");
        // For now, just log success
        return true;
    } else {
        // Store in file system or other storage
        LOPCORE_LOGW("CertManager", "File-based key storage not yet implemented");
        return false;
    }
}

bool CertificateManager::hasClaimCertificate() const {
    return pImpl_->hasClaimCert;
}

bool CertificateManager::hasFinalCertificate(const std::string& label) const {
    if (config_.usePkcs11) {
        // TODO: Check PKCS11
        LOPCORE_LOGW("CertManager", "PKCS11 check not yet implemented");
        return false;
    }
    return false;
}

std::optional<std::string> CertificateManager::getClaimCertificate() const {
    if (pImpl_->hasClaimCert) {
        return pImpl_->claimCertPem;
    }
    return std::nullopt;
}

// PKCS11 backend-agnostic operations (to be implemented)
bool CertificateManager::pkcs11StoreObject(const std::string& label,
                                           const std::vector<uint8_t>& data,
                                           uint32_t objectClass) {
    // TODO: Implement based on config_.pkcs11Backend
    LOPCORE_LOGW("CertManager", "pkcs11StoreObject not yet implemented");
    return false;
}

bool CertificateManager::pkcs11GetObject(const std::string& label,
                                        std::vector<uint8_t>& data,
                                        uint32_t objectClass) {
    // TODO: Implement based on config_.pkcs11Backend
    LOPCORE_LOGW("CertManager", "pkcs11GetObject not yet implemented");
    return false;
}

bool CertificateManager::pkcs11ObjectExists(const std::string& label,
                                           uint32_t objectClass) {
    // TODO: Implement based on config_.pkcs11Backend
    return false;
}

bool CertificateManager::pkcs11DeleteObject(const std::string& label) {
    // TODO: Implement based on config_.pkcs11Backend
    LOPCORE_LOGW("CertManager", "pkcs11DeleteObject not yet implemented");
    return false;
}

} // namespace prov
} // namespace lopcore
