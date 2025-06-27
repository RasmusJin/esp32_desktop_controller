#include "keyswitches.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "http/http_client_server.h"
#include "ssd1306.h"
#include "oled_screen/oled_screen.h"
#include "wifi_connection/wifi_connection.h"
#include "relay_driver/relay_driver.h"
#include "hid_device/hid_device.h"

static const char *KEYTAG = "KEYSWITCHES";
bool lights_on = false;
int brightness_value = 255;  // Start at max brightness
int brightness_step = 25;
int current_scene = 1;  // Track current Hue scene (1-4)

#define DEBOUNCE_DELAY_MS 150  // Debounce delay of 150 ms
#define TAP_THRESHOLD_MS 500
// Define desk movement states
typedef enum {
    DESK_IDLE,
    DESK_MOVING_UP,
    DESK_MOVING_DOWN
} desk_state_t;

desk_state_t desk_state = DESK_IDLE;
bool button_up_pressed = false;
bool button_down_pressed = false;
uint32_t button_press_time = 0;
// Rotary Encoder Variables
// Rotary Encoder 1:
int32_t encoder1_value = 0;
int previous_rot1_clk = 1;
int previous_rot1_dt = 1;  // Previous state of the DT pin
int previous_rot1_sw = 1;  // Previous state of the switch
uint32_t last_press_time_rot1_sw = 0;  // Store the last press time for debounce
// Rotary Encoder 2:
int32_t encoder2_value = 0;
int previous_rot2_clk = 1;
int previous_rot2_dt = 1;
int previous_rot2_sw = 1;
uint32_t last_press_time_rot2_sw = 0;  

// Function to handle debouncing for any GPIO pin
bool debounce(uint32_t current_time, int gpio_pin, uint32_t *last_press_time) {
    if (gpio_get_level(gpio_pin) == 0 && (current_time - *last_press_time > DEBOUNCE_DELAY_MS)) {
        *last_press_time = current_time;
        return true;  // Debounce successful, register press
    }
    return false;  // Debounce not met, do not register press
}

static uint32_t last_press_time_single_row[5] = {0};  // Store last press time for each single-row switch
static uint32_t last_press_time_matrix[2][3] = {{0}}; // Store last press time for each matrix switch

void setup_switch_single_row(void) {
    // Configure single row GPIOs as input with pull-up resistors
    gpio_config_t single_row_io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,  // Use pull-up resistors
        .pin_bit_mask = (1ULL << KEY_GPIO1) | (1ULL << KEY_GPIO2) | 
                        (1ULL << KEY_GPIO3) | (1ULL << KEY_GPIO4) | 
                        (1ULL << KEY_GPIO5),
    };
    gpio_config(&single_row_io_conf);

    setup_switch_matrix();
    setup_rotary_encoders();  // Re-enabled - 3.3V rail fixed!
}

// Function to get current Hue light state on startup
void get_current_hue_state(void) {
    ESP_LOGI(KEYTAG, "Fetching current Hue group state...");

    // Make HTTP GET request to get current group state
    esp_http_client_config_t config = {
        .url = "http://192.168.50.170/api/AicZqASmH6YLHxDyBxD-pci3vEmn0jLU0XvQ9g9N/groups/1",
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);

        if (status_code == 200) {
            // Read response data
            char response_buffer[512];
            int data_read = esp_http_client_read_response(client, response_buffer, sizeof(response_buffer) - 1);
            if (data_read > 0) {
                response_buffer[data_read] = '\0';
                ESP_LOGI(KEYTAG, "Hue group response: %s", response_buffer);

                // Parse JSON to check if lights are on
                // Look for "all_on":true or "any_on":true in the response
                if (strstr(response_buffer, "\"any_on\":true") != NULL) {
                    lights_on = true;
                    ESP_LOGI(KEYTAG, "Detected lights are currently ON");
                } else {
                    lights_on = false;
                    ESP_LOGI(KEYTAG, "Detected lights are currently OFF");
                }
            } else {
                ESP_LOGW(KEYTAG, "No response data received, assuming lights OFF");
                lights_on = false;
            }
        } else {
            ESP_LOGW(KEYTAG, "HTTP GET failed with status %d, assuming lights OFF", status_code);
            lights_on = false;
        }
    } else {
        ESP_LOGE(KEYTAG, "HTTP GET request failed: %s, assuming lights OFF", esp_err_to_name(err));
        lights_on = false;
    }

    esp_http_client_cleanup(client);
}

void setup_switch_matrix(void) {
    // Configure rows as output
    gpio_config_t row_io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << ROW1_PIN) | (1ULL << ROW2_PIN),
    };
    gpio_config(&row_io_conf);

    // Configure columns as input with pull-up resistors
    gpio_config_t col_io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pin_bit_mask = (1ULL << COL1_PIN) | (1ULL << COL2_PIN) | (1ULL << COL3_PIN),
    };
    gpio_config(&col_io_conf);
}

void setup_rotary_encoders(void) {
    // Configure rotary encoder 1 GPIOs
    gpio_config_t io_conf1 = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,  // Use pull-up resistors
        .pin_bit_mask = (1ULL << ROT1_CLK) | (1ULL << ROT1_DT) | (1ULL << ROT1_SW),
    };
    gpio_config(&io_conf1);

    // Configure rotary encoder 2 GPIOs
    gpio_config_t io_conf2 = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,  // Use pull-up resistors
        .pin_bit_mask = (1ULL << ROT2_CLK) | (1ULL << ROT2_DT) | (1ULL << ROT2_SW),
    };
    gpio_config(&io_conf2);

    ESP_LOGI(KEYTAG, "Both rotary encoders configured - ROT1: brightness, ROT2: scenes");

    // Get current Hue state on boot so buttons work immediately
    ESP_LOGI(KEYTAG, "Getting current Hue light state...");
    get_current_hue_state();
    ESP_LOGI(KEYTAG, "Hue state initialized: lights_on = %s", lights_on ? "true" : "false");
}

void poll_rotary_encoders_task(void *pvParameter) {
    SSD1306_t *dev = (SSD1306_t *)pvParameter;
    while (1) {
        poll_rotary_encoders(dev);  // Call the rotary encoder polling function
        vTaskDelay(10 / portTICK_PERIOD_MS);  // Small delay to prevent CPU overload
    }
}
void poll_rotary_encoders(SSD1306_t *dev) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    display_event_t event;
    event.event_type = DISPLAY_UPDATE_LIGHT_STATUS;
    // Poll Rotary Encoder 1 (Brightness Control)
    int rot1_clk = gpio_get_level(ROT1_CLK);
    int rot1_dt = gpio_get_level(ROT1_DT);
    int rot1_sw = gpio_get_level(ROT1_SW);

    // Button Press Detection with Debouncing for Encoder 1
    if (rot1_sw == 0 && previous_rot1_sw == 1 && (current_time - last_press_time_rot1_sw) > DEBOUNCE_DELAY_MS) {
        ESP_LOGI(KEYTAG, "Rotary Encoder 1 Button Pressed!");
        if (lights_on) {
            // Send a command to turn off the lights
            hue_send_command("http://192.168.50.170/api/AicZqASmH6YLHxDyBxD-pci3vEmn0jLU0XvQ9g9N/groups/1/action", "{\"on\": false}");
            lights_on = false;
        } else {
            // Send a command to turn on the lights
            hue_send_command("http://192.168.50.170/api/AicZqASmH6YLHxDyBxD-pci3vEmn0jLU0XvQ9g9N/groups/1/action", "{\"on\": true}");
            lights_on = true;
        }
        snprintf(event.display_text, sizeof(event.display_text), "Lights: %s", lights_on ? "ON" : "OFF");
        event.event_type = DISPLAY_UPDATE_LIGHT_STATUS;
        oled_send_display_event(&event); 
        last_press_time_rot1_sw = current_time;
    }
    previous_rot1_sw = rot1_sw;

    // Rotary Encoder 1 Turning Logic (Edge Detection) - FIXED DIRECTION
    if (rot1_clk != previous_rot1_clk) {  // Only act when CLK changes state
        if (rot1_clk == 0) {  // Detect the falling edge of CLK
            if (rot1_dt == 0) {  // SWAPPED: DT=0 means clockwise
                // Clockwise, increase brightness
                brightness_value = brightness_value + 25;
                if (brightness_value > 255) {
                brightness_value = 255;
                }
                ESP_LOGI(KEYTAG, "Rotary Encoder 1 turned Clockwise, increasing brightness to %d", brightness_value);
            } else {  // SWAPPED: DT=1 means counterclockwise
                // Counterclockwise, decrease brightness
                brightness_value = brightness_value - 25;
                if (brightness_value < 0) {
                brightness_value = 0;
                }
                ESP_LOGI(KEYTAG, "Rotary Encoder 1 turned Counterclockwise, decreasing brightness to %d", brightness_value);
            }
            // Send brightness command to the Hue lights
            hue_set_group_brightness(brightness_value);
            snprintf(event.display_text, sizeof(event.display_text), "Brightness: %d", brightness_value);
            event.event_type = DISPLAY_UPDATE_LIGHT_STATUS;  // Reuse event type for brightness
            oled_send_display_event(&event);
            }
    }
    previous_rot1_clk = rot1_clk;

    // Poll Rotary Encoder 2 (for hue lights or other actions)
    int rot2_clk = gpio_get_level(ROT2_CLK);
    int rot2_dt = gpio_get_level(ROT2_DT);
    int rot2_sw = gpio_get_level(ROT2_SW);

    // Button Press Detection with Debouncing for Encoder 2 (Light Toggle)
    if (rot2_sw == 0 && previous_rot2_sw == 1 && (current_time - last_press_time_rot2_sw) > DEBOUNCE_DELAY_MS) {
        ESP_LOGI(KEYTAG, "Rotary Encoder 2 Button Pressed! - Toggling lights");
        if (lights_on) {
            // Turn off the lights
            hue_send_command("http://192.168.50.170/api/AicZqASmH6YLHxDyBxD-pci3vEmn0jLU0XvQ9g9N/groups/1/action", "{\"on\": false}");
            lights_on = false;
            ESP_LOGI(KEYTAG, "Hue lights turned OFF");
        } else {
            // Turn on the lights
            hue_send_command("http://192.168.50.170/api/AicZqASmH6YLHxDyBxD-pci3vEmn0jLU0XvQ9g9N/groups/1/action", "{\"on\": true}");
            lights_on = true;
            ESP_LOGI(KEYTAG, "Hue lights turned ON");
        }
        last_press_time_rot2_sw = current_time;  // Update last press time
    }
    previous_rot2_sw = rot2_sw;

    // Rotary Encoder 2 Turning Logic (Scene Switching) - FIXED DIRECTION
    if (rot2_clk != previous_rot2_clk) {
        if (rot2_clk == 0) {  // Detect the falling edge of CLK
            if (rot2_dt == 0) {  // SWAPPED: DT=0 means clockwise
                // Clockwise - next scene
                current_scene++;
                if (current_scene > 7) {
                    current_scene = 1;
                }
                ESP_LOGI(KEYTAG, "Rotary Encoder 2 turned Clockwise - Next scene");
            } else {  // SWAPPED: DT=1 means counterclockwise
                // Counterclockwise - previous scene
                current_scene--;
                if (current_scene < 1) {
                    current_scene = 7;
                }
                ESP_LOGI(KEYTAG, "Rotary Encoder 2 turned Counterclockwise - Previous scene");
            }

            // Send scene command to Hue using actual scene IDs
            char scene_command[200];
            switch (current_scene) {
                case 1: // Energize
                    snprintf(scene_command, sizeof(scene_command),
                            "{\"scene\": \"yeMUOrmyqMim52B\"}");
                    ESP_LOGI(KEYTAG, "Scene %d: Få ny energi (Energize)", current_scene);
                    break;
                case 2: // Concentrate
                    snprintf(scene_command, sizeof(scene_command),
                            "{\"scene\": \"-OT9KSoQe5AL6xI\"}");
                    ESP_LOGI(KEYTAG, "Scene %d: Koncentrer dig (Concentrate)", current_scene);
                    break;
                case 3: // Read
                    snprintf(scene_command, sizeof(scene_command),
                            "{\"scene\": \"juAjcZZqsbftwWd\"}");
                    ESP_LOGI(KEYTAG, "Scene %d: Læs (Read)", current_scene);
                    break;
                case 4: // Relax
                    snprintf(scene_command, sizeof(scene_command),
                            "{\"scene\": \"fmYJ1qrn20RqoYP\"}");
                    ESP_LOGI(KEYTAG, "Scene %d: Slap af (Relax)", current_scene);
                    break;
                case 5: // Relax (different version)
                    snprintf(scene_command, sizeof(scene_command),
                            "{\"scene\": \"GAdVLQisxGioz7y\"}");
                    ESP_LOGI(KEYTAG, "Scene %d: Slap af v2 (Relax Alt)", current_scene);
                    break;
                case 6: // Studying
                    snprintf(scene_command, sizeof(scene_command),
                            "{\"scene\": \"CLT3fzKp02r8L2Ys\"}");
                    ESP_LOGI(KEYTAG, "Scene %d: Studyin'", current_scene);
                    break;
                case 7: // Night Light
                    snprintf(scene_command, sizeof(scene_command),
                            "{\"scene\": \"a9pvx0Cqq8oM1x7\"}");
                    ESP_LOGI(KEYTAG, "Scene %d: Natlys (Night Light)", current_scene);
                    break;
            }

            hue_send_command("http://192.168.50.170/api/AicZqASmH6YLHxDyBxD-pci3vEmn0jLU0XvQ9g9N/groups/1/action", scene_command);
        }
    }
    previous_rot2_clk = rot2_clk;

    // Small delay to prevent excessive CPU usage
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

void poll_single_row(void) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Poll each switch in the single row with debounce logic
    if (debounce(current_time, KEY_GPIO1, &last_press_time_single_row[0])) {
        ESP_LOGI(KEYTAG, "Switch 1 pressed!");
        switch_pc();

    }
    if (debounce(current_time, KEY_GPIO2, &last_press_time_single_row[1])) {
        ESP_LOGI(KEYTAG, "Switch 2 pressed!");

    }
    if (debounce(current_time, KEY_GPIO3, &last_press_time_single_row[2])) {
        ESP_LOGI(KEYTAG, "Switch 3 pressed!");
    }
    if (debounce(current_time, KEY_GPIO4, &last_press_time_single_row[3])) {
        ESP_LOGI(KEYTAG, "Switch 4 pressed!");
    }
    if (debounce(current_time, KEY_GPIO5, &last_press_time_single_row[4])) {
        ESP_LOGI(KEYTAG, "Switch 5 pressed! - System Sleep/Suspend");
        hid_system_sleep();
    }
}

void poll_switch_matrix(void) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    static uint32_t up_press_time = 0;
    static uint32_t down_press_time = 0;
    static bool up_button_pressed = false;
    static bool down_button_pressed = false;
    // Ensure all rows are OFF before starting
    gpio_set_level(ROW1_PIN, 0);
    gpio_set_level(ROW2_PIN, 0);
    vTaskDelay(2 / portTICK_PERIOD_MS);  // Let signals settle

    // Poll Row 1
    gpio_set_level(ROW1_PIN, 1);
    vTaskDelay(3 / portTICK_PERIOD_MS);  // Longer stabilization delay

    
    // UP button handling - SAFETY CRITICAL: immediate response, no debounce
    int up_button_state = gpio_get_level(COL1_PIN);
    if (up_button_state == 0) {  // Button pressed (active-low)
        if (!up_button_pressed) {         // Button just pressed
            up_button_pressed = true;
            start_moving_desk(DESK_MOVE_UP);
            ESP_LOGI("DESK_CONTROL", "UP button pressed - starting movement");
        }
        // Monitor distance while moving
        uint32_t distance_cm = measure_distance();
        if (distance_cm > 0) {  // Valid reading
            ESP_LOGI("ULTRASONIC", "Moving up - Distance: %lu cm", distance_cm);
        }
    } else {  // Button released
        if (up_button_pressed) {       // Button was pressed, now released
            stop_moving_desk();
            up_button_pressed = false;
            ESP_LOGI("DESK_CONTROL", "UP button released - STOPPED");
        }
    }
    
    if (debounce(current_time, COL2_PIN, &last_press_time_matrix[0][1])) {
        ESP_LOGI("BUTTON", "Up command triggered");
        skylight_command_up();
    }
    if (debounce(current_time, COL3_PIN, &last_press_time_matrix[0][2])) {
        // Read GPIO levels immediately while row is still active
        int col3_level = gpio_get_level(COL3_PIN);
        int row1_level = gpio_get_level(ROW1_PIN);
        int row2_level = gpio_get_level(ROW2_PIN);
        ESP_LOGI(KEYTAG, "Row 1, Column 3 pressed! (COL3: %d, ROW1: %d, ROW2: %d) - Volume UP",
                 col3_level, row1_level, row2_level);

        // Send HID volume up command
        hid_volume_up();

        // Add extra delay after detecting a button press to prevent double detection
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    gpio_set_level(ROW1_PIN, 0);  // Deactivate Row 1
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Longer delay to ensure row isolation

    // Debug: Check that Row 1 is actually OFF before scanning Row 2
    if (gpio_get_level(ROW1_PIN) != 0) {
        ESP_LOGW(KEYTAG, "WARNING: ROW1 still HIGH when starting Row 2 scan!");
    }

    // Poll Row 2
    gpio_set_level(ROW2_PIN, 1);
    vTaskDelay(5 / portTICK_PERIOD_MS);  // Longer stabilization delay

    // DOWN button handling - SAFETY CRITICAL: immediate response, no debounce
    int down_button_state = gpio_get_level(COL1_PIN);
    if (down_button_state == 0) {  // Button pressed (active-low)
        if (!down_button_pressed) {       // Button just pressed
            down_button_pressed = true;
            start_moving_desk(DESK_MOVE_DOWN);
            ESP_LOGI("DESK_CONTROL", "DOWN button pressed - starting movement");
        }
        // Monitor distance while moving
        uint32_t distance_cm = measure_distance();
        if (distance_cm > 0) {  // Valid reading
            ESP_LOGI("ULTRASONIC", "Moving down - Distance: %lu cm", distance_cm);
        }
    } else {  // Button released
        if (down_button_pressed) {     // Button was pressed, now released
            stop_moving_desk();
            down_button_pressed = false;
            ESP_LOGI("DESK_CONTROL", "DOWN button released - STOPPED");
        }
    }

    // Check Row 2 buttons BEFORE deactivating the row
    if (debounce(current_time, COL2_PIN, &last_press_time_matrix[1][1])) {
        ESP_LOGI(KEYTAG, "Row 2, Column 2 pressed!");
        ESP_LOGI("BUTTON", "Down command triggered");
        skylight_command_down();
    }
    if (debounce(current_time, COL3_PIN, &last_press_time_matrix[1][2])) {
        // Read GPIO levels immediately while row is still active
        int col3_level = gpio_get_level(COL3_PIN);
        int row1_level = gpio_get_level(ROW1_PIN);
        int row2_level = gpio_get_level(ROW2_PIN);
        ESP_LOGI(KEYTAG, "Row 2, Column 3 pressed! (COL3: %d, ROW1: %d, ROW2: %d) - Volume DOWN",
                 col3_level, row1_level, row2_level);

        // Send HID volume down command
        hid_volume_down();
    }

    gpio_set_level(ROW2_PIN, 0);  // Deactivate Row 2

    // Ensure all rows are deactivated at the end
    gpio_set_level(ROW1_PIN, 0);
    gpio_set_level(ROW2_PIN, 0);
}