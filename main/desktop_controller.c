#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fan_control/fan_control.h"
#include "keyswitches/keyswitches.h"
#include "oled_screen/oled_screen.h"
#include "wifi_connection/wifi_connection.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "ssd1306.h"
void app_main(void)
{
    SSD1306_t dev;
    
    oled_init(&dev);
 
    // Clear the screen
    ssd1306_clear_screen(&dev, false);
    // Register the event handlers after initializing Wi-Fi

    display_bluetooth_icon(&dev);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for 1 second
    //oled_display_text(&dev, 0, "Hello, OLED!", false);
    //display_time_x3(&dev, "11:11");    // Display "Hello, OLED!" on page 0
    //display_wifi_icon(&dev);           // Display the WiFi icon on page 1
    wifi_init_sta();
    bool connected = wifi_poll_status(&dev);
    //initialize_ntp_and_time();
    //xTaskCreate(time_update_task, "time_update_task", 4096, (void *)&dev, 5, NULL);
    //xTaskCreate(poll_rotary_encoders_task, "rotary_encoder_task", 4096, (void *)&dev, 5, NULL);

    potentiometer_init();
    fan_pwm_init();
    setup_switch_single_row();
    while(1){
        update_fan_speed();
        poll_single_row();
        poll_switch_matrix();
        poll_rotary_encoders(&dev);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
