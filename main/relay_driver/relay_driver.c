#include "relay_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_rom_sys.h"   
#include "esp_timer.h" 

static const char *RELAYTAG = "RELAY";

// Initialize the relay GPIOs as outputs
void relay_driver_init(void) {
    ESP_LOGI("RELAY_INIT", "Starting relay driver setup...");

    // Set the GPIO levels high *before* configuring as outputs


    // Configure relays as outputs, with no internal pull-down
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_UP_PIN) | (1ULL << RELAY_DOWN_PIN) | (1ULL << RELAY_PC_SWITCH),
        .pull_down_en = 0, // No pull-down to prevent triggering
        .pull_up_en = 0    // Enable pull-up to hold HIGH state more reliably
    };
    gpio_config(&io_conf);

    gpio_set_level(RELAY_UP_PIN, 1);
    gpio_set_level(RELAY_DOWN_PIN, 1);
    gpio_set_level(RELAY_PC_SWITCH, 1);

    ESP_LOGI("RELAY_INIT", "Relay UP initial state: %d", gpio_get_level(RELAY_UP_PIN));
    ESP_LOGI("RELAY_INIT", "Relay DOWN initial state: %d", gpio_get_level(RELAY_DOWN_PIN));

    // Final confirmation delay
    vTaskDelay(50 / portTICK_PERIOD_MS);
}
void init_ultrasonic_sensor() {
    // Set up TRIG pin as output
    gpio_config_t trig_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << TRIG_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&trig_conf);

    // Set up ECHO pin as input
    gpio_config_t echo_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << ECHO_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&echo_conf);
}

void start_moving_desk(desk_move_t direction) {
    if (direction == DESK_MOVE_UP) {
        gpio_set_level(RELAY_UP_PIN, 0);    // Activate relay for UP
        gpio_set_level(RELAY_DOWN_PIN, 1);  // Ensure DOWN relay is off
        ESP_LOGI("DESK", "Moving UP");
    } else if (direction == DESK_MOVE_DOWN) {
        gpio_set_level(RELAY_DOWN_PIN, 0);  // Activate relay for DOWN
        gpio_set_level(RELAY_UP_PIN, 1);    // Ensure UP relay is off
        ESP_LOGI("DESK", "Moving DOWN");
    }
}

void stop_moving_desk(void) {
    gpio_set_level(RELAY_UP_PIN, 1);    // Ensure both relays are off
    gpio_set_level(RELAY_DOWN_PIN, 1);
    ESP_LOGI("DESK", "Stopped moving");
} 

void switch_pc(void) {
    gpio_set_level(RELAY_PC_SWITCH, 0);
    ESP_LOGI("DESK", "Switching PC");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(RELAY_PC_SWITCH, 1);
}

#define TIMEOUT_US 50000  // Set timeout to 50ms

uint32_t measure_distance() {
    // Send 10µs pulse on TRIG pin
    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    // Measure ECHO pulse duration with timeout handling
    uint32_t start_time = esp_timer_get_time();
    uint32_t timeout_time = start_time + TIMEOUT_US;

    // Wait for ECHO to go high or timeout
    while (gpio_get_level(ECHO_PIN) == 0) {
        if (esp_timer_get_time() > timeout_time) {
            ESP_LOGW("ULTRASONIC", "Timeout waiting for ECHO to go HIGH");
            return 0;  // Return 0 to indicate timeout
        }
    }
    
    start_time = esp_timer_get_time();  // Record the start time of the high pulse
    timeout_time = start_time + TIMEOUT_US;

    // Wait for ECHO to go low or timeout
    while (gpio_get_level(ECHO_PIN) == 1) {
        if (esp_timer_get_time() > timeout_time) {
            ESP_LOGW("ULTRASONIC", "Timeout waiting for ECHO to go LOW");
            return 0;  // Return 0 to indicate timeout
        }
    }

    // Calculate duration of the high pulse
    uint32_t end_time = esp_timer_get_time();
    uint32_t duration = end_time - start_time;

    // Convert duration to distance in centimeters
    uint32_t distance_cm = duration / 58;

    return distance_cm;
}