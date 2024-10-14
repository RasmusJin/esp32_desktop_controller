#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "ssd1306.h"

#define WIFI_SSID "NeverNude"
#define WIFI_PASS "RasmusEr4Sej"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

void wifi_init_sta(void);
void wifi_connect(void);
bool wifi_poll_status(SSD1306_t *dev);
void obtain_time(void);
void initialize_sntp(void);
void initialize_ntp_and_time(void);
char *get_current_time_str(void);
#endif // WIFI_CONNECTION_H