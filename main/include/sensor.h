//
// Created by derk on 7-9-20.
//

#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <driver/adc.h>

typedef struct
{
    adc1_channel_t pin;
    int32_t value;
} analog_sensor_t;

void initialize_analog_sensor(analog_sensor_t* sensor, adc1_channel_t pin);
void read_analog_sensor(analog_sensor_t* sensor);

#endif //SENSOR_H
