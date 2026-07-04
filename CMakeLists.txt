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
    "src/logging/ring_file_sink.cpp"
    "src/logging/queued_sink.cpp"
    "src/storage/ring_file_storage.cpp"

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

    # Provisioning subsystem (conditional - enabled via Kconfig)
    # Will be removed from build if CONFIG_LOPCORE_ENABLE_PROVISIONING is not set
    "src/prov/storage_helpers.cpp"
    "src/prov/certificate_manager.cpp"
    "src/prov/wifi_provisioning.cpp"
    "src/prov/fleet_provisioning_serializer.cpp"
    "src/prov/ble_data_framer.cpp"
    "src/prov/aws_data_endpoint_handler.cpp"

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
    wifi_provisioning  # For BLE/WiFi provisioning
    protocomm          # Protocol communication (used by wifi_provisioning)
    # Note: coreMQTT and corePKCS11 are optional and added dynamically
    # if esp-aws-iot is available (see below)
    # Note: esp_littlefs is optional and added dynamically if available
)

if(CONFIG_LOPCORE_PROV_TRANSPORT_BLE)
    list(APPEND LOPCORE_REQUIRES bt)
endif()

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
# Provisioning Subsystem Detection
# =============================================================================

# Check if provisioning is enabled in Kconfig
if(NOT CONFIG_LOPCORE_ENABLE_PROVISIONING)
    message(STATUS "LopCore: Provisioning DISABLED (not enabled in Kconfig)")
    # Remove all provisioning sources
    list(REMOVE_ITEM LOPCORE_SRCS
        "src/prov/storage_helpers.cpp"
        "src/prov/certificate_manager.cpp"
        "src/prov/wifi_provisioning.cpp"
        "src/prov/fleet_provisioning_serializer.cpp"
        "src/prov/ble_data_framer.cpp"
        "src/prov/aws_data_endpoint_handler.cpp"
    )
else()
    message(STATUS "LopCore: Provisioning ENABLED")

    # Check for CBOR component (for Fleet Provisioning serialization)
    if(CONFIG_LOPCORE_PROV_AWS_CBOR)
        if("espressif__cbor" IN_LIST build_components OR "cbor" IN_LIST build_components)
            message(STATUS "LopCore: CBOR available for Fleet Provisioning")
            if("espressif__cbor" IN_LIST build_components)
                list(APPEND LOPCORE_REQUIRES espressif__cbor)
            else()
                list(APPEND LOPCORE_REQUIRES cbor)
            endif()
        else()
            message(WARNING "LopCore: CBOR not found, Fleet Provisioning will use JSON instead")
        endif()
    endif()

    # Add Fleet Provisioning SDK when AWS fleet provisioning is enabled
    if(CONFIG_LOPCORE_PROV_AWS_FLEET AND ESP_AWS_IOT_AVAILABLE)
        message(STATUS "LopCore: AWS Fleet Provisioning enabled")
        # Fleet Provisioning library will be added to includes later
    endif()
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
            "${ESP_AWS_IOT_PATH}/libraries/common/posix_compat/clock_esp.c"
            "${ESP_AWS_IOT_PATH}/libraries/common/posix_compat/semaphore.c"
        )

        # Public coreMQTT include directories - consumers of lopcore's public headers
        # (e.g. coremqtt_client.hpp) need core_mqtt.h, transport_interface.h, and the
        # lopcore-supplied core_mqtt_config.h that core_mqtt.h pulls in transitively.
        target_include_directories(${COMPONENT_LIB} PUBLIC
            "${ESP_AWS_IOT_PATH}/libraries/coreMQTT/coreMQTT/source/include"
            "${ESP_AWS_IOT_PATH}/libraries/coreMQTT/coreMQTT/source/interface"
            "${ESP_AWS_IOT_PATH}/libraries/coreMQTT/config"
        )

        # Public corePKCS11 include directories - consumers of lopcore public headers
        # (e.g. mbedtls_transport.hpp) transitively include core_pkcs11.h and the
        # PKCS#11 type definitions, so these must be PUBLIC.
        target_include_directories(${COMPONENT_LIB} PUBLIC
            "${ESP_AWS_IOT_PATH}/libraries/corePKCS11/corePKCS11/source/include"
            "${ESP_AWS_IOT_PATH}/libraries/corePKCS11/corePKCS11/source/dependency/3rdparty/pkcs11/published/2-40-errata-1"
            "${ESP_AWS_IOT_PATH}/libraries/corePKCS11/config"
        )

        # Private implementation-only include directories - not needed by consumers.
        target_include_directories(${COMPONENT_LIB} PRIVATE
            "${ESP_AWS_IOT_PATH}/libraries/corePKCS11/corePKCS11/source/dependency/3rdparty/mbedtls_utils"
            "${ESP_AWS_IOT_PATH}/libraries/common/posix_compat"
            "${ESP_AWS_IOT_PATH}/libraries/backoffAlgorithm/backoffAlgorithm/source/include"
        )

        # Enable CoreMQTT support
        target_compile_definitions(${COMPONENT_LIB} PUBLIC
            LOPCORE_COREMQTT_ENABLED=1
        )

        # Add Fleet Provisioning SDK sources and includes if provisioning enabled
        if(CONFIG_LOPCORE_PROV_AWS_FLEET)
            target_sources(${COMPONENT_LIB} PRIVATE
                "${ESP_AWS_IOT_PATH}/libraries/Fleet-Provisioning-for-AWS-IoT-embedded-sdk/Fleet-Provisioning-for-AWS-IoT-embedded-sdk/source/fleet_provisioning.c"
            )

            target_include_directories(${COMPONENT_LIB} PRIVATE
                "${ESP_AWS_IOT_PATH}/libraries/Fleet-Provisioning-for-AWS-IoT-embedded-sdk/Fleet-Provisioning-for-AWS-IoT-embedded-sdk/source/include"
                "${ESP_AWS_IOT_PATH}/libraries/Fleet-Provisioning-for-AWS-IoT-embedded-sdk/config"
            )

            target_compile_definitions(${COMPONENT_LIB} PUBLIC
                LOPCORE_PROV_AWS_FLEET_ENABLED=1
            )

            message(STATUS "LopCore: Fleet Provisioning SDK integrated")
        endif()

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
# Provisioning System Compile Definitions
# =============================================================================

if(CONFIG_LOPCORE_ENABLE_PROVISIONING)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC
        LOPCORE_PROVISIONING_ENABLED=1
    )

    # Certificate backend selection
    if(CONFIG_LOPCORE_PROV_CERT_COREP11)
        target_compile_definitions(${COMPONENT_LIB} PUBLIC
            LOPCORE_PROV_CERT_COREP11_ENABLED=1
        )
    elseif(CONFIG_LOPCORE_PROV_CERT_MBEDTLS_ONLY)
        target_compile_definitions(${COMPONENT_LIB} PUBLIC
            LOPCORE_PROV_CERT_MBEDTLS_ONLY_ENABLED=1
        )
    endif()
endif()

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
if(ESP_AWS_IOT_AVAILABLE)
    message(STATUS "    - ESP-MQTT:   ENABLED")
    message(STATUS "    - CoreMQTT:   ENABLED (via esp-aws-iot)")
else()
    message(STATUS "    - ESP-MQTT:   ENABLED")
    message(STATUS "    - CoreMQTT:   DISABLED (esp-aws-iot not found)")
endif()
if(CONFIG_LOPCORE_ENABLE_PROVISIONING)
    message(STATUS "  Provisioning:   ENABLED")
    if(CONFIG_LOPCORE_PROV_AWS_FLEET)
        message(STATUS "    - AWS Fleet:  ENABLED")
        if(CONFIG_LOPCORE_PROV_AWS_CBOR)
            message(STATUS "      Payload:    CBOR")
        else()
            message(STATUS "      Payload:    JSON")
        endif()
    endif()
    if(CONFIG_LOPCORE_PROV_CERT_COREP11)
        message(STATUS "    - Cert Mgmt:  corePKCS11")
    elseif(CONFIG_LOPCORE_PROV_CERT_MBEDTLS_ONLY)
        message(STATUS "    - Cert Mgmt:  mbedTLS only")
    endif()
else()
    message(STATUS "  Provisioning:   DISABLED")
endif()
message(STATUS "  State Machine:  DISABLED (future)")
message(STATUS "  Commands:       DISABLED (future)")
message(STATUS "========================================")
