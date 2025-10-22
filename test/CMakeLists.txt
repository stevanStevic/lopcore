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
include_directories(
    ${LOPCORE_BASE_DIR}/include
    ${LOPCORE_BASE_DIR}/provisioning/include
    ${LOPCORE_BASE_DIR}/state_machine/include
    ${LOPCORE_BASE_DIR}/commands/include
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

# Week 2: Logging tests ✅ ACTIVE
add_executable(test_logger
    unit/test_logger.cpp
    ${LOPCORE_BASE_DIR}/src/logging/logger.cpp
    ${LOPCORE_BASE_DIR}/src/logging/console_sink.cpp
)
target_link_libraries(test_logger GTest::gtest_main)

# Week 2: FileSink tests ✅ ACTIVE
add_executable(test_file_sink
    unit/test_file_sink.cpp
    ${LOPCORE_BASE_DIR}/src/logging/logger.cpp
    ${LOPCORE_BASE_DIR}/src/logging/console_sink.cpp
    ${LOPCORE_BASE_DIR}/src/logging/file_sink.cpp
)
target_link_libraries(test_file_sink GTest::gtest_main)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(test_dummy)
gtest_discover_tests(test_logger)
gtest_discover_tests(test_file_sink)

# Week 3: Storage tests ✅ ACTIVE
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

add_executable(test_storage_factory
    unit/test_storage_factory.cpp
    ${LOPCORE_BASE_DIR}/src/storage/storage_factory.cpp
    ${LOPCORE_BASE_DIR}/src/storage/spiffs_storage.cpp
    ${LOPCORE_BASE_DIR}/src/storage/nvs_storage.cpp
)
target_link_libraries(test_storage_factory GTest::gtest_main pthread)
gtest_discover_tests(test_storage_factory)

# Week 4: MQTT tests ✅ ACTIVE
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

# Week 4 - Phase 4: CoreMQTT client tests (simplified with mocks) ✅ NEW
add_executable(test_coremqtt_client_simple
    unit/mqtt/test_coremqtt_client_simple.cpp
    ${LOPCORE_BASE_DIR}/src/mqtt/coremqtt_client.cpp
    ${LOPCORE_BASE_DIR}/src/mqtt/mqtt_budget.cpp
    ${LOPCORE_BASE_DIR}/src/logging/logger.cpp
    ${LOPCORE_BASE_DIR}/src/logging/console_sink.cpp
)
target_link_libraries(test_coremqtt_client_simple GTest::gtest_main pthread)
gtest_discover_tests(test_coremqtt_client_simple)

# Week 4 - Phase 4: CoreMQTT client tests (full - disabled until ESP-IDF integration)
#add_executable(test_coremqtt_client
#    unit/mqtt/test_coremqtt_client.cpp
#)
#target_link_libraries(test_coremqtt_client GTest::gtest_main)
#gtest_discover_tests(test_coremqtt_client)

# Week 4 - Phase 4: MQTT Client Factory tests ✅ NEW
# NOTE: This test requires ESP-IDF environment to run (ESP-MQTT, mbedtls dependencies)
# Run in ESP-IDF environment or add full ESP-IDF mocks
add_executable(test_mqtt_client_factory
    unit/mqtt/test_mqtt_client_factory.cpp
    ${LOPCORE_BASE_DIR}/src/mqtt/mqtt_client_factory.cpp
)
target_link_libraries(test_mqtt_client_factory GTest::gtest_main pthread)
gtest_discover_tests(test_mqtt_client_factory)

# Mock TLS Transport tests (host-compatible)
add_executable(test_mock_tls_transport
    unit/tls/test_mock_tls_transport.cpp
)
target_link_libraries(test_mock_tls_transport GTest::gtest_main pthread)
gtest_discover_tests(test_mock_tls_transport)

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

# Add more tests as components are implemented...

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
