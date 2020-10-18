//
// Created by derk on 20-9-20.
//

#ifndef LED_H
#define LED_H

#define STATUS_LED GPIO_NUM_16

enum led_modes
{
    LED_MODE_OFF,
    LED_MODE_ON,
    LED_MODE_BLINK,
    LED_MODE_FAST_BLINK,
    LED_MODE_FASTER_BLINK,
    LED_MODE_NOT_SET
};

void initialize_status_led(void);
void set_led_status(enum led_modes led_mode);

#endif //LED_H
