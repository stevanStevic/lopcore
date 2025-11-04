cmake_minimum_required(VERSION 3.16)

# Set component paths
# - "../.." finds lopcore itself
# - "../../components/esp-aws-iot/libraries" finds coreMQTT, corePKCS11, etc.
# set(EXTRA_COMPONENT_DIRS "../.." "../../components/esp-aws-iot/libraries")
# Set the components to include
set(EXTRA_COMPONENT_DIRS "../.." "../../components/esp-aws-iot/libraries")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mqtt_esp_client_example)
