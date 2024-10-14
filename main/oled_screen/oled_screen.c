#include "ssd1306.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "oled_screen.h"
#include <string.h>
#include "bitmap_numbers/bitmap_numbers.h"
#include "iot_iconset_16x16.h"
#include "time.h"
#include "wifi_connection/wifi_connection.h"
#include "string.h"
// Log tag
static const char *TAG = "OLED";

#define DISPLAY_QUEUE_LENGTH 10



QueueHandle_t display_queue;

// Function declarations
static void display_task(void *pvParameter);
void oled_init(SSD1306_t *dev);

// Initialize OLED and create display task and queue
void oled_init(SSD1306_t *dev) {
    // Initialize the OLED screen
    dev->_address = OLED_I2C_ADDRESS;
    dev->_width = 128;
    dev->_height = 64;
    dev->_pages = dev->_height / 8;
    dev->_i2c_num = I2C_NUM_0;

    i2c_master_init_custom(dev, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, RESET_PIN);
    ssd1306_init(dev, dev->_width, dev->_height);

    ESP_LOGI("OLED", "OLED initialized successfully");

    // Create the display queue
    display_queue = xQueueCreate(DISPLAY_QUEUE_LENGTH, sizeof(display_event_t));

    // Create the display task
    xTaskCreate(display_task, "display_task", 4096, (void *)dev, 5, NULL);
}

// Send an event to the display queue
bool oled_send_display_event(display_event_t *event) {
    if (display_queue != NULL) {
        return xQueueSend(display_queue, event, portMAX_DELAY) == pdTRUE;
    }
    return false;
}

// Task responsible for handling display updates
static void display_task(void *pvParameter) {
    SSD1306_t *dev = (SSD1306_t *)pvParameter;
    display_event_t event;

    while (1) {
        // Wait for a message in the display queue
        if (xQueueReceive(display_queue, &event, portMAX_DELAY) == pdTRUE) {
            // Clear the screen before updating
            ssd1306_clear_screen(dev, false);

            // Handle the different types of display events
            switch (event.event_type) {
                case DISPLAY_UPDATE_CLOCK:
                    // Display clock (event.display_text contains the time "HH:MM")
                    display_time_x3(dev, event.display_text);
                    break;

                case DISPLAY_UPDATE_LIGHT_STATUS:
                    // Display light status (event.display_text contains light info and brightness)
                    ssd1306_display_text(dev, 0, event.display_text, strlen(event.display_text), false);
                    break;
            }
            ssd1306_show_buffer(dev);  // Refresh the OLED display
        }
    }
}
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


// Display text on OLED
void oled_display_text(SSD1306_t *dev, int page, const char *text, bool invert) {
    if (page < 0 || page >= dev->_pages) {
        ESP_LOGE(TAG, "Invalid page number");
        return;
    }
    ssd1306_display_text(dev, page, (char *)text, strlen(text), invert);
    ESP_LOGI(TAG, "Text displayed: %s", text);
}


#define ZONE_4_START_PAGE 2  // Starting at the third page (indexing from 0)
#define ZONE_4_END_PAGE 5    // Spanning four pages in total
#define CHAR_WIDTH_3X 24     // Each character is 24 pixels wide when scaled 3x
#define SCREEN_WIDTH 128     // OLED screen width

void display_time_x3(SSD1306_t *dev, const char *time) {
    int len = strlen(time);
    int total_width = len * CHAR_WIDTH_3X;  // Calculate total width needed
    int seg_offset = (SCREEN_WIDTH - total_width) / 2;  // Center the text horizontally
    int page = ZONE_4_START_PAGE;  // Start displaying in the middle zone

    ESP_LOGI(TAG, "Displaying large text at page %d with seg_offset %d", page, seg_offset);

    // Clear the zone first
    for (int i = page; i <= ZONE_4_END_PAGE; i++) {
        ssd1306_clear_line(dev, i, false);
    }

    // Display each character using ssd1306_display_text_x3
    ssd1306_display_text_x3(dev, page, (char *)time, len, false);

    ssd1306_show_buffer(dev);  // Refresh the display
}

void ssd1306_draw_icon_16x16(SSD1306_t *dev, int xpos, int ypos, const uint8_t *icon, bool invert) {
    // Make sure xpos and ypos are within valid range
    if (xpos < 0 || xpos >= dev->_width || ypos < 0 || ypos >= dev->_height) {
        ESP_LOGE("SSD1306", "Invalid icon position: (%d, %d)", xpos, ypos);
        return;
    }

    // Call the bitmap drawing function, width = 16, height = 16
    _ssd1306_bitmaps(dev, xpos, ypos, (uint8_t *)icon, 16, 16, invert);
}


void display_wifi_icon(SSD1306_t *dev) {
    // Display the WiFi icon at (x = 0, y = 48) (near the bottom-left of the screen)
    ssd1306_draw_icon_16x16(dev, 0, 48, wifi1_icon16x16, false);

    // Refresh the display to show the icon
    ssd1306_show_buffer(dev);
}

void time_update_task(void *pvParameter) {
    display_event_t event;
    event.event_type = DISPLAY_UPDATE_CLOCK;

    while (1) {
        time_t now;
        struct tm timeinfo;
        char time_str[6];  // Buffer for time string (HH:MM)

        // Get the current time
        time(&now);
        localtime_r(&now, &timeinfo);

        // Format time as "HH:MM"
        strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);

        // Send the event to update the clock display
        snprintf(event.display_text, sizeof(event.display_text), "%s", time_str);
        oled_send_display_event(&event);

        // Wait for 1 minute before updating again
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}









