#ifndef OLED_SCREEN_H
#define OLED_SCREEN_H

#include "driver/i2c.h"
#include "ssd1306.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// I2C Configuration
#define I2C_MASTER_SCL_IO          39           // GPIO for SCL
#define I2C_MASTER_SDA_IO          38           // GPIO for SDA
#define I2C_MASTER_NUM             I2C_NUM_0    // I2C port number
#define I2C_MASTER_FREQ_HZ         100000       // I2C frequency
#define I2C_MASTER_TX_BUF_DISABLE  0            // I2C master doesn't need buffer
#define I2C_MASTER_RX_BUF_DISABLE  0            // I2C master doesn't need buffer
#define OLED_I2C_ADDRESS           0x3C         // I2C address of OLED
#define SCREEN_WIDTH               128          // OLED screen width
#define SCREEN_HEIGHT              64           // OLED screen height
#define RESET_PIN -1

// UI State Management for contextual displays
typedef enum {
    UI_STATE_MAIN,          // Default clock + WiFi
    UI_STATE_DESK,          // Desk height + movement
    UI_STATE_VOLUME,        // Volume level
    UI_STATE_HUE,           // Hue scene + brightness
    UI_STATE_PC_SWITCH,     // PC switching
    UI_STATE_WINDOW,        // Window control
    UI_STATE_FAN,           // Fan speed control
    UI_STATE_SUSPEND,       // PC suspend/sleep
    UI_STATE_BOOT_INFO,     // Boot information display
    UI_STATE_DEBUG_REBOOT   // Debug info before reboot
} ui_state_t;

// Legacy event types for backward compatibility
typedef enum {
    DISPLAY_UPDATE_CLOCK,
    DISPLAY_UPDATE_LIGHT_STATUS,
    DISPLAY_UPDATE_HEIGHT,
    DISPLAY_UPDATE_POMODORO,
    DISPLAY_UPDATE_SKYLIGHT
} display_event_type_t;

// Legacy event structure for backward compatibility
typedef struct {
    display_event_type_t event_type;
    char display_text[32];
} display_event_t;

// UI Context structure to hold all display data
typedef struct {
    ui_state_t current_state;
    uint32_t state_start_time;
    uint32_t display_duration_ms;

    // Context data for all UI states
    struct {
        // WiFi/Network
        bool wifi_connected;
        char ip_address[16];

        // Desk control
        float desk_height;
        bool desk_moving;
        bool desk_moving_up;

        // Volume control
        int volume_level;
        bool volume_direction_up; // true = last change was UP, false = DOWN

        // Hue lighting
        char hue_scene[32];
        int hue_brightness;
        bool hue_lights_on;

        // PC switching
        int pc_number;

        // Window control
        bool window_opening;
        bool window_closing;
        bool http_ack_received;

        // Fan control
        int fan_speed_percent;
        bool fan_active;

        // Boot/Debug information
        char boot_message[64];
        int wifi_retry_count;
        bool http_connected;
        char error_message[32];
    } context;
} ui_context_t;

// External declaration of global UI context
extern ui_context_t ui_ctx;

// OLED initialization and configuration (KEEP EXISTING - WORKS)
void oled_init(SSD1306_t *dev);
void i2c_master_init_custom(SSD1306_t *dev, int16_t sda, int16_t scl, int16_t reset);

// NEW UI SYSTEM - Main display functions
void ui_show_main(SSD1306_t *dev);
void ui_show_desk(SSD1306_t *dev);
void ui_show_volume(SSD1306_t *dev);
void ui_show_hue(SSD1306_t *dev);
void ui_show_pc_switch(SSD1306_t *dev);
void ui_show_window(SSD1306_t *dev);
void ui_show_fan(SSD1306_t *dev);
void ui_show_suspend(SSD1306_t *dev);
void ui_show_boot_info(SSD1306_t *dev);
void ui_show_debug_reboot(SSD1306_t *dev);

// NEW UI SYSTEM - State management
void ui_set_state(ui_state_t state, uint32_t duration_ms);
void ui_force_refresh_state(ui_state_t state, uint32_t duration_ms);
void ui_update_display(SSD1306_t *dev);
bool ui_should_return_to_main(void);
bool ui_should_update_desk_distance(void);

// NEW UI SYSTEM - Context setters
void ui_set_wifi_status(bool connected, const char* ip);
void ui_set_desk_context(float height, bool moving, bool moving_up);
void ui_set_volume_context(int volume_percent);
void ui_set_hue_context(const char* scene, int brightness, bool lights_on);
void ui_set_pc_context(int pc_number);
void ui_set_window_context(bool opening, bool closing, bool ack);
void ui_set_fan_context(int fan_percent, bool active);
void ui_set_boot_context(const char* message, int wifi_retries, bool http_ok, const char* error);
void ui_set_wifi_context(bool connected, const char* ip_address);

// NEW UI SYSTEM - Utility functions
void ui_clear_screen(SSD1306_t *dev);
void ui_draw_progress_bar(SSD1306_t *dev, int x, int y, int width, int height, int percentage);
void ui_draw_text_centered(SSD1306_t *dev, int page, const char* text);
void ui_draw_text_right_aligned(SSD1306_t *dev, int page, const char* text);
void ui_display_text_x3_offset(SSD1306_t *dev, int page, char *text, int text_len, int offset_pixels, bool invert);

// NEW UI SYSTEM - Main update task
void ui_display_task(void *pvParameter);

// Safe volume access functions for HID device
int get_current_ui_volume_level(void);
void set_ui_volume_level(int volume);
void set_ui_volume_direction(bool direction_up);

// LEGACY FUNCTIONS - For backward compatibility with existing code
bool oled_send_display_event(display_event_t *event);
void display_event_message(SSD1306_t *dev, const char *message, int display_time_ms);

#endif // OLED_SCREEN_H
