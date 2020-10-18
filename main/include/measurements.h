//
// Created by derk on 7-9-20.
//

#ifndef MEASUREMENTS_H
#define MEASUREMENTS_H

#define HUMIDITY_GPIO ADC1_GPIO34_CHANNEL
#define LDR_GPIO ADC1_GPIO36_CHANNEL
#define DHT11_GPIO GPIO_NUM_21
#define KAKU_GPIO GPIO_NUM_23
#define KAKU_ID 123456

#include "stdint.h"

typedef void (*measurement_threshold_cb_t)(uint16_t value, uint16_t threshold);

void register_reached_light_threshold_cb(measurement_threshold_cb_t callback);
void register_reached_moisture_threshold_cb(measurement_threshold_cb_t callback);
void register_above_light_threshold_cb(measurement_threshold_cb_t callback);

void initialize_measurements(void);
void switch_radio_outlet(void);

int32_t get_temperature(void);
int32_t get_humidity(void);
int32_t get_soil_moisture_level(void);
int32_t get_light_level(void);

void set_light_threshold(uint16_t threshold);
void set_moisture_threshold(uint16_t threshold);

#endif //MEASUREMENTS_H
