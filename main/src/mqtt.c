//
// Created by derk on 13-9-20.
//

#include "mqtt.h"

/* MQTT (over TCP) Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "measurements.h"

static const char *TAG = "MQTT";

//To what data do we want to subscribe?
//Data to send: temperature, humidity, soil moisture level

//Topic structure /plant/{plant_id}/temperature
//Topic structure /plant/{plant_id}/humidity
//Topic structure /plant/{plant_id}/soil_moisture

static EventGroupHandle_t mqtt_event_group;
static esp_mqtt_client_handle_t client;

static mqtt_callbacks_t callbacks;
static light_states_t light_state = LIGHT_STATES_NOT_SET;

#define MQTT_CLIENT_CONNECTED BIT0
#define MQTT_TURN_ON_LIGHT BIT1
#define MQTT_TURN_OFF_LIGHT BIT2
#define MQTT_FORCE_STOP BIT3


typedef struct
{
    int32_t current;
    int32_t last;
} in32_t_pair_t;

typedef in32_t_pair_t temperature_t;
typedef in32_t_pair_t humidity_t;
typedef in32_t_pair_t soil_moisture_level_t;
typedef in32_t_pair_t light_level_t;

typedef struct
{
    temperature_t temperature;
    humidity_t humidity;
    soil_moisture_level_t soil_moisture_level;
    light_level_t light_level;
} sensor_data_t;


light_states_t get_light_state(void)
{
    return light_state;
}

void register_received_light_threshold_cb(mqtt_threshold_cb_t callback)
{
    callbacks.received_light_threshold = callback;
}

void register_received_moisture_threshold_cb(mqtt_threshold_cb_t callback)
{
    callbacks.received_moisture_threshold = callback;
}

static void update_sensor_data(sensor_data_t* sensor_data)
{
    assert(sensor_data);
    sensor_data->temperature.current = get_temperature();
    sensor_data->humidity.current = get_humidity();
    sensor_data->soil_moisture_level.current = get_soil_moisture_level();
    sensor_data->light_level.current = get_light_level();
}

static void send_temperature(char* buffer, size_t buffer_len, esp_mqtt_client_handle_t* client,
    temperature_t* temperature)
{
    assert(buffer);
    assert(temperature);
    assert(client);

    if(temperature->current != temperature->last)
    {
        memset(buffer, 0, buffer_len);
        sprintf(buffer, "{\"value\":%d,\"unit\":\"celsius\"}", temperature->current);
        esp_mqtt_client_publish(*client, "plant/1/temperature", buffer, 0, 1, 1);
        temperature->last = temperature->current;
    }
}

static void send_humidity(char* buffer, size_t buffer_len, esp_mqtt_client_handle_t* client, humidity_t* humidity)
{
    assert(buffer);
    assert(humidity);
    assert(client);

    if(humidity->current != humidity->last)
    {
        memset(buffer, 0, buffer_len);
        sprintf(buffer, "{\"value\":%d,\"unit\":\"rh\"}", humidity->current);
        esp_mqtt_client_publish(*client, "plant/1/humidity", buffer, 0, 1, 1);
        humidity->last = humidity->current;
    }
}

static void send_soil_moisture_level(char* buffer, size_t buffer_len, esp_mqtt_client_handle_t* client,
    soil_moisture_level_t* soil_moisture_level)
{
    assert(buffer);
    assert(soil_moisture_level);
    assert(client);

    if(soil_moisture_level->current != soil_moisture_level->last)
    {
        memset(buffer, 0, buffer_len);
        sprintf(buffer, "{\"value\":%d,\"unit\":\"raw\"}", soil_moisture_level->current);
        esp_mqtt_client_publish(*client, "plant/1/soil_moisture_level", buffer, 0, 1, 1);
        soil_moisture_level->last = soil_moisture_level->current;
    }
}

static void send_light_level(char* buffer, size_t buffer_len, esp_mqtt_client_handle_t* client,
                                     light_level_t* light_level)
{
    assert(buffer);
    assert(light_level);
    assert(client);

    if(light_level->current != light_level->last)
    {
        memset(buffer, 0, buffer_len);
        sprintf(buffer, "{\"value\":%d,\"unit\":\"raw\"}", light_level->current);
        esp_mqtt_client_publish(*client, "plant/1/light_level", buffer, 0, 1, 1);
        light_level->last = light_level->current;
    }
}

static void send_data(void *pv_parameters)
{
    esp_mqtt_client_handle_t* client = (esp_mqtt_client_handle_t*) pv_parameters;
    sensor_data_t sensor_data;
    char buf[100];
    esp_mqtt_client_publish(*client, "plant/1/status", "\"connected\"", 0, 1, 1);

    for(;;)
    {
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group,
                                               MQTT_CLIENT_CONNECTED | MQTT_TURN_ON_LIGHT | MQTT_TURN_OFF_LIGHT,
                                               pdFALSE,
                                               pdFALSE,
                                               portMAX_DELAY);
        if(bits & MQTT_CLIENT_CONNECTED) {

            if(light_state == LIGHT_STATES_NOT_SET)
            {
                esp_mqtt_client_publish(*client, "socket/1/state", "\"off\"", 0, 1, 1);
                light_state = LIGHT_STATES_OFF;
            }
            //TODO: maybe this can be replaced by a callback?
            update_sensor_data(&sensor_data);
            send_temperature(buf, sizeof(buf), client, &sensor_data.temperature);
            send_humidity(buf, sizeof(buf), client, &sensor_data.humidity);
            send_soil_moisture_level(buf, sizeof(buf), client, &sensor_data.soil_moisture_level);
            send_light_level(buf, sizeof(buf), client, &sensor_data.light_level);
        }

        if(bits & MQTT_TURN_ON_LIGHT)
        {
            if(light_state == LIGHT_STATES_OFF)
                esp_mqtt_client_publish(*client, "socket/1/state", "\"on\"", 0, 1, 1);
            xEventGroupClearBits(mqtt_event_group, MQTT_TURN_ON_LIGHT);
            light_state = LIGHT_STATES_ON;
        }

        if(bits & MQTT_TURN_OFF_LIGHT)
        {
            if(light_state == LIGHT_STATES_ON)
                esp_mqtt_client_publish(*client, "socket/1/state", "\"off\"", 0, 1, 1);
            xEventGroupClearBits(mqtt_event_group, MQTT_TURN_OFF_LIGHT);
            light_state = LIGHT_STATES_OFF;
        }

        vTaskDelay(10);
    }
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    static TaskHandle_t send_data_handle = NULL;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Mqtt connected!");
        xEventGroupSetBits(mqtt_event_group, MQTT_CLIENT_CONNECTED);
        esp_mqtt_client_subscribe(client, "plant/1/threshold/+", 1);
        if(!send_data_handle)
        {
            xTaskCreate(&send_data, "send_data", 4096, (void *) &client, 5, &send_data_handle);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        //TODO: if got signal to stop then do nothing, otherwise reconnect
        xEventGroupClearBits(mqtt_event_group, MQTT_CLIENT_CONNECTED);

        if(send_data_handle)
        {
            vTaskDelete(send_data_handle);
            send_data_handle = NULL;
        }

        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group,
                                               MQTT_FORCE_STOP,
                                               pdFALSE,
                                               pdFALSE,
                                               10);

        if(bits & MQTT_FORCE_STOP)
        {
            ESP_LOGI(TAG, "force stopping mqtt client");
            xEventGroupClearBits(mqtt_event_group, MQTT_FORCE_STOP);
        }
        else
        {
            ESP_LOGI(TAG, "client randomly disconnected, trying to reconnect...");
            esp_mqtt_client_start(client);
        }

        break;
    case MQTT_EVENT_SUBSCRIBED:
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        break;
    case MQTT_EVENT_PUBLISHED:
        break;
    case MQTT_EVENT_DATA:
        if(strncmp(event->topic, "plant/1/threshold/light", event->topic_len) == 0)
        {
            ESP_LOGI(TAG, "Setting light threshold");
            if(callbacks.received_light_threshold)
                callbacks.received_light_threshold((uint16_t)strtol(event->data, NULL, 10));
        }
        else if(strncmp(event->topic, "plant/1/threshold/moisture", event->topic_len) == 0)
        {
            ESP_LOGI(TAG, "Setting moisture threshold");
            if(callbacks.received_moisture_threshold)
                callbacks.received_moisture_threshold((uint16_t)strtol(event->data, NULL, 10));
        }
            break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

void mqtt_send_light_message(bool status)
{
    if(mqtt_event_group)
        xEventGroupSetBits(mqtt_event_group, status ? MQTT_TURN_ON_LIGHT : MQTT_TURN_OFF_LIGHT);
}



void start_mqtt_client(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://142.93.224.106",
        .username = "derk",
        .password = "sopkut",
        .lwt_topic = "plant/1/status",
        .lwt_msg = "\"disconnected\"",
        .lwt_qos = 1,
        .lwt_retain = 1,
        .disable_auto_reconnect = false
    };

    if(!mqtt_event_group)
        mqtt_event_group = xEventGroupCreate();

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

void stop_mqtt_client(void)
{
    if(client)
    {
        xEventGroupSetBits(mqtt_event_group, MQTT_FORCE_STOP);
        esp_mqtt_client_disconnect(client);
        esp_mqtt_client_destroy(client);
    }
}
