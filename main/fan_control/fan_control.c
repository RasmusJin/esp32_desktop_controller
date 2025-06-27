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

    // Simple filtering - just use the raw value with light smoothing
    uint32_t adc_value;

    if (last_adc == 0) {
        // First reading - use raw value directly
        adc_value = raw_adc;
    } else {
        // Light smoothing: 70% new, 30% old for responsiveness
        adc_value = (raw_adc * 7 + last_adc * 3) / 10;
    }
    last_adc = adc_value;

    // Your actual potentiometer range (based on RAW ADC readings)
    const uint32_t max_adc_value = 520;  // Pot turned counterclockwise (actual max observed)

    // Stepped fan control - discrete speed levels for smooth operation
    const uint32_t off_threshold = 450;   // OFF when ADC >= 450 (counterclockwise position)

    // Define speed steps (ADC ranges and corresponding PWM duty cycles)
    typedef struct {
        uint32_t adc_min;
        uint32_t adc_max;
        uint32_t duty_cycle;
        uint8_t speed_percent;
    } fan_step_t;

    const fan_step_t fan_steps[] = {
        {0,   80,  255, 100},  // Step 1: Max speed (full clockwise)
        {81,  160, 200, 78},   // Step 2: High speed
        {161, 240, 150, 59},   // Step 3: Medium-high speed
        {241, 320, 100, 39},   // Step 4: Medium speed
        {321, 449, 70,  27},   // Step 5: Low speed
    };
    const uint32_t num_steps = sizeof(fan_steps) / sizeof(fan_step_t);

    uint32_t duty_cycle = 0;  // Initialize duty cycle
    uint8_t fan_speed_percentage = 0;  // Initialize fan speed percentage

    // Fan control logging - uncomment for debugging fan issues
    log_counter++;
    bool should_log = (log_counter % 20 == 0);  // Log every 1 second (20Hz / 20 = 1Hz)

    // Debug logging disabled for clean output
    // if (should_log) {
    //     ESP_LOGI(TAG, "ADC: %d (Raw: %d), Duty: %d, Threshold: %d", adc_value, raw_adc, duty_cycle, off_threshold);
    // }

    // Clamp ADC value to expected range
    if (adc_value > max_adc_value) adc_value = max_adc_value;

    // Stepped fan control logic
    if (adc_value >= off_threshold) {
        // OFF zone
        duty_cycle = 0;
        fan_speed_percentage = 0;
        // ESP_LOGW(TAG, "FANS OFF - ADC %d >= threshold %d", adc_value, off_threshold);
    } else {
        // Find the appropriate speed step
        bool step_found = false;
        for (uint32_t i = 0; i < num_steps; i++) {
            if (adc_value >= fan_steps[i].adc_min && adc_value <= fan_steps[i].adc_max) {
                duty_cycle = fan_steps[i].duty_cycle;
                fan_speed_percentage = fan_steps[i].speed_percent;
                step_found = true;
                // if (should_log) {
                //     ESP_LOGI(TAG, "Step %d: ADC %d -> %d%% (duty %d)", i+1, adc_value, fan_speed_percentage, duty_cycle);
                // }
                break;
            }
        }

        // Fallback if no step found (shouldn't happen)
        if (!step_found) {
            duty_cycle = 40;  // Default low speed
            fan_speed_percentage = 16;
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

    // PWM logging disabled for clean output
    // if (should_log) {
    //     ESP_LOGI(TAG, "PWM SET: %d (%d%%)", duty_cycle, fan_speed_percentage);
    // }

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
