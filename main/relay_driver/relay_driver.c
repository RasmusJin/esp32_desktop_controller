#include "relay_driver.h"

// Initialize the relay GPIOs as outputs
void relay_driver_init(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,      // No interrupts
        .mode = GPIO_MODE_OUTPUT,            // Set as output
        .pin_bit_mask = (1ULL << RELAY_UP_PIN) | (1ULL << RELAY_DOWN_PIN),  // Relay GPIOs
        .pull_down_en = 0,                   // No pull-down
        .pull_up_en = 0                      // No pull-up
    };
    gpio_config(&io_conf);

    // Set both relays to off (LOW) initially
    gpio_set_level(RELAY_UP_PIN, 0);
    gpio_set_level(RELAY_DOWN_PIN, 0);
}

// Move desk up by setting the "Up" relay to HIGH
void move_desk_up(void) {
    gpio_set_level(RELAY_UP_PIN, 1);
}

// Stop moving up by setting the "Up" relay to LOW
void stop_moving_up(void) {
    gpio_set_level(RELAY_UP_PIN, 0);
}

// Move desk down by setting the "Down" relay to HIGH
void move_desk_down(void) {
    gpio_set_level(RELAY_DOWN_PIN, 1);
}

// Stop moving down by setting the "Down" relay to LOW
void stop_moving_down(void) {
    gpio_set_level(RELAY_DOWN_PIN, 0);
}
