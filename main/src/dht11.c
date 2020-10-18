/*
 * MIT License
 *
 * Copyright (c) 2018 Michele Biondi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp32/rom/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "dht11.h"

static int wait_or_timeout(dht11_t* dht11, uint16_t microseconds, int32_t level) {
    int32_t micros_ticks = 0;
    while(gpio_get_level(dht11->pin) == level)
    {
        if(micros_ticks++ > microseconds)
            return DHT11_TIMEOUT_ERROR;
        ets_delay_us(1);
    }
    return micros_ticks;
}

static int32_t check_crc(const uint8_t* data)
{
    if(data[4] == (data[0] + data[1] + data[2] + data[3]))
        return DHT11_OK;
    return DHT11_CRC_ERROR;
}

static void send_start_signal(dht11_t* dht11)
{
    gpio_num_t pin = dht11->pin;
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    ets_delay_us(20 * 1000);
    gpio_set_level(pin, 1);
    ets_delay_us(40);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
}

static int32_t check_response(dht11_t* dht11)
{
    // Wait for next step ~80us
    if(wait_or_timeout(dht11, 80, 0) == DHT11_TIMEOUT_ERROR)
        return DHT11_TIMEOUT_ERROR;

    // Wait for next step ~80us
    if(wait_or_timeout(dht11, 80, 1) == DHT11_TIMEOUT_ERROR)
        return DHT11_TIMEOUT_ERROR;

    return DHT11_OK;
}

void initialize_dht11(dht11_t* dht11, gpio_num_t gpio)
{
    if(!dht11) return;

    // Wait 1 seconds to make the device pass its initial unstable status
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    dht11->pin = gpio;
}

void read_dht11(dht11_t* dht11)
{
    if(!dht11) return;
    // Tried to sense too soon since last read (dht11 needs ~2 seconds to make a new read)
    if(esp_timer_get_time() - 2000000 < dht11->last_read_time) return;

    dht11->last_read_time = esp_timer_get_time();

    uint8_t data[5] = {0, 0, 0, 0, 0};

    send_start_signal(dht11);
    if(check_response(dht11) == DHT11_TIMEOUT_ERROR) return;

    // Read response
    for(int i = 0; i < 40; i++)
    {
        // Initial data
        if(wait_or_timeout(dht11, 50, 0) == DHT11_TIMEOUT_ERROR) return;

        if(wait_or_timeout(dht11, 70, 1) > 28)
        {
            /* Bit received was a 1 */
            data[i/8] |= (1 << (7-(i%8)));
        }
    }

    if(check_crc(data) != DHT11_CRC_ERROR)
    {
        dht11->temperature = data[2];
        dht11->humidity = data[0];
    }
}