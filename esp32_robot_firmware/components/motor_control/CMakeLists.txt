idf_component_register(
    SRCS "motor_control.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_gpio esp_driver_ledc
)
 