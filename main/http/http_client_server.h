#ifndef HTTP_CLIENT_SERVER_H
#define HTTP_CLIENT_SERVER_H

#include "esp_http_client.h"

#define MAX_BRIGHTNESS 254
#define MIN_BRIGHTNESS 1
#define HUE_API_KEY "AicZqASmH6YLHxDyBxD-pci3vEmn0jLU0XvQ9g9N"
#define HUE_GROUP_ID "1"
void skylight_command_up();
void skylight_command_down();
int send_http_request(const char *url);

void hue_send_command(const char *url, const char *body);
void hue_set_group_brightness(int brightness_value);
#endif // HTTP_CLIENT_SERVER_H