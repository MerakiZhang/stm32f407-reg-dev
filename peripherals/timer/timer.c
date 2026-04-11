#include "timer.h"

/*
 * TIM3 通用定时器配置：产生周期性更新中断
 *
 * TIM3 挂载在 APB1 总线（42MHz），因 APB1 分频系数 != 1，
 * 定时器时钟 = 2 × APB1 = 84MHz。
 *
 * 中断周期计算公式：
 *   T = (PSC + 1) × (ARR + 1) / 84MHz
 *
 * 示例（500ms）：
 *   PSC = 8399  →  计数频率 = 84MHz / 8400 = 10kHz（0.1ms / tick）
 *   ARR = 4999  →  5000 × 0.1ms = 500ms
 */

volatile uint8_t timer3_flag = 0;

void timer3_init(uint16_t psc, uint16_t arr)
{
    /* 1. 使能 TIM3 时钟（APB1，bit1） */
    RCC->APB1ENR |= (1U << 1);   /* TIM3EN */

    /* 2. 设置预分频和自动重载值 */
    TIM3->PSC = psc;
    TIM3->ARR = arr;

    /* 3. 产生更新事件，将 PSC/ARR 立即装入影子寄存器 */
    TIM3->EGR = (1U << 0);       /* UG：软件产生更新事件 */

    /* 4. 清除 EGR 写 UG 产生的 UIF，避免立即触发中断 */
    TIM3->SR &= ~(1U << 0);      /* 清 UIF */

    /* 5. 使能更新中断（DIER bit0 UIE） */
    TIM3->DIER |= (1U << 0);

    /* 6. 配置 NVIC：优先级 1（低于 SysTick=0，高于 EXTI=2） */
    NVIC_SetPriority(TIM3_IRQn, 1);
    NVIC_EnableIRQ(TIM3_IRQn);

    /* 7. 启动计数器（CR1 bit0 CEN） */
    TIM3->CR1 |= (1U << 0);
}

void TIM3_IRQHandler(void)
{
    if (TIM3->SR & (1U << 0))    /* 检查更新中断标志 UIF */
    {
        TIM3->SR &= ~(1U << 0);  /* 清除 UIF */
        timer3_flag = 1;
    }
}
