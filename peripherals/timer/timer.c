#include "timer.h"

/* ============================================================
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

/* ============================================================
 * TIM5 输入捕获：测量 PA0（KEY_UP）高电平脉宽
 *
 * 硬件映射：PA0 → TIM5_CH1，AF2
 * 时钟：APB1 定时器时钟 = 84 MHz
 * 预分频：PSC = 83 → 计数频率 1 MHz（1 µs / tick）
 * 自动重载：ARR = 0xFFFFFFFF（32 位，满量程约 4295 s）
 *
 * 测量原理：
 *   上升沿捕获 → 记录 CCR1 起始值，切换为下降沿捕获
 *   下降沿捕获 → 脉宽 = CCR1 − 起始值（32 位减法自动处理溢出）
 *
 * 注意：tim5ic_init() 会将 PA0 重新配置为 AF2，
 *       须在 key_init() 之后调用（下拉由 key_init 配置，保持不变）。
 * ============================================================ */

volatile uint32_t tim5_pulse_us    = 0;
volatile uint8_t  tim5_pulse_ready = 0;

static uint32_t tim5_rise_val = 0;
static uint8_t  tim5_edge     = 0;   /* 0 = 等待上升沿，1 = 等待下降沿 */

void tim5ic_init(void)
{
    /* 1. 使能 GPIOA 时钟（key_init 已使能，重复无害） */
    RCC->AHB1ENR |= (1U << 0);

    /* 2. PA0 → 复用功能模式（MODER = 10） */
    GPIOA->MODER &= ~(3U << (0 * 2));
    GPIOA->MODER |=  (2U << (0 * 2));

    /* 3. PA0 → AF2（TIM5_CH1） */
    GPIOA->AFR[0] &= ~(0xFU << (0 * 4));
    GPIOA->AFR[0] |=  (2U   << (0 * 4));

    /* 下拉保留 key_init() 设定，按键未按时 PA0 保持低电平，不误触发 */

    /* 4. 使能 TIM5 时钟（APB1 bit3） */
    RCC->APB1ENR |= (1U << 3);

    /* 5. 预分频：84 MHz / 84 = 1 MHz */
    TIM5->PSC = 83;

    /* 6. 自动重载：32 位最大值 */
    TIM5->ARR = 0xFFFFFFFFU;

    /* 7. CH1 输入捕获：映射到 TI1（CC1S = 01），无滤波，无分频 */
    TIM5->CCMR1 &= ~(0xFFU);
    TIM5->CCMR1 |=  (1U << 0);   /* CC1S[1:0] = 01 */

    /* 8. 捕获极性：上升沿（CC1P = 0，CC1NP = 0） */
    TIM5->CCER &= ~((1U << 1) | (1U << 3));

    /* 9. 使能通道 1 捕获（CC1E = 1） */
    TIM5->CCER |= (1U << 0);

    /* 10. 使能 CC1 捕获中断（CC1IE = 1） */
    TIM5->DIER |= (1U << 1);

    /* 11. 产生更新事件装载影子寄存器，然后清除所有标志 */
    TIM5->EGR = (1U << 0);
    TIM5->SR  = 0;

    /* 12. NVIC：优先级 2（与 EXTI 同级，低于 TIM3） */
    NVIC_SetPriority(TIM5_IRQn, 2);
    NVIC_EnableIRQ(TIM5_IRQn);

    /* 13. 启动计数器 */
    TIM5->CR1 |= (1U << 0);

    tim5_edge = 0;
}

void TIM5_IRQHandler(void)
{
    if (TIM5->SR & (1U << 1))    /* CC1IF */
    {
        TIM5->SR &= ~(1U << 1);  /* 清除 CC1IF */

        if (tim5_edge == 0)
        {
            /* 上升沿：记录起始值，切换为下降沿捕获 */
            tim5_rise_val  = TIM5->CCR1;
            TIM5->CCER    |=  (1U << 1);   /* CC1P = 1 → 下降沿 */
            tim5_edge      = 1;
        }
        else
        {
            /* 下降沿：计算脉宽，32 位减法自动处理计数器溢出 */
            tim5_pulse_us   = TIM5->CCR1 - tim5_rise_val;
            tim5_pulse_ready = 1;
            TIM5->CCER     &= ~(1U << 1);  /* CC1P = 0 → 切回上升沿 */
            tim5_edge       = 0;
        }
    }
}
