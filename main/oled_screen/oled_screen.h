#ifndef OLED_SCREEN_H
#define OLED_SCREEN_H

#include "driver/i2c.h"
#include "ssd1306.h"


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
#define ZONE_4_START_PAGE 2    // Pages 2-5 for the middle section (events and clock)
#define ZONE_4_END_PAGE 5      

// Event types for display events
typedef enum {
    DISPLAY_UPDATE_CLOCK,
    DISPLAY_UPDATE_LIGHT_STATUS,
    // Add other event types as needed (e.g., DISPLAY_UPDATE_HEIGHT, DISPLAY_UPDATE_POMODORO)
    DISPLAY_UPDATE_HEIGHT,
    DISPLAY_UPDATE_POMODORO,
    DISPLAY_UPDATE_SKYLIGHT
} display_event_type_t;

// UI State Management for contextual displays
typedef enum {
    UI_STATE_MAIN,          // Default clock + WiFi
    UI_STATE_DESK,          // Desk height + movement
    UI_STATE_VOLUME,        // Volume level
    UI_STATE_HUE,           // Hue scene + brightness
    UI_STATE_PC_SWITCH,     // PC switching
    UI_STATE_WINDOW,        // Window control
    UI_STATE_FAN            // Fan speed control
} ui_state_t;

typedef struct {
    ui_state_t current_state;
    uint32_t state_start_time;
    uint32_t display_duration_ms;

    // Context data
    struct {
        float desk_height;
        bool desk_moving;
        bool desk_moving_up;
        int volume_level;
        char hue_scene[32];
        int hue_brightness;
        int pc_number;
        bool window_opening;
        bool window_closing;
        bool http_ack_received;
        char wifi_status[16];
        char ip_address[16];
        int fan_speed_percent;
        bool fan_active;
    } context;
} ui_context_t;

// Event structure to be sent via queue
typedef struct {
    display_event_type_t event_type;
    char display_text[32];   // Buffer to hold event-specific text (e.g., "Lights: ON")
} display_event_t;


extern const uint16_t bluetooth[];   // Declare the bitmap (will be defined in the .c file)
// OLED initialization and configuration
void oled_init(SSD1306_t *dev);
void ssd1306_draw_bitmap(SSD1306_t *dev, int x, int page, const uint8_t *bitmap, int width, int height);
// Function to send display events (e.g., clock, brightness, status) to the display task
bool oled_send_display_event(display_event_t *event);
void display_bluetooth_icon(SSD1306_t *dev);
// Universal function to display event-specific messages for a certain duration
void display_event_message(SSD1306_t *dev, const char *message, int display_time_ms);

// Function to handle reverting back to the clock display after an event
void revert_to_clock(TimerHandle_t xTimer);
void i2c_master_init_custom(SSD1306_t *dev, int16_t sda, int16_t scl, int16_t reset);
// Function to display the time in a large font (Pages 2-5)
void display_time_x3(SSD1306_t *dev, const char *time);
void time_update_task(void *pvParameter);

// New contextual UI functions
void ui_show_main(SSD1306_t *dev);
void ui_show_desk(SSD1306_t *dev, float height, bool moving, bool moving_up);
void ui_show_volume(SSD1306_t *dev, int volume_percent);
void ui_show_hue(SSD1306_t *dev, const char* scene, int brightness);
void ui_show_pc_switch(SSD1306_t *dev, int pc_number);
void ui_show_window(SSD1306_t *dev, bool opening, bool closing, bool ack);
void ui_show_fan(SSD1306_t *dev, int fan_percent, bool active);

// UI state management
void ui_set_state(ui_state_t state, uint32_t duration_ms);
void ui_update_display(SSD1306_t *dev);
bool ui_should_return_to_main(void);
void ui_set_wifi_status(const char* status, const char* ip);
void ui_set_desk_context(float height, bool moving, bool moving_up);
void ui_set_volume_context(int volume_percent);
void ui_set_hue_context(const char* scene, int brightness);
void ui_set_pc_context(int pc_number);
void ui_set_window_context(bool opening, bool closing, bool ack);
void ui_set_fan_context(int fan_percent, bool active);

#endif // OLED_SCREEN_H
