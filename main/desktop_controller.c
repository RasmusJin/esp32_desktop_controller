#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fan_control/fan_control.h"
#include "keyswitches/keyswitches.h"
#include "oled_screen/oled_screen.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "ssd1306.h"

void app_main(void)
{

    SSD1306_t dev;

    // Initialize OLED display
    oled_init(&dev);

    // Clear the screen
    ssd1306_clear_screen(&dev, false);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for 1 second
    oled_display_text(&dev, 0, "Hello, OLED!", false);
    display_time_x3(&dev, "11:11");    // Display "Hello, OLED!" on page 0
    display_wifi_icon(&dev);           // Display the WiFi icon on page 1
    potentiometer_init();
    fan_pwm_init();
    setup_switch_single_row();
    while(1){
        update_fan_speed();
        poll_single_row();
        poll_switch_matrix();
        poll_rotary_encoders();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
