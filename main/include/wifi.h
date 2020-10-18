//
// Created by derk on 7-9-20.
//

#ifndef NETWORKING_H
#define NETWORKING_H

#define MAX_SSID_LENGTH 33
#define MAX_PASSWORD_LENGTH 65
#define MAX_RETRIES 3

typedef struct
{
    char ssid[MAX_SSID_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
} network_credentials_t;

typedef void (*wifi_cb_t)(void* data);

typedef struct
{
    wifi_cb_t on_sta_start;
    wifi_cb_t on_connect;
    wifi_cb_t on_disconnect;
    wifi_cb_t on_receive_credentials;
    wifi_cb_t on_connection_reset;
} wifi_callbacks_t;

void initialize_wifi(void);
void register_on_wifi_sta_start_cb(wifi_cb_t callback);
void register_on_wifi_connect_cb(wifi_cb_t callback);
void register_on_wifi_disconnect_cb(wifi_cb_t callback);
void register_on_wifi_receive_credentials_cb(wifi_cb_t callback);
void register_on_wifi_connection_reset(wifi_cb_t callback);

#endif //NETWORKING_H
