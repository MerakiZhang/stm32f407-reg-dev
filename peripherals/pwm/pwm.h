#ifndef __PWM_H
#define __PWM_H

#include "stm32f4xx.h"

/*
 * LED0 (PF9) — TIM14_CH1 (AF9) 硬件 PWM
 *
 * TIM14 时钟：APB1 分频系数 = 4 ≠ 1，TIM 时钟 = 2 × 42MHz = 84MHz
 * PSC = 83，ARR = 999  →  f_PWM = 84MHz / 84 / 1000 = 1kHz
 */

void pwm_init(void);
void pwm_set_duty(uint8_t duty);    /* duty: 0（灭）~ 100（全亮） */

#endif /* __PWM_H */
