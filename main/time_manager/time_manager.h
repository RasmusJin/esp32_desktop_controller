#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <stdbool.h>
#include <stddef.h>     /* size_t */

/* optional; does nothing beyond a log line for now */
void time_manager_init(void);

/* start the background task (waits for Wi-Fi, first sync, daily re-sync) */
void time_manager_start_sync_task(void);

/* perform a blocking one-shot sync (≤60 s) */
void time_manager_sync_now(void);

/* helpers for UI */
void get_current_time_string(char *buf, size_t len);  /* “HH:MM” */
void get_current_date_string(char *buf, size_t len);  /* “DD/MM” */
bool is_time_synchronized(void);

#endif /* TIME_MANAGER_H */
