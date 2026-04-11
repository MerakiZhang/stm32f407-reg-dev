#include "pwm.h"

/*
 * TIM14 硬件 PWM — 控制 LED0（PF9）亮度
 *
 * 引脚复用：PF9 → AF9 → TIM14_CH1
 * TIM14 时钟：84MHz（APB1 × 2）
 * PWM 参数：PSC=83，ARR=999 → f_PWM = 84MHz / 84 / 1000 = 1kHz
 *
 * LED0 为低电平点亮（Active-Low）。
 * PWM 模式 1：计数值 < CCR1 时输出高电平，≥ CCR1 时输出低电平。
 * 低电平（LED 亮）占比 = (ARR + 1 - CCR1) / (ARR + 1)
 * 因此：CCR1 = (100 - duty) × 10
 *   duty=0   → CCR1=1000 → 始终高电平 → LED 灭
 *   duty=50  → CCR1=500  → 50% 低电平 → LED 半亮
 *   duty=100 → CCR1=0    → 始终低电平 → LED 全亮
 */

void pwm_init(void)
{
    /* --------------------------------------------------------
     * 1. GPIO：PF9 配置为复用功能模式，AF9 = TIM14_CH1
     * -------------------------------------------------------- */
    RCC->AHB1ENR |= (1U << 5);              /* 使能 GPIOF 时钟 */

    GPIOF->MODER  &= ~(3U << 18);           /* 清除 PF9 模式位 */
    GPIOF->MODER  |=  (2U << 18);           /* PF9 = 复用功能（AF） */
    GPIOF->OSPEEDR |= (2U << 18);           /* 高速 */
    GPIOF->OTYPER  &= ~(1U << 9);           /* 推挽输出 */
    GPIOF->PUPDR   &= ~(3U << 18);          /* 无上下拉 */

    /* AFRH（AFR[1]）PF9 对应 bits[7:4]，AF9 = 0b1001 */
    GPIOF->AFR[1] &= ~(0xFU << 4);
    GPIOF->AFR[1] |=  (9U   << 4);          /* AF9 = TIM14_CH1 */

    /* --------------------------------------------------------
     * 2. TIM14 PWM 配置
     * -------------------------------------------------------- */
    RCC->APB1ENR |= (1U << 8);              /* 使能 TIM14 时钟（APB1ENR bit8） */

    TIM14->PSC = 83;                         /* 84MHz / 84 = 1MHz 计数频率 */
    TIM14->ARR = 999;                        /* 1MHz / 1000 = 1kHz PWM 频率 */

    /*
     * CCMR1：
     *   OC1M[2:0] = 110（bits[6:4]）→ PWM 模式 1
     *   OC1PE     = 1  （bit 3）    → 输出比较预装载使能
     */
    TIM14->CCMR1 = (6U << 4) | (1U << 3);

    /* CCER：CC1E=1（输出使能），CC1P=0（高电平有效极性，不取反） */
    TIM14->CCER |= (1U << 0);

    /* CR1：ARPE=1（ARR 预装载使能） */
    TIM14->CR1 |= (1U << 7);

    /* 产生更新事件，将 PSC/ARR 立即装入影子寄存器 */
    TIM14->EGR = (1U << 0);                 /* UG */

    /* 初始占空比 0：LED0 熄灭 */
    TIM14->CCR1 = 1000;

    /* 启动计数器 */
    TIM14->CR1 |= (1U << 0);               /* CEN = 1 */
}

/*
 * pwm_set_duty — 设置 LED0 亮度
 *
 * duty: 0（完全熄灭）~ 100（全亮）
 */
void pwm_set_duty(uint8_t duty)
{
    if (duty > 100) duty = 100;
    TIM14->CCR1 = (uint16_t)((100U - duty) * 10U);
}
