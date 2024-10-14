#ifndef RELAY_DRIVER_H
#define RELAY_DRIVER_H

#include "driver/gpio.h"

// Define GPIO pins for the relay channels
#define RELAY_UP_PIN GPIO_NUM_40   // GPIO for "Up" relay
#define RELAY_DOWN_PIN GPIO_NUM_37  // GPIO for "Down" relay

// Initialize the relay pins
void relay_driver_init(void);

// Function to move the desk up (activate the "Up" relay)
void move_desk_up(void);

// Function to stop moving up (deactivate the "Up" relay)
void stop_moving_up(void);

// Function to move the desk down (activate the "Down" relay)
void move_desk_down(void);

// Function to stop moving down (deactivate the "Down" relay)
void stop_moving_down(void);

#endif // RELAY_DRIVER_H