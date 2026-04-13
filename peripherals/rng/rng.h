#ifndef __RNG_H
#define __RNG_H

#include "stm32f4xx.h"

/*
 * 硬件随机数发生器（RNG）
 *
 * 时钟来源：PLLQ = 48MHz（AHB2 总线，需 ≥ 40MHz 且 ≤ 48MHz）
 * 基地址：0x50060800（AHB2 外设）
 *
 * 每次调用 rng_get() 阻塞等待 DRDY 置位（约 40 个 RNG 时钟周期），
 * 返回硬件生成的 32 位随机数。
 */

void     rng_init(void);
uint32_t rng_get(void);
uint8_t  rng_get_range(uint8_t max);   /* 返回 [0, max] 范围内的随机整数 */

#endif /* __RNG_H */
