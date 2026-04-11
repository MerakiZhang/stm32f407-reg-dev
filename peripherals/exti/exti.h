#ifndef __EXTI_H
#define __EXTI_H

#include "stm32f4xx.h"

/*
 * 按键中断标志位
 * ISR 触发后置 1，主循环读取并清 0
 */
extern volatile uint8_t exti_key_up_flag;   /* KEY_UP (PA0) 触发 */
extern volatile uint8_t exti_key0_flag;     /* KEY0   (PE4) 触发 */
extern volatile uint8_t exti_key1_flag;     /* KEY1   (PE3) 触发 */
extern volatile uint8_t exti_key2_flag;     /* KEY2   (PE2) 触发 */

void exti_key_init(void);

#endif /* __EXTI_H */
