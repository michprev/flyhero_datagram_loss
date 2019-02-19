#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <string.h>
#include <lwip/sockets.h>

#include "common/mavlink.h"

#define PORT            14540
#define SYSTEM_ID       0xF7

static int socket_fd;

static esp_err_t event_handler(void * context, system_event_t * event)
{
    return ESP_OK;
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t wifi_config;
    strcpy((char *)wifi_config.ap.ssid, "TEST_SSID");
    strcpy((char *)wifi_config.ap.password, "testpassword");
    wifi_config.ap.ssid_len = 0;
    wifi_config.ap.channel = 7;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.ap.ssid_hidden = 0;
    wifi_config.ap.max_connection = 1;
    wifi_config.ap.beacon_interval = 100;
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    assert(socket_fd >= 0);
    assert(fcntl(socket_fd, F_SETFL, O_NONBLOCK) == 0);

    struct sockaddr_in address;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);

    assert(bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) >= 0);

    mavlink_message_t message;
    mavlink_heartbeat_t heartbeat =
    {
        .custom_mode = 0,
        .type = MAV_TYPE_QUADROTOR,
        .autopilot = MAV_AUTOPILOT_GENERIC,
        .base_mode = MAV_MODE_FLAG_TEST_ENABLED,
        .system_status = MAV_STATE_STANDBY
    };
    mavlink_msg_heartbeat_encode_chan(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, MAVLINK_COMM_0, &message, &heartbeat);

    struct sockaddr_in broadcast;
    broadcast.sin_addr.s_addr = inet_addr("255.255.255.255");
    broadcast.sin_family = AF_INET;
    broadcast.sin_port = htons(PORT);

    static uint8_t buffer[100];
    uint16_t length = mavlink_msg_to_send_buffer(buffer, &message);

    while (true)
    {
        assert (sendto(socket_fd, buffer, length, 0, (struct sockaddr *)&broadcast, sizeof(broadcast)) == length);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}
