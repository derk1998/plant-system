//
// Created by derk on 21-9-20.
//

#include "button.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <driver/gpio.h>

#define ESP_INTR_FLAG_DEFAULT 0

SemaphoreHandle_t reset_button_semaphore = NULL ;


static void IRAM_ATTR isr_reset_button_pressed(void *args)
{
    xSemaphoreGiveFromISR(reset_button_semaphore, NULL);
}

void setup_reset_button(void)
{
    reset_button_semaphore = xSemaphoreCreateBinary();
    gpio_pad_select_gpio(RESET_CONNECTION_BUTTON_GPIO);
    gpio_set_direction(RESET_CONNECTION_BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(RESET_CONNECTION_BUTTON_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(RESET_CONNECTION_BUTTON_GPIO, GPIO_INTR_POSEDGE);

    //Configure interrupt and add handler
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(RESET_CONNECTION_BUTTON_GPIO, isr_reset_button_pressed, NULL);
}

bool wait_until_reset_button_pressed(void)
{
    if ( xSemaphoreTake (reset_button_semaphore , portMAX_DELAY)  == pdTRUE )
    {
        return gpio_get_level(RESET_CONNECTION_BUTTON_GPIO);
    }
    return false;
}