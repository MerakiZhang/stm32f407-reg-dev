#ifndef __LED_H
#define __LED_H

#include <stdint.h>

#define LED0 0
#define LED1 1

void led_init(void);
void led_on(uint8_t led);
void led_off(uint8_t led);

#endif
