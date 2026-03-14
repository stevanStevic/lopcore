/**
 * @file test_aws_data_endpoint_handler.cpp
 * @brief Unit tests for AwsDataEndpointHandler
 *
 * Tests JSON parsing, field validation, per-field storage dispatch, and
 * response formatting.  Uses in-memory StorageCallbacks (no ESP-IDF required).
 * Requires: cJSON (linked via CMakeLists).
 */

#include <map>
#include <string>

#include <gtest/gtest.h>

#include "lopcore/prov/aws_data_endpoint_handler.hpp"
#include "lopcore/prov/storage_types.hpp"

using namespace lopcore::prov;

// ----------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------

static StorageCallbacks makeMemoryStorage(std::map<std::string, std::string> &store)
{
    StorageCallbacks cb;
    cb.write = [&store](const std::string &k, const std::string &v) {
        store[k] = v;
        return true;
    };
    cb.read = [&store](const std::string &k) -> std::optional<std::string> {
        auto it = store.find(k);
        if (it != store.end())
            return it->second;
        return std::nullopt;
    };
    return cb;
}

/** Build a valid JSON payload with all 5 required fields */
static std::string
makeValidJson(const std::string &cert = "-----BEGIN CERTIFICATE-----\nCERT\n-----END CERTIFICATE-----\n",
              const std::string &key = "-----BEGIN EC PRIVATE KEY-----\nKEY\n-----END EC PRIVATE KEY-----\n",
              const std::string &rootCA = "-----BEGIN CERTIFICATE-----\nROOT\n-----END CERTIFICATE-----\n",
              const std::string &ep = "xyz.iot.amazonaws.com",
              const std::string &tmpl = "GrowBorgTemplate")
{
    // Use simple string-build; real handler uses cJSON_Parse internally
    return std::string(R"({"certificate":")") + cert + R"(","private_key":")" + key + R"(","root_ca":")" +
           rootCA + R"(","aws_endpoint":")" + ep + R"(","provisioning_template":")" + tmpl + R"("})";
}

// ----------------------------------------------------------------
// Fixture
// ----------------------------------------------------------------

class AwsDataEndpointHandlerTest : public ::testing::Test
{
protected:
    std::map<std::string, std::string> store;
    AwsDataEndpointConfig cfg;
    std::unique_ptr<AwsDataEndpointHandler> handler;

    void SetUp() override
    {
        auto mem = makeMemoryStorage(store);
        // All 5 fields go to the same in-memory store
        cfg.storageMap["claim_cert"] = mem;
        cfg.storageMap["claim_key"] = mem;
        cfg.storageMap["root_ca"] = mem;
        cfg.storageMap["aws_endpoint"] = mem;
        cfg.storageMap["provisioning_template"] = mem;

        handler = std::make_unique<AwsDataEndpointHandler>(cfg);
    }

    /** Convenience: invoke onDataReceived with the given string payload */
    bool feed(const std::string &json)
    {
        const auto *data = reinterpret_cast<const uint8_t *>(json.c_str());
        return handler->onDataReceived(1, data, json.size(), /*isComplete=*/true);
    }

    /** Read back recorded response into string */
    std::string response()
    {
        uint8_t buf[256] = {};
        size_t len = handler->getResponse(1, buf, sizeof(buf));
        return std::string(reinterpret_cast<char *>(buf), len);
    }
};

// ----------------------------------------------------------------
// Valid payload
// ----------------------------------------------------------------

TEST_F(AwsDataEndpointHandlerTest, ValidJson_AllFields_ReturnsTrue)
{
    EXPECT_TRUE(feed(makeValidJson()));
}

TEST_F(AwsDataEndpointHandlerTest, ValidJson_Response_ContainsSuccess)
{
    feed(makeValidJson());
    EXPECT_EQ(response(), "SUCCESS");
}

TEST_F(AwsDataEndpointHandlerTest, ValidJson_StoresAllFieldsToStorage)
{
    feed(makeValidJson("CERT_PEM", "KEY_PEM", "ROOTCA_PEM", "ep.example.com", "TemplateName"));

    EXPECT_EQ(store["aws_endpoint"], "ep.example.com");
    EXPECT_EQ(store["provisioning_template"], "TemplateName");
    EXPECT_EQ(store["claim_cert"], "CERT_PEM");
    EXPECT_EQ(store["claim_key"], "KEY_PEM");
    EXPECT_EQ(store["root_ca"], "ROOTCA_PEM");
}

// ----------------------------------------------------------------
// Missing fields
// ----------------------------------------------------------------

TEST_F(AwsDataEndpointHandlerTest, MissingCertificate_ReturnsFalse)
{
    std::string json =
        R"({"private_key":"KEY","root_ca":"CA","aws_endpoint":"ep","provisioning_template":"t"})";
    EXPECT_FALSE(feed(json));
}

TEST_F(AwsDataEndpointHandlerTest, MissingPrivateKey_ReturnsFalse)
{
    std::string json =
        R"({"certificate":"CERT","root_ca":"CA","aws_endpoint":"ep","provisioning_template":"t"})";
    EXPECT_FALSE(feed(json));
}

TEST_F(AwsDataEndpointHandlerTest, EmptyPayload_ReturnsFalse)
{
    EXPECT_FALSE(feed(""));
}

// ----------------------------------------------------------------
// Invalid JSON
// ----------------------------------------------------------------

TEST_F(AwsDataEndpointHandlerTest, MalformedJson_ReturnsFalse)
{
    EXPECT_FALSE(feed("{NOT_VALID_JSON"));
}

TEST_F(AwsDataEndpointHandlerTest, EmptyObject_ReturnsFalse)
{
    EXPECT_FALSE(feed("{}"));
}

// ----------------------------------------------------------------
// Null / zero-length data
// ----------------------------------------------------------------

TEST_F(AwsDataEndpointHandlerTest, NullData_ReturnsFalse)
{
    EXPECT_FALSE(handler->onDataReceived(1, nullptr, 10, true));
}

TEST_F(AwsDataEndpointHandlerTest, ZeroLength_ReturnsFalse)
{
    uint8_t dummy = 0;
    EXPECT_FALSE(handler->onDataReceived(1, &dummy, 0, true));
}

// ----------------------------------------------------------------
// Incomplete flag
// ----------------------------------------------------------------

TEST_F(AwsDataEndpointHandlerTest, IsCompleteFlag_False_ReturnsFalse)
{
    auto json = makeValidJson();
    const auto *data = reinterpret_cast<const uint8_t *>(json.c_str());
    EXPECT_FALSE(handler->onDataReceived(1, data, json.size(), /*isComplete=*/false));
}

// ----------------------------------------------------------------
// Storage failure
// ----------------------------------------------------------------

TEST_F(AwsDataEndpointHandlerTest, StorageFailure_ReturnsFalse)
{
    // Replace one entry with a failing writer
    cfg.storageMap["aws_endpoint"].write = [](const std::string &, const std::string &) { return false; };
    handler = std::make_unique<AwsDataEndpointHandler>(cfg);

    EXPECT_FALSE(feed(makeValidJson()));
}

// ----------------------------------------------------------------
// Reset
// ----------------------------------------------------------------

TEST_F(AwsDataEndpointHandlerTest, ResetAfterError_AllowsNextCall)
{
    feed("{INVALID}");
    handler->reset();
    EXPECT_TRUE(feed(makeValidJson()));
    EXPECT_EQ(response(), "SUCCESS");
}

// ----------------------------------------------------------------
// getProtocol
// ----------------------------------------------------------------

TEST_F(AwsDataEndpointHandlerTest, GetProtocol_ReturnsJsonLengthPrefixed)
{
    EXPECT_EQ(handler->getProtocol(), EndpointProtocol::JSON_LENGTH_PREFIXED);
}
