#include <inttypes.h>  // Include this for PRIu32 macro
#include "fan_control.h"
#include "esp_adc/adc_oneshot.h"  // Ensure ESP-IDF components are included in the project's include path
#include "esp_adc/adc_cali.h"
#include "esp_log.h"
#include "driver/ledc.h"

#define POTENTIOMETER_ADC_CHANNEL ADC_CHANNEL_3  // GPIO4 (ADC1)
#define DEFAULT_VREF 1100  // Default ADC reference voltage in mV

#define FAN_PWM_GPIO GPIO_NUM_14
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_FREQUENCY 25000

static const char *TAG = "POTENTIOMETER";
static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;

// Function to initialize the potentiometer using the new ADC APIs
void potentiometer_init(void) {
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
    };
    adc_oneshot_new_unit(&unit_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12, // Set attenuation to 11 db
        .bitwidth = ADC_BITWIDTH_12, // Set bitwidth to 12 bits 
    };
    adc_oneshot_config_channel(adc_handle, POTENTIOMETER_ADC_CHANNEL, &config);

    // Calibration (Optional)
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_0,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
}

// Function to initialize PWM for fan control
void fan_pwm_init(void) {
    // Configure the PWM timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_TIMER_8_BIT,  // 8-bit resolution (0-255)
        .freq_hz          = LEDC_FREQUENCY,  // 25 kHz PWM frequency
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Configure the PWM channel
    ledc_channel_config_t ledc_channel = {
        .gpio_num   = FAN_PWM_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .duty       = 0,  // Start with 0% duty cycle
        .hpoint     = 0
    };
    ledc_channel_config(&ledc_channel);
}
// Function to update fan speed based on potentiometer value
void update_fan_speed(void) {
    static uint32_t last_adc = 0;
    static uint8_t off_count = 0;
    static uint8_t on_count = 0;
    static uint32_t log_counter = 0;

    uint32_t raw_adc = potentiometer_read();  // Read the potentiometer value

    // Aggressive filtering for max speed position
    uint32_t adc_value;

    // If we're at max speed position (ADC near 0), be very conservative about changes
    if (last_adc <= 20 && raw_adc > 100) {
        // Ignore sudden jumps from max speed position - likely noise
        adc_value = last_adc;
        // ESP_LOGW(TAG, "NOISE DETECTED: Ignoring jump from %d to %d", last_adc, raw_adc);
    } else if (abs((int)raw_adc - (int)last_adc) > 50) {
        // Big change - use new value immediately (responsive)
        adc_value = raw_adc;
    } else {
        // Small change - light smoothing: 50% old, 50% new
        adc_value = (last_adc + raw_adc) / 2;
    }
    last_adc = adc_value;

    // Your actual potentiometer range (based on RAW ADC readings)
    const uint32_t max_adc_value = 760;  // Pot turned counterclockwise (use max observed)

    // Fan control parameters - "best effort" approach with buffer zones
    const uint32_t off_threshold = 735;   // Simple threshold
    const uint32_t max_speed_zone = 10;   // Buffer zone at max speed (ADC 0-10 = max speed)
    const uint32_t min_duty_cycle = 25;   // ~10% minimum when running (25/255 = 9.8%)
    const uint32_t max_duty_cycle = 255;  // 100% maximum

    uint32_t duty_cycle = 0;  // Initialize duty cycle
    uint8_t fan_speed_percentage = 0;  // Initialize fan speed percentage

    // Fan control logging - uncomment for debugging fan issues
    log_counter++;
    bool should_log = (log_counter % 20 == 0);  // Log every 1 second (20Hz / 20 = 1Hz)

    // if (should_log) {
    //     ESP_LOGI(TAG, "ADC: %d (Raw: %d), off_count: %d, on_count: %d, threshold: %d", adc_value, raw_adc, off_count, on_count, off_threshold);
    // }

    // Clamp ADC value to expected range
    if (adc_value > max_adc_value) adc_value = max_adc_value;

    // Simple logic - always calculate and update PWM
    if (adc_value >= off_threshold) {
        off_count++;
        on_count = 0;
        if (off_count >= 3) {  // Need 3 consecutive readings to turn OFF
            duty_cycle = 0;
            fan_speed_percentage = 0;
            // ESP_LOGW(TAG, "FANS TURNED OFF after %d readings!", off_count);
        } else {
            // Still in OFF zone but not confirmed - keep fans OFF during confirmation
            duty_cycle = 0;
            fan_speed_percentage = 0;
            // ESP_LOGW(TAG, "OFF pending (%d/3), fans OFF", off_count);
        }
    } else {
        // Normal operation - reset OFF counter
        off_count = 0;
        on_count++;

        // Check if we're in the max speed zone
        if (adc_value <= max_speed_zone) {
            // Max speed zone - always 100%
            duty_cycle = max_duty_cycle;
            fan_speed_percentage = 100;
            // Max speed zone logging - uncomment for debugging fan issues
            // if (should_log) {
            //     ESP_LOGI(TAG, "MAX SPEED ZONE: ADC %d <= %d, Duty Cycle: %d",
            //              adc_value, max_speed_zone, duty_cycle);
            // }
        } else {
            // Normal speed control range
            duty_cycle = min_duty_cycle + (((off_threshold - adc_value) * (max_duty_cycle - min_duty_cycle)) / off_threshold);
            fan_speed_percentage = (duty_cycle * 100) / 255;

            // Debug calculation logging - uncomment for debugging fan issues
            // if (should_log) {
            //     ESP_LOGI(TAG, "Calculation: (%d - %d) * (%d - %d) / %d = %d",
            //              off_threshold, adc_value, max_duty_cycle, min_duty_cycle, off_threshold, duty_cycle);
            // }
        }
    }

    // Fan calculation logging - uncomment for debugging fan issues
    // if (should_log) {
    //     ESP_LOGI(TAG, "Calculated Duty Cycle: %" PRIu32, duty_cycle);
    //     ESP_LOGI(TAG, "Fan Speed Percentage: %" PRIu8 "%%", fan_speed_percentage);
    // }

    // Update the PWM duty cycle
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, duty_cycle);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);

    // PWM logging - uncomment for debugging fan issues
    if (should_log) {
        ESP_LOGI(TAG, "PWM SET: %d (%d%%)", duty_cycle, fan_speed_percentage);
    }

    // Later, you can display fan_speed_percentage on a display
}


// Function to read the potentiometer value using the new ADC APIs
uint32_t potentiometer_read(void) {
    int raw_value;
    adc_oneshot_read(adc_handle, POTENTIOMETER_ADC_CHANNEL, &raw_value);

    int calibrated_value;
    adc_cali_raw_to_voltage(cali_handle, raw_value, &calibrated_value);

    // Debug: Log both raw and calibrated values - uncomment for debugging
    // ESP_LOGI(TAG, "ADC Raw: %d, Calibrated (mV): %d", raw_value, calibrated_value);

    // Return RAW value instead of calibrated voltage
    return raw_value;
}

// Function to print the potentiometer value (for testing)
void potentiometer_print_value(void) {
    uint32_t adc_value = potentiometer_read();
    ESP_LOGI(TAG, "Potentiometer ADC Value: %" PRIu32 " mV", adc_value);
}
