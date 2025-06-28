/*
 * time_manager.c – robust SNTP helper for ESP-IDF ≤ v5.5 (lwIP wrapper)
 */

#include "time_manager.h"

#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <sys/time.h>
#include <string.h>

static const char *TAG = "TIME_MANAGER";
static bool        time_is_synced = false;

/* Copenhagen: UTC+1, DST last-Sun-Mar ↔ last-Sun-Oct */
#define COPENHAGEN_TZ  "CET-1CEST,M3.5.0/2,M10.5.0/3"

/*--------------------------------------------------------------------*/
/* public: one-shot init (optional)                                   */
/*--------------------------------------------------------------------*/
void time_manager_init(void)
{
    ESP_LOGI(TAG,
             "Time manager ready – SNTP will start once Wi-Fi is up");
}

/*--------------------------------------------------------------------*/
/* SNTP callback                                                      */
/*--------------------------------------------------------------------*/
static void sntp_sync_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "SNTP: first time sync");
    time_is_synced = true;
}

/*--------------------------------------------------------------------*/
/* wait until STA really has an IP & gateway                          */
/*--------------------------------------------------------------------*/
static void wait_for_ip(void)
{
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t info;

    while (true) {
        esp_netif_get_ip_info(sta, &info);
        if (info.gw.addr != 0) {           /* gateway non-zero → DHCP done */
            ESP_LOGI(TAG, "STA IP: " IPSTR ", GW: " IPSTR,
                     IP2STR(&info.ip), IP2STR(&info.gw));
            return;
        }
        ESP_LOGI(TAG, "Waiting for DHCP lease…");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*--------------------------------------------------------------------*/
/* core: configure, start and wait for SNTP                           */
/*--------------------------------------------------------------------*/
static void do_initial_sntp_sync(void)
{
    /* 1. stop any previous instance */
    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }

    /* 2. server list – use literal IPs to skip DNS on first boot     */
    /*    (enable DHCP-NTP in menuconfig to override these)           */
    esp_sntp_setservername(0, "216.239.35.4");   /* time.google.com  */
    esp_sntp_setservername(1, "13.86.101.172");  /* time.windows.com */
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_set_time_sync_notification_cb(sntp_sync_cb);

    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP started (poll mode)");

    /* 3. wait ≤60 s for first good packet                            */
    const int timeout_ms = 60 * 1000;
    int waited = 0;

    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET &&
           waited < timeout_ms)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        waited += 500;
    }

    if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        ESP_LOGI(TAG, "SNTP: initial sync after %d ms", waited);
        time_is_synced = true;
    } else {
        ESP_LOGW(TAG, "SNTP: timeout after 60 s");
    }
}

/*--------------------------------------------------------------------*/
/* local helper                                                       */
/*--------------------------------------------------------------------*/
static inline void apply_cph_tz(void)
{
    setenv("TZ", COPENHAGEN_TZ, 1);
    tzset();
}

/*--------------------------------------------------------------------*/
/* public: blocking one-shot sync                                     */
/*--------------------------------------------------------------------*/
void time_manager_sync_now(void)
{
    ESP_LOGI(TAG, "Starting one-shot NTP sync…");
    time_is_synced = false;

    wait_for_ip();
    do_initial_sntp_sync();

    if (time_is_synced) {
        apply_cph_tz();

        time_t now; struct tm tm;
        time(&now); localtime_r(&now, &tm);
        ESP_LOGI(TAG, "Local time: %04d-%02d-%02d %02d:%02d:%02d",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec);
    } else {
        ESP_LOGE(TAG, "Time sync failed");
    }
}

/*--------------------------------------------------------------------*/
/* background task – first sync + 24 h re-sync                        */
/*--------------------------------------------------------------------*/
static void time_sync_task(void *arg)
{
    ESP_LOGI(TAG, "Time sync task waiting for Wi-Fi…");

    /* wait until STA is associated (doesn’t guarantee IP yet) */
    wifi_ap_record_t dummy;
    while (esp_wifi_sta_get_ap_info(&dummy) != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "Wi-Fi associated");

    time_manager_sync_now();                       /* first sync */

    const TickType_t day = pdMS_TO_TICKS(24 * 60 * 60 * 1000);
    for (;;) {
        vTaskDelay(day);
        ESP_LOGI(TAG, "Daily NTP re-sync…");
        time_manager_sync_now();
    }
}

void time_manager_start_sync_task(void)
{
    xTaskCreate(time_sync_task, "time_sync",
                4096, NULL, 5, NULL);
}

/*--------------------------------------------------------------------*/
/* helpers for UI                                                     */
/*--------------------------------------------------------------------*/
void get_current_time_string(char *buf, size_t len)
{
    if (!time_is_synced) { snprintf(buf, len, "--:--"); return; }
    time_t now; struct tm tm;
    time(&now); localtime_r(&now, &tm);
    snprintf(buf, len, "%02d:%02d", tm.tm_hour, tm.tm_min);
}

void get_current_date_string(char *buf, size_t len)
{
    if (!time_is_synced) { snprintf(buf, len, "--/--"); return; }
    time_t now; struct tm tm;
    time(&now); localtime_r(&now, &tm);
    snprintf(buf, len, "%02d/%02d", tm.tm_mday, tm.tm_mon + 1);
}

bool is_time_synchronized(void)
{
    return time_is_synced;
}
