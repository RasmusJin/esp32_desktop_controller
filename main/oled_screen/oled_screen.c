#include "ssd1306.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "oled_screen.h"
#include <string.h>
#include "time.h"
#include "wifi_connection/wifi_connection.h"

// Log tag
static const char *TAG = "OLED";
const uint16_t bluetooth[] = {0x0000, 0x0300, 0x0380, 0x02c0, 0x1260, 0x1a60, 0x0ec0, 0x0780, 0x0300, 0x0780, 0x0ec0, 0x1a60, 0x1260, 0x02c0, 0x0380, 0x0300};
// Define constants for screen layout and buffer
#define ZONE_4_START_PAGE 2    // Pages 2-5 for the middle section (events and clock)
#define ZONE_4_END_PAGE 5      
#define DISPLAY_QUEUE_LENGTH 10
#define CHAR_WIDTH_3X 24       // Each character is 24 pixels wide when scaled 3x
#define SCREEN_WIDTH 128       // OLED screen width

// Define the queue for display events
QueueHandle_t display_queue;

// Timer for event-driven display and reverting to the clock
TimerHandle_t event_timer;
bool is_showing_event = false;  // To track if an event is being displayed

// Function declarations
static void display_task(void *pvParameter);
void oled_init(SSD1306_t *dev);
void revert_to_clock(TimerHandle_t xTimer);  // Timer callback for reverting to clock

// Initialize OLED and create display task and queue
void oled_init(SSD1306_t *dev) {
    dev->_address = OLED_I2C_ADDRESS;
    dev->_width = 128;
    dev->_height = 64;
    dev->_pages = dev->_height / 8;
    dev->_i2c_num = I2C_NUM_0;

    // Initialize I2C and OLED
    i2c_master_init_custom(dev, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, RESET_PIN);
    ssd1306_init(dev, dev->_width, dev->_height);
    ESP_LOGI(TAG, "OLED initialized successfully");

    // Create the display queue
    display_queue = xQueueCreate(DISPLAY_QUEUE_LENGTH, sizeof(display_event_t));

    // Create the display task
    xTaskCreate(display_task, "display_task", 4096, (void *)dev, 5, NULL);

    // Create the event timer to revert to the clock after 5-8 seconds
    event_timer = xTimerCreate("EventTimer", pdMS_TO_TICKS(5000), pdFALSE, (void *)0, revert_to_clock);
}

// Universal event message display function
void display_event_message(SSD1306_t *dev, const char *message, int display_time_ms) {
    // Clear the middle part of the screen (Pages 2-5)
    for (int i = ZONE_4_START_PAGE; i <= ZONE_4_END_PAGE; i++) {
        ssd1306_clear_line(dev, i, false);
    }

    // Display the event message centered on Pages 2-5
    ssd1306_display_text(dev, ZONE_4_START_PAGE, (char *)message, strlen(message), false);
    ssd1306_show_buffer(dev);

    // Set flag to indicate an event is being displayed
    is_showing_event = true;

    // Start or reset the event timer
    if (xTimerIsTimerActive(event_timer)) {
        xTimerReset(event_timer, 0);
    } else {
        xTimerStart(event_timer, 0);
    }
}

// Revert to the clock display after the event timer expires
void revert_to_clock(TimerHandle_t xTimer) {
    is_showing_event = false;
    display_event_t event;
    event.event_type = DISPLAY_UPDATE_CLOCK;
    snprintf(event.display_text, sizeof(event.display_text), "%s", get_current_time_str());
    oled_send_display_event(&event);  // Revert to clock display
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
            // Clear the middle screen (Pages 2-5)
            for (int i = ZONE_4_START_PAGE; i <= ZONE_4_END_PAGE; i++) {
                ssd1306_clear_line(dev, i, false);
            }

            // Handle different display events
            switch (event.event_type) {
                case DISPLAY_UPDATE_CLOCK:
                    display_time_x3(dev, event.display_text);
                    break;

                case DISPLAY_UPDATE_LIGHT_STATUS:
                    ssd1306_display_text(dev, 0, event.display_text, strlen(event.display_text), false);
                    break;

                case DISPLAY_UPDATE_HEIGHT:
                    ssd1306_display_text(dev, ZONE_4_START_PAGE, event.display_text, strlen(event.display_text), false);
                    break;

                case DISPLAY_UPDATE_POMODORO:
                    ssd1306_display_text(dev, ZONE_4_START_PAGE, event.display_text, strlen(event.display_text), false);
                    break;

                case DISPLAY_UPDATE_SKYLIGHT:
                    ssd1306_display_text(dev, ZONE_4_START_PAGE, event.display_text, strlen(event.display_text), false);
                    break;

                default:
                    ESP_LOGE(TAG, "Unhandled display event type: %d", event.event_type);
                    break;
            }


            ssd1306_show_buffer(dev);  // Refresh the OLED display
        }
    }
}

// I2C Initialization
void i2c_master_init_custom(SSD1306_t *dev, int16_t sda, int16_t scl, int16_t reset) {
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 1000000  // 1 MHz
    };
    i2c_param_config(dev->_i2c_num, &i2c_config);
    i2c_driver_install(dev->_i2c_num, I2C_MODE_MASTER, 0, 0, 0);

    if (reset != -1) {
        gpio_set_direction(reset, GPIO_MODE_OUTPUT);
        gpio_set_level(reset, 0);
        vTaskDelay(pdMS_TO_TICKS(10));  // Hold reset low for 10ms
        gpio_set_level(reset, 1);
        vTaskDelay(pdMS_TO_TICKS(10));  // Wait 10ms after reset
    }
    ESP_LOGI(TAG, "I2C initialized successfully");
}

// Display clock in large font (Pages 2-5)
void display_time_x3(SSD1306_t *dev, const char *time) {
    int len = strlen(time);
    int total_width = len * CHAR_WIDTH_3X;
    int seg_offset = (SCREEN_WIDTH - total_width) / 2;  // Center the text

    // Clear the zone first
    for (int i = ZONE_4_START_PAGE; i <= ZONE_4_END_PAGE; i++) {
        ssd1306_clear_line(dev, i, false);
    }

    // Display large time text on Pages 2-5
    ssd1306_display_text_x3(dev, ZONE_4_START_PAGE, (char *)time, len, false);
    ssd1306_show_buffer(dev);  // Refresh the display
}
void time_update_task(void *pvParameter) {
    SSD1306_t *dev = (SSD1306_t *)pvParameter;
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

        // Send the clock update event to the queue
        snprintf(event.display_text, sizeof(event.display_text), "%s", time_str);
        oled_send_display_event(&event);  // Send clock update to the display queue

        // Wait for 1 minute before updating again
        vTaskDelay(pdMS_TO_TICKS(60000));  // Delay for 60 seconds
    }
}
// Function to draw a bitmap of specified width and height at a given position
void ssd1306_draw_bitmap_16x16(SSD1306_t *dev, int x, int y, const uint16_t *bitmap) {
    // Iterate over each of the 16 columns
    for (int col = 0; col < 16; col++) {
        uint16_t column_data = bitmap[col];  // Get the column's data

        // Iterate over each bit in the column
        for (int bit = 0; bit < 16; bit++) {
            int ypos = y + bit;  // Calculate the y position

            // Extract the pixel value (1 or 0) for the current bit
            bool is_on = (column_data >> bit) & 0x01;

            // Draw the pixel at (x + col, ypos)
            _ssd1306_pixel(dev, x + col, ypos, !is_on);  // Assuming your display logic is inverted
        }
    }

    // Refresh the display to show the changes
    ssd1306_show_buffer(dev);
}


void display_bluetooth_icon(SSD1306_t *dev) {
    int xpos = 72;  // Center the icon horizontally (128 - 16) / 2
    int page = 16;   // Starting vertical position (page 3)
    ssd1306_draw_bitmap_16x16(dev, xpos, page, bluetooth);
}