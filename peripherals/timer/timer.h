#ifndef __TIMER_H
#define __TIMER_H

#include "stm32f4xx.h"

/* ---------- TIM3：周期性更新中断 ---------- */
extern volatile uint8_t timer3_flag;

void timer3_init(uint16_t psc, uint16_t arr);

/* ---------- TIM5：PA0 高电平脉宽捕获 ---------- */
/*
 * tim5_pulse_us   : 最近一次高电平脉宽，单位 µs（32 位）
 * tim5_pulse_ready: 1 = 新数据就绪，读取后需手动清零
 */
extern volatile uint32_t tim5_pulse_us;
extern volatile uint8_t  tim5_pulse_ready;

void tim5ic_init(void);

#endif /* __TIMER_H */
