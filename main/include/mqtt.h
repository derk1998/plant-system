//
// Created by derk on 13-9-20.
//

#ifndef MQTT_H
#define MQTT_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*mqtt_threshold_cb_t)(uint16_t threshold);

typedef struct
{
    mqtt_threshold_cb_t received_light_threshold;
    mqtt_threshold_cb_t received_moisture_threshold;
} mqtt_callbacks_t;

typedef enum {LIGHT_STATES_ON, LIGHT_STATES_OFF, LIGHT_STATES_NOT_SET} light_states_t;

void start_mqtt_client(void);
void stop_mqtt_client(void);

void register_received_light_threshold_cb(mqtt_threshold_cb_t callback);
void register_received_moisture_threshold_cb(mqtt_threshold_cb_t callback);

void mqtt_send_light_message(bool status);
light_states_t get_light_state(void);

#endif //MQTT_H
