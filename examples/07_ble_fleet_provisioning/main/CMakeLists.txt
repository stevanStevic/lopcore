idf_component_register(
    SRCS
        "main.cpp"
        "application_state_machine.cpp"
        "init_state.cpp"
        "configuration_state.cpp"
        "nominal_state.cpp"
        "factory_reset_state.cpp"
        "provisioning_helpers.cpp"
    INCLUDE_DIRS "."
    REQUIRES lopcore nvs_flash bt esp_wifi mqtt
)
