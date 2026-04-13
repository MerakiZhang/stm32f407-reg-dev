#ifndef __IWDG_H
#define __IWDG_H

#include "stm32f4xx.h"

/*
 * IWDG — 独立看门狗
 *
 * 时钟源：LSI 内部 RC（约 32 kHz），启动 IWDG 时自动激活，无需 RCC 手动使能。
 * 超时范围：约 0.125 ms ~ 32768 ms（取决于 LSI 精度，误差约 ±20%）。
 *
 * 用法：
 *   iwdg_init(1000);   // 初始化，超时 1000ms，超时后自动复位
 *   while (1) {
 *       iwdg_feed();   // 在主循环中定期喂狗，间隔需小于超时时间
 *       ...
 *   }
 *
 * 注意：IWDG 一旦启动无法关闭，调试时可在 DBGMCU 寄存器中配置调试暂停时停止计数。
 */

void iwdg_init(uint32_t timeout_ms);   /* 初始化并启动，超时单位 ms */
void iwdg_feed(void);                  /* 喂狗（重载计数器） */

#endif /* __IWDG_H */
