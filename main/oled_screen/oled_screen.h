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
#endif // OLED_SCREEN_H
