#include "hid_device.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "oled_screen/oled_screen.h"
#include "esp_log.h"

static bool hid_device_ready = false;
static int current_volume_level = 50; // Track current volume level (0-100)

// TinyUSB descriptors (following official ESP-IDF example)
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

// HID Report Descriptor for Consumer Controls + System Controls
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(1)),
    TUD_HID_REPORT_DESC_SYSTEM_CONTROL(HID_REPORT_ID(2))
};

// String descriptor
const char* hid_string_descriptor[5] = {
    (char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "ESP32-S3",           // 1: Manufacturer
    "Desktop Controller", // 2: Product
    "123456",            // 3: Serials, should use chip ID
    "HID Interface",     // 4: HID
};

// Configuration descriptor
static const uint8_t hid_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

// TinyUSB HID callbacks
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return hid_report_descriptor;
}

// TinyUSB HID callbacks
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len) {
    (void) instance;
    (void) report;
    (void) len;
    // Report sent successfully
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

void tud_mount_cb(void) {
    ESP_LOGI(HID_DEVICE_TAG, "USB HID device mounted");
    hid_device_ready = true;
}

void tud_umount_cb(void) {
    ESP_LOGI(HID_DEVICE_TAG, "USB HID device unmounted");
    hid_device_ready = false;
}

esp_err_t hid_device_init(void) {
    ESP_LOGI(HID_DEVICE_TAG, "USB initialization");

    // Configure TinyUSB (following official ESP-IDF example)
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = hid_string_descriptor,
        .string_descriptor_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]),
        .external_phy = false,
        .configuration_descriptor = hid_configuration_descriptor,
    };

    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEVICE_TAG, "Failed to install TinyUSB driver: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(HID_DEVICE_TAG, "USB initialization DONE");
    return ESP_OK;
}

static esp_err_t send_consumer_report(uint16_t usage_code) {
    if (!tud_mounted()) {
        ESP_LOGW(HID_DEVICE_TAG, "HID device not mounted - connect USB OTG port to PC");
        return ESP_ERR_INVALID_STATE;
    }

    if (!tud_hid_ready()) {
        ESP_LOGW(HID_DEVICE_TAG, "HID device not ready");
        return ESP_ERR_INVALID_STATE;
    }

    // Send consumer control report (volume control)
    uint16_t consumer_report = usage_code;
    bool sent = tud_hid_report(1, &consumer_report, sizeof(consumer_report)); // Report ID 1
    if (!sent) {
        ESP_LOGE(HID_DEVICE_TAG, "Failed to send consumer report");
        return ESP_FAIL;
    }

    // Longer delay to ensure key press is registered properly
    vTaskDelay(pdMS_TO_TICKS(50));

    // Send key release (empty report) with retry
    consumer_report = 0;
    for (int retry = 0; retry < 3; retry++) {
        sent = tud_hid_report(1, &consumer_report, sizeof(consumer_report));
        if (sent) {
            break;
        }
        ESP_LOGW(HID_DEVICE_TAG, "Failed to send consumer release report, retry %d", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (!sent) {
        ESP_LOGE(HID_DEVICE_TAG, "Failed to send consumer release report after 3 retries");
        // Don't return error - the key press was sent successfully
    }

    return ESP_OK;
}

esp_err_t hid_volume_up(void) {
    ESP_LOGI(HID_DEVICE_TAG, "Sending Volume Up");

    // Update volume level and show UI
    current_volume_level += 5;
    if (current_volume_level > 100) current_volume_level = 100;

    ui_set_volume_context(current_volume_level);
    ui_force_refresh_state(UI_STATE_VOLUME, 3000); // Force show volume UI for 3 seconds

    return send_consumer_report(VOLUME_UP_USAGE);
}

esp_err_t hid_volume_down(void) {
    ESP_LOGI(HID_DEVICE_TAG, "Sending Volume Down");

    // Update volume level and show UI
    current_volume_level -= 5;
    if (current_volume_level < 0) current_volume_level = 0;

    ui_set_volume_context(current_volume_level);
    ui_force_refresh_state(UI_STATE_VOLUME, 3000); // Force show volume UI for 3 seconds

    return send_consumer_report(VOLUME_DOWN_USAGE);
}

esp_err_t hid_volume_mute(void) {
    ESP_LOGI(HID_DEVICE_TAG, "Sending Volume Mute");
    return send_consumer_report(VOLUME_MUTE_USAGE);
}

esp_err_t hid_media_play_pause(void) {
    ESP_LOGI(HID_DEVICE_TAG, "Sending Media Play/Pause");
    return send_consumer_report(MEDIA_PLAY_PAUSE_USAGE);
}

esp_err_t hid_media_next_track(void) {
    ESP_LOGI(HID_DEVICE_TAG, "Sending Media Next Track");
    return send_consumer_report(MEDIA_NEXT_TRACK_USAGE);
}

esp_err_t hid_media_prev_track(void) {
    ESP_LOGI(HID_DEVICE_TAG, "Sending Media Previous Track");
    return send_consumer_report(MEDIA_PREV_TRACK_USAGE);
}

esp_err_t hid_system_sleep(void) {
    static bool system_sleeping = false; // Track sleep state

    if (!tud_mounted() || !tud_hid_ready()) {
        ESP_LOGW(HID_DEVICE_TAG, "HID device not ready");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t system_code;
    if (system_sleeping) {
        // System is sleeping, send wake up
        system_code = 0x83; // System Wake Up
        system_sleeping = false;
        ESP_LOGI(HID_DEVICE_TAG, "Sending System Wake Up (0x83)");
    } else {
        // System is awake, send sleep
        system_code = 0x82; // System Sleep
        system_sleeping = true;
        ESP_LOGI(HID_DEVICE_TAG, "Sending System Sleep (0x82)");
    }

    // Send system control report (Report ID 2)
    bool sent = tud_hid_report(2, &system_code, sizeof(system_code));
    if (!sent) {
        ESP_LOGE(HID_DEVICE_TAG, "Failed to send system control report");
        return ESP_FAIL;
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    // Send release (empty report)
    uint8_t empty_report = 0;
    sent = tud_hid_report(2, &empty_report, sizeof(empty_report));
    if (!sent) {
        ESP_LOGE(HID_DEVICE_TAG, "Failed to send system control release");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t hid_send_key(uint8_t modifier, uint8_t keycode) {
    if (!hid_device_ready) {
        ESP_LOGW(HID_DEVICE_TAG, "HID device not ready");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Keyboard report structure: [modifier, reserved, key1, key2, key3, key4, key5, key6]
    uint8_t keyboard_report[8] = {0};
    keyboard_report[0] = modifier;
    keyboard_report[2] = keycode;
    
    // Send key press
    bool sent = tud_hid_report(0, keyboard_report, sizeof(keyboard_report));
    if (!sent) {
        ESP_LOGE(HID_DEVICE_TAG, "Failed to send keyboard report");
        return ESP_FAIL;
    }

    // Small delay
    vTaskDelay(pdMS_TO_TICKS(10));

    // Send key release (empty report)
    memset(keyboard_report, 0, sizeof(keyboard_report));
    sent = tud_hid_report(0, keyboard_report, sizeof(keyboard_report));
    if (!sent) {
        ESP_LOGE(HID_DEVICE_TAG, "Failed to send keyboard release report");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// Getter function for current volume level
int get_current_volume_level(void) {
    return current_volume_level;
}
