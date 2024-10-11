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
void oled_init(SSD1306_t *dev);
void i2c_master_init_custom(SSD1306_t *dev, int16_t sda, int16_t scl, int16_t reset);
void oled_display_text(SSD1306_t *dev, int page, const char *text, bool invert);
void display_time_x3(SSD1306_t *dev, const char *time);
void ssd1306_draw_icon_16x16(SSD1306_t *dev, int xpos, int ypos, const uint8_t *icon, bool invert);
void display_wifi_icon(SSD1306_t *dev);
void update_wifi_status(SSD1306_t *dev, bool connected);

#endif // OLED_SCREEN_H
