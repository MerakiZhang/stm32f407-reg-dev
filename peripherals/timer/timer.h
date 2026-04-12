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

/* ---------- TIM2：PA5 电容按键输入捕获 ---------- */
/*
 * 检测原理：充放电时间测量法
 *   拉低 PA5 放电 → 切换 AF 让上拉充电 → TIM2 捕获上升沿时间
 *   触摸时电容增大，充电变慢，时间 > 基线 + 阈值 → 判定按下
 *
 * tim2_cap_state: 0 = 未触摸，1 = 触摸按下（由 tim2cap_scan 更新）
 *
 * 注意：tim2cap_init() 内自动校准基线，调用时须保持无触摸状态
 */
extern volatile uint8_t tim2_cap_state;

void    tim2cap_init(void);       /* 初始化并校准基线 */
uint8_t tim2cap_scan(void);       /* 单次检测，返回 1=触摸，0=未触摸 */

#endif /* __TIMER_H */
