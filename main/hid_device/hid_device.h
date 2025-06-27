#ifndef HID_DEVICE_H
#define HID_DEVICE_H

#include "esp_err.h"
#include "esp_log.h"
#include "tusb.h"
#include "tinyusb.h"

#ifdef __cplusplus
extern "C" {
#endif

// HID device configuration
#define HID_DEVICE_TAG "HID_DEVICE"

// HID Report IDs
#define HID_REPORT_ID_KEYBOARD      1
#define HID_REPORT_ID_CONSUMER      2

// Volume control constants
#define VOLUME_UP_USAGE             0xE9
#define VOLUME_DOWN_USAGE           0xEA
#define VOLUME_MUTE_USAGE           0xE2

// Media control constants
#define MEDIA_PLAY_PAUSE_USAGE      0xCD
#define MEDIA_NEXT_TRACK_USAGE      0xB5
#define MEDIA_PREV_TRACK_USAGE      0xB6

// System control constants
#define SYSTEM_SLEEP_USAGE          0x82
#define SYSTEM_POWER_DOWN_USAGE     0x81

// Function declarations
esp_err_t hid_device_init(void);
esp_err_t hid_volume_up(void);
esp_err_t hid_volume_down(void);
esp_err_t hid_volume_mute(void);
esp_err_t hid_media_play_pause(void);
esp_err_t hid_media_next_track(void);
esp_err_t hid_media_prev_track(void);
esp_err_t hid_system_sleep(void);
esp_err_t hid_send_key(uint8_t modifier, uint8_t keycode);

// HID descriptor
extern const uint8_t hid_report_descriptor[];
extern const size_t hid_report_descriptor_len;

#ifdef __cplusplus
}
#endif

#endif // HID_DEVICE_H
