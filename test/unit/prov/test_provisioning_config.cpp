/**
 * @file test_provisioning_config.cpp
 * @brief Unit tests for AwsProvisioningConfig and WiFiProvisioningConfig builders.
 *
 * Tests config construction, per-key storage routing, default fallbacks and
 * the readConfig / writeConfig round-trips using in-memory lambda storage.
 * All tests run on the host — no ESP-IDF target required.
 */

#include <map>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "lopcore/prov/provisioning_config.hpp"
#include "lopcore/prov/storage_types.hpp"
#include "lopcore/prov/wifi_provisioning.hpp"

using namespace lopcore::prov;

// ----------------------------------------------------------------
// Helper: simple in-memory storage backend
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
        {
            return it->second;
        }
        return std::nullopt;
    };
    return cb;
}

// ---------------------------------------------------------------
// WiFiProvisioningConfig tests
// ---------------------------------------------------------------

class WiFiProvisioningConfigTest : public ::testing::Test
{
protected:
    WiFiProvisioningConfig config;
};

TEST_F(WiFiProvisioningConfigTest, DefaultServiceName_IsProvDevice)
{
    EXPECT_EQ(config.getServiceName(), "PROV_DEVICE");
}

TEST_F(WiFiProvisioningConfigTest, DefaultSecurity_IsSecurity1)
{
    EXPECT_EQ(config.getSecurity(), ProvisioningSecurity::SECURITY_1);
}

TEST_F(WiFiProvisioningConfigTest, DefaultTransport_IsBle)
{
    EXPECT_EQ(config.getTransport(), ProvisioningTransport::BLE);
}

TEST_F(WiFiProvisioningConfigTest, SetServiceName_Fluent_Stores)
{
    config.setServiceName("GrowBorg-AABBCC");
    EXPECT_EQ(config.getServiceName(), "GrowBorg-AABBCC");
}

TEST_F(WiFiProvisioningConfigTest, SetSecurity0_Stores)
{
    config.setSecurity(ProvisioningSecurity::SECURITY_0);
    EXPECT_EQ(config.getSecurity(), ProvisioningSecurity::SECURITY_0);
}

TEST_F(WiFiProvisioningConfigTest, SetProofOfPossession_Stores)
{
    config.setProofOfPossession("abcd1234");
    ASSERT_TRUE(config.getProofOfPossession().has_value());
    EXPECT_EQ(*config.getProofOfPossession(), "abcd1234");
}

TEST_F(WiFiProvisioningConfigTest, NoProofOfPossession_ReturnsNullopt)
{
    EXPECT_FALSE(config.getProofOfPossession().has_value());
}

TEST_F(WiFiProvisioningConfigTest, SetWiFiStorage_Stores)
{
    std::map<std::string, std::string> store;
    config.setWiFiStorage(makeMemoryStorage(store));
    EXPECT_TRUE(config.getWiFiStorage().has_value());
}

TEST_F(WiFiProvisioningConfigTest, AddCustomEndpoint_IncreasesCount)
{
    EXPECT_EQ(config.getCustomEndpoints().size(), 0u);
    auto dummyHandler = std::shared_ptr<ICustomEndpointHandler>(nullptr);
    config.addCustomEndpoint({"ep1", dummyHandler});
    config.addCustomEndpoint({"ep2", dummyHandler});
    EXPECT_EQ(config.getCustomEndpoints().size(), 2u);
    EXPECT_EQ(config.getCustomEndpoints()[0].endpointName, "ep1");
    EXPECT_EQ(config.getCustomEndpoints()[1].endpointName, "ep2");
}

TEST_F(WiFiProvisioningConfigTest, SetEventCallbacks_Stores)
{
    bool started = false;
    WiFiProvisioningEvents events;
    events.onStarted = [&started]() { started = true; };
    config.setEventCallbacks(events);

    // Retrieve and invoke
    const auto &cb = config.getEventCallbacks();
    ASSERT_TRUE(cb.onStarted);
    cb.onStarted();
    EXPECT_TRUE(started);
}

TEST_F(WiFiProvisioningConfigTest, FluentChaining_AllSetters_ReturnRef)
{
    // Verify all setters return *this for chaining
    std::map<std::string, std::string> store;
    WiFiProvisioningConfig &ref = config.setServiceName("MyDevice")
                                      .setSecurity(ProvisioningSecurity::SECURITY_0)
                                      .setProofOfPossession("pop123")
                                      .setTransport(ProvisioningTransport::BLE)
                                      .setWiFiStorage(makeMemoryStorage(store));

    EXPECT_EQ(&ref, &config);
}

TEST_F(WiFiProvisioningConfigTest, SetTransportSoftAP_Stores)
{
    config.setTransport(ProvisioningTransport::SOFTAP);
    EXPECT_EQ(config.getTransport(), ProvisioningTransport::SOFTAP);
}

TEST_F(WiFiProvisioningConfigTest, SetServiceKey_ForSoftAP)
{
    EXPECT_FALSE(config.getServiceKey().has_value());
    config.setServiceKey("MyWPA2Pass");
    ASSERT_TRUE(config.getServiceKey().has_value());
    EXPECT_EQ(*config.getServiceKey(), "MyWPA2Pass");
}

TEST_F(WiFiProvisioningConfigTest, SetBleServiceUuid_Stores16Bytes)
{
    EXPECT_FALSE(config.getBleServiceUuid().has_value());

    std::array<uint8_t, 16> uuid = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    config.setBleServiceUuid(uuid);

    ASSERT_TRUE(config.getBleServiceUuid().has_value());
    EXPECT_EQ(*config.getBleServiceUuid(), uuid);
}

TEST_F(WiFiProvisioningConfigTest, BleUuidAndServiceKey_FluentChain)
{
    std::array<uint8_t, 16> uuid = {};
    WiFiProvisioningConfig &ref = config.setBleServiceUuid(uuid).setServiceKey("pass");
    EXPECT_EQ(&ref, &config);
    EXPECT_TRUE(config.getBleServiceUuid().has_value());
    EXPECT_TRUE(config.getServiceKey().has_value());
}

// ---------------------------------------------------------------
// AwsProvisioningConfig tests
// ---------------------------------------------------------------

class AwsProvisioningConfigTest : public ::testing::Test
{
protected:
    AwsProvisioningConfig config;
    std::map<std::string, std::string> store;
    StorageCallbacks callbacks;

    void SetUp() override
    {
        callbacks = makeMemoryStorage(store);
    }
};

TEST_F(AwsProvisioningConfigTest, AddConfigStorage_PerKey_RoutesProperly)
{
    std::map<std::string, std::string> store1, store2;
    auto cb1 = makeMemoryStorage(store1);
    auto cb2 = makeMemoryStorage(store2);

    config.addConfigStorage("aws_endpoint", cb1);
    config.addConfigStorage("thing_name", cb2);

    config.writeConfig("aws_endpoint", "a.iot.us-east-1.amazonaws.com");
    config.writeConfig("thing_name", "GrowBorg-001");

    // Values must be routed to their respective backends
    EXPECT_EQ(store1["aws_endpoint"], "a.iot.us-east-1.amazonaws.com");
    EXPECT_EQ(store2["thing_name"], "GrowBorg-001");
    EXPECT_TRUE(store1.find("thing_name") == store1.end());
    EXPECT_TRUE(store2.find("aws_endpoint") == store2.end());
}

TEST_F(AwsProvisioningConfigTest, ReadConfig_ExistingKey_ReturnsValue)
{
    config.addConfigStorage("aws_endpoint", callbacks);
    config.writeConfig("aws_endpoint", "endpoint.example.com");

    auto result = config.readConfig("aws_endpoint");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "endpoint.example.com");
}

TEST_F(AwsProvisioningConfigTest, ReadConfig_MissingKey_ReturnsNullopt)
{
    auto result = config.readConfig("nonexistent_key");
    EXPECT_FALSE(result.has_value());
}

TEST_F(AwsProvisioningConfigTest, DefaultFallback_UsedWhenKeyNotMapped)
{
    std::map<std::string, std::string> defaultStore;
    config.setDefaultConfigStorage(makeMemoryStorage(defaultStore));

    config.writeConfig("unmapped_key", "value");
    EXPECT_EQ(defaultStore["unmapped_key"], "value");
}

TEST_F(AwsProvisioningConfigTest, SetRetries_Stores)
{
    config.setRetries(5, 10);
    EXPECT_EQ(config.maxRetries(), 5);
    EXPECT_EQ(config.retryDelaySeconds(), 10);
}

TEST_F(AwsProvisioningConfigTest, SetCsrSubjectName_Stores)
{
    config.setCsrSubjectName("CN=GrowBorgDevice");
    EXPECT_EQ(config.csrSubjectName(), "CN=GrowBorgDevice");
}

TEST_F(AwsProvisioningConfigTest, SetDeviceIdProvider_Invokable)
{
    config.setDeviceIdProvider([]() { return std::string("AA:BB:CC:DD:EE:FF"); });
    auto provider = config.deviceIdProvider();
    ASSERT_TRUE(provider);
    EXPECT_EQ(provider(), "AA:BB:CC:DD:EE:FF");
}

TEST_F(AwsProvisioningConfigTest, SpecificStorageKey_ThingName_IsReadable)
{
    config.addConfigStorage("thing_name", callbacks);
    config.writeConfig("thing_name", "device-00001");

    auto name = config.getThingName();
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "device-00001");
}

TEST_F(AwsProvisioningConfigTest, SpecificStorageKey_AwsEndpoint_IsReadable)
{
    config.addConfigStorage(config.endpointKey(), callbacks);
    config.writeConfig(config.endpointKey(), "xyz.iot.amazonaws.com");

    auto ep = config.getEndpoint();
    ASSERT_TRUE(ep.has_value());
    EXPECT_EQ(*ep, "xyz.iot.amazonaws.com");
}
