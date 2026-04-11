#ifndef __TIMER_H
#define __TIMER_H

#include "stm32f4xx.h"

extern volatile uint8_t timer3_flag;

void timer3_init(uint16_t psc, uint16_t arr);

#endif /* __TIMER_H */
