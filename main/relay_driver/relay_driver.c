#include "relay_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "oled_screen/oled_screen.h"

static const char *RELAYTAG = "RELAY";

// Initialize the relay GPIOs as outputs
void relay_driver_init(void) {
    ESP_LOGI("RELAY_INIT", "Starting relay driver setup...");

    // Add initial delay to let ESP32 boot stabilize
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Configure each relay pin individually to minimize glitches
    gpio_config_t relay_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 1,  // Enable pull-up to help maintain HIGH state during boot
    };

    // Configure and set each relay individually
    relay_conf.pin_bit_mask = (1ULL << RELAY_UP_PIN);
    gpio_config(&relay_conf);
    gpio_set_level(RELAY_UP_PIN, 1);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    relay_conf.pin_bit_mask = (1ULL << RELAY_DOWN_PIN);
    gpio_config(&relay_conf);
    gpio_set_level(RELAY_DOWN_PIN, 1);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    relay_conf.pin_bit_mask = (1ULL << RELAY_PC_SWITCH);
    gpio_config(&relay_conf);
    gpio_set_level(RELAY_PC_SWITCH, 1);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    ESP_LOGI("RELAY_INIT", "Relay UP initial state: %d", gpio_get_level(RELAY_UP_PIN));
    ESP_LOGI("RELAY_INIT", "Relay DOWN initial state: %d", gpio_get_level(RELAY_DOWN_PIN));
    ESP_LOGI("RELAY_INIT", "Relay PC_SWITCH initial state: %d", gpio_get_level(RELAY_PC_SWITCH));

    // Final stabilization delay
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_LOGI("RELAY_INIT", "✅ All relays initialized and stable - system ready!");
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

    // Test TRIG pin functionality
    ESP_LOGI("ULTRASONIC_INIT", "Testing TRIG pin (GPIO%d)...", TRIG_PIN);
    gpio_set_level(TRIG_PIN, 0);
    ESP_LOGI("ULTRASONIC_INIT", "TRIG set LOW, reading: %d", gpio_get_level(TRIG_PIN));
    gpio_set_level(TRIG_PIN, 1);
    ESP_LOGI("ULTRASONIC_INIT", "TRIG set HIGH, reading: %d", gpio_get_level(TRIG_PIN));
    gpio_set_level(TRIG_PIN, 0);
    ESP_LOGI("ULTRASONIC_INIT", "TRIG set LOW again, reading: %d", gpio_get_level(TRIG_PIN));
}

// Safety timeout for desk movement (10 seconds max)
static uint32_t movement_start_time = 0;
static bool movement_active = false;
#define MAX_MOVEMENT_TIME_MS 10000

void start_moving_desk(desk_move_t direction) {
    // Record movement start time for safety timeout
    movement_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    movement_active = true;

    if (direction == DESK_MOVE_UP) {
        gpio_set_level(RELAY_UP_PIN, 0);    // Activate relay for UP
        gpio_set_level(RELAY_DOWN_PIN, 1);  // Ensure DOWN relay is off
        ESP_LOGI("DESK", "Moving UP - SAFETY TIMEOUT: %d seconds", MAX_MOVEMENT_TIME_MS/1000);

        // Show desk UI
        ui_set_desk_context(67.0, true, true); // Approximate height, moving up
        ui_set_state(UI_STATE_DESK, 5000); // Show desk UI for 5 seconds
    } else if (direction == DESK_MOVE_DOWN) {
        gpio_set_level(RELAY_DOWN_PIN, 0);  // Activate relay for DOWN
        gpio_set_level(RELAY_UP_PIN, 1);    // Ensure UP relay is off
        ESP_LOGI("DESK", "Moving DOWN - SAFETY TIMEOUT: %d seconds", MAX_MOVEMENT_TIME_MS/1000);

        // Show desk UI
        ui_set_desk_context(67.0, true, false); // Approximate height, moving down
        ui_set_state(UI_STATE_DESK, 5000); // Show desk UI for 5 seconds
    }
}

void stop_moving_desk(void) {
    gpio_set_level(RELAY_UP_PIN, 1);    // Ensure both relays are off
    gpio_set_level(RELAY_DOWN_PIN, 1);
    movement_active = false;
    ESP_LOGI("DESK", "Stopped moving");

    // Update desk UI to show stopped state
    ui_set_desk_context(67.0, false, false); // Approximate height, not moving
    ui_set_state(UI_STATE_DESK, 3000); // Show desk UI for 3 seconds
}

// Safety function to check for timeout and force stop
void check_desk_safety_timeout(void) {
    if (movement_active) {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if ((current_time - movement_start_time) > MAX_MOVEMENT_TIME_MS) {
            ESP_LOGW("DESK_SAFETY", "EMERGENCY STOP! Movement timeout exceeded (%d ms)", MAX_MOVEMENT_TIME_MS);
            stop_moving_desk();
        }
    }
}

// Global variable to track current PC
static int current_pc = 1;

void switch_pc(void) {
    gpio_set_level(RELAY_PC_SWITCH, 0);
    ESP_LOGI("DESK", "Switching PC");

    // Toggle PC number and show UI
    current_pc = (current_pc == 1) ? 2 : 1;
    ui_set_pc_context(current_pc);
    ui_set_state(UI_STATE_PC_SWITCH, 3000); // Show PC switch UI for 3 seconds

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(RELAY_PC_SWITCH, 1);
}

// Get current PC number
int get_current_pc(void) {
    return current_pc;
}

#define TIMEOUT_US 50000  // Set timeout to 50ms (back to normal)

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
    // Precise calculation: duration_µs * 0.0343 / 2 = distance_cm
    uint32_t distance_cm = (duration * 343) / (2 * 10000);

    // Optional debug logging - uncomment for calibration
    // ESP_LOGI("ULTRASONIC", "Raw: %lu µs → %lu cm", duration, distance_cm);

    return distance_cm;
}

// Test function to continuously monitor sensor without desk movement
void test_sensor_readings(void) {
    ESP_LOGI("SENSOR_TEST", "=== SENSOR TEST MODE ===");
    ESP_LOGI("SENSOR_TEST", "Manually move the desk and observe readings");
    ESP_LOGI("SENSOR_TEST", "Distance should INCREASE when desk moves UP");

    for (int i = 0; i < 20; i++) {
        uint32_t distance = measure_distance();
        ESP_LOGI("SENSOR_TEST", "Reading %d: %lu cm", i+1, distance);
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // 1 second between readings
    }

    ESP_LOGI("SENSOR_TEST", "=== TEST COMPLETE ===");
}

// Test function to help debug wiring issues
void test_ultrasonic_pins(void) {
    ESP_LOGI("ULTRASONIC_TEST", "=== ULTRASONIC PIN TEST ===");
    ESP_LOGI("ULTRASONIC_TEST", "TRIG pin: GPIO%d, ECHO pin: GPIO%d", TRIG_PIN, ECHO_PIN);

    // Test TRIG pin - should toggle between 0 and 3.3V
    ESP_LOGI("ULTRASONIC_TEST", "Testing TRIG pin (use multimeter on GPIO%d):", TRIG_PIN);
    for (int i = 0; i < 5; i++) {
        gpio_set_level(TRIG_PIN, 1);
        ESP_LOGI("ULTRASONIC_TEST", "TRIG HIGH (should read ~3.3V)");
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        gpio_set_level(TRIG_PIN, 0);
        ESP_LOGI("ULTRASONIC_TEST", "TRIG LOW (should read ~0V)");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // Test ECHO pin reading
    ESP_LOGI("ULTRASONIC_TEST", "ECHO pin current state: %d", gpio_get_level(ECHO_PIN));
    ESP_LOGI("ULTRASONIC_TEST", "=== TEST COMPLETE ===");
}