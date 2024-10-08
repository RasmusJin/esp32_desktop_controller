#include "ssd1306.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "oled_screen.h"
#include <string.h>

// Log tag
static const char *TAG = "OLED";

// Initialize I2C
void i2c_master_init_custom(SSD1306_t *dev, int16_t sda, int16_t scl, int16_t reset) {
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 1000000  // Set your desired I2C clock speed, 1 MHz in this example
    };
    i2c_param_config(dev->_i2c_num, &i2c_config);
    i2c_driver_install(dev->_i2c_num, I2C_MODE_MASTER, 0, 0, 0);

    if (reset != -1) {
        gpio_set_direction(reset, GPIO_MODE_OUTPUT);
        gpio_set_level(reset, 0);
        vTaskDelay(pdMS_TO_TICKS(10));  // hold reset low for 10ms
        gpio_set_level(reset, 1);
        vTaskDelay(pdMS_TO_TICKS(10));  // wait for 10ms after reset
    }
    ESP_LOGI(TAG, "I2C initialized successfully");
}


// Initialize OLED
void oled_init(SSD1306_t *dev) {
    dev->_address = OLED_I2C_ADDRESS;
    dev->_width = 128;  // Width of the OLED display
    dev->_height = 64;  // Height of the OLED display
    dev->_pages = dev->_height / 8;  // 8 pixels per page
    dev->_i2c_num = I2C_NUM_0;  // Use I2C port 0

    // Initialize I2C with a reset pin if applicable
    i2c_master_init(dev, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, RESET_PIN);  
    ssd1306_init(dev, dev->_width, dev->_height);
    ESP_LOGI(TAG, "OLED initialized successfully");
}

// Display text on OLED
void oled_display_text(SSD1306_t *dev, int page, const char *text, bool invert) {
    if (page < 0 || page >= dev->_pages) {
        ESP_LOGE(TAG, "Invalid page number");
        return;
    }
    ssd1306_display_text(dev, page, (char *)text, strlen(text), invert);
    ESP_LOGI(TAG, "Text displayed: %s", text);
}

