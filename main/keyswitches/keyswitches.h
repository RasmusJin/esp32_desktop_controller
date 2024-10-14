#ifndef KEYSWITCHES_H
#define KEYSWITCHES_H

#include "driver/gpio.h"
#include "ssd1306.h"
// Single row switches
#define KEY_GPIO1 GPIO_NUM_18  // Leftmost switch in single row
#define KEY_GPIO2 GPIO_NUM_17
#define KEY_GPIO3 GPIO_NUM_16
#define KEY_GPIO4 GPIO_NUM_15
#define KEY_GPIO5 GPIO_NUM_1   // Rightmost switch in single row

// 2 rows 3 column switch matrix:
#define ROW1_PIN GPIO_NUM_6  // Define row pins
#define ROW2_PIN GPIO_NUM_5

#define COL1_PIN GPIO_NUM_2  // Define column pins
#define COL2_PIN GPIO_NUM_42
#define COL3_PIN GPIO_NUM_41

// Rotary Encoder 1
#define ROT1_CLK GPIO_NUM_46
#define ROT1_DT GPIO_NUM_3
#define ROT1_SW GPIO_NUM_8

// Rotary Encoder 2
#define ROT2_CLK GPIO_NUM_11
#define ROT2_DT GPIO_NUM_10
#define ROT2_SW GPIO_NUM_9
// relay gpios



void setup_switch_single_row(void);
void poll_single_row(void);
void poll_switch_matrix(void);
void setup_switch_matrix(void);
bool debounce(uint32_t current_time, int gpio_pin, uint32_t *last_press_time);
void setup_rotary_encoders(void);
void poll_rotary_encoders(SSD1306_t *dev);
void poll_rotary_encoders_task(void *pvParameter);
#endif // KEYSWITCHES_H