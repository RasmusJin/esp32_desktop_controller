#ifndef RELAY_DRIVER_H
#define RELAY_DRIVER_H

#include "driver/gpio.h"

// Define GPIO pins for the relay channels
#define RELAY_UP_PIN GPIO_NUM_47   // GPIO for "Up" relay
#define RELAY_DOWN_PIN GPIO_NUM_37  // GPIO for "Down" relay
#define RELAY_PC_SWITCH GPIO_NUM_36


#define TRIG_PIN GPIO_NUM_35
#define ECHO_PIN GPIO_NUM_45

typedef enum {
    DESK_MOVE_UP,
    DESK_MOVE_DOWN,
    DESK_STOP
} desk_move_t;
// Initialize the relay pins
void relay_driver_init(void);
void switch_pc(void);
// Function to move the desk up (activate the "Up" relay)
void start_moving_desk(desk_move_t direction);

// Function to stop moving up (deactivate the "Up" relay)
void stop_moving_desk(void);
void init_ultrasonic_sensor() ;
uint32_t measure_distance();
#endif // RELAY_DRIVER_H