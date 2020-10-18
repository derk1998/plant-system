//
// Created by derk on 7-9-20.
//
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"

#include "wifi.h"
#include "button.h"

static EventGroupHandle_t wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define ESPTOUCH_DONE_BIT BIT2

static const char *TAG = "wifi";

static void start_smart_config(void * param);

static void save_wifi_credentials(const char* ssid, const char* password);
static bool connect_wifi(network_credentials_t* credentials);
static void reset_wifi_connection ( void * arg );
static void delete_wifi_credentials(void);
static void start_connecting(void);


typedef enum
{
    MODE_NOT_SET,
    MODE_NEW_CONFIGURATION,
    MODE_SAVED_CONFIGURATION,
} connect_modes_t;

static connect_modes_t current_mode = MODE_NOT_SET;
static wifi_callbacks_t wifi_callbacks;

void register_on_wifi_sta_start_cb(wifi_cb_t callback)
{
    wifi_callbacks.on_sta_start = callback;
}

void register_on_wifi_connect_cb(wifi_cb_t callback)
{
    wifi_callbacks.on_connect = callback;
}

void register_on_wifi_disconnect_cb(wifi_cb_t callback)
{
    wifi_callbacks.on_disconnect = callback;
}

void register_on_wifi_receive_credentials_cb(wifi_cb_t callback)
{
    wifi_callbacks.on_receive_credentials = callback;
}

void register_on_wifi_connection_reset(wifi_cb_t callback)
{
    wifi_callbacks.on_connection_reset = callback;
}

static void handle_new_configuration(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    static network_credentials_t temp_wifi_credentials;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        if(wifi_callbacks.on_sta_start)
            wifi_callbacks.on_sta_start(NULL);
        xTaskCreate(start_smart_config, "start_smart_config", 4096, NULL, 3, NULL);
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
    {
        if(wifi_callbacks.on_receive_credentials)
            wifi_callbacks.on_receive_credentials(NULL);
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        memcpy(temp_wifi_credentials.ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(temp_wifi_credentials.password, evt->password, sizeof(evt->password));

        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
        ESP_ERROR_CHECK( esp_wifi_connect() );
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE)
    {
        xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
        save_wifi_credentials(temp_wifi_credentials.ssid, temp_wifi_credentials.password);
        if(wifi_callbacks.on_connect)
            wifi_callbacks.on_connect(NULL);
    }
}

static void handle_saved_configuration(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            if(wifi_callbacks.on_sta_start)
                wifi_callbacks.on_sta_start(NULL);
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
            ESP_LOGI(TAG, "connect to the AP fail");
            if(wifi_callbacks.on_disconnect)
                wifi_callbacks.on_disconnect(NULL);
            break;
        default:break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        if(wifi_callbacks.on_connect)
            wifi_callbacks.on_connect(NULL);
    }
}

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    switch(current_mode)
    {
    case MODE_NEW_CONFIGURATION:
        handle_new_configuration(arg, event_base, event_id, event_data);
        break;
    case MODE_SAVED_CONFIGURATION:
        handle_saved_configuration(arg, event_base, event_id, event_data);
        break;
    default:
        break;
    };
}

static void delete_wifi_credentials(void)
{
    nvs_handle_t nvs_handle;
    if(nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_ERR_NVS_NOT_FOUND)
        return;
    nvs_erase_key(nvs_handle, "ssid");
    nvs_erase_key(nvs_handle, "password");
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}

static bool connect_to_saved_wifi(void)
{
    network_credentials_t credentials;
    size_t ssid_size = sizeof(credentials.ssid);
    size_t password_size = sizeof(credentials.password);
    nvs_handle_t  nvs_handle;
    if(nvs_open("storage", NVS_READONLY, &nvs_handle) == ESP_ERR_NVS_NOT_FOUND)
        return false;

    ESP_LOGI(TAG, "Opened nvs");
    esp_err_t ssid_err = nvs_get_str(nvs_handle, "ssid", credentials.ssid, &ssid_size);
    esp_err_t pass_err = nvs_get_str(nvs_handle, "password", credentials.password, &password_size);

    if(ssid_err == ESP_ERR_NVS_NOT_FOUND || pass_err == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(nvs_handle);
        return false;
    }

    ESP_LOGI(TAG, "Trying to connect to the saved wifi network");
    for(uint8_t i = 0; i < MAX_RETRIES; ++i)
    {
        if(connect_wifi(&credentials))
            return true;
    }
    return false;
}

/**
 * @brief Connect with wifi
 * @note Wifi auth mode has to be WPA2 PSK
 * @param credentials
 */
static bool connect_wifi(network_credentials_t* credentials)
{
    assert(credentials);
    wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config));
    strlcpy((char*)wifi_config.sta.ssid, credentials->ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char*)wifi_config.sta.password, credentials->password, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    return bits & WIFI_CONNECTED_BIT;
}


static void save_wifi_credentials(const char* ssid, const char* password)
{
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "ssid", ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "password", password));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
}


static void start_smart_config(void * param)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT()
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
    for (;;) {
        uxBits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if(uxBits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}


void initialize_wifi(void)
{
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );

    start_connecting();
    setup_reset_button();
    xTaskCreate(reset_wifi_connection, "reset_wifi_connection", 2048, NULL, 3, NULL);
}

static void start_connecting(void)
{
    current_mode = MODE_SAVED_CONFIGURATION;
    if(!connect_to_saved_wifi())
    {
        current_mode = MODE_NEW_CONFIGURATION;
        ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
        ESP_ERROR_CHECK( esp_wifi_start() );
    }
}

static void reset_wifi_connection ( void * arg )
{
    for ( ;; )
    {
        if(wait_until_reset_button_pressed())
        {
            if (wifi_callbacks.on_connection_reset)
                wifi_callbacks.on_connection_reset(NULL);
            ESP_LOGI(TAG, "Stopping wifi...");
            esp_wifi_stop();
            ESP_LOGI(TAG, "Deleting wifi credentials...");
            delete_wifi_credentials();
            ESP_LOGI(TAG, "Starting wifi...");
            start_connecting();
        }
    }
}

