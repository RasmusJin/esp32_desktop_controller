#include "ssd1306.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "oled_screen.h"
#include "wifi_connection/wifi_connection.h"
#include "esp_wifi.h"
#include "keyswitches/keyswitches.h"
#include "fan_control/fan_control.h"
#include "relay_driver/relay_driver.h"
#include "hid_device/hid_device.h"
#include "time_manager/time_manager.h"
#include <string.h>
#include <time.h>
#include <stdio.h>

// Removed unused typedef - using simpler approach for font positioning

// Log tag
static const char *TAG = "OLED_UI";

// Global variable to track last displayed state for proper screen clearing
static ui_state_t last_displayed_state = UI_STATE_MAIN;

// Global UI context for the new display system (non-static for external access)
ui_context_t ui_ctx = {
    .current_state = UI_STATE_MAIN,
    .state_start_time = 0,
    .display_duration_ms = 3000,  // 3 seconds default
    .context = {
        // WiFi/Network defaults
        .wifi_connected = false,
        .ip_address = "0.0.0.0",

        // Desk control defaults
        .desk_height = 67.0,
        .desk_moving = false,
        .desk_moving_up = false,

        // Volume control defaults
        .volume_level = 50,
        .volume_direction_up = true, // Default to UP

        // Hue lighting defaults
        .hue_scene = "Unknown",
        .hue_brightness = 50,
        .hue_lights_on = false,

        // PC switching defaults
        .pc_number = 1,

        // Window control defaults
        .window_opening = false,
        .window_closing = false,
        .http_ack_received = false,

        // Fan control defaults
        .fan_speed_percent = 0,
        .fan_active = false,

        // Boot/Debug defaults
        .boot_message = "Starting...",
        .wifi_retry_count = 0,
        .http_connected = false,
        .error_message = ""
    }
};

// ============================================================================
// OLED INITIALIZATION (KEEP EXISTING - WORKS)
// ============================================================================

// Initialize OLED display - KEEP THIS WORKING FUNCTION
void oled_init(SSD1306_t *dev) {
    dev->_address = OLED_I2C_ADDRESS;
    dev->_width = 128;
    dev->_height = 64;
    dev->_pages = dev->_height / 8;
    dev->_i2c_num = I2C_NUM_0;

    // Initialize I2C and OLED
    i2c_master_init_custom(dev, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, RESET_PIN);
    ssd1306_init(dev, dev->_width, dev->_height);
    ESP_LOGI(TAG, "OLED initialized successfully");

    // Create the UI display task
    xTaskCreate(ui_display_task, "ui_display_task", 4096, (void *)dev, 5, NULL);

    ESP_LOGI(TAG, "UI display task created");
}

// I2C Initialization - KEEP THIS WORKING FUNCTION
void i2c_master_init_custom(SSD1306_t *dev, int16_t sda, int16_t scl, int16_t reset) {
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 1000000  // 1 MHz
    };
    i2c_param_config(dev->_i2c_num, &i2c_config);
    i2c_driver_install(dev->_i2c_num, I2C_MODE_MASTER, 0, 0, 0);

    if (reset != -1) {
        gpio_set_direction(reset, GPIO_MODE_OUTPUT);
        gpio_set_level(reset, 0);
        vTaskDelay(pdMS_TO_TICKS(10));  // Hold reset low for 10ms
        gpio_set_level(reset, 1);
        vTaskDelay(pdMS_TO_TICKS(10));  // Wait 10ms after reset
    }
    ESP_LOGI(TAG, "I2C initialized successfully");
}

// ============================================================================
// NEW UI UTILITY FUNCTIONS
// ============================================================================

// Clear the entire screen properly - handle SH1106 (132 bytes) vs SSD1306 (128 bytes)
void ui_clear_screen(SSD1306_t *dev) {
    // Removed excessive logging to reduce performance impact

    // Clear the standard 128 bytes in the buffer
    for (int page = 0; page < dev->_pages; page++) {
        for (int seg = 0; seg < 128; seg++) {
            dev->_page[page]._segs[seg] = 0x00;
        }
    }

    // For SH1106, we need to manually clear the extra 4 bytes per page
    // These are at columns 128-131 and don't get cleared by the standard library
    for (int page = 0; page < dev->_pages; page++) {
        // Set page address
        uint8_t page_cmd[] = {0x00, 0xB0 | page};
        i2c_master_write_to_device(dev->_i2c_num, dev->_address, page_cmd, sizeof(page_cmd), 1000 / portTICK_PERIOD_MS);

        // Set column address to 128 (start of extra bytes)
        uint8_t col_cmd[] = {0x00, 0x10 | ((128 >> 4) & 0x0F), 0x00 | (128 & 0x0F)};
        i2c_master_write_to_device(dev->_i2c_num, dev->_address, col_cmd, sizeof(col_cmd), 1000 / portTICK_PERIOD_MS);

        // Clear the 4 extra bytes
        uint8_t clear_data[] = {0x40, 0x00, 0x00, 0x00, 0x00}; // 0x40 = data mode, then 4 zero bytes
        i2c_master_write_to_device(dev->_i2c_num, dev->_address, clear_data, sizeof(clear_data), 1000 / portTICK_PERIOD_MS);
    }

    // Force display update to push the cleared buffer
    ssd1306_show_buffer(dev);
}

// Draw a progress bar
void ui_draw_progress_bar(SSD1306_t *dev, int x, int y, int width, int height, int percentage) {
    // Clamp percentage to 0-100
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;

    // Calculate filled width (for future use)
    (void)width; (void)height; // Suppress unused parameter warnings

    // Draw the progress bar outline (simplified - just filled blocks)
    char bar[32] = {0};
    int bar_chars = width / 6; // Approximate character width
    if (bar_chars > 31) bar_chars = 31;

    int filled_chars = (bar_chars * percentage) / 100;

    for (int i = 0; i < filled_chars; i++) {
        bar[i] = '=';
    }
    for (int i = filled_chars; i < bar_chars; i++) {
        bar[i] = '-';
    }

    ssd1306_display_text(dev, y / 8, bar, strlen(bar), false);
}

// Draw text centered on a page
void ui_draw_text_centered(SSD1306_t *dev, int page, const char* text) {
    int text_len = strlen(text);
    // For simplicity, just display the text (SSD1306 library handles positioning)
    // TODO: Implement proper centering calculation
    ssd1306_display_text(dev, page, (char*)text, text_len, false);
}

// Draw text right-aligned on a page
void ui_draw_text_right_aligned(SSD1306_t *dev, int page, const char* text) {
    // For simplicity, just display the text with some right padding
    char padded_text[32];
    int text_len = strlen(text);
    int padding = 16 - text_len; // Approximate right alignment
    if (padding < 0) padding = 0;

    memset(padded_text, ' ', padding);
    strcpy(padded_text + padding, text);

    ssd1306_display_text(dev, page, padded_text, strlen(padded_text), false);
}

// Custom function to display x3 text with pixel offset (for clock alignment)
void ui_display_text_x3_offset(SSD1306_t *dev, int page, char *text, int text_len, int offset_pixels, bool invert) {
    // Just use regular text with space padding instead of pixel shifting
    // This eliminates all the jumping and shifting artifacts

    // Clear the area
    for (int p = page; p < page + 3 && p < dev->_pages; p++) {
        for (int seg = 0; seg < 128; seg++) {
            dev->_page[p]._segs[seg] = 0x00;
        }
    }

    // Add a space at the beginning to simulate the 6-pixel offset
    char padded_text[8];
    snprintf(padded_text, sizeof(padded_text), " %s", text);

    // Use standard x3 text rendering - no shifting, no artifacts
    ssd1306_display_text_x3(dev, page, padded_text, strlen(padded_text), invert);
}

// ============================================================================
// NEW UI DISPLAY FUNCTIONS
// ============================================================================

// Main screen: Clock + WiFi status
void ui_show_main(SSD1306_t *dev) {
    // Only clear screen if we're switching to this state, not on every update
    if (last_displayed_state != UI_STATE_MAIN) {
        ui_clear_screen(dev);
        last_displayed_state = UI_STATE_MAIN;
    }

    // Check actual WiFi status
    wifi_ap_record_t ap_info;
    esp_err_t wifi_status = esp_wifi_sta_get_ap_info(&ap_info);
    bool wifi_connected = (wifi_status == ESP_OK);

    // Top line: WiFi status (pad to exactly 16 characters to fill 128 pixels)
    char wifi_line[17];
    if (wifi_connected) {
        strcpy(wifi_line, " WiFi: OK       "); // Exactly 16 chars
    } else {
        strcpy(wifi_line, " WiFi: --       "); // Exactly 16 chars
    }
    ssd1306_display_text(dev, 0, wifi_line, 16, false);

    // Get current time from time manager
    char time_str[8];
    get_current_time_string(time_str, sizeof(time_str));

    // Large time display - NO OFFSET for clock, it should stay in original position
    ssd1306_display_text_x3(dev, 2, time_str, strlen(time_str), false);

    // Date display (pad to exactly 16 characters to fill 128 pixels)
    char date_str[17];
    get_current_date_string(date_str + 1, sizeof(date_str) - 1); // +1 for space padding
    date_str[0] = ' '; // Add leading space
    // Pad with spaces to exactly 16 characters
    while (strlen(date_str) < 16) {
        strcat(date_str, " ");
    }
    ssd1306_display_text(dev, 6, date_str, 16, false);

    // Force buffer flush to display everything
    ssd1306_show_buffer(dev);
}

// Desk control screen
void ui_show_desk(SSD1306_t *dev) {
    // Only clear screen if we're switching to this state, not on every update
    if (last_displayed_state != UI_STATE_DESK) {
        ui_clear_screen(dev);
        last_displayed_state = UI_STATE_DESK;
    }

    // Get live distance reading
    uint32_t distance_cm = measure_distance();
    float live_height = (distance_cm > 0) ? distance_cm + 30.0 : ui_ctx.context.desk_height;

    // Title (1-space padding like main screen)
    if (ui_ctx.context.desk_moving) {
        char title[16];
        snprintf(title, sizeof(title), " DESK %s",
                ui_ctx.context.desk_moving_up ? "UP" : "DOWN");
        ssd1306_display_text(dev, 0, title, strlen(title), false);
    } else {
        ssd1306_display_text(dev, 0, " DESK STOPPED", 13, false);
    }

    // Height display (1-space padding for x3 text) - use live reading with fixed width
    char height_str[8];
    snprintf(height_str, sizeof(height_str), "%3.0fcm", live_height); // Fixed 3-digit width
    ui_display_text_x3_offset(dev, 2, height_str, 5, 6, false); // Always 5 chars: "123cm"

    // Status line (1-space padding)
    if (ui_ctx.context.desk_moving) {
        ssd1306_display_text(dev, 6, " Moving...", 10, false);
    } else {
        ssd1306_display_text(dev, 6, " Ready", 6, false);
    }

    ssd1306_show_buffer(dev);
}

// Volume control screen
void ui_show_volume(SSD1306_t *dev) {
    // Only clear screen if we're switching to this state, not on every update
    if (last_displayed_state != UI_STATE_VOLUME) {
        ui_clear_screen(dev);
        last_displayed_state = UI_STATE_VOLUME;
    }

    // Title (1-space padding like main screen)
    ssd1306_display_text(dev, 0, " VOL", 4, false);

    // Show volume direction instead of level (since HID can't read actual volume)
    char vol_text[5];

    // Show the direction of the last volume change
    if (ui_ctx.context.volume_direction_up) {
        strcpy(vol_text, "UP  ");
    } else {
        strcpy(vol_text, "DOWN");
    }

    ui_display_text_x3_offset(dev, 2, vol_text, 4, 6, false); // Always 4 chars: UP or DOWN

    // Control instructions (1-space padding)
    ssd1306_display_text(dev, 5, " Press buttons", 14, false);
    ssd1306_display_text(dev, 6, " to adjust", 10, false);

    ssd1306_show_buffer(dev);
}

// Hue lighting control screen
void ui_show_hue(SSD1306_t *dev) {
    // Only clear screen if we're switching to this state, not on every update
    if (last_displayed_state != UI_STATE_HUE) {
        ui_clear_screen(dev);
        last_displayed_state = UI_STATE_HUE;
    }

    // Get live values from keyswitches
    int live_brightness = get_current_hue_brightness();
    bool live_lights_on = get_current_hue_lights_on();
    const char* live_scene = get_current_hue_scene_name();

    // Title (1-space padding like main screen)
    ssd1306_display_text(dev, 0, " HUE", 4, false);

    // Light status (1-space padding for x3 text) - fixed width
    char status_str[8];
    snprintf(status_str, sizeof(status_str), "%-3s",
            live_lights_on ? "ON" : "OFF"); // Fixed 3-char width, left-aligned
    ui_display_text_x3_offset(dev, 2, status_str, 3, 6, false); // Always 3 chars

    // Scene and brightness on bottom line (1-space padding)
    char bright_str[50]; // Larger buffer to handle long scene names
    snprintf(bright_str, sizeof(bright_str), " %s %d%%", live_scene, live_brightness);
    // Truncate to 16 characters if needed
    if (strlen(bright_str) > 16) {
        bright_str[16] = '\0';
    }
    // Pad to 16 characters
    while (strlen(bright_str) < 16) {
        strcat(bright_str, " ");
    }
    ssd1306_display_text(dev, 6, bright_str, 16, false);

    ssd1306_show_buffer(dev);
}

// PC switching screen
void ui_show_pc_switch(SSD1306_t *dev) {
    // Only clear screen if we're switching to this state, not on every update
    if (last_displayed_state != UI_STATE_PC_SWITCH) {
        ui_clear_screen(dev);
        last_displayed_state = UI_STATE_PC_SWITCH;
    }

    // Title (1-space padding like main screen)
    ssd1306_display_text(dev, 0, " USB", 4, false);

    // PC number (1-space padding for x3 text) - fixed width
    char pc_str[8];
    snprintf(pc_str, sizeof(pc_str), "PC%d", ui_ctx.context.pc_number);
    ui_display_text_x3_offset(dev, 2, pc_str, 3, 6, false); // Always 3 chars: "PC1" or "PC2"

    ssd1306_show_buffer(dev);
}

// Window control screen
void ui_show_window(SSD1306_t *dev) {
    // Only clear screen if we're switching to this state, not on every update
    if (last_displayed_state != UI_STATE_WINDOW) {
        ui_clear_screen(dev);
        last_displayed_state = UI_STATE_WINDOW;
    }

    // Title (1-space padding like main screen)
    ssd1306_display_text(dev, 0, " WINDOW", 7, false);

    // Action status with descriptive text
    if (ui_ctx.context.window_opening) {
        ssd1306_display_text(dev, 2, " Window:", 8, false);
        ssd1306_display_text(dev, 3, " opening", 8, false);
    } else if (ui_ctx.context.window_closing) {
        ssd1306_display_text(dev, 2, " Window:", 8, false);
        ssd1306_display_text(dev, 3, " closing", 8, false);
    } else {
        ssd1306_display_text(dev, 2, " Window:", 8, false);
        ssd1306_display_text(dev, 3, " idle", 5, false);
    }

    // HTTP acknowledgment status (1-space padding)
    if (ui_ctx.context.http_ack_received) {
        ssd1306_display_text(dev, 6, " Success!", 9, false);
    } else {
        ssd1306_display_text(dev, 6, " Sending...", 11, false);
    }

    ssd1306_show_buffer(dev);
}

// Fan control screen
void ui_show_fan(SSD1306_t *dev) {
    // Only clear screen if we're switching to this state, not on every update
    if (last_displayed_state != UI_STATE_FAN) {
        ui_clear_screen(dev);
        last_displayed_state = UI_STATE_FAN;
    }

    // Get live fan values
    int live_fan_percent = get_current_fan_speed_percent();
    bool live_fan_active = get_current_fan_active();

    // Title (1-space padding like main screen)
    ssd1306_display_text(dev, 0, " FAN", 4, false);

    if (live_fan_active) {
        // Fan percentage (1-space padding for x3 text) - fixed width
        char fan_str[8];
        snprintf(fan_str, sizeof(fan_str), "%3d%%", live_fan_percent); // Fixed 3-digit width
        ui_display_text_x3_offset(dev, 2, fan_str, 4, 6, false); // Always 4 chars: "100%"

        // Simple fan speed bar using text (1-space padding)
        char bar[17] = " ";
        int filled = live_fan_percent / 10; // 0-10 scale
        for (int i = 0; i < filled && i < 10; i++) {
            bar[i + 1] = '=';
        }
        for (int i = filled; i < 10; i++) {
            bar[i + 1] = '-';
        }
        // Pad to 16 characters
        while (strlen(bar) < 16) {
            strcat(bar, " ");
        }
        ssd1306_display_text(dev, 6, bar, 16, false);
    } else {
        // Fan off message (1-space padding for x3 text) - fixed width
        ui_display_text_x3_offset(dev, 2, "OFF ", 4, 6, false); // Padded to 4 chars like percentages
    }

    ssd1306_show_buffer(dev);
}

// Suspend/sleep screen
void ui_show_suspend(SSD1306_t *dev) {
    // Only clear screen if we're switching to this state, not on every update
    if (last_displayed_state != UI_STATE_SUSPEND) {
        ui_clear_screen(dev);
        last_displayed_state = UI_STATE_SUSPEND;
    }

    // Title (1-space padding like main screen)
    ssd1306_display_text(dev, 0, " SUSPEND", 8, false);

    // PC number and action (1-space padding for x3 text) - fixed width
    char suspend_str[8];
    snprintf(suspend_str, sizeof(suspend_str), "PC%d", ui_ctx.context.pc_number);
    ui_display_text_x3_offset(dev, 2, suspend_str, 3, 6, false); // Always 3 chars: "PC1" or "PC2"

    // Status (1-space padding)
    ssd1306_display_text(dev, 6, " Sleeping...", 12, false);

    ssd1306_show_buffer(dev);
}

// Boot information screen
void ui_show_boot_info(SSD1306_t *dev) {
    // Only clear screen if we're switching to this state, not on every update
    if (last_displayed_state != UI_STATE_BOOT_INFO) {
        ui_clear_screen(dev);
        last_displayed_state = UI_STATE_BOOT_INFO;
    }

    // Title
    ssd1306_display_text(dev, 0, " BOOT INFO", 10, false);

    // Boot message (truncate if too long)
    char boot_line[17];
    snprintf(boot_line, sizeof(boot_line), " %.14s", ui_ctx.context.boot_message); // Limit to 14 chars + space
    while (strlen(boot_line) < 16) strcat(boot_line, " ");
    ssd1306_display_text(dev, 2, boot_line, 16, false);

    // WiFi info
    char wifi_line[17];
    if (ui_ctx.context.wifi_retry_count > 0 && ui_ctx.context.wifi_retry_count < 100) {
        snprintf(wifi_line, sizeof(wifi_line), " WiFi:%s(%d)",
                 ui_ctx.context.wifi_connected ? "OK" : "FAIL", ui_ctx.context.wifi_retry_count);
    } else {
        snprintf(wifi_line, sizeof(wifi_line), " WiFi: %s",
                 ui_ctx.context.wifi_connected ? "OK" : "FAIL");
    }
    while (strlen(wifi_line) < 16) strcat(wifi_line, " ");
    ssd1306_display_text(dev, 3, wifi_line, 16, false);

    // IP Address
    char ip_line[17];
    snprintf(ip_line, sizeof(ip_line), " %s", ui_ctx.context.ip_address);
    while (strlen(ip_line) < 16) strcat(ip_line, " ");
    ssd1306_display_text(dev, 4, ip_line, 16, false);

    // HTTP status
    char http_line[17];
    snprintf(http_line, sizeof(http_line), " HTTP: %s",
             ui_ctx.context.http_connected ? "OK" : "FAIL");
    while (strlen(http_line) < 16) strcat(http_line, " ");
    ssd1306_display_text(dev, 5, http_line, 16, false);

    // Error message (if any)
    if (strlen(ui_ctx.context.error_message) > 0) {
        char error_line[17];
        snprintf(error_line, sizeof(error_line), " ERR:%.10s", ui_ctx.context.error_message); // Limit to 10 chars
        while (strlen(error_line) < 16) strcat(error_line, " ");
        ssd1306_display_text(dev, 6, error_line, 16, false);
    }

    ssd1306_show_buffer(dev);
}

// Debug/Reboot screen
void ui_show_debug_reboot(SSD1306_t *dev) {
    // Only clear screen if we're switching to this state, not on every update
    if (last_displayed_state != UI_STATE_DEBUG_REBOOT) {
        ui_clear_screen(dev);
        last_displayed_state = UI_STATE_DEBUG_REBOOT;
    }

    // Title
    ssd1306_display_text(dev, 0, " DEBUG REBOOT", 13, false);

    // Reboot message - use small font since "REBOOTING" is too long for large font
    ssd1306_display_text(dev, 2, " REBOOTING...", 13, false);
    ssd1306_display_text(dev, 3, " Please wait", 12, false);

    // Instructions
    ssd1306_display_text(dev, 5, " Hold SW2+SW3", 13, false);
    ssd1306_display_text(dev, 6, " to restart", 11, false);

    ssd1306_show_buffer(dev);
}

// ============================================================================
// NEW UI STATE MANAGEMENT
// ============================================================================

// Set the current UI state with duration
void ui_set_state(ui_state_t state, uint32_t duration_ms) {
    ui_ctx.current_state = state;
    ui_ctx.state_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ui_ctx.display_duration_ms = duration_ms;
    ESP_LOGI(TAG, "UI state changed to %d for %d ms", state, duration_ms);
}

// Force refresh the UI state (always shows even if same state)
void ui_force_refresh_state(ui_state_t state, uint32_t duration_ms) {
    ui_ctx.current_state = state;
    ui_ctx.state_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ui_ctx.display_duration_ms = duration_ms;
    ESP_LOGI(TAG, "UI state force refreshed to %d for %d ms", state, duration_ms);
}

// Check if we should return to main screen
bool ui_should_return_to_main(void) {
    if (ui_ctx.current_state == UI_STATE_MAIN) {
        return false;
    }

    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return (current_time - ui_ctx.state_start_time) >= ui_ctx.display_duration_ms;
}

// Check if we should update desk distance (for live updates)
bool ui_should_update_desk_distance(void) {
    return (ui_ctx.current_state == UI_STATE_DESK && !ui_ctx.context.desk_moving);
}

// Main UI update function - displays current state
void ui_update_display(SSD1306_t *dev) {
    // Check if we should return to main screen
    if (ui_should_return_to_main()) {
        ui_ctx.current_state = UI_STATE_MAIN;
        ESP_LOGI(TAG, "Returning to main screen");
    }

    // Display based on current state
    switch (ui_ctx.current_state) {
        case UI_STATE_MAIN:
            ui_show_main(dev);
            break;

        case UI_STATE_DESK:
            ui_show_desk(dev);
            break;

        case UI_STATE_VOLUME:
            ui_show_volume(dev);
            break;

        case UI_STATE_HUE:
            ui_show_hue(dev);
            break;

        case UI_STATE_PC_SWITCH:
            ui_show_pc_switch(dev);
            break;

        case UI_STATE_WINDOW:
            ui_show_window(dev);
            break;

        case UI_STATE_FAN:
            ui_show_fan(dev);
            break;

        case UI_STATE_SUSPEND:
            ui_show_suspend(dev);
            break;

        case UI_STATE_BOOT_INFO:
            ui_show_boot_info(dev);
            break;

        case UI_STATE_DEBUG_REBOOT:
            ui_show_debug_reboot(dev);
            break;

        default:
            ESP_LOGW(TAG, "Unknown UI state: %d", ui_ctx.current_state);
            ui_ctx.current_state = UI_STATE_MAIN;
            ui_show_main(dev);
            break;
    }
}

// ============================================================================
// NEW UI CONTEXT SETTERS
// ============================================================================

// Set WiFi status
void ui_set_wifi_status(bool connected, const char* ip) {
    ui_ctx.context.wifi_connected = connected;
    if (ip) {
        strncpy(ui_ctx.context.ip_address, ip, sizeof(ui_ctx.context.ip_address) - 1);
        ui_ctx.context.ip_address[sizeof(ui_ctx.context.ip_address) - 1] = '\0';
    }
}

// Set desk context
void ui_set_desk_context(float height, bool moving, bool moving_up) {
    ui_ctx.context.desk_height = height;
    ui_ctx.context.desk_moving = moving;
    ui_ctx.context.desk_moving_up = moving_up;
}

// Set volume context
void ui_set_volume_context(int volume_percent) {
    if (volume_percent < 0) volume_percent = 0;
    if (volume_percent > 100) volume_percent = 100;
    ui_ctx.context.volume_level = volume_percent;
}

// Set Hue context
void ui_set_hue_context(const char* scene, int brightness, bool lights_on) {
    if (scene) {
        strncpy(ui_ctx.context.hue_scene, scene, sizeof(ui_ctx.context.hue_scene) - 1);
        ui_ctx.context.hue_scene[sizeof(ui_ctx.context.hue_scene) - 1] = '\0';
    }
    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;
    ui_ctx.context.hue_brightness = brightness;
    ui_ctx.context.hue_lights_on = lights_on;
}

// Set PC context
void ui_set_pc_context(int pc_number) {
    ui_ctx.context.pc_number = pc_number;
}

// Set window context
void ui_set_window_context(bool opening, bool closing, bool ack) {
    ui_ctx.context.window_opening = opening;
    ui_ctx.context.window_closing = closing;
    ui_ctx.context.http_ack_received = ack;
}

// Set fan context
void ui_set_fan_context(int fan_percent, bool active) {
    if (fan_percent < 0) fan_percent = 0;
    if (fan_percent > 100) fan_percent = 100;
    ui_ctx.context.fan_speed_percent = fan_percent;
    ui_ctx.context.fan_active = active;
}

// Safe volume access functions for HID device
int get_current_ui_volume_level(void) {
    return ui_ctx.context.volume_level;
}

void set_ui_volume_level(int volume) {
    ui_set_volume_context(volume);
}

// Set volume direction for display
void set_ui_volume_direction(bool direction_up) {
    ui_ctx.context.volume_direction_up = direction_up;
}

// Set boot/debug context
void ui_set_boot_context(const char* message, int wifi_retries, bool http_ok, const char* error) {
    if (message) {
        strncpy(ui_ctx.context.boot_message, message, sizeof(ui_ctx.context.boot_message) - 1);
        ui_ctx.context.boot_message[sizeof(ui_ctx.context.boot_message) - 1] = '\0';
    }
    ui_ctx.context.wifi_retry_count = wifi_retries;
    ui_ctx.context.http_connected = http_ok;
    if (error) {
        strncpy(ui_ctx.context.error_message, error, sizeof(ui_ctx.context.error_message) - 1);
        ui_ctx.context.error_message[sizeof(ui_ctx.context.error_message) - 1] = '\0';
    }
}

// Set WiFi context
void ui_set_wifi_context(bool connected, const char* ip_address) {
    ui_ctx.context.wifi_connected = connected;
    if (ip_address) {
        strncpy(ui_ctx.context.ip_address, ip_address, sizeof(ui_ctx.context.ip_address) - 1);
        ui_ctx.context.ip_address[sizeof(ui_ctx.context.ip_address) - 1] = '\0';
    }
}

// ============================================================================
// NEW UI DISPLAY TASK
// ============================================================================

// Main UI display task - updates display when needed
void ui_display_task(void *pvParameter) {
    SSD1306_t *dev = (SSD1306_t *)pvParameter;

    ESP_LOGI(TAG, "UI display task started");

    // Initial display
    ui_update_display(dev);

    uint32_t last_update_time = 0;
    ui_state_t last_state = UI_STATE_MAIN;

    while (1) {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // Only update if state changed or if we're on main screen and 1 minute has passed (for clock)
        bool should_update = false;

        if (ui_ctx.current_state != last_state) {
            should_update = true;
            last_state = ui_ctx.current_state;
        } else if (ui_ctx.current_state == UI_STATE_MAIN &&
                   (current_time - last_update_time) >= 60000) { // 60 seconds for clock update
            should_update = true;
        } else if (ui_should_return_to_main()) {
            should_update = true;
        } else if (ui_ctx.current_state == UI_STATE_HUE &&
                   (current_time - last_update_time) >= 500) { // Update Hue UI every 500ms for live updates
            should_update = true;
        } else if (ui_ctx.current_state == UI_STATE_DESK &&
                   (current_time - last_update_time) >= 500) { // Update desk UI every 500ms for live distance
            should_update = true;
        } else if (ui_ctx.current_state == UI_STATE_FAN &&
                   (current_time - last_update_time) >= 500) { // Update fan UI every 500ms for live speed
            should_update = true;
        } else if (ui_ctx.current_state == UI_STATE_VOLUME &&
                   (current_time - last_update_time) >= 500) { // Update volume UI every 500ms for live updates
            should_update = true;
        }

        if (should_update) {
            ui_update_display(dev);
            last_update_time = current_time;
        }

        // Update every 250ms to reduce blinking but stay responsive
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

// ============================================================================
// LEGACY FUNCTIONS - For backward compatibility
// ============================================================================

// Legacy function for sending display events (now just logs)
bool oled_send_display_event(display_event_t *event) {
    if (!event) {
        return false;
    }

    ESP_LOGI(TAG, "Legacy display event: type=%d, text='%s'",
             event->event_type, event->display_text);

    // For now, just log the event. Could be extended to trigger UI updates
    // based on event type if needed for backward compatibility
    return true;
}

// Legacy function for displaying event messages (now just logs)
void display_event_message(SSD1306_t *dev, const char *message, int display_time_ms) {
    if (!dev || !message) {
        return;
    }

    ESP_LOGI(TAG, "Legacy event message: '%s' for %d ms", message, display_time_ms);

    // For now, just log the message. Could be extended to show temporary
    // messages on the display if needed for backward compatibility
}



