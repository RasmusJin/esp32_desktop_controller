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

    uint32_t adc_value = potentiometer_read();  // Read the potentiometer value

    const uint32_t min_adc_value = 0;    // Min ADC value for the potentiometer
    const uint32_t max_adc_value = 942;  // Max ADC value from your potentiometer (adjust if needed)

    uint32_t duty_cycle = 0;  // Initialize duty cycle
    uint8_t fan_speed_percentage = 0;  // Initialize fan speed percentage

    //ESP_LOGI(TAG, "Raw ADC Value: %" PRIu32, adc_value);

    // Calculate the duty cycle based on the adjusted range
    duty_cycle = ((max_adc_value - adc_value) * 255) / (max_adc_value - min_adc_value);

    // Calculate the fan speed percentage
    fan_speed_percentage = (duty_cycle * 100) / 255;

    //ESP_LOGI(TAG, "Calculated Duty Cycle: %" PRIu32, duty_cycle);
    //ESP_LOGI(TAG, "Fan Speed Percentage: %" PRIu8 "%%", fan_speed_percentage);

    // Update the PWM duty cycle
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, duty_cycle);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);

    // Log the final duty cycle for debugging
    //ESP_LOGI(TAG, "Final Duty Cycle Set: %" PRIu32, duty_cycle);

    // Later, you can display fan_speed_percentage on a display
}


// Function to read the potentiometer value using the new ADC APIs
uint32_t potentiometer_read(void) {
    int raw_value;
    adc_oneshot_read(adc_handle, POTENTIOMETER_ADC_CHANNEL, &raw_value);

    int calibrated_value;
    adc_cali_raw_to_voltage(cali_handle, raw_value, &calibrated_value);

    return calibrated_value;
}

// Function to print the potentiometer value (for testing)
void potentiometer_print_value(void) {
    uint32_t adc_value = potentiometer_read();
    ESP_LOGI(TAG, "Potentiometer ADC Value: %" PRIu32 " mV", adc_value);
}
