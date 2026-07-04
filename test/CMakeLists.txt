# LopCore Unit Tests
# Using Google Test framework

cmake_minimum_required(VERSION 3.16)
project(lopcore_tests)

# Enable C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download and build Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Set lopcore base directory (parent of test/)
set(LOPCORE_BASE_DIR ${CMAKE_SOURCE_DIR}/..)

# Include directories - new unified structure
# NOTE: include/lopcore is added so pre-restructure test includes
# (e.g. "mqtt/mqtt_config.hpp") keep resolving alongside the canonical
# "lopcore/mqtt/..." form used by the library itself.
include_directories(
    ${LOPCORE_BASE_DIR}/include
    ${LOPCORE_BASE_DIR}/include/lopcore
    ${CMAKE_SOURCE_DIR}/mocks
)

# Mock includes (ESP-IDF stubs for host testing)
include_directories(
    ${CMAKE_SOURCE_DIR}/mocks/esp32
)

# Test executables
add_executable(test_dummy
    unit/test_dummy.cpp
)

target_link_libraries(test_dummy
    GTest::gtest_main
)

add_executable(test_logger
    unit/test_logger.cpp
    ${LOPCORE_BASE_DIR}/src/logging/logger.cpp
    ${LOPCORE_BASE_DIR}/src/logging/console_sink.cpp
)
target_link_libraries(test_logger GTest::gtest_main)

add_executable(test_file_sink
    unit/test_file_sink.cpp
    ${LOPCORE_BASE_DIR}/src/logging/logger.cpp
    ${LOPCORE_BASE_DIR}/src/logging/console_sink.cpp
    ${LOPCORE_BASE_DIR}/src/logging/file_sink.cpp
)
target_link_libraries(test_file_sink GTest::gtest_main)

add_executable(test_ring_file_sink
    unit/test_ring_file_sink.cpp
    ${LOPCORE_BASE_DIR}/src/logging/ring_file_sink.cpp
    ${LOPCORE_BASE_DIR}/src/storage/ring_file_storage.cpp
)
target_link_libraries(test_ring_file_sink GTest::gtest_main)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(test_dummy)
gtest_discover_tests(test_logger)
gtest_discover_tests(test_file_sink)
gtest_discover_tests(test_ring_file_sink)

add_executable(test_ring_record_file
    unit/test_ring_record_file.cpp
    ${LOPCORE_BASE_DIR}/src/storage/ring_file_storage.cpp
)
target_link_libraries(test_ring_record_file GTest::gtest_main)
gtest_discover_tests(test_ring_record_file)

add_executable(test_littlefs_storage
    unit/test_littlefs_storage.cpp
    ${LOPCORE_BASE_DIR}/src/storage/littlefs_storage.cpp
)
target_link_libraries(test_littlefs_storage GTest::gtest_main pthread)
gtest_discover_tests(test_littlefs_storage)

add_executable(test_spiffs_storage
    unit/test_spiffs_storage.cpp
    ${LOPCORE_BASE_DIR}/src/storage/spiffs_storage.cpp
)
target_link_libraries(test_spiffs_storage GTest::gtest_main pthread)
gtest_discover_tests(test_spiffs_storage)

add_executable(test_nvs_storage
    unit/test_nvs_storage.cpp
    ${LOPCORE_BASE_DIR}/src/storage/nvs_storage.cpp
)
target_link_libraries(test_nvs_storage GTest::gtest_main pthread)
gtest_discover_tests(test_nvs_storage)

add_executable(test_storage_traits
    unit/storage/test_storage_traits.cpp
    ${LOPCORE_BASE_DIR}/src/storage/spiffs_storage.cpp
    ${LOPCORE_BASE_DIR}/src/storage/nvs_storage.cpp
)
target_link_libraries(test_storage_traits GTest::gtest_main pthread)
gtest_discover_tests(test_storage_traits)

add_executable(test_state_machine
    unit/state_machine/test_state_machine.cpp
)
target_link_libraries(test_state_machine GTest::gtest_main pthread)
gtest_discover_tests(test_state_machine)

add_executable(test_mqtt_types
    unit/mqtt/test_mqtt_types.cpp
)
target_link_libraries(test_mqtt_types GTest::gtest_main)
gtest_discover_tests(test_mqtt_types)

add_executable(test_mqtt_config
    unit/mqtt/test_mqtt_config.cpp
)
target_link_libraries(test_mqtt_config GTest::gtest_main)
gtest_discover_tests(test_mqtt_config)

add_executable(test_mqtt_budget
    unit/mqtt/test_mqtt_budget.cpp
)
target_link_libraries(test_mqtt_budget GTest::gtest_main pthread)
gtest_discover_tests(test_mqtt_budget)

add_executable(test_esp_mqtt_client
    unit/mqtt/test_esp_mqtt_client.cpp
)
target_link_libraries(test_esp_mqtt_client GTest::gtest_main)
gtest_discover_tests(test_esp_mqtt_client)

add_executable(test_coremqtt_client_simple
    unit/mqtt/test_coremqtt_client_simple.cpp
    ${LOPCORE_BASE_DIR}/src/mqtt/coremqtt_client.cpp
    ${LOPCORE_BASE_DIR}/src/mqtt/mqtt_budget.cpp
    ${LOPCORE_BASE_DIR}/src/logging/logger.cpp
    ${LOPCORE_BASE_DIR}/src/logging/console_sink.cpp
)
target_link_libraries(test_coremqtt_client_simple GTest::gtest_main pthread)
gtest_discover_tests(test_coremqtt_client_simple)

# Regression tests for the processLoop callback re-entrancy deadlock.
# A regression re-introducing the deadlock hangs; the TIMEOUT catches it.
add_executable(test_coremqtt_client_reentrancy
    unit/mqtt/test_coremqtt_client_reentrancy.cpp
    ${LOPCORE_BASE_DIR}/src/mqtt/coremqtt_client.cpp
    ${LOPCORE_BASE_DIR}/src/mqtt/mqtt_budget.cpp
    ${LOPCORE_BASE_DIR}/src/logging/logger.cpp
    ${LOPCORE_BASE_DIR}/src/logging/queued_sink.cpp
)
target_link_libraries(test_coremqtt_client_reentrancy GTest::gtest_main pthread)
gtest_discover_tests(test_coremqtt_client_reentrancy PROPERTIES TIMEOUT 60)

# Asynchronous (queued) log sink decorator
add_executable(test_queued_sink
    unit/test_queued_sink.cpp
    ${LOPCORE_BASE_DIR}/src/logging/queued_sink.cpp
)
target_link_libraries(test_queued_sink GTest::gtest_main pthread)
gtest_discover_tests(test_queued_sink PROPERTIES TIMEOUT 60)

# Phase 4: CoreMQTT client tests (full - disabled until ESP-IDF integration)
#add_executable(test_coremqtt_client
#    unit/mqtt/test_coremqtt_client.cpp
#)
#target_link_libraries(test_coremqtt_client GTest::gtest_main)
#gtest_discover_tests(test_coremqtt_client)

# Phase 4: MQTT Client Factory tests - REMOVED
# Factory pattern replaced with direct construction of CoreMqttClient/EspMqttClient
# See mqtt_traits.hpp for capability-based template programming

# Mock TLS Transport tests (host-compatible)
add_executable(test_mock_tls_transport
    unit/tls/test_mock_tls_transport.cpp
)
target_link_libraries(test_mock_tls_transport GTest::gtest_main pthread)
gtest_discover_tests(test_mock_tls_transport)

# TLS Config validation tests (host-compatible)
add_executable(test_tls_config
    unit/tls/test_tls_config.cpp
    ${LOPCORE_BASE_DIR}/src/logging/logger.cpp
    ${LOPCORE_BASE_DIR}/src/logging/console_sink.cpp
)
target_link_libraries(test_tls_config GTest::gtest_main pthread)
gtest_discover_tests(test_tls_config)

# MQTT Operations Tests (publish/subscribe/unsubscribe logic)
add_executable(test_mqtt_operations
    unit/mqtt/test_mqtt_operations.cpp
)

target_include_directories(test_mqtt_operations PRIVATE
    ${LOPCORE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)

target_link_libraries(test_mqtt_operations PRIVATE
    gtest
    gtest_main
)

gtest_discover_tests(test_mqtt_operations)

# Standalone MQTT Clients Tests (Trait-based compile-time tests)
# Note: Full client tests require ESP-IDF environment
add_executable(test_mqtt_clients_standalone
    unit/mqtt/test_mqtt_clients_standalone.cpp
    ${LOPCORE_BASE_DIR}/src/logging/logger.cpp
    ${LOPCORE_BASE_DIR}/src/logging/console_sink.cpp
)

target_include_directories(test_mqtt_clients_standalone PRIVATE
    ${LOPCORE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)

target_link_libraries(test_mqtt_clients_standalone PRIVATE
    gtest
    gtest_main
    pthread
)

gtest_discover_tests(test_mqtt_clients_standalone)

# Add more tests as components are implemented...

# ===========================================================================
# Provisioning Module Tests (§4.1)
# ===========================================================================

# ---- mbedTLS (required by CertificateManager) --------------------------------
# Try system library first; if not found, fall back to FetchContent
find_library(MBEDTLS_LIB  mbedtls  NAMES mbedtls)
find_library(MBEDCRYPTO_LIB mbedcrypto NAMES mbedcrypto)
find_library(MBEDX509_LIB  mbedx509   NAMES mbedx509)
find_path(MBEDTLS_INCLUDE_DIR mbedtls/ssl.h)

if(MBEDTLS_LIB AND MBEDCRYPTO_LIB AND MBEDX509_LIB AND MBEDTLS_INCLUDE_DIR)
    message(STATUS "Using system mbedTLS: ${MBEDTLS_LIB}")
    add_library(mbedtls_host INTERFACE)
    target_include_directories(mbedtls_host INTERFACE ${MBEDTLS_INCLUDE_DIR})
    target_link_libraries(mbedtls_host INTERFACE ${MBEDTLS_LIB} ${MBEDCRYPTO_LIB} ${MBEDX509_LIB})
else()
    message(STATUS "System mbedTLS not found — fetching via FetchContent")
    FetchContent_Declare(
        mbedtls
        GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
        GIT_TAG        v3.6.0
        GIT_SHALLOW    TRUE
    )
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(ENABLE_TESTING  OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(mbedtls)
    add_library(mbedtls_host INTERFACE)
    target_link_libraries(mbedtls_host INTERFACE mbedtls mbedcrypto mbedx509)
endif()

# ---- cJSON (required by AwsDataEndpointHandler) ------------------------------
find_library(CJSON_LIB cjson NAMES cjson libcjson)
find_path(CJSON_INCLUDE_DIR cJSON.h PATH_SUFFIXES cjson)

if(CJSON_LIB AND CJSON_INCLUDE_DIR)
    message(STATUS "Using system cJSON: ${CJSON_LIB}")
    add_library(cjson_host INTERFACE)
    target_include_directories(cjson_host INTERFACE ${CJSON_INCLUDE_DIR})
    target_link_libraries(cjson_host INTERFACE ${CJSON_LIB})
else()
    message(STATUS "System cJSON not found — fetching via FetchContent")
    FetchContent_Declare(
        cjson
        GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
        GIT_TAG        v1.7.17
        GIT_SHALLOW    TRUE
    )
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(cjson)
    add_library(cjson_host INTERFACE)
    target_link_libraries(cjson_host INTERFACE cjson)
endif()

# ---- Common prov include directories -----------------------------------------
set(PROV_MOCK_DIRS
    ${CMAKE_SOURCE_DIR}/mocks/esp32
    ${CMAKE_SOURCE_DIR}/mocks/pkcs11
)

# ---- test_ble_data_framer ----------------------------------------------------
add_executable(test_ble_data_framer
    unit/prov/test_ble_data_framer.cpp
    ${LOPCORE_BASE_DIR}/src/prov/ble_data_framer.cpp
    ${LOPCORE_BASE_DIR}/src/logging/logger.cpp
    ${LOPCORE_BASE_DIR}/src/logging/console_sink.cpp
)
target_include_directories(test_ble_data_framer PRIVATE
    ${LOPCORE_BASE_DIR}/include
    ${PROV_MOCK_DIRS}
)
target_link_libraries(test_ble_data_framer GTest::gtest_main pthread)
gtest_discover_tests(test_ble_data_framer)

# ---- test_certificate_manager -----------------------------------------------
add_executable(test_certificate_manager
    unit/prov/test_certificate_manager.cpp
    ${LOPCORE_BASE_DIR}/src/prov/certificate_manager.cpp
    ${LOPCORE_BASE_DIR}/src/logging/logger.cpp
    ${LOPCORE_BASE_DIR}/src/logging/console_sink.cpp
)
target_include_directories(test_certificate_manager PRIVATE
    ${LOPCORE_BASE_DIR}/include
    ${PROV_MOCK_DIRS}
)
# PKCS#11 path is compiled only when CONFIG_LOPCORE_PROV_CERT_COREP11 is set;
# host tests use the mbedTLS SW path only (usePkcs11 = false).
target_link_libraries(test_certificate_manager GTest::gtest_main pthread mbedtls_host)
gtest_discover_tests(test_certificate_manager)

# ---- test_provisioning_config -----------------------------------------------
# Header-only test — no implementation source files needed
add_executable(test_provisioning_config
    unit/prov/test_provisioning_config.cpp
)
target_include_directories(test_provisioning_config PRIVATE
    ${LOPCORE_BASE_DIR}/include
    ${PROV_MOCK_DIRS}
)
target_link_libraries(test_provisioning_config GTest::gtest_main)
gtest_discover_tests(test_provisioning_config)

# ---- test_aws_data_endpoint_handler -----------------------------------------
add_executable(test_aws_data_endpoint_handler
    unit/prov/test_aws_data_endpoint_handler.cpp
    ${LOPCORE_BASE_DIR}/src/prov/aws_data_endpoint_handler.cpp
    ${LOPCORE_BASE_DIR}/src/logging/logger.cpp
    ${LOPCORE_BASE_DIR}/src/logging/console_sink.cpp
)
target_include_directories(test_aws_data_endpoint_handler PRIVATE
    ${LOPCORE_BASE_DIR}/include
    ${PROV_MOCK_DIRS}
)
target_link_libraries(test_aws_data_endpoint_handler GTest::gtest_main pthread cjson_host)
gtest_discover_tests(test_aws_data_endpoint_handler)

# ---- test_fleet_provisioning_serializer -------------------------------------
# The serializer test is guarded by CONFIG_LOPCORE_PROV_AWS_CBOR at the C++ level.
# Enable CBOR tests by passing -DLOP_PROV_CBOR_TESTS=ON to cmake (requires tinyCBOR).
option(LOP_PROV_CBOR_TESTS "Enable CBOR-path tests for FleetProvisioningSerializer" OFF)

if(LOP_PROV_CBOR_TESTS)
    FetchContent_Declare(
        tinycbor
        GIT_REPOSITORY https://github.com/intel/tinycbor.git
        GIT_TAG        v0.6.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(tinycbor)
    if(NOT tinycbor_POPULATED)
        FetchContent_Populate(tinycbor)
        # tinyCBOR has no CMakeLists; build source directly
        file(GLOB TINYCBOR_SRCS "${tinycbor_SOURCE_DIR}/src/*.c")
        add_library(tinycbor_lib STATIC ${TINYCBOR_SRCS})
        target_include_directories(tinycbor_lib PUBLIC ${tinycbor_SOURCE_DIR}/src)
    endif()

    add_executable(test_fleet_provisioning_serializer
        unit/prov/test_fleet_provisioning_serializer.cpp
        ${LOPCORE_BASE_DIR}/src/prov/fleet_provisioning_serializer.cpp
        ${LOPCORE_BASE_DIR}/src/logging/logger.cpp
        ${LOPCORE_BASE_DIR}/src/logging/console_sink.cpp
    )
    target_include_directories(test_fleet_provisioning_serializer PRIVATE
        ${LOPCORE_BASE_DIR}/include
        ${PROV_MOCK_DIRS}
    )
    target_compile_definitions(test_fleet_provisioning_serializer PRIVATE
        CONFIG_LOPCORE_PROV_AWS_CBOR=1
    )
    target_link_libraries(test_fleet_provisioning_serializer
        GTest::gtest_main pthread tinycbor_lib
    )
    gtest_discover_tests(test_fleet_provisioning_serializer)
else()
    # Stub-only build: no CBOR define set, only the placeholder test compiles
    add_executable(test_fleet_provisioning_serializer
        unit/prov/test_fleet_provisioning_serializer.cpp
    )
    target_include_directories(test_fleet_provisioning_serializer PRIVATE
        ${LOPCORE_BASE_DIR}/include
        ${PROV_MOCK_DIRS}
    )
    target_link_libraries(test_fleet_provisioning_serializer GTest::gtest_main)
    gtest_discover_tests(test_fleet_provisioning_serializer)
endif()

# Print test summary
message(STATUS "")
message(STATUS "LopCore Tests Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Google Test: v1.14.0")
message(STATUS "")
message(STATUS "To build and run tests:")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake ..")
message(STATUS "  make")
message(STATUS "  ctest --verbose")
message(STATUS "")
