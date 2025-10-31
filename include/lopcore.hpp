/**
 * @file lopcore.hpp
 * @brief Main LopCore convenience header - includes all subsystems
 *
 * This header provides a single include point for all LopCore functionality.
 * For smaller binary sizes, include only the specific subsystem headers you need.
 *
 * Usage:
 *   #include "lopcore.hpp"  // Include everything
 *   // OR
 *   #include "lopcore/logging/logger.hpp"  // Include specific subsystem
 */

#pragma once

// ============================================================================
// Logging Subsystem
// ============================================================================
#include "lopcore/logging/console_sink.hpp"
#include "lopcore/logging/file_sink.hpp"
#include "lopcore/logging/log_level.hpp"
#include "lopcore/logging/log_sink.hpp"
#include "lopcore/logging/logger.hpp"

// ============================================================================
// Storage Subsystem
// ============================================================================
#include "lopcore/storage/istorage.hpp"
#include "lopcore/storage/nvs_storage.hpp"
#include "lopcore/storage/spiffs_storage.hpp"
#include "lopcore/storage/storage_factory.hpp"
#include "lopcore/storage/storage_type.hpp"

// ============================================================================
// MQTT Subsystem
// ============================================================================
#include "lopcore/mqtt/esp_mqtt_client.hpp"
#include "lopcore/mqtt/imqtt_client.hpp"
#include "lopcore/mqtt/mqtt_budget.hpp"
#include "lopcore/mqtt/mqtt_config.hpp"
#include "lopcore/mqtt/mqtt_types.hpp"
#include "lopcore/mqtt/mqtt_traits.hpp"
#if LOPCORE_COREMQTT_ENABLED
#include "lopcore/mqtt/coremqtt_client.hpp"
#endif

// ============================================================================
// TLS Subsystem
// ============================================================================
#include "lopcore/tls/mbedtls_transport.hpp"
#include "lopcore/tls/pkcs11_provider.hpp"
#include "lopcore/tls/pkcs11_session.hpp"
#include "lopcore/tls/tls_config.hpp"

// ============================================================================
// State Machine Subsystem
// ============================================================================
#include "lopcore/state_machine/istate.hpp"
#include "lopcore/state_machine/state_machine.hpp"
#include "lopcore/tls/tls_transport.hpp"

// Future subsystems:
// - Provisioning
// - State Machine
// - Commands
