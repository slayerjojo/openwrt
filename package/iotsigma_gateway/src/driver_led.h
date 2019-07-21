#ifndef __DRIVER_LED_H__
#define __DRIVER_LED_H__

#include "env.h"

#define LED_ID_NET_RED 0ul
#define LED_ID_APP 1ul

void driver_led_init(void);
void driver_led_update(void);

void led_set(uint8_t led, uint8_t pulse, uint8_t repeat);

#endif
