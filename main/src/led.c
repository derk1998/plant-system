//
// Created by derk on 20-9-20.
//

#include "led.h"

#include <esp32/rom/gpio.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static void led_task(void* param);

static TaskHandle_t led_task_handle = NULL;

void initialize_status_led(void)
{
    gpio_pad_select_gpio(STATUS_LED);
    gpio_set_direction(STATUS_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(STATUS_LED, LED_MODE_OFF);

    xTaskCreate(led_task, "led_task", 2048, NULL, 3, &led_task_handle);
}

void set_led_status(enum led_modes led_mode)
{
    xTaskNotify(led_task_handle, led_mode, eSetValueWithOverwrite);
}

static void led_task(void* param)
{
    uint32_t value = 0;
    uint32_t received_value;

    for(;;)
    {
        received_value = LED_MODE_NOT_SET;
        if(xTaskNotifyWait(0x00, ULONG_MAX, &received_value, 10) == pdTRUE)
            value = received_value;

        switch(value)
        {
        case LED_MODE_OFF:
        case LED_MODE_ON:
            gpio_set_level(STATUS_LED, value);
            vTaskDelay(10);
            break;
        case LED_MODE_BLINK:
            gpio_set_level(STATUS_LED, LED_MODE_ON);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            gpio_set_level(STATUS_LED, LED_MODE_OFF);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            break;
        case LED_MODE_FAST_BLINK:
            gpio_set_level(STATUS_LED, LED_MODE_ON);
            vTaskDelay(300 / portTICK_PERIOD_MS);
            gpio_set_level(STATUS_LED, LED_MODE_OFF);
            vTaskDelay(300 / portTICK_PERIOD_MS);
            break;
        case LED_MODE_FASTER_BLINK:
            gpio_set_level(STATUS_LED, LED_MODE_ON);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            gpio_set_level(STATUS_LED, LED_MODE_OFF);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            break;
        }

    }
}