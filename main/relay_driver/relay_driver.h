#ifndef RELAY_DRIVER_H
#define RELAY_DRIVER_H

#include "driver/gpio.h"

// Define GPIO pins for the relay channels
#define RELAY_UP_PIN GPIO_NUM_47   // GPIO for "Up" relay
#define RELAY_DOWN_PIN GPIO_NUM_37  // GPIO for "Down" relay
#define RELAY_PC_SWITCH GPIO_NUM_36


#define TRIG_PIN GPIO_NUM_45  // Corrected: TRIG is GPIO45
#define ECHO_PIN GPIO_NUM_35  // Corrected: ECHO is GPIO35 (with voltage divider)

// Desk position limits (distance from desk to CEILING/SHELF in cm - sensor pointing UP)
#define DESK_LOWEST_POSITION_CM   62    // Desk at lowest position (farthest from ceiling)
#define DESK_HIGHEST_POSITION_CM  28    // Desk at highest position (closest to ceiling)
#define DESK_SITTING_PRESET_CM    60    // Preferred sitting height (low desk, far from ceiling)
#define DESK_STANDING_PRESET_CM   30    // Preferred standing height (high desk, close to ceiling)

// Desk position limits (based on your measurements)
#define DESK_MIN_HEIGHT_CM 25   // Minimum safe height (highest position)
#define DESK_MAX_HEIGHT_CM 65   // Maximum safe height (lowest position)
#define DESK_SITTING_HEIGHT_CM 60   // Preferred sitting height
#define DESK_STANDING_HEIGHT_CM 30  // Preferred standing height

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
void check_desk_safety_timeout(void);  // Safety timeout check
void init_ultrasonic_sensor() ;
uint32_t measure_distance();
void test_sensor_readings(void);  // Test function for sensor debugging
void test_ultrasonic_pins(void);  // Test function for debugging
#endif // RELAY_DRIVER_H