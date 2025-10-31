/**
 * @file test_mqtt_traits.cpp
 * @brief Compile-time tests for MQTT client type traits
 *
 * This file uses static_assert to verify that type traits correctly
 * detect capabilities of MQTT client implementations at compile-time.
 *
 * @copyright Copyright (c) 2025
 */

#include <gtest/gtest.h>

#include "lopcore/mqtt/coremqtt_client.hpp"
#include "lopcore/mqtt/esp_mqtt_client.hpp"
#include "lopcore/mqtt/mqtt_traits.hpp"

using namespace lopcore::mqtt;
using namespace lopcore::mqtt::traits;

// =============================================================================
// CoreMqttClient Trait Tests
// =============================================================================

// Basic MQTT Client
static_assert(is_mqtt_client_v<CoreMqttClient>, "CoreMqttClient must satisfy is_mqtt_client");

// Manual Processing Support (CoreMQTT specific!)
static_assert(has_manual_processing_v<CoreMqttClient>, "CoreMqttClient must support manual processing");

// Statistics Support
static_assert(has_statistics_v<CoreMqttClient>, "CoreMqttClient must support statistics");

// QoS 2 Support
static_assert(supports_qos2_v<CoreMqttClient>, "CoreMqttClient must support QoS 2");

// Composite Traits
static_assert(is_synchronous_capable_v<CoreMqttClient>,
              "CoreMqttClient must support synchronous patterns (manual processing)");
static_assert(is_asynchronous_capable_v<CoreMqttClient>,
              "CoreMqttClient must support asynchronous patterns (callbacks)");
static_assert(is_production_ready_v<CoreMqttClient>, "CoreMqttClient must be production-ready");

// =============================================================================
// EspMqttClient Trait Tests
// =============================================================================

// Basic MQTT Client
static_assert(is_mqtt_client_v<EspMqttClient>, "EspMqttClient must satisfy is_mqtt_client");

// Manual Processing Support (ESP-MQTT does NOT have this!)
static_assert(!has_manual_processing_v<EspMqttClient>, "EspMqttClient must NOT support manual processing");

// Statistics Support
static_assert(has_statistics_v<EspMqttClient>, "EspMqttClient must support statistics");

// QoS 2 Support
static_assert(supports_qos2_v<EspMqttClient>, "EspMqttClient must support QoS 2");

// Composite Traits
static_assert(!is_synchronous_capable_v<EspMqttClient>,
              "EspMqttClient must NOT support synchronous patterns (no manual processing)");
static_assert(is_asynchronous_capable_v<EspMqttClient>,
              "EspMqttClient must support asynchronous patterns (callbacks)");
static_assert(is_production_ready_v<EspMqttClient>, "EspMqttClient must be production-ready");

// =============================================================================
// Compile-Time Decision Making Tests
// =============================================================================

/**
 * @brief Test that we can make compile-time decisions based on traits
 */
TEST(MqttTraitsTest, CompileTimeDecisions)
{
    // Test CoreMqttClient can do manual processing
    if constexpr (has_manual_processing_v<CoreMqttClient>)
    {
        SUCCEED() << "CoreMqttClient supports manual processing (compile-time check)";
    }
    else
    {
        FAIL() << "CoreMqttClient should support manual processing";
    }

    // Test EspMqttClient cannot do manual processing
    if constexpr (has_manual_processing_v<EspMqttClient>)
    {
        FAIL() << "EspMqttClient should NOT support manual processing";
    }
    else
    {
        SUCCEED() << "EspMqttClient does not support manual processing (compile-time check)";
    }
}

/**
 * @brief Test adaptive algorithm selection
 */
template<typename MqttClient>
std::string getProcessingMode()
{
    if constexpr (has_manual_processing_v<MqttClient>)
    {
        return "manual";
    }
    else
    {
        return "async";
    }
}

TEST(MqttTraitsTest, AdaptiveAlgorithm)
{
    // CoreMqttClient can do manual processing
    EXPECT_EQ("manual", getProcessingMode<CoreMqttClient>());

    // EspMqttClient only does async
    EXPECT_EQ("async", getProcessingMode<EspMqttClient>());
}

/**
 * @brief Test that template constraints work
 */
template<typename MqttClient>
void requiresSynchronousClient()
{
    static_assert(is_synchronous_capable_v<MqttClient>,
                  "This function requires a client with manual processing");
}

template<typename MqttClient>
void requiresAsynchronousClient()
{
    static_assert(is_asynchronous_capable_v<MqttClient>,
                  "This function requires a client with async callbacks");
}

TEST(MqttTraitsTest, TemplateConstraints)
{
    // CoreMqttClient works for both
    requiresSynchronousClient<CoreMqttClient>();
    requiresAsynchronousClient<CoreMqttClient>();

    // EspMqttClient only works for async
    // requiresSynchronousClient<EspMqttClient>();  // Would fail to compile!
    requiresAsynchronousClient<EspMqttClient>();

    SUCCEED() << "Template constraints work correctly";
}

// =============================================================================
// Capability Report Tests
// =============================================================================

/**
 * @brief Test capability reporting (compile-time)
 */
TEST(MqttTraitsTest, CoreMqttClientCapabilities)
{
    // Verify CoreMqttClient capabilities
    EXPECT_TRUE(is_mqtt_client_v<CoreMqttClient>);
    EXPECT_TRUE(has_manual_processing_v<CoreMqttClient>);
    EXPECT_TRUE(has_statistics_v<CoreMqttClient>);
    EXPECT_TRUE(supports_qos2_v<CoreMqttClient>);
    EXPECT_TRUE(is_synchronous_capable_v<CoreMqttClient>);
    EXPECT_TRUE(is_asynchronous_capable_v<CoreMqttClient>);
    EXPECT_TRUE(is_production_ready_v<CoreMqttClient>);
}

TEST(MqttTraitsTest, EspMqttClientCapabilities)
{
    // Verify EspMqttClient capabilities
    EXPECT_TRUE(is_mqtt_client_v<EspMqttClient>);
    EXPECT_FALSE(has_manual_processing_v<EspMqttClient>); // Key difference!
    EXPECT_TRUE(has_statistics_v<EspMqttClient>);
    EXPECT_TRUE(supports_qos2_v<EspMqttClient>);
    EXPECT_FALSE(is_synchronous_capable_v<EspMqttClient>); // Cannot do sync!
    EXPECT_TRUE(is_asynchronous_capable_v<EspMqttClient>);
    EXPECT_TRUE(is_production_ready_v<EspMqttClient>);
}

// =============================================================================
// Real-World Usage Example
// =============================================================================

/**
 * @brief Example: Fleet Provisioning that adapts to client capabilities
 */
template<typename MqttClient>
class FleetProvisionerExample
{
public:
    static_assert(is_mqtt_client_v<MqttClient>, "Must be an MQTT client");

    bool provision()
    {
        if constexpr (has_manual_processing_v<MqttClient>)
        {
            // Manual processing path (CoreMQTT)
            return provisionManual();
        }
        else
        {
            // Async callback path (ESP-MQTT)
            return provisionAsync();
        }
    }

private:
    bool provisionManual()
    {
        // Uses processLoop() for synchronous request-response
        return true;
    }

    bool provisionAsync()
    {
        // Uses callbacks + semaphore for async request-response
        return true;
    }
};

TEST(MqttTraitsTest, FleetProvisioningAdaptation)
{
    // CoreMqttClient uses manual processing path
    FleetProvisionerExample<CoreMqttClient> coremqttProvisioner;
    static_assert(has_manual_processing_v<CoreMqttClient>);

    // EspMqttClient uses async callback path
    FleetProvisionerExample<EspMqttClient> espmqttProvisioner;
    static_assert(!has_manual_processing_v<EspMqttClient>);

    SUCCEED() << "Fleet provisioning adapts to client capabilities at compile-time";
}

// =============================================================================
// Invalid Type Tests (Should Not Compile if Uncommented)
// =============================================================================

// Test 1: Non-MQTT class should fail trait checks
struct NotAnMqttClient
{
    void doSomething()
    {
    }
};

static_assert(!is_mqtt_client_v<NotAnMqttClient>, "NotAnMqttClient should not satisfy is_mqtt_client");
static_assert(!has_manual_processing_v<NotAnMqttClient>, "NotAnMqttClient should not have manual processing");
static_assert(!has_statistics_v<NotAnMqttClient>, "NotAnMqttClient should not have statistics");

// Test 2: Attempting to use NotAnMqttClient with MQTT algorithm would fail at compile-time
// template <typename MqttClient>
// void useMqttClient() {
//     static_assert(is_mqtt_client_v<MqttClient>, "Must be an MQTT client");
// }
//
// TEST(MqttTraitsTest, InvalidClientType) {
//     useMqttClient<NotAnMqttClient>();  // Compile error: static_assert failed
// }

// =============================================================================
// Summary
// =============================================================================

/**
 * These tests verify:
 *
 * 1. CoreMqttClient has manual processing (processLoop, setAutoProcessing)
 * 2. EspMqttClient does NOT have manual processing
 * 3. Both satisfy basic MQTT client requirements
 * 4. Both support statistics, QoS 2, async operation
 * 5. Template algorithms can adapt based on capabilities
 * 6. Compile-time errors prevent misuse
 * 7. Zero runtime overhead (all checks at compile-time)
 */
