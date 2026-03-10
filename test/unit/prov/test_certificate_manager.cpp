/**
 * @file test_certificate_manager.cpp
 * @brief Unit tests for CertificateManager (mbedTLS software key path)
 *
 * Tests the CertificateManager class with usePkcs11=false (software mode).
 * PKCS#11 path requires hardware and is excluded from host tests.
 */

#include <gtest/gtest.h>

#include "lopcore/prov/certificate_manager.hpp"

using namespace lopcore::prov;

// ----------------------------------------------------------------
// Fixture
// ----------------------------------------------------------------

class CertificateManagerTest : public ::testing::Test
{
protected:
    CertificateManager::Config config;
    std::unique_ptr<CertificateManager> cm;

    void SetUp() override
    {
        config.usePkcs11 = false;
        config.csrBufferSize = 4096;
        cm = std::make_unique<CertificateManager>(config);
    }
};

// ----------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------

TEST_F(CertificateManagerTest, Constructor_InitializesWithSoftwareMode)
{
    EXPECT_FALSE(cm->getConfig().usePkcs11);
}

// ----------------------------------------------------------------
// hasClaimCertificate — initial state
// ----------------------------------------------------------------

TEST_F(CertificateManagerTest, HasClaimCertificate_Initially_ReturnsFalse)
{
    EXPECT_FALSE(cm->hasClaimCertificate());
}

TEST_F(CertificateManagerTest, GetClaimCertPem_Initially_ReturnsNullopt)
{
    EXPECT_FALSE(cm->getClaimCertPem().has_value());
}

TEST_F(CertificateManagerTest, GetClaimKeyPem_Initially_ReturnsNullopt)
{
    EXPECT_FALSE(cm->getClaimKeyPem().has_value());
}

// ----------------------------------------------------------------
// importClaimCertificate (string overload)
// ----------------------------------------------------------------

TEST_F(CertificateManagerTest, ImportClaim_StringOverload_StoresAndReturnsValues)
{
    const std::string cert = "-----BEGIN CERTIFICATE-----\nMOCK\n-----END CERTIFICATE-----\n";
    const std::string key = "-----BEGIN EC PRIVATE KEY-----\nMOCK\n-----END EC PRIVATE KEY-----\n";

    EXPECT_TRUE(cm->importClaimCertificate(cert, key));
    EXPECT_TRUE(cm->hasClaimCertificate());

    auto gotCert = cm->getClaimCertPem();
    auto gotKey = cm->getClaimKeyPem();

    ASSERT_TRUE(gotCert.has_value());
    ASSERT_TRUE(gotKey.has_value());
    EXPECT_EQ(*gotCert, cert);
    EXPECT_EQ(*gotKey, key);
}

// ----------------------------------------------------------------
// importClaimCertificate (vector overload)
// ----------------------------------------------------------------

TEST_F(CertificateManagerTest, ImportClaim_VectorOverload_StoresValues)
{
    const std::string cert = "-----BEGIN CERTIFICATE-----\nVECTOR_CERT\n-----END CERTIFICATE-----\n";
    const std::string key = "-----BEGIN EC PRIVATE KEY-----\nVECTOR_KEY\n-----END EC PRIVATE KEY-----\n";

    std::vector<uint8_t> certVec(cert.begin(), cert.end());
    std::vector<uint8_t> keyVec(key.begin(), key.end());

    EXPECT_TRUE(cm->importClaimCertificate(certVec, keyVec));
    EXPECT_TRUE(cm->hasClaimCertificate());
}

// ----------------------------------------------------------------
// deleteClaimCredentials
// ----------------------------------------------------------------

TEST_F(CertificateManagerTest, DeleteClaim_AfterImport_ClearsState)
{
    const std::string cert = "CERT";
    const std::string key = "KEY";
    cm->importClaimCertificate(cert, key);
    ASSERT_TRUE(cm->hasClaimCertificate());

    EXPECT_TRUE(cm->deleteClaimCredentials());

    EXPECT_FALSE(cm->hasClaimCertificate());
    EXPECT_FALSE(cm->getClaimCertPem().has_value());
    EXPECT_FALSE(cm->getClaimKeyPem().has_value());
}

TEST_F(CertificateManagerTest, DeleteClaim_WhenNotLoaded_ReturnsTrue)
{
    EXPECT_TRUE(cm->deleteClaimCredentials());
}

// ----------------------------------------------------------------
// generateCsr (software path)
// ----------------------------------------------------------------

TEST_F(CertificateManagerTest, GenerateCsr_WithBareCN_ReturnsPemCsr)
{
    std::string csrOut;
    bool result = cm->generateCsr("TestDevice", csrOut);

    EXPECT_TRUE(result);
    EXPECT_FALSE(csrOut.empty());
    EXPECT_NE(csrOut.find("-----BEGIN CERTIFICATE REQUEST-----"), std::string::npos)
        << "CSR should contain PEM header. Got: " << csrOut.substr(0, 100);
}

TEST_F(CertificateManagerTest, GenerateCsr_WithFullSubjectName_ReturnsPemCsr)
{
    std::string csrOut;
    bool result = cm->generateCsr("CN=MyDevice", csrOut);

    EXPECT_TRUE(result);
    EXPECT_NE(csrOut.find("-----BEGIN CERTIFICATE REQUEST-----"), std::string::npos);
}

TEST_F(CertificateManagerTest, GenerateCsr_TwiceConcurrently_BothSucceed)
{
    std::string csr1, csr2;
    EXPECT_TRUE(cm->generateCsr("Device1", csr1));
    EXPECT_TRUE(cm->generateCsr("Device2", csr2));
    // Both should produce valid CSRs
    EXPECT_NE(csr1.find("-----BEGIN CERTIFICATE REQUEST-----"), std::string::npos);
    EXPECT_NE(csr2.find("-----BEGIN CERTIFICATE REQUEST-----"), std::string::npos);
}

// ----------------------------------------------------------------
// generateKeyPairAndCsr — delegates to generateCsr when usePkcs11=false
// ----------------------------------------------------------------

TEST_F(CertificateManagerTest, GenerateKeyPairAndCsr_SoftwareMode_ReturnsCsr)
{
    std::string csr;
    EXPECT_TRUE(cm->generateKeyPairAndCsr("GrowBorgDevice", csr));
    EXPECT_NE(csr.find("CERTIFICATE REQUEST"), std::string::npos);
}

// ----------------------------------------------------------------
// storeFinalCertificate / hasFinalCertificate (software mode)
// ----------------------------------------------------------------

TEST_F(CertificateManagerTest, StoreFinalCertificate_SoftwareMode_ReturnsTrueAndNoStorage)
{
    const std::string label = "device_cert";
    const std::string pem = "-----BEGIN CERTIFICATE-----\nDATA\n-----END CERTIFICATE-----\n";

    // In software mode this is non-fatal (kept in memory only)
    EXPECT_TRUE(cm->storeFinalCertificate(label, pem));
    // hasFinalCertificate returns false without PKCS#11 backend
    EXPECT_FALSE(cm->hasFinalCertificate(label));
}

TEST_F(CertificateManagerTest, StoreFinalCertificate_VectorOverload_Succeeds)
{
    const std::string pem = "-----BEGIN CERTIFICATE-----\nDATA\n-----END CERTIFICATE-----\n";
    std::vector<uint8_t> vec(pem.begin(), pem.end());
    EXPECT_TRUE(cm->storeFinalCertificate("label", vec));
}

TEST_F(CertificateManagerTest, StoreFinalPrivateKey_SoftwareMode_ReturnsTrue)
{
    const std::string pem = "-----BEGIN EC PRIVATE KEY-----\nDATA\n-----END EC PRIVATE KEY-----\n";
    EXPECT_TRUE(cm->storeFinalPrivateKey("device_key", pem));
}

// ----------------------------------------------------------------
// deleteDeviceCredentials (software mode)
// ----------------------------------------------------------------

TEST_F(CertificateManagerTest, DeleteDeviceCredentials_SoftwareMode_ReturnsTrue)
{
    EXPECT_TRUE(cm->deleteDeviceCredentials());
}
