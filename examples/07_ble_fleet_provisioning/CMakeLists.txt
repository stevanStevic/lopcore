cmake_minimum_required(VERSION 3.16)

# Set component paths:
# - "../.." finds lopcore itself
# - "../../components/esp-aws-iot/libraries" finds coreMQTT, corePKCS11, etc.
set(EXTRA_COMPONENT_DIRS "../.." "../../components/esp-aws-iot/libraries")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ble_fleet_provisioning_example)
