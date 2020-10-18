/*  Switch Klik aan/Klik uit switches transmitter
 *  Usage: switch_kaku(int pin, unsigned long id, int group, int dev, bool state, int repeat)
 *  pin = pin number
 *  id = unique id of transmitter (max 4194303)
 *  group = between 1 and 4,
 *  device = between 1 and 4, -1 switches the entire group
 *  state = true (on) or false (off)
 *  repeat = transmit repeats
 *  dimlevel = -1 (no dimmer), between 0 and 15 for the dimlevel
 */
#include "switch_kaku.h"
#include <driver/gpio.h>
#include <esp32/rom/ets_sys.h>

static void send_kaku_code(gpio_num_t pin, uint32_t code, uint8_t repeat);
static void send_syc(gpio_num_t pin, uint8_t period);
static void send_bit(int8_t value, gpio_num_t pin, uint8_t period);
static void send_kaku_dim_code(gpio_num_t pin, uint32_t id, uint32_t code, uint8_t repeat);

void initialize_kaku(gpio_num_t pin, uint32_t id, int8_t dim_level, kaku_group_t group, kaku_device_t device,
                     uint8_t repeat, kaku_t* kaku)
{
    assert(kaku);
    assert(dim_level >= -1 && dim_level <= 15);

    kaku->pin = pin;
    kaku->id = id;
    kaku->dim_level = dim_level;
    kaku->group = group;
    kaku->device = device;
    kaku->repeat = repeat;
    kaku->state = KAKU_STATE_OFF;

    gpio_pad_select_gpio(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
}

void switch_kaku(kaku_t* kaku)
{
    assert(kaku);
    kaku->state =!kaku->state;
    int8_t dev = kaku->device - 1;
    if (kaku->device == KAKU_DEVICE_ALL) dev = 1u<<5u;
    if (kaku->dim_level == -1)
    {
        uint32_t code = ((kaku->id << 6u | dev) | kaku->state << 4u) | (kaku->group - 1) << 2u;
        printf("code: %d\n", code);
        send_kaku_code(kaku->pin, code, kaku->repeat);
    }
    else
        send_kaku_dim_code(kaku->pin, kaku->id, (((dev << 4u) | kaku->state << 8u) | (kaku->group - 1) << 6u) |
        kaku->dim_level, kaku->repeat);
}

static void send_kaku_code(gpio_num_t pin, uint32_t code, uint8_t repeat)
{
    uint8_t period = 230;
    for (uint8_t i = 0; i < repeat; i++)
    {
        send_syc(pin, period);
        for (int8_t j = 31; j>=0; j--)
        {
            send_bit(((code & (1 << j)) == (1 << j)), pin, period);
        }
        gpio_set_level(pin, 1);
        ets_delay_us(period);
        gpio_set_level(pin, 0);
    }
}

static void send_kaku_dim_code(gpio_num_t pin, uint32_t id, uint32_t code, uint8_t repeat)
{
    uint8_t period = 230;
    for (uint8_t i = 0; i < repeat; i++){
        send_syc(pin, period);
        for (int8_t j = 25; j>=0; j--){
            send_bit(((id & (1 << j)) == (1 << j)), pin, period);
        }
        for (int8_t i = 9; i>=0; i--){
            if (i == 8) {
                send_bit(-1, pin, period);
            } else {
                send_bit(((code & (1 << i)) == (1 << i)), pin, period);
            }
        }
        gpio_set_level(pin, 1);
        ets_delay_us(period);
        gpio_set_level(pin, 0);
    };
}


static void send_syc(gpio_num_t pin, uint8_t period)
{
    gpio_set_level(pin, 0);
    ets_delay_us(47*period);
    gpio_set_level(pin, 1);
    ets_delay_us(period);
    gpio_set_level(pin, 0);
    ets_delay_us(period*12);
}

static void send_bit(int8_t value, gpio_num_t pin, uint8_t period)
{
//    printf("Send bit: value -> %d, pin -> %d, period-> %d\n", value, pin, period);
    if (value == 0){
        gpio_set_level(pin, 1);
        ets_delay_us(period);
        gpio_set_level(pin, 0);
        ets_delay_us((uint32_t)(period*1.4));
        gpio_set_level(pin,1);
        ets_delay_us(period);
        gpio_set_level(pin, 0);
        ets_delay_us(period*6);
    }
    else if (value == 1)
    {
        gpio_set_level(pin,1);
        ets_delay_us(period);
        gpio_set_level(pin, 0);
        ets_delay_us(period*6);
        gpio_set_level(pin,1);
        ets_delay_us(period);
        gpio_set_level(pin, 0);
        ets_delay_us((uint32_t)(period*1.4));
    }
    else
    {
        gpio_set_level(pin, 1);
        ets_delay_us(period);
        gpio_set_level(pin, 0);
        ets_delay_us((uint32_t)(period*1.4));
        gpio_set_level(pin, 1);
        ets_delay_us(period);
        gpio_set_level(pin, 0);
        ets_delay_us((uint32_t)(period*1.4));
    }
}