#ifndef NTP_TIME_H
#define NTP_TIME_H

#include <time.h>
#include <stdbool.h>

// NTP time synchronization functions
void ntp_time_init(void);
void ntp_time_sync(void);
bool ntp_time_is_synced(void);
void ntp_time_task(void *pvParameter);

// Time getter functions for UI
void get_current_time_string(char *time_str, size_t max_len);
void get_current_date_string(char *date_str, size_t max_len);
int get_current_hour(void);
int get_current_minute(void);
int get_current_second(void);
int get_current_day(void);
int get_current_month(void);
int get_current_year(void);

#endif // NTP_TIME_H
