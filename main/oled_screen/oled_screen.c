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

// Global UI context for new contextual display system
static ui_context_t ui_ctx = {
    .current_state = UI_STATE_MAIN,
    .state_start_time = 0,
    .display_duration_ms = 3000,  // 3 seconds default
    .context = {
        .desk_height = 0.0,
        .desk_moving = false,
        .desk_moving_up = false,
        .volume_level = 50,
        .hue_scene = "Unknown",
        .hue_brightness = 50,
        .pc_number = 1,
        .window_opening = false,
        .window_closing = false,
        .http_ack_received = false,
        .wifi_status = "●●●○",
        .ip_address = "192.168.x.x",
        .fan_speed_percent = 0,
        .fan_active = false
    }
};

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

// ============================================================================
// NEW CONTEXTUAL UI SYSTEM
// ============================================================================

// UI State Management Functions
void ui_set_state(ui_state_t state, uint32_t duration_ms) {
    ui_ctx.current_state = state;
    ui_ctx.state_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ui_ctx.display_duration_ms = duration_ms;
}

bool ui_should_return_to_main(void) {
    if (ui_ctx.current_state == UI_STATE_MAIN) {
        return false;
    }

    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return (current_time - ui_ctx.state_start_time) >= ui_ctx.display_duration_ms;
}

// Context setters
void ui_set_wifi_status(const char* status, const char* ip) {
    strncpy(ui_ctx.context.wifi_status, status, sizeof(ui_ctx.context.wifi_status) - 1);
    strncpy(ui_ctx.context.ip_address, ip, sizeof(ui_ctx.context.ip_address) - 1);
}

void ui_set_desk_context(float height, bool moving, bool moving_up) {
    ui_ctx.context.desk_height = height;
    ui_ctx.context.desk_moving = moving;
    ui_ctx.context.desk_moving_up = moving_up;
}

void ui_set_volume_context(int volume_percent) {
    ui_ctx.context.volume_level = volume_percent;
}

void ui_set_hue_context(const char* scene, int brightness) {
    strncpy(ui_ctx.context.hue_scene, scene, sizeof(ui_ctx.context.hue_scene) - 1);
    ui_ctx.context.hue_brightness = brightness;
}

void ui_set_pc_context(int pc_number) {
    ui_ctx.context.pc_number = pc_number;
}

void ui_set_window_context(bool opening, bool closing, bool ack) {
    ui_ctx.context.window_opening = opening;
    ui_ctx.context.window_closing = closing;
    ui_ctx.context.http_ack_received = ack;
}

void ui_set_fan_context(int fan_percent, bool active) {
    ui_ctx.context.fan_speed_percent = fan_percent;
    ui_ctx.context.fan_active = active;
}

// UI Display Functions
void ui_show_main(SSD1306_t *dev) {
    // Clear the entire screen first
    ssd1306_clear_screen(dev, false);
    ssd1306_clear_line(dev, 0, false);
    ssd1306_clear_line(dev, 1, false);
    ssd1306_clear_line(dev, 2, false);
    ssd1306_clear_line(dev, 3, false);
    ssd1306_clear_line(dev, 4, false);
    ssd1306_clear_line(dev, 5, false);
    ssd1306_clear_line(dev, 6, false);
    ssd1306_clear_line(dev, 7, false);

    // Top line: WiFi status (moved right)
    char wifi_line[32];
    snprintf(wifi_line, sizeof(wifi_line), "WiFi: Connected");
    ssd1306_display_text(dev, 0, wifi_line, strlen(wifi_line), false);

    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Large time display (center, moved right)
    char time_str[16];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
    ssd1306_display_text_x3(dev, 2, time_str, strlen(time_str), false);

    // Date display (bottom, moved right)
    char date_str[32];
    strftime(date_str, sizeof(date_str), "%a %d/%m", &timeinfo);
    ssd1306_display_text(dev, 6, date_str, strlen(date_str), false);

    ssd1306_show_buffer(dev);
}

void ui_show_desk(SSD1306_t *dev, float height, bool moving, bool moving_up) {
    ssd1306_clear_screen(dev, false);

    // Title
    if (moving) {
        ssd1306_display_text(dev, 0, moving_up ? "DESK MOVING UP" : "DESK MOVING DOWN",
                           moving_up ? 13 : 15, false);
    } else {
        ssd1306_display_text(dev, 0, "DESK HEIGHT", 11, false);
    }

    // Height display
    char height_str[32];
    snprintf(height_str, sizeof(height_str), "Height: %.1fcm", height);
    ssd1306_display_text(dev, 2, height_str, strlen(height_str), false);

    // Progress bar (if moving)
    if (moving) {
        // Simple progress bar based on height (assuming 60-80cm range)
        int progress = (int)((height - 60.0) / 20.0 * 10); // 0-10 scale
        char bar[16] = "          ";
        for (int i = 0; i < progress && i < 10; i++) {
            bar[i] = 0xFF; // Full block character
        }
        ssd1306_display_text(dev, 5, bar, 10, false);
    }

    ssd1306_show_buffer(dev);
}

void ui_show_volume(SSD1306_t *dev, int volume_percent) {
    // Clear the entire screen properly
    ssd1306_clear_screen(dev, false);
    for (int i = 0; i < 8; i++) {
        ssd1306_clear_line(dev, i, false);
    }

    // Title (moved right)
    ssd1306_display_text(dev, 0, "    VOLUME", 10, false);

    // Volume percentage (moved right)
    char vol_str[16];
    snprintf(vol_str, sizeof(vol_str), "    %d%%", volume_percent);
    ssd1306_display_text_x3(dev, 2, vol_str, strlen(vol_str), false);

    // Volume bar (moved right)
    char bar[20] = "    ";
    int filled = volume_percent / 10; // 0-10 scale
    for (int i = 0; i < filled && i < 10; i++) {
        bar[i + 4] = '='; // Use = instead of 0xFF
    }
    for (int i = filled; i < 10; i++) {
        bar[i + 4] = '-'; // Empty part
    }
    bar[14] = '\0'; // Null terminate
    ssd1306_display_text(dev, 6, bar, strlen(bar), false);

    ssd1306_show_buffer(dev);
}

void ui_show_hue(SSD1306_t *dev, const char* scene, int brightness) {
    ssd1306_clear_screen(dev, false);

    // Title
    ssd1306_display_text(dev, 0, "HUE LIGHTS", 10, false);

    // Scene name
    char scene_str[32];
    snprintf(scene_str, sizeof(scene_str), "Scene: %s", scene);
    ssd1306_display_text(dev, 2, scene_str, strlen(scene_str), false);

    // Brightness bar
    char bright_str[16];
    snprintf(bright_str, sizeof(bright_str), "Bright: %d%%", brightness);
    ssd1306_display_text(dev, 4, bright_str, strlen(bright_str), false);

    // Brightness bar
    char bar[16] = "          ";
    int filled = brightness / 10; // 0-10 scale
    for (int i = 0; i < filled && i < 10; i++) {
        bar[i] = 0xFF; // Full block
    }
    ssd1306_display_text(dev, 5, bar, 10, false);

    ssd1306_show_buffer(dev);
}

void ui_show_pc_switch(SSD1306_t *dev, int pc_number) {
    ssd1306_clear_screen(dev, false);

    // Title
    ssd1306_display_text(dev, 0, "USB SWITCH", 10, false);

    // Switching message
    ssd1306_display_text(dev, 2, "Switching to", 12, false);

    // PC number (large)
    char pc_str[16];
    snprintf(pc_str, sizeof(pc_str), "PC %d", pc_number);
    ssd1306_display_text_x3(dev, 4, pc_str, strlen(pc_str), false);

    ssd1306_show_buffer(dev);
}

void ui_show_window(SSD1306_t *dev, bool opening, bool closing, bool ack) {
    ssd1306_clear_screen(dev, false);

    // Title
    ssd1306_display_text(dev, 0, "WINDOW CONTROL", 14, false);

    // Action
    if (opening) {
        ssd1306_display_text(dev, 2, "OPENING", 7, false);
    } else if (closing) {
        ssd1306_display_text(dev, 2, "CLOSING", 7, false);
    }

    // Acknowledgment
    if (ack) {
        ssd1306_display_text(dev, 6, "Command Sent OK", 15, false);
    } else {
        ssd1306_display_text(dev, 6, "Sending...", 10, false);
    }

    ssd1306_show_buffer(dev);
}

void ui_show_fan(SSD1306_t *dev, int fan_percent, bool active) {
    // Clear the entire screen properly
    ssd1306_clear_screen(dev, false);
    for (int i = 0; i < 8; i++) {
        ssd1306_clear_line(dev, i, false);
    }

    // Title (moved right)
    ssd1306_display_text(dev, 0, "    FAN SPEED", 13, false);

    // Fan status
    if (active) {
        // Fan percentage (moved right)
        char fan_str[16];
        snprintf(fan_str, sizeof(fan_str), "    %d%%", fan_percent);
        ssd1306_display_text_x3(dev, 2, fan_str, strlen(fan_str), false);

        // Fan speed bar (moved right)
        char bar[20] = "    ";
        int filled = fan_percent / 10; // 0-10 scale
        for (int i = 0; i < filled && i < 10; i++) {
            bar[i + 4] = '='; // Use = for filled
        }
        for (int i = filled; i < 10; i++) {
            bar[i + 4] = '-'; // Empty part
        }
        bar[14] = '\0'; // Null terminate
        ssd1306_display_text(dev, 6, bar, strlen(bar), false);
    } else {
        // Fan off message
        ssd1306_display_text_x3(dev, 2, "   OFF", 6, false);
        ssd1306_display_text(dev, 6, "    Fan Stopped", 15, false);
    }

    ssd1306_show_buffer(dev);
}

// Main UI update function - call this regularly to update display
void ui_update_display(SSD1306_t *dev) {
    // Check if we should return to main screen
    if (ui_should_return_to_main()) {
        ui_ctx.current_state = UI_STATE_MAIN;
    }

    // Display based on current state
    switch (ui_ctx.current_state) {
        case UI_STATE_MAIN:
            ui_show_main(dev);
            break;

        case UI_STATE_DESK:
            ui_show_desk(dev, ui_ctx.context.desk_height,
                        ui_ctx.context.desk_moving, ui_ctx.context.desk_moving_up);
            break;

        case UI_STATE_VOLUME:
            ui_show_volume(dev, ui_ctx.context.volume_level);
            break;

        case UI_STATE_HUE:
            ui_show_hue(dev, ui_ctx.context.hue_scene, ui_ctx.context.hue_brightness);
            break;

        case UI_STATE_PC_SWITCH:
            ui_show_pc_switch(dev, ui_ctx.context.pc_number);
            break;

        case UI_STATE_WINDOW:
            ui_show_window(dev, ui_ctx.context.window_opening,
                          ui_ctx.context.window_closing, ui_ctx.context.http_ack_received);
            break;

        case UI_STATE_FAN:
            ui_show_fan(dev, ui_ctx.context.fan_speed_percent, ui_ctx.context.fan_active);
            break;
    }
}