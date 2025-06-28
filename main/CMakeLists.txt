idf_component_register(SRCS "desktop_controller.c" "fan_control/fan_control.c" "keyswitches/keyswitches.c" "oled_screen/oled_screen.c" "wifi_connection/wifi_connection.c" "http/http_client_server.c" "relay_driver/relay_driver.c" "hid_device/hid_device.c" "time_manager/time_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_netif
                    PRIV_REQUIRES driver ssd1306 esp_adc iot_iconset esp_wifi esp_event freertos nvs_flash esp_http_client esp_timer espressif__tinyusb lwip)
