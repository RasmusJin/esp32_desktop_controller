#include "keyswitches.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *KEYTAG = "KEYSWITCHES";

#define DEBOUNCE_DELAY_MS 150  // Debounce delay of 150 ms

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


void poll_rotary_encoders(void) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Poll Rotary Encoder 1 (Volume Control)
    int rot1_clk = gpio_get_level(ROT1_CLK);
    int rot1_dt = gpio_get_level(ROT1_DT);
    int rot1_sw = gpio_get_level(ROT1_SW);

    // Button Press Detection with Debouncing for Encoder 1
    if (rot1_sw == 0 && previous_rot1_sw == 1 && (current_time - last_press_time_rot1_sw) > DEBOUNCE_DELAY_MS) {
        ESP_LOGI(KEYTAG, "Rotary Encoder 1 Button Pressed!");
        last_press_time_rot1_sw = current_time;  // Update last press time
    }
    previous_rot1_sw = rot1_sw;

    // Rotary Encoder 1 Turning Logic (Edge Detection)
    if (rot1_clk != previous_rot1_clk) {
        ESP_LOGI(KEYTAG, "Rotary Encoder 1 - CLK changed: Current CLK: %d, DT: %d", rot1_clk, rot1_dt);
        if (rot1_clk == 1) {
            if (rot1_dt == 0) {
                encoder1_value++;  // Clockwise
                ESP_LOGI(KEYTAG, "Rotary Encoder 1 turned Clockwise");
            } else {
                encoder1_value--;  // Counterclockwise
                ESP_LOGI(KEYTAG, "Rotary Encoder 1 turned Counterclockwise");
            }
            ESP_LOGI(KEYTAG, "Rotary Encoder 1 Value: %ld", (long)encoder1_value);
        }
    }
    previous_rot1_clk = rot1_clk;

    // Poll Rotary Encoder 2 (Hue Light Control)
    int rot2_clk = gpio_get_level(ROT2_CLK);
    int rot2_dt = gpio_get_level(ROT2_DT);
    int rot2_sw = gpio_get_level(ROT2_SW);

    // Button Press Detection with Debouncing for Encoder 2
    if (rot2_sw == 0 && previous_rot2_sw == 1 && (current_time - last_press_time_rot2_sw) > DEBOUNCE_DELAY_MS) {
        ESP_LOGI(KEYTAG, "Rotary Encoder 2 Button Pressed!");
        last_press_time_rot2_sw = current_time;  // Update last press time
    }
    previous_rot2_sw = rot2_sw;

    // Rotary Encoder 2 Turning Logic (Edge Detection)
    if (rot2_clk != previous_rot2_clk) {
        ESP_LOGI(KEYTAG, "Rotary Encoder 2 - CLK changed: Current CLK: %d, DT: %d", rot2_clk, rot2_dt);
        if (rot2_clk == 1) {
            if (rot2_dt == 0) {
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

    // Poll Row 1
    gpio_set_level(ROW1_PIN, 1);
    vTaskDelay(1 / portTICK_PERIOD_MS);  // Allow signal stabilization

    if (debounce(current_time, COL1_PIN, &last_press_time_matrix[0][0])) {
        ESP_LOGI(KEYTAG, "Row 1, Column 1 pressed!");
    }
    if (debounce(current_time, COL2_PIN, &last_press_time_matrix[0][1])) {
        ESP_LOGI(KEYTAG, "Row 1, Column 2 pressed!");
    }
    if (debounce(current_time, COL3_PIN, &last_press_time_matrix[0][2])) {
        ESP_LOGI(KEYTAG, "Row 1, Column 3 pressed!");
    }
    gpio_set_level(ROW1_PIN, 0);  // Deactivate Row 1

    // Poll Row 2
    gpio_set_level(ROW2_PIN, 1);
    vTaskDelay(1 / portTICK_PERIOD_MS);  // Allow signal stabilization

    if (debounce(current_time, COL1_PIN, &last_press_time_matrix[1][0])) {
        ESP_LOGI(KEYTAG, "Row 2, Column 1 pressed!");
    }
    if (debounce(current_time, COL2_PIN, &last_press_time_matrix[1][1])) {
        ESP_LOGI(KEYTAG, "Row 2, Column 2 pressed!");
    }
    if (debounce(current_time, COL3_PIN, &last_press_time_matrix[1][2])) {
        ESP_LOGI(KEYTAG, "Row 2, Column 3 pressed!");
    }
    gpio_set_level(ROW2_PIN, 0);  // Deactivate Row 2
}