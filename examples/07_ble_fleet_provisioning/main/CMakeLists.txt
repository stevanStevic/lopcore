idf_component_register(
    SRCS
        "main.cpp"
        "application_state_machine.cpp"
        "states/init_state.cpp"
        "states/configuration_state.cpp"
        "states/nominal_state.cpp"
        "states/degraded_state.cpp"
        "states/factory_reset_state.cpp"
        "provisioning/provisioning_mqtt_adapter.cpp"
        "provisioning/ble_helper.cpp"
        "provisioning/aws_helper.cpp"
    INCLUDE_DIRS "."
    REQUIRES lopcore nvs_flash bt esp_wifi mqtt esp_driver_gpio
)
