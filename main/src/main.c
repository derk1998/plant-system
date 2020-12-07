#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp32/rom/gpio.h>
#include <esp_log.h>

#include "measurements.h"
#include "wifi.h"
#include "base.h"
#include "led.h"
#include "mqtt.h"
#include "http.h"

static uint16_t light_value_before = 0;

void initialize_relay(void)
{
    gpio_pad_select_gpio(RELAY_GPIO);
    gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_GPIO, 0);
}

static void on_wifi_connect(void *data)
{
    set_led_status(LED_MODE_ON);
    start_mqtt_client();
}

static void on_wifi_disconnect(void *data)
{
    set_led_status(LED_MODE_OFF);
    stop_mqtt_client();
}

static void on_wifi_connection_reset(void* data)
{
    set_led_status(LED_MODE_FASTER_BLINK);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

static void on_wifi_receive_credentials(void* data)
{
    set_led_status(LED_MODE_FAST_BLINK);
}

static void on_wifi_sta_start(void* data)
{
    set_led_status(LED_MODE_BLINK);
}

static void on_wifi_ap_start(void* data)
{
    set_led_status(LED_MODE_BLINK);
    xTaskCreate(start_webserver, "start_webserver", 4096, NULL, 5, NULL);
}

static void received_credentials(void* data)
{
    set_led_status(LED_MODE_FAST_BLINK);
    stop_webserver();

    //There are better ways to do it, but this is simple
    esp_restart();
}

static void received_light_threshold(uint16_t threshold)
{
    set_light_threshold(threshold);
}

static void received_moisture_threshold(uint16_t threshold)
{
    set_moisture_threshold(threshold);
}

static void reached_light_threshold(uint16_t value, uint16_t threshold)
{
    //Measure ldr value before
    //assume light is off
    light_states_t light_state = get_light_state();
    if(light_state == LIGHT_STATES_NOT_SET) return;

    //Only start measuring when light is off
    if(light_state == LIGHT_STATES_OFF)
    {
        light_value_before = value;
        mqtt_send_light_message(true);
    }
}

static void above_light_threshold(uint16_t value, uint16_t threshold)
{
    //Determine how much above the threshold
    //light value before tells us what the light is when
    light_states_t light_state = get_light_state();
    if(light_state == LIGHT_STATES_NOT_SET) return;

    if(light_state == LIGHT_STATES_ON)
    {
        //difference of switching the light
        uint16_t difference = value - light_value_before;
        ESP_LOGI("main", "difference: %d", difference);

        //If old value - new value > threshold + certain margin
        if(difference > threshold + LIGHT_THRESHOLD_MARGIN)
            mqtt_send_light_message(false);
    }
}

static void reached_moisture_threshold(uint16_t value, uint16_t threshold)
{
    printf("Watering...\n");
    gpio_set_level(RELAY_GPIO, 1);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    gpio_set_level(RELAY_GPIO, 0);
    //TODO: create software timer for this
    vTaskDelay(10000 / portTICK_PERIOD_MS);
}

void app_main()
{
    //Initialize components
    initialize_nvs();
    initialize_status_led();

    //Register the wifi callbacks before wifi initialization
    register_on_wifi_connect_cb(&on_wifi_connect);
    register_on_wifi_disconnect_cb(&on_wifi_disconnect);
    register_on_wifi_connection_reset(&on_wifi_connection_reset);
    register_on_wifi_receive_credentials_cb(&on_wifi_receive_credentials);
    register_on_wifi_sta_start_cb(&on_wifi_sta_start);
    register_on_wifi_ap_start_cb(&on_wifi_ap_start);

    //Register mqtt callbacks
    register_received_light_threshold_cb(&received_light_threshold);
    register_received_moisture_threshold_cb(&received_moisture_threshold);

    //Register http callbacks
    register_on_receive_credentials_cb(&received_credentials);

    //Register measurement callbacks
    register_reached_light_threshold_cb(&reached_light_threshold);
    register_above_light_threshold_cb(&above_light_threshold);
    register_reached_moisture_threshold_cb(&reached_moisture_threshold);

    //Initializations
    initialize_wifi();
    initialize_measurements();
    initialize_relay();
}
