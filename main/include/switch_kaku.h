/*  Switch Klik aan/Klik uit switches transmitter
 *  Usage: switch_kaku(int pin, unsigned long id, int group, int dev, bool state, int repeat)
 *  pin = pin number
 *  id = unique id of transmitter (max 4194303)
 *  group = between 1 and 4,
 *  device = between 1 and 4, -1 switches the entire group
 *  state = true (on) or false (off)
 *  repeat = transmit repeats
 */

#include <stdbool.h>
#include <stdint.h>
#include <hal/gpio_types.h>

typedef enum
{
    KAKU_GROUP_1 = 1,
    KAKU_GROUP_2,
    KAKU_GROUP_3,
    KAKU_GROUP_4
} kaku_group_t;

typedef enum
{
    KAKU_DEVICE_ALL = -1,
    KAKU_DEVICE_1 = 1,
    KAKU_DEVICE_2,
    KAKU_DEVICE_3,
    KAKU_DEVICE_4
} kaku_device_t;

typedef enum
{
    KAKU_STATE_OFF,
    KAKU_STATE_ON
} kaku_state_t;

typedef struct
{
    gpio_num_t pin;
    uint32_t id;
    kaku_state_t state;
    int8_t dim_level;
    kaku_group_t group;
    kaku_device_t device;
    uint8_t repeat;
} kaku_t;

void initialize_kaku(gpio_num_t pin, uint32_t id, int8_t dim_level, kaku_group_t group, kaku_device_t device,
                     uint8_t repeat, kaku_t* kaku);

void switch_kaku(kaku_t* kaku);