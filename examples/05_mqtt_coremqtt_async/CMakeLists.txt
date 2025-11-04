cmake_minimum_required(VERSION 3.16)

# Set component paths
# - "../.." finds lopcore itself
# - "../../components/esp-aws-iot/libraries" finds coreMQTT, corePKCS11, etc.
set(EXTRA_COMPONENT_DIRS "../.." "../../components/esp-aws-iot/libraries")

# Use custom partition table with SPIFFS
set(PARTITION_CSV_PATH "${CMAKE_CURRENT_SOURCE_DIR}/partitions.csv")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mqtt_coremqtt_async_example)
