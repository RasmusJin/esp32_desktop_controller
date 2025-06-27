#ifndef fan_control_h
#define fan_control_h

#include <stdint.h>
#include <stdbool.h>

void potentiometer_init(void);
uint32_t potentiometer_read(void);
void potentiometer_print_value(void);
void fan_pwm_init(void);
void update_fan_speed(void);

// Live value getters for real-time UI updates
int get_current_fan_speed_percent(void);
bool get_current_fan_active(void);


#endif // fan_control_h