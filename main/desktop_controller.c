#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fan_control/fan_control.h"
#include "keyswitches/keyswitches.h"
#include "oled_screen/oled_screen.h"
#include "wifi_connection/wifi_connection.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "ssd1306.h"
#include "relay_driver/relay_driver.h"
void app_main(void)
{
    SSD1306_t dev;

    // DISABLED FOR DEBUGGING - oled_init(&dev);

    // DISABLED FOR DEBUGGING - Clear the screen
    // ssd1306_clear_screen(&dev, false);
    // Register the event handlers after initializing Wi-Fi

    // DISABLED FOR DEBUGGING - display_bluetooth_icon(&dev);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for 1 second
    //oled_display_text(&dev, 0, "Hello, OLED!", false);
    //display_time_x3(&dev, "11:11");    // Display "Hello, OLED!" on page 0
    //display_wifi_icon(&dev);           // Display the WiFi icon on page 1
    relay_driver_init();
    init_ultrasonic_sensor();

    // Test ultrasonic pins for debugging
    ESP_LOGI("MAIN", "Running ultrasonic pin test...");
    test_ultrasonic_pins();

    // Initialize WiFi with static IP
    ESP_LOGI("MAIN", "Initializing WiFi...");
    wifi_init_sta();
    //initialize_ntp_and_time();
    //xTaskCreate(time_update_task, "time_update_task", 4096, (void *)&dev, 5, NULL);
    //xTaskCreate(poll_rotary_encoders_task, "rotary_encoder_task", 4096, (void *)&dev, 5, NULL);

    potentiometer_init();
    fan_pwm_init();
    setup_switch_single_row();

    ESP_LOGI("MAIN", "All systems initialized - starting main loop");
    ESP_LOGI("MAIN", "Fan control: ACTIVE");
    ESP_LOGI("MAIN", "Single row switches: ACTIVE");
    ESP_LOGI("MAIN", "Switch matrix (desk control): ACTIVE");
    ESP_LOGI("MAIN", "Relay system: ACTIVE");

    // Uncomment next line to run sensor test (comment out after testing)
    // test_sensor_readings();

    // WiFi monitoring counter
    static uint32_t wifi_check_counter = 0;

    while(1){
        update_fan_speed();
        poll_single_row();        // PC switch and other single buttons
        poll_switch_matrix();     // Desk UP/DOWN buttons + skylight controls
        check_desk_safety_timeout();  // CRITICAL: Check for desk movement timeout

        // Check WiFi connection every 10 seconds (200 * 50ms = 10s)
        wifi_check_counter++;
        if (wifi_check_counter >= 200) {
            wifi_check_and_reconnect();
            wifi_check_counter = 0;
        }

        // DISABLED FOR DEBUGGING - poll_rotary_encoders(&dev);  // Keep disabled (needs display)
        vTaskDelay(50 / portTICK_PERIOD_MS);  // 50ms = 20Hz polling rate
    }
}
