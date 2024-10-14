#include "esp_http_client.h"
#include "esp_log.h"
#include "http_client_server.h"

// Global variable to store the current brightness
int current_brightness = 100; 

static const char *TAG = "HTTP_CLIENT";
esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI("HTTP_EVENT", "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI("HTTP_EVENT", "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI("HTTP_EVENT", "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI("HTTP_EVENT", "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI("HTTP_EVENT", "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI("HTTP_EVENT", "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI("HTTP_EVENT", "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:  // Add the missing case
            ESP_LOGI("HTTP_EVENT", "HTTP_EVENT_REDIRECT");
            break;
        default:
            ESP_LOGI("HTTP_EVENT", "Unhandled event id: %lld", (long long int)evt->event_id);            
            break;
    }
    return ESP_OK;
}


void send_http_request(const char *url) {
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                (long long int)esp_http_client_get_content_length(client));

    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

void skylight_command_up() {
    send_http_request("http://192.168.50.228/control_remote?command=up");
}

void skylight_command_down() {
    send_http_request("http://192.168.50.228/control_remote?command=down");
}


void hue_send_command(const char *url, const char *body) {
    esp_http_client_config_t config = {
        .url = url,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_PUT);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP PUT Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 (long long int)esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

void hue_set_group_brightness(int brightness_value) {
    // Ensure brightness_value is within bounds (0-255)
    if (brightness_value < 0) brightness_value = 0;
    if (brightness_value > 255) brightness_value = 255;

    // Create the URL for the group command
    char url[256];
    snprintf(url, sizeof(url), "http://192.168.50.170/api/%s/groups/%s/action", HUE_API_KEY, HUE_GROUP_ID);

    // Create the JSON payload to set the brightness
    char data[50];
    snprintf(data, sizeof(data), "{\"bri\":%d}", brightness_value);

    esp_http_client_config_t config = {
        .url = url,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set up the HTTP PUT request
    esp_http_client_set_method(client, HTTP_METHOD_PUT);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, data, strlen(data));

    // Perform the HTTP request and check for errors
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Brightness for Gaming group set to %d", brightness_value);
    } else {
        ESP_LOGE(TAG, "Failed to set brightness: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}