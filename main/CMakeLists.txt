idf_component_register(SRCS "desktop_controller.c" "fan_control/fan_control.c" "keyswitches/keyswitches.c" "oled_screen/oled_screen.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES driver ssd1306 esp_adc)
