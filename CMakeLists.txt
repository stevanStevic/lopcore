# LopCore - ESP32 C++ Middleware Framework
# Reusable middleware for ESP32 IoT projects with AWS IoT support

set(LOPCORE_VERSION_MAJOR 1)
set(LOPCORE_VERSION_MINOR 0)
set(LOPCORE_VERSION_PATCH 0)

# Component directory (works when standalone or as subcomponent)
set(LOPCORE_DIR ${CMAKE_CURRENT_LIST_DIR})

# =============================================================================
# Source Files
# =============================================================================

# New unified src/ structure
set(LOPCORE_SRCS
    # Logging subsystem
    "src/logging/logger.cpp"
    "src/logging/console_sink.cpp"
    "src/logging/file_sink.cpp"

    # Storage subsystem
    "src/storage/spiffs_storage.cpp"
    "src/storage/nvs_storage.cpp"
    "src/storage/sdcard_storage.cpp"
    "src/storage/littlefs_storage.cpp"

    # TLS subsystem
    "src/tls/c_wrappers/mbedtls_pkcs11_posix.c"
    "src/tls/pkcs11_provider.cpp"
    "src/tls/pkcs11_session.cpp"
    "src/tls/mbedtls_transport.cpp"

    # MQTT subsystem
    "src/mqtt/mqtt_budget.cpp"
    "src/mqtt/esp_mqtt_client.cpp"
    "src/mqtt/coremqtt_client.cpp"

    # State Machine subsystem (header-only template)
    # No .cpp files needed - template implementation in headers

    # Provisioning (Future)
    # "src/provisioning/provisioning_manager.cpp"
    # "src/provisioning/ble_provisioner.cpp"
    # "src/provisioning/wifi_provisioner.cpp"
    # "src/provisioning/aws_provision_handler.cpp"

    # Commands (Future)
    # "src/commands/command_router.cpp"
)

# =============================================================================
# Include Directories (Public API)
# =============================================================================

# Unified include structure: include/lopcore/*
# Usage: #include "lopcore/logging/logger.hpp"
set(LOPCORE_INCLUDES
    "include"
)

# =============================================================================
# Component Requirements
# =============================================================================

# Base requirements (always needed)
set(LOPCORE_REQUIRES
    nvs_flash
    spiffs
    fatfs         # For SD card storage
    esp_wifi
    esp_event
    mqtt          # ESP-IDF native MQTT (for EspMqttClient)
    mbedtls
    json
    # Note: coreMQTT and corePKCS11 are optional and added dynamically
    # if esp-aws-iot is available (see below)
    # Note: esp_littlefs is optional and added dynamically if available
    # bt          # Optional: for BLE provisioning
)

# =============================================================================
# esp-aws-iot Detection (BEFORE component registration)
# =============================================================================

# Detect if esp-aws-iot component libraries are available
# We need to check this BEFORE idf_component_register to conditionally
# add coreMQTT/corePKCS11 to requirements
idf_build_get_property(build_components BUILD_COMPONENTS)

# Check for coreMQTT (indicates esp-aws-iot is available)
set(ESP_AWS_IOT_AVAILABLE FALSE)

if("coreMQTT" IN_LIST build_components OR "corePKCS11" IN_LIST build_components)
    set(ESP_AWS_IOT_AVAILABLE TRUE)
    message(STATUS "LopCore: esp-aws-iot available, adding coreMQTT/corePKCS11/backoffAlgorithm to requirements")
    list(APPEND LOPCORE_REQUIRES coreMQTT corePKCS11 backoffAlgorithm)
else()
    message(STATUS "LopCore: esp-aws-iot NOT available, building without CoreMQTT support")
    # Remove sources that depend on esp-aws-iot libraries
    list(REMOVE_ITEM LOPCORE_SRCS
        "src/mqtt/coremqtt_client.cpp"
        "src/tls/pkcs11_provider.cpp"
        "src/tls/pkcs11_session.cpp"
        "src/tls/mbedtls_transport.cpp"
        "src/tls/c_wrappers/mbedtls_pkcs11_posix.c"
    )
endif()

# Check for esp_littlefs (for LittleFS storage)
set(ESP_LITTLEFS_AVAILABLE FALSE)

if("esp_littlefs" IN_LIST build_components OR "joltwallet__littlefs" IN_LIST build_components)
    set(ESP_LITTLEFS_AVAILABLE TRUE)
    message(STATUS "LopCore: LittleFS component available, adding to requirements")
    # Add whichever component name is found
    if("esp_littlefs" IN_LIST build_components)
        list(APPEND LOPCORE_REQUIRES esp_littlefs)
    else()
        list(APPEND LOPCORE_REQUIRES joltwallet__littlefs)
    endif()
else()
    message(STATUS "LopCore: LittleFS NOT available, building without LittleFS storage support")
    # Remove LittleFS storage source
    list(REMOVE_ITEM LOPCORE_SRCS
        "src/storage/littlefs_storage.cpp"
    )
endif()

# =============================================================================
# Component Registration
# =============================================================================

idf_component_register(
    SRCS ${LOPCORE_SRCS}
    INCLUDE_DIRS ${LOPCORE_INCLUDES}
    REQUIRES ${LOPCORE_REQUIRES}
)

# =============================================================================
# esp-aws-iot Integration (for CoreMQTT and PKCS#11)
# =============================================================================

# Only configure esp-aws-iot if it's available
if(ESP_AWS_IOT_AVAILABLE)
    # Find the esp-aws-iot base path
    set(ESP_AWS_IOT_PATH "")

    # Try common locations
    if(EXISTS "${CMAKE_SOURCE_DIR}/components/esp-aws-iot")
        set(ESP_AWS_IOT_PATH "${CMAKE_SOURCE_DIR}/components/esp-aws-iot")
    elseif(EXISTS "${LOPCORE_DIR}/../esp-aws-iot")
        set(ESP_AWS_IOT_PATH "${LOPCORE_DIR}/../esp-aws-iot")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../esp-aws-iot")
        set(ESP_AWS_IOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../esp-aws-iot")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/components/esp-aws-iot")
        set(ESP_AWS_IOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/components/esp-aws-iot")
    endif()

    if(ESP_AWS_IOT_PATH)
        message(STATUS "LopCore: esp-aws-iot path: ${ESP_AWS_IOT_PATH}")

        # Add coreMQTT sources from esp-aws-iot
        target_sources(${COMPONENT_LIB} PRIVATE
            "${ESP_AWS_IOT_PATH}/libraries/coreMQTT/coreMQTT/source/core_mqtt.c"
            "${ESP_AWS_IOT_PATH}/libraries/coreMQTT/coreMQTT/source/core_mqtt_state.c"
            "${ESP_AWS_IOT_PATH}/libraries/coreMQTT/coreMQTT/source/core_mqtt_serializer.c"
            "${ESP_AWS_IOT_PATH}/libraries/backoffAlgorithm/backoffAlgorithm/source/backoff_algorithm.c"
        )

        # Add esp-aws-iot include directories for coreMQTT and corePKCS11
        target_include_directories(${COMPONENT_LIB} PRIVATE
            "${ESP_AWS_IOT_PATH}/libraries/coreMQTT/coreMQTT/source/include"
            "${ESP_AWS_IOT_PATH}/libraries/coreMQTT/coreMQTT/source/interface"
            "${ESP_AWS_IOT_PATH}/libraries/coreMQTT/config"
            "${ESP_AWS_IOT_PATH}/libraries/corePKCS11/corePKCS11/source/include"
            "${ESP_AWS_IOT_PATH}/libraries/corePKCS11/corePKCS11/source/dependency/3rdparty/pkcs11/published/2-40-errata-1"
            "${ESP_AWS_IOT_PATH}/libraries/corePKCS11/config"
            "${ESP_AWS_IOT_PATH}/libraries/common/posix_compat"
            "${ESP_AWS_IOT_PATH}/libraries/backoffAlgorithm/backoffAlgorithm/source/include"
        )

        # Enable CoreMQTT support
        target_compile_definitions(${COMPONENT_LIB} PUBLIC
            LOPCORE_COREMQTT_ENABLED=1
        )

        message(STATUS "LopCore: CoreMQTT client enabled via esp-aws-iot")
    else()
        message(WARNING "LopCore: esp-aws-iot path not found. CoreMQTT disabled.")
        target_compile_definitions(${COMPONENT_LIB} PUBLIC
            LOPCORE_COREMQTT_ENABLED=0
        )
    endif()
else()
    message(STATUS "LopCore: Building without CoreMQTT support (esp-aws-iot not available)")
    target_compile_definitions(${COMPONENT_LIB} PUBLIC
        LOPCORE_COREMQTT_ENABLED=0
    )
endif()

# =============================================================================
# C++ Standard and Compiler Options
# =============================================================================

# Enable C++17 standard (required for std::optional, etc.)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

# Compiler warnings
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Werror
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-field-initializers>
)

# =============================================================================
# Version Information
# =============================================================================

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    LOPCORE_VERSION_MAJOR=${LOPCORE_VERSION_MAJOR}
    LOPCORE_VERSION_MINOR=${LOPCORE_VERSION_MINOR}
    LOPCORE_VERSION_PATCH=${LOPCORE_VERSION_PATCH}
    LOPCORE_VERSION="${LOPCORE_VERSION_MAJOR}.${LOPCORE_VERSION_MINOR}.${LOPCORE_VERSION_PATCH}"
)

# =============================================================================
# Build Summary
# =============================================================================

message(STATUS "========================================")
message(STATUS "LopCore v${LOPCORE_VERSION_MAJOR}.${LOPCORE_VERSION_MINOR}.${LOPCORE_VERSION_PATCH} Configuration")
message(STATUS "----------------------------------------")
message(STATUS "  Logging:        ENABLED")
message(STATUS "  Storage:        ENABLED")
message(STATUS "  TLS:            ENABLED")
message(STATUS "  MQTT:           ENABLED")
if(ESP_AWS_IOT_FOUND)
    message(STATUS "    - ESP-MQTT:   ENABLED")
    message(STATUS "    - CoreMQTT:   ENABLED (via esp-aws-iot)")
else()
    message(STATUS "    - ESP-MQTT:   ENABLED")
    message(STATUS "    - CoreMQTT:   DISABLED (esp-aws-iot not found)")
endif()
message(STATUS "  Provisioning:   DISABLED (future)")
message(STATUS "  State Machine:  DISABLED (future)")
message(STATUS "  Commands:       DISABLED (future)")
message(STATUS "========================================")
