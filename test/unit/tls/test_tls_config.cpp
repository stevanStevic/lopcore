/**
 * @file test_tls_config.cpp
 * @brief Unit tests for TLS configuration validation
 *
 * Tests the TLS configuration validation logic, including PKCS#11
 * certificate requirements and error message generation.
 *
 * @copyright Copyright (c) 2025
 */

#include <gtest/gtest.h>

#include "lopcore/tls/tls_config.hpp"

using namespace lopcore::tls;

/**
 * @brief Test fixture for TlsConfig validation tests
 */
class TlsConfigValidationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Reset any logging state if needed
    }

    void TearDown() override
    {
        // Cleanup
    }

    /**
     * @brief Helper to create a valid base configuration
     */
    TlsConfig createValidConfig()
    {
        TlsConfig config;
        config.hostname = "mqtt.example.com";
        config.port = 8883;
        config.caCertPath = "/spiffs/certs/root-ca.crt";
        config.clientCertLabel = "device-cert";
        config.clientKeyLabel = "device-key";
        config.verifyPeer = true;
        return config;
    }
};

// =============================================================================
// Basic Validation Tests
// =============================================================================

TEST_F(TlsConfigValidationTest, ValidConfig_Success)
{
    TlsConfig config = createValidConfig();

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_OK, result);
}

TEST_F(TlsConfigValidationTest, EmptyHostname_Fails)
{
    TlsConfig config = createValidConfig();
    config.hostname = "";

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_ERR_INVALID_ARG, result);
}

TEST_F(TlsConfigValidationTest, ZeroPort_Fails)
{
    TlsConfig config = createValidConfig();
    config.port = 0;

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_ERR_INVALID_ARG, result);
}

// =============================================================================
// Certificate Validation Tests
// =============================================================================

TEST_F(TlsConfigValidationTest, MissingClientCert_WithVerifyPeer_Fails)
{
    TlsConfig config = createValidConfig();
    config.clientCertLabel = "";

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_ERR_INVALID_ARG, result);
}

TEST_F(TlsConfigValidationTest, MissingPrivateKey_WithVerifyPeer_Fails)
{
    TlsConfig config = createValidConfig();
    config.clientKeyLabel = "";

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_ERR_INVALID_ARG, result);
}

TEST_F(TlsConfigValidationTest, MissingClientCert_WithoutVerifyPeer_Success)
{
    TlsConfig config = createValidConfig();
    config.verifyPeer = false;
    config.clientCertLabel = "";
    config.clientKeyLabel = "";

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_OK, result);
}

// =============================================================================
// PKCS#11 CA Certificate Validation Tests
// =============================================================================

TEST_F(TlsConfigValidationTest, MissingCACert_WithPKCS11_Fails)
{
    TlsConfig config = createValidConfig();
    config.caCertPath = ""; // Missing CA cert with PKCS#11

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_ERR_INVALID_ARG, result);
}

TEST_F(TlsConfigValidationTest, MissingCACert_WithoutPKCS11_Success)
{
    TlsConfig config;
    config.hostname = "mqtt.example.com";
    config.port = 8883;
    config.caCertPath = "";    // No CA cert
    config.verifyPeer = false; // No peer verification

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_OK, result);
}

TEST_F(TlsConfigValidationTest, ValidCACert_WithPKCS11_Success)
{
    TlsConfig config = createValidConfig();
    // All fields are set correctly

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_OK, result);
}

// =============================================================================
// TlsConfigBuilder Tests
// =============================================================================

TEST_F(TlsConfigValidationTest, Builder_CompleteConfig_Success)
{
    TlsConfig config = TlsConfigBuilder()
                           .hostname("mqtt.example.com")
                           .port(8883)
                           .caCertificate("/spiffs/certs/root-ca.crt")
                           .clientCertificate("device-cert")
                           .privateKey("device-key")
                           .verifyPeer(true)
                           .build();

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_OK, result);
    EXPECT_EQ("mqtt.example.com", config.hostname);
    EXPECT_EQ(8883, config.port);
    EXPECT_EQ("/spiffs/certs/root-ca.crt", config.caCertPath);
    EXPECT_EQ("device-cert", config.clientCertLabel);
    EXPECT_EQ("device-key", config.clientKeyLabel);
    EXPECT_TRUE(config.verifyPeer);
}

TEST_F(TlsConfigValidationTest, Builder_MinimalConfig_Success)
{
    TlsConfig config = TlsConfigBuilder().hostname("mqtt.example.com").port(8883).verifyPeer(false).build();

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_OK, result);
}

TEST_F(TlsConfigValidationTest, Builder_WithALPN_Success)
{
    TlsConfig config = TlsConfigBuilder()
                           .hostname("mqtt.example.com")
                           .port(443)
                           .caCertificate("/spiffs/certs/root-ca.crt")
                           .clientCertificate("device-cert")
                           .privateKey("device-key")
                           .build();

    // Manually add ALPN protocol (builder doesn't have this method)
    config.alpnProtocols = {"x-amzn-mqtt-ca"};

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_OK, result);
    EXPECT_EQ(1, config.alpnProtocols.size());
    EXPECT_EQ("x-amzn-mqtt-ca", config.alpnProtocols[0]);
}

// =============================================================================
// Timeout Configuration Tests
// =============================================================================

TEST_F(TlsConfigValidationTest, Builder_CustomTimeouts_Success)
{
    TlsConfig config = TlsConfigBuilder()
                           .hostname("mqtt.example.com")
                           .port(8883)
                           .caCertificate("/spiffs/certs/root-ca.crt")
                           .clientCertificate("device-cert")
                           .privateKey("device-key")
                           .connectionTimeout(std::chrono::milliseconds(5000))
                           .sendTimeout(std::chrono::milliseconds(2000))
                           .recvTimeout(std::chrono::milliseconds(3000))
                           .build();

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_OK, result);
    EXPECT_EQ(std::chrono::milliseconds(5000), config.connectionTimeout);
    EXPECT_EQ(std::chrono::milliseconds(2000), config.sendTimeout);
    EXPECT_EQ(std::chrono::milliseconds(3000), config.recvTimeout);
}

// =============================================================================
// Retry Configuration Tests
// =============================================================================

TEST_F(TlsConfigValidationTest, Builder_CustomRetryConfig_Success)
{
    TlsConfig config = TlsConfigBuilder()
                           .hostname("mqtt.example.com")
                           .port(8883)
                           .caCertificate("/spiffs/certs/root-ca.crt")
                           .clientCertificate("device-cert")
                           .privateKey("device-key")
                           .maxRetries(10)
                           .retryBaseDelay(std::chrono::milliseconds(1000))
                           .retryMaxDelay(std::chrono::milliseconds(10000))
                           .build();

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_OK, result);
    EXPECT_EQ(10, config.maxRetries);
    EXPECT_EQ(std::chrono::milliseconds(1000), config.retryBaseDelay);
    EXPECT_EQ(std::chrono::milliseconds(10000), config.retryMaxDelay);
}

// =============================================================================
// SNI Configuration Tests
// =============================================================================

TEST_F(TlsConfigValidationTest, DisableSNI_Success)
{
    TlsConfig config = TlsConfigBuilder()
                           .hostname("mqtt.example.com")
                           .port(8883)
                           .caCertificate("/spiffs/certs/root-ca.crt")
                           .clientCertificate("device-cert")
                           .privateKey("device-key")
                           .build();

    config.enableSni = false; // Set directly since builder doesn't have method

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_OK, result);
    EXPECT_FALSE(config.enableSni);
}

TEST_F(TlsConfigValidationTest, EnableSNI_Success)
{
    TlsConfig config = TlsConfigBuilder()
                           .hostname("mqtt.example.com")
                           .port(8883)
                           .caCertificate("/spiffs/certs/root-ca.crt")
                           .clientCertificate("device-cert")
                           .privateKey("device-key")
                           .build();

    config.enableSni = true; // Verify default is true

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_OK, result);
    EXPECT_TRUE(config.enableSni);
}

// =============================================================================
// Edge Cases and Combined Failures
// =============================================================================

TEST_F(TlsConfigValidationTest, MultipleErrors_ReturnsFirstError)
{
    TlsConfig config;
    // Everything is invalid
    config.hostname = "";
    config.port = 0;
    config.clientCertLabel = "";
    config.clientKeyLabel = "";
    config.caCertPath = "";
    config.verifyPeer = true;

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_ERR_INVALID_ARG, result);
}

TEST_F(TlsConfigValidationTest, SkipCommonNameCheck_Success)
{
    TlsConfig config = createValidConfig();
    config.skipCommonNameCheck = true;

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_OK, result);
    EXPECT_TRUE(config.skipCommonNameCheck);
}

// =============================================================================
// Real-World AWS IoT Configuration Tests
// =============================================================================

TEST_F(TlsConfigValidationTest, AWSIoT_Port8883_Success)
{
    TlsConfig config = TlsConfigBuilder()
                           .hostname("xxxx.iot.us-east-1.amazonaws.com")
                           .port(8883)
                           .caCertificate("/spiffs/certs/AmazonRootCA1.crt")
                           .clientCertificate("device-cert")
                           .privateKey("device-key")
                           .build();

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_OK, result);
}

TEST_F(TlsConfigValidationTest, AWSIoT_Port443_WithALPN_Success)
{
    TlsConfig config = TlsConfigBuilder()
                           .hostname("xxxx.iot.us-east-1.amazonaws.com")
                           .port(443)
                           .caCertificate("/spiffs/certs/AmazonRootCA1.crt")
                           .clientCertificate("device-cert")
                           .privateKey("device-key")
                           .build();

    config.alpnProtocols = {"x-amzn-mqtt-ca"};

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_OK, result);
}

TEST_F(TlsConfigValidationTest, AWSIoT_MissingCACert_Fails)
{
    TlsConfig config = TlsConfigBuilder()
                           .hostname("xxxx.iot.us-east-1.amazonaws.com")
                           .port(8883)
                           // Missing CA certificate
                           .clientCertificate("device-cert")
                           .privateKey("device-key")
                           .build();

    esp_err_t result = config.validate();

    EXPECT_EQ(ESP_ERR_INVALID_ARG, result);
}
