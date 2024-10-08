#ifndef fan_control_h
#define fan_control_h

#include <stdint.h>

void potentiometer_init(void);
uint32_t potentiometer_read(void);
void potentiometer_print_value(void);
void fan_pwm_init(void);
void update_fan_speed(void);


#endif // fan_control_h