//
// Created by derk on 7-9-20.
//

#include "sensor.h"
#include <esp32/rom/gpio.h>
#include <driver/gpio.h>


void initialize_analog_sensor(analog_sensor_t* sensor, adc1_channel_t pin)
{
    sensor->value = 0;
    sensor->pin = pin;
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_12));
    ESP_ERROR_CHECK(adc1_config_channel_atten(pin,ADC_ATTEN_DB_11));
}

void read_analog_sensor(analog_sensor_t* sensor)
{
    if(!sensor) return;
    sensor->value = adc1_get_raw(sensor->pin);
}
