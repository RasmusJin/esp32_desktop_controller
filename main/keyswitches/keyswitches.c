#include "keyswitches.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "http/http_client_server.h"
#include "ssd1306.h"
#include "oled_screen/oled_screen.h"
#include "wifi_connection/wifi_connection.h" 
#include "relay_driver/relay_driver.h"

static const char *KEYTAG = "KEYSWITCHES";
bool lights_on = false;
int brightness_value = 255;  // Start at max brightnessconst 
int brightness_step = 25;

#define DEBOUNCE_DELAY_MS 150  // Debounce delay of 150 ms
#define TAP_THRESHOLD_MS 500
// Define desk movement states
typedef enum {
    DESK_IDLE,
    DESK_MOVING_UP,
    DESK_MOVING_DOWN
} desk_state_t;

desk_state_t desk_state = DESK_IDLE;
bool button_up_pressed = false;
bool button_down_pressed = false;
uint32_t button_press_time = 0;
// Rotary Encoder Variables
// Rotary Encoder 1:
int32_t encoder1_value = 0;
int previous_rot1_clk = 1;
int previous_rot1_dt = 1;  // Previous state of the DT pin
int previous_rot1_sw = 1;  // Previous state of the switch
uint32_t last_press_time_rot1_sw = 0;  // Store the last press time for debounce
// Rotary Encoder 2:
int32_t encoder2_value = 0;
int previous_rot2_clk = 1;
int previous_rot2_dt = 1;
int previous_rot2_sw = 1;
uint32_t last_press_time_rot2_sw = 0;  

// Function to handle debouncing for any GPIO pin
bool debounce(uint32_t current_time, int gpio_pin, uint32_t *last_press_time) {
    if (gpio_get_level(gpio_pin) == 0 && (current_time - *last_press_time > DEBOUNCE_DELAY_MS)) {
        *last_press_time = current_time;
        return true;  // Debounce successful, register press
    }
    return false;  // Debounce not met, do not register press
}

static uint32_t last_press_time_single_row[5] = {0};  // Store last press time for each single-row switch
static uint32_t last_press_time_matrix[2][3] = {{0}}; // Store last press time for each matrix switch

void setup_switch_single_row(void) {
    // Configure single row GPIOs as input with pull-up resistors
    gpio_config_t single_row_io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,  // Use pull-up resistors
        .pin_bit_mask = (1ULL << KEY_GPIO1) | (1ULL << KEY_GPIO2) | 
                        (1ULL << KEY_GPIO3) | (1ULL << KEY_GPIO4) | 
                        (1ULL << KEY_GPIO5),
    };
    gpio_config(&single_row_io_conf);

    setup_switch_matrix();
    setup_rotary_encoders();
}

void setup_switch_matrix(void) {
    // Configure rows as output
    gpio_config_t row_io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << ROW1_PIN) | (1ULL << ROW2_PIN),
    };
    gpio_config(&row_io_conf);

    // Configure columns as input with pull-up resistors
    gpio_config_t col_io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pin_bit_mask = (1ULL << COL1_PIN) | (1ULL << COL2_PIN) | (1ULL << COL3_PIN),
    };
    gpio_config(&col_io_conf);
}

void setup_rotary_encoders(void) {
    // Configure rotary encoder 1 GPIOs
    gpio_config_t io_conf1 = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,  // Use pull-up resistors
        .pin_bit_mask = (1ULL << ROT1_CLK) | (1ULL << ROT1_DT) | (1ULL << ROT1_SW),
    };
    gpio_config(&io_conf1);

    // Configure rotary encoder 2 GPIOs
    gpio_config_t io_conf2 = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,  // Use pull-up resistors
        .pin_bit_mask = (1ULL << ROT2_CLK) | (1ULL << ROT2_DT) | (1ULL << ROT2_SW),
    };
    gpio_config(&io_conf2);
}

void poll_rotary_encoders_task(void *pvParameter) {
    SSD1306_t *dev = (SSD1306_t *)pvParameter;
    while (1) {
        poll_rotary_encoders(dev);  // Call the rotary encoder polling function
        vTaskDelay(10 / portTICK_PERIOD_MS);  // Small delay to prevent CPU overload
    }
}
void poll_rotary_encoders(SSD1306_t *dev) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    display_event_t event;
    event.event_type = DISPLAY_UPDATE_LIGHT_STATUS;
    // Poll Rotary Encoder 1 (Brightness Control)
    int rot1_clk = gpio_get_level(ROT1_CLK);
    int rot1_dt = gpio_get_level(ROT1_DT);
    int rot1_sw = gpio_get_level(ROT1_SW);

    // Button Press Detection with Debouncing for Encoder 1
    if (rot1_sw == 0 && previous_rot1_sw == 1 && (current_time - last_press_time_rot1_sw) > DEBOUNCE_DELAY_MS) {
        ESP_LOGI(KEYTAG, "Rotary Encoder 1 Button Pressed!");
        if (lights_on) {
            // Send a command to turn off the lights
            hue_send_command("http://192.168.50.170/api/AicZqASmH6YLHxDyBxD-pci3vEmn0jLU0XvQ9g9N/groups/1/action", "{\"on\": false}");
            lights_on = false;
        } else {
            // Send a command to turn on the lights
            hue_send_command("http://192.168.50.170/api/AicZqASmH6YLHxDyBxD-pci3vEmn0jLU0XvQ9g9N/groups/1/action", "{\"on\": true}");
            lights_on = true;
        }
        snprintf(event.display_text, sizeof(event.display_text), "Lights: %s", lights_on ? "ON" : "OFF");
        event.event_type = DISPLAY_UPDATE_LIGHT_STATUS;
        oled_send_display_event(&event); 
        last_press_time_rot1_sw = current_time;
    }
    previous_rot1_sw = rot1_sw;

    // Rotary Encoder 1 Turning Logic (Edge Detection)
    if (rot1_clk != previous_rot1_clk) {  // Only act when CLK changes state
        if (rot1_clk == 0) {  // Detect the falling edge of CLK
            if (rot1_dt == 1) {
                // Clockwise, increase brightness
                brightness_value = brightness_value + 25;
                if (brightness_value > 255) {
                brightness_value = 255;
                }
                ESP_LOGI(KEYTAG, "Rotary Encoder 1 turned Clockwise, increasing brightness to %d", brightness_value);
            } else {
                // Counterclockwise, decrease brightness
                brightness_value = brightness_value - 25;
                if (brightness_value < 0) {
                brightness_value = 0;
                }
                ESP_LOGI(KEYTAG, "Rotary Encoder 1 turned Counterclockwise, decreasing brightness to %d", brightness_value);
            }
            // Send brightness command to the Hue lights
            snprintf(event.display_text, sizeof(event.display_text), "Brightness: %d", brightness_value);
            event.event_type = DISPLAY_UPDATE_LIGHT_STATUS;  // Reuse event type for brightness
            oled_send_display_event(&event);  
            }
    }
    previous_rot1_clk = rot1_clk;

    // Poll Rotary Encoder 2 (for hue lights or other actions)
    int rot2_clk = gpio_get_level(ROT2_CLK);
    int rot2_dt = gpio_get_level(ROT2_DT);
    int rot2_sw = gpio_get_level(ROT2_SW);

    // Button Press Detection with Debouncing for Encoder 2
    if (rot2_sw == 0 && previous_rot2_sw == 1 && (current_time - last_press_time_rot2_sw) > DEBOUNCE_DELAY_MS) {
        ESP_LOGI(KEYTAG, "Rotary Encoder 2 Button Pressed!");
        last_press_time_rot2_sw = current_time;  // Update last press time
    }
    previous_rot2_sw = rot2_sw;

    // Rotary Encoder 2 Turning Logic (similar to Rotary 1)
    if (rot2_clk != previous_rot2_clk) {
        if (rot2_clk == 0) {  // Detect the falling edge of CLK
            if (rot2_dt == 1) {
                encoder2_value++;  // Clockwise
                ESP_LOGI(KEYTAG, "Rotary Encoder 2 turned Clockwise");
            } else {
                encoder2_value--;  // Counterclockwise
                ESP_LOGI(KEYTAG, "Rotary Encoder 2 turned Counterclockwise");
            }
            ESP_LOGI(KEYTAG, "Rotary Encoder 2 Value: %ld", (long)encoder2_value);
        }
    }
    previous_rot2_clk = rot2_clk;

    // Small delay to prevent excessive CPU usage
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

void poll_single_row(void) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Poll each switch in the single row with debounce logic
    if (debounce(current_time, KEY_GPIO1, &last_press_time_single_row[0])) {
        ESP_LOGI(KEYTAG, "Switch 1 pressed!");
        switch_pc();

    }
    if (debounce(current_time, KEY_GPIO2, &last_press_time_single_row[1])) {
        ESP_LOGI(KEYTAG, "Switch 2 pressed!");

    }
    if (debounce(current_time, KEY_GPIO3, &last_press_time_single_row[2])) {
        ESP_LOGI(KEYTAG, "Switch 3 pressed!");
    }
    if (debounce(current_time, KEY_GPIO4, &last_press_time_single_row[3])) {
        ESP_LOGI(KEYTAG, "Switch 4 pressed!");
    }
    if (debounce(current_time, KEY_GPIO5, &last_press_time_single_row[4])) {
        ESP_LOGI(KEYTAG, "Switch 5 pressed!");
    }
}

void poll_switch_matrix(void) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    static uint32_t up_press_time = 0;
    static uint32_t down_press_time = 0;
    static bool up_button_pressed = false;
    static bool down_button_pressed = false;
    // Poll Row 1
    gpio_set_level(ROW1_PIN, 1);
    vTaskDelay(1 / portTICK_PERIOD_MS);  // Allow signal stabilization

    
    // UP button handling - SAFETY CRITICAL: immediate response, no debounce
    int up_button_state = gpio_get_level(COL1_PIN);
    if (up_button_state == 0) {  // Button pressed (active-low)
        if (!up_button_pressed) {         // Button just pressed
            up_button_pressed = true;
            start_moving_desk(DESK_MOVE_UP);
            ESP_LOGI("DESK_CONTROL", "UP button pressed - starting movement");
        }
        // Monitor distance while moving
        uint32_t distance_cm = measure_distance();
        if (distance_cm > 0) {  // Valid reading
            ESP_LOGI("ULTRASONIC", "Moving up - Distance: %lu cm", distance_cm);
        }
    } else {  // Button released
        if (up_button_pressed) {       // Button was pressed, now released
            stop_moving_desk();
            up_button_pressed = false;
            ESP_LOGI("DESK_CONTROL", "UP button released - STOPPED");
        }
    }
    
    if (debounce(current_time, COL2_PIN, &last_press_time_matrix[0][1])) {
        ESP_LOGI("BUTTON", "Up command triggered");
        skylight_command_up();
    }
    if (debounce(current_time, COL3_PIN, &last_press_time_matrix[0][2])) {
        ESP_LOGI(KEYTAG, "Row 1, Column 3 pressed!");
    }
    gpio_set_level(ROW1_PIN, 0);  // Deactivate Row 1

    // Poll Row 2
    gpio_set_level(ROW2_PIN, 1);
    vTaskDelay(1 / portTICK_PERIOD_MS);  // Allow signal stabilization

    // DOWN button handling - SAFETY CRITICAL: immediate response, no debounce
    int down_button_state = gpio_get_level(COL1_PIN);
    if (down_button_state == 0) {  // Button pressed (active-low)
        if (!down_button_pressed) {       // Button just pressed
            down_button_pressed = true;
            start_moving_desk(DESK_MOVE_DOWN);
            ESP_LOGI("DESK_CONTROL", "DOWN button pressed - starting movement");
        }
        // Monitor distance while moving
        uint32_t distance_cm = measure_distance();
        if (distance_cm > 0) {  // Valid reading
            ESP_LOGI("ULTRASONIC", "Moving down - Distance: %lu cm", distance_cm);
        }
    } else {  // Button released
        if (down_button_pressed) {     // Button was pressed, now released
            stop_moving_desk();
            down_button_pressed = false;
            ESP_LOGI("DESK_CONTROL", "DOWN button released - STOPPED");
        }
    }
    gpio_set_level(ROW2_PIN, 0);
    if (debounce(current_time, COL2_PIN, &last_press_time_matrix[1][1])) {
        ESP_LOGI(KEYTAG, "Row 2, Column 2 pressed!");
        ESP_LOGI("BUTTON", "Down command triggered");
        skylight_command_down();
    }
    if (debounce(current_time, COL3_PIN, &last_press_time_matrix[1][2])) {
        ESP_LOGI(KEYTAG, "Row 2, Column 3 pressed!");
    }
    gpio_set_level(ROW2_PIN, 0);  // Deactivate Row 2
}