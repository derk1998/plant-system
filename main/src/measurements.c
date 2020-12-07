//
// Created by derk on 7-9-20.
//
#include "measurements.h"
#include  <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <nvs.h>
#include "esp_log.h"
#include "sensor.h"
#include "dht11.h"
#include "base.h"
#include "switch_kaku.h"

analog_sensor_t moisture_sensor, light_sensor;
dht11_t dht11;
SemaphoreHandle_t measure_semaphore = NULL;
SemaphoreHandle_t threshold_semaphore = NULL;
kaku_t kaku;


typedef struct
{
    measurement_threshold_cb_t reached_light_threshold;
    measurement_threshold_cb_t above_light_threshold;
    measurement_threshold_cb_t reached_moisture_threshold;
} measurements_callbacks_t;

typedef struct
{
    uint16_t moisture;
    uint16_t light;
} thresholds_t;

static const char *TAG = "measure";
static thresholds_t thresholds;
static measurements_callbacks_t callbacks;

static void measure(void);
static void measure_data(void *param);
static void apply_threshold(void *param);

void register_reached_light_threshold_cb(measurement_threshold_cb_t callback)
{
    callbacks.reached_light_threshold = callback;
}

void register_reached_moisture_threshold_cb(measurement_threshold_cb_t callback)
{
    callbacks.reached_moisture_threshold = callback;
}

void register_above_light_threshold_cb(measurement_threshold_cb_t callback)
{
    callbacks.above_light_threshold = callback;
}

void initialize_measurements(void)
{
    initialize_analog_sensor(&moisture_sensor, HUMIDITY_GPIO);
    initialize_analog_sensor(&light_sensor, LDR_GPIO);
    initialize_dht11(&dht11, DHT11_GPIO);
    initialize_kaku(KAKU_GPIO, KAKU_ID, -1, KAKU_GROUP_1, KAKU_DEVICE_ALL, 10, &kaku);

    measure_semaphore = xSemaphoreCreateMutex();
    threshold_semaphore = xSemaphoreCreateMutex();

    xTaskCreate(&measure_data, "measure_data", 4096, NULL, 5, NULL);
    xTaskCreate(&apply_threshold, "apply_threshold", 4096, NULL, 5, NULL);
}

static void measure_data(void *param)
{
    for(;;)
    {
        measure();
        vTaskDelay(100);
    }
}

static void apply_threshold(void *param)
{
    uint16_t light_threshold = 0;
    uint16_t moisture_threshold = 0;
    nvs_handle_t  nvs_handle;
    nvs_open("storage", NVS_READONLY, &nvs_handle);
    nvs_get_u16(nvs_handle, "light_th", &light_threshold);
    nvs_get_u16(nvs_handle, "moist_th", &moisture_threshold);
    nvs_close(nvs_handle);

    int32_t current_light_value = 0;
    int32_t current_soil_moisture_value = 0;

    if( xSemaphoreTake( threshold_semaphore, portMAX_DELAY == pdTRUE ))
    {
        thresholds.moisture = moisture_threshold;
        thresholds.light = light_threshold;
        xSemaphoreGive( threshold_semaphore );
    }

    ESP_LOGI(TAG, "Light threshold loaded from flash, value: %d", light_threshold);
    ESP_LOGI(TAG, "Moisture threshold loaded from flash, value: %d", moisture_threshold);
    for(;;)
    {
        //retrieve thresholds
        if( threshold_semaphore != NULL )
        {
            if( xSemaphoreTake( threshold_semaphore, (TickType_t) 10) == pdTRUE )
            {
                light_threshold = thresholds.light;
                moisture_threshold = thresholds.moisture;
                xSemaphoreGive( threshold_semaphore );
            }
        }

        //retrieve sensor values
        current_light_value = get_light_level();
        current_soil_moisture_value = get_soil_moisture_level();

        //If soil moisture is under threshold -> then give water for max 2 seconds
        //If ldr value is above threshold -> turn on light
        if(current_light_value < light_threshold)
        {
            if(callbacks.reached_light_threshold)
                callbacks.reached_light_threshold(current_light_value, light_threshold);
        }
        else
        {
            if(callbacks.above_light_threshold)
                callbacks.above_light_threshold(current_light_value, light_threshold);
        }

        if(current_soil_moisture_value < moisture_threshold)
        {
            if(callbacks.reached_moisture_threshold)
                callbacks.reached_moisture_threshold(current_soil_moisture_value, moisture_threshold);
        }

        vTaskDelay(100);
    }
}

void set_light_threshold(uint16_t threshold)
{
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "light_th", threshold));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
    if( threshold_semaphore != NULL )
    {
        if( xSemaphoreTake( threshold_semaphore, portMAX_DELAY) == pdTRUE )
        {
            ESP_LOGI(TAG, "Setting moisture threshold to %d", threshold);
            thresholds.light = threshold;
            xSemaphoreGive( threshold_semaphore );
        }
    }
}

void set_moisture_threshold(uint16_t threshold)
{
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "moist_th", threshold));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
    if( threshold_semaphore != NULL )
    {
        if( xSemaphoreTake( threshold_semaphore, portMAX_DELAY) == pdTRUE )
        {
            ESP_LOGI(TAG, "Setting moisture threshold to %d", threshold);
            thresholds.moisture = threshold;
            xSemaphoreGive( threshold_semaphore );
        }
    }
}

static void measure(void)
{
    if( measure_semaphore != NULL )
    {
        if( xSemaphoreTake( measure_semaphore, ( TickType_t ) 10 ) == pdTRUE )
        {
            read_analog_sensor(&moisture_sensor);
            read_analog_sensor(&light_sensor);
            read_dht11(&dht11);
            xSemaphoreGive( measure_semaphore );
        }
    }
}

int32_t get_temperature(void)
{
    int32_t temperature = 0;
    if(measure_semaphore != NULL)
    {
        if( xSemaphoreTake( measure_semaphore, ( TickType_t ) 10 ) == pdTRUE )
        {
            temperature =  dht11.temperature;
            xSemaphoreGive( measure_semaphore );
            return temperature;
        }
    }
    return temperature;
}

int32_t get_humidity(void)
{
    int32_t humidity = 0;
    if(measure_semaphore != NULL)
    {
        if( xSemaphoreTake( measure_semaphore, ( TickType_t ) 10 ) == pdTRUE )
        {
            humidity =  dht11.humidity;
            xSemaphoreGive( measure_semaphore );
            return humidity;
        }
    }
    return humidity;
}

int32_t get_soil_moisture_level(void)
{
    static int32_t moisture_level = 0;
    if(measure_semaphore != NULL)
    {
        if( xSemaphoreTake( measure_semaphore, ( TickType_t ) 10 ) == pdTRUE )
        {
            moisture_level =  moisture_sensor.value;
            xSemaphoreGive( measure_semaphore );
            return moisture_level;
        }
    }
    return moisture_level;
}

int32_t get_light_level(void)
{
    static int32_t light_level = 0;
    if(measure_semaphore != NULL)
    {
        if( xSemaphoreTake( measure_semaphore, ( TickType_t ) 10 ) == pdTRUE )
        {
            light_level =  light_sensor.value;
            xSemaphoreGive( measure_semaphore );
            return light_level;
        }
    }
    return light_level;
}

void switch_radio_outlet(void)
{
    switch_kaku(&kaku);
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    switch_kaku(&kaku);
}