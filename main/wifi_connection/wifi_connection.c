#include "wifi_connection.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "ssd1306.h"
#include <string.h>
#include "time.h"
#include "esp_sntp.h"

// Define a tag for logging
static const char *TAG = "WiFi";

// FreeRTOS event group to signal when Wi-Fi is connected
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
#define MAXIMUM_RETRY 10  // Increased retry attempts
static esp_netif_t *sta_netif = NULL;

// Event handler for Wi-Fi events
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi started, connecting...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "WiFi disconnected (reason: %d), retry %d/%d", disconnected->reason, s_retry_num + 1, MAXIMUM_RETRY);

        // Always try to reconnect (no retry limit for continuous operation)
        esp_wifi_connect();
        s_retry_num++;

        // Only set fail bit during initial connection attempts
        if (s_retry_num >= MAXIMUM_RETRY && !(xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT)) {
            ESP_LOGE(TAG, "Initial WiFi connection failed after %d attempts", MAXIMUM_RETRY);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

        char ip_str[16]; // Buffer to hold the IP address string
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));

        ESP_LOGI(TAG, "Connected! IP: %s", ip_str);

        s_retry_num = 0;  // Reset retry counter on successful connection
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);  // Clear fail bit
    }
}

// Configure static IP
static void configure_static_ip(void) {
    ESP_LOGI(TAG, "Configuring static IP: %s", STATIC_IP_ADDR);

    // Stop DHCP client
    esp_netif_dhcpc_stop(sta_netif);

    // Configure static IP
    esp_netif_ip_info_t ip_info;
    memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));

    ip_info.ip.addr = esp_ip4addr_aton(STATIC_IP_ADDR);
    ip_info.gw.addr = esp_ip4addr_aton(GATEWAY_ADDR);
    ip_info.netmask.addr = esp_ip4addr_aton(NETMASK_ADDR);

    esp_err_t ret = esp_netif_set_ip_info(sta_netif, &ip_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set static IP: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Static IP configured successfully");
    }
}

// Initialize Wi-Fi as a station (client)
void wifi_init_sta(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    s_wifi_event_group = xEventGroupCreate();

    ESP_LOGI(TAG, "Initializing Wi-Fi in station mode");

    esp_netif_init();
    esp_event_loop_create_default();
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,  // Minimum security
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    // Configure static IP before starting WiFi
    configure_static_ip();

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);

    // Disable power saving for better stability
    esp_wifi_set_ps(WIFI_PS_NONE);

    esp_wifi_start();

    ESP_LOGI(TAG, "Wi-Fi initialization done.");

    // Wait for Wi-Fi connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to Wi-Fi");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to Wi-Fi");
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void wifi_connect(void) {
    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    esp_wifi_connect();
}

bool wifi_poll_status(SSD1306_t *dev) {
    wifi_ap_record_t ap_info;
    esp_err_t status = esp_wifi_sta_get_ap_info(&ap_info);

    ssd1306_clear_line(dev, 7, false);  // Clear the bottom line (line 7)

    if (status == ESP_OK) {
        // Wi-Fi connected, print "WiFi: OK" and the RSSI (signal strength)
        char wifi_status[20];
        snprintf(wifi_status, sizeof(wifi_status), "WiFi: OK");
        ssd1306_display_text(dev, 7, wifi_status, strlen(wifi_status), false);
        ssd1306_show_buffer(dev);  // Refresh the OLED display
        return true;
    } else {
        // Wi-Fi not connected, print "No WiFi"
        ssd1306_display_text(dev, 7, "No WiFi", 7, false);
        ssd1306_show_buffer(dev);  // Refresh the OLED display
        return false;
    }
}

void initialize_sntp(void) {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");  // Use a reliable NTP server
    sntp_init();
}

void obtain_time(void) {
    time_t now;
    struct tm timeinfo;
    int retry = 0;
    const int retry_count = 10;

    initialize_sntp();

    // Set timezone for Copenhagen (CET/CEST)
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();

    while (timeinfo.tm_year < (2020 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (retry == retry_count) {
        ESP_LOGE(TAG, "Failed to get NTP time");
    } else {
        ESP_LOGI(TAG, "Time synchronized");
    }
}
void initialize_ntp_and_time(void) {
    obtain_time();  // Sync time from NTP
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGE(TAG, "Time not set properly");
    } else {
        ESP_LOGI(TAG, "Time synchronized");
    }
}

char *get_current_time_str(void) {
    static char time_str[6];  // Buffer for "HH:MM"
    time_t now;
    struct tm timeinfo;

    // Get the current time from the system clock (already synchronized with NTP)
    time(&now);
    localtime_r(&now, &timeinfo);

    // Format time as "HH:MM"
    strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);

    return time_str;
}

// Function to check WiFi connection and reconnect if needed
void wifi_check_and_reconnect(void) {
    wifi_ap_record_t ap_info;
    esp_err_t status = esp_wifi_sta_get_ap_info(&ap_info);

    if (status != ESP_OK) {
        // Not connected, try to reconnect
        ESP_LOGW(TAG, "WiFi not connected, attempting reconnection...");
        esp_wifi_connect();
    }
}