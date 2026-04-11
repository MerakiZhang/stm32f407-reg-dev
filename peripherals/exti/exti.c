#include "exti.h"
#include "delay.h"

/*
 * 外部中断配置：将按键引脚映射到 EXTI 线，触发中断
 *
 * KEY_UP → PA0 → EXTI0，上升沿触发（按下为高电平）
 * KEY0   → PE4 → EXTI4，下降沿触发（按下为低电平）
 * KEY1   → PE3 → EXTI3，下降沿触发（按下为低电平）
 * KEY2   → PE2 → EXTI2，下降沿触发（按下为低电平）
 *
 * 消抖：ISR 内使用 delay_get_tick() 时间戳，两次触发间隔 < 20ms 则忽略
 */

/* 按键中断标志位定义 */
volatile uint8_t exti_key_up_flag = 0;
volatile uint8_t exti_key0_flag   = 0;
volatile uint8_t exti_key1_flag   = 0;
volatile uint8_t exti_key2_flag   = 0;

void exti_key_init(void)
{
    /* 1. 使能 GPIOA、GPIOE 时钟 */
    RCC->AHB1ENR |= (1U << 0);   /* GPIOAEN */
    RCC->AHB1ENR |= (1U << 4);   /* GPIOEEN */

    /* 2. PA0：输入模式，内部下拉 */
    GPIOA->MODER &= ~(3U << (0 * 2));
    GPIOA->PUPDR &= ~(3U << (0 * 2));
    GPIOA->PUPDR |=  (2U << (0 * 2));   /* 10 = pull-down */

    /* 3. PE4/PE3/PE2：输入模式，内部上拉 */
    GPIOE->MODER &= ~((3U << (4 * 2)) | (3U << (3 * 2)) | (3U << (2 * 2)));
    GPIOE->PUPDR &= ~((3U << (4 * 2)) | (3U << (3 * 2)) | (3U << (2 * 2)));
    GPIOE->PUPDR |=  ((1U << (4 * 2)) | (1U << (3 * 2)) | (1U << (2 * 2)));  /* 01 = pull-up */

    /* 4. 使能 SYSCFG 时钟（APB2，bit14） */
    RCC->APB2ENR |= (1U << 14);

    /* 5. SYSCFG EXTICR：将 GPIO 端口映射到对应 EXTI 线
     *    端口编码：PA=0, PB=1, PC=2, PD=3, PE=4
     *    EXTICR[0] 管 EXTI0~3，EXTICR[1] 管 EXTI4~7
     *    每条线占 4 位，EXTI0 在 EXTICR[0][3:0]，EXTI1 在 [7:4]，以此类推
     */

    /* EXTI0 → PA0（port A = 0x0） */
    SYSCFG->EXTICR[0] &= ~(0xFU << 0);
    SYSCFG->EXTICR[0] |=  (0x0U << 0);

    /* EXTI2 → PE2（port E = 0x4） */
    SYSCFG->EXTICR[0] &= ~(0xFU << 8);
    SYSCFG->EXTICR[0] |=  (0x4U << 8);

    /* EXTI3 → PE3（port E = 0x4） */
    SYSCFG->EXTICR[0] &= ~(0xFU << 12);
    SYSCFG->EXTICR[0] |=  (0x4U << 12);

    /* EXTI4 → PE4（port E = 0x4） */
    SYSCFG->EXTICR[1] &= ~(0xFU << 0);
    SYSCFG->EXTICR[1] |=  (0x4U << 0);

    /* 6. 触发沿配置 */
    EXTI->RTSR |= (1U << 0);                              /* EXTI0：上升沿（KEY_UP 高电平有效） */
    EXTI->FTSR |= (1U << 2) | (1U << 3) | (1U << 4);     /* EXTI2/3/4：下降沿（KEY0/1/2 低电平有效） */

    /* 7. 取消中断屏蔽 */
    EXTI->IMR |= (1U << 0) | (1U << 2) | (1U << 3) | (1U << 4);

    /* 8. 配置 NVIC：优先级 2，低于 SysTick（优先级 0） */
    NVIC_SetPriority(EXTI0_IRQn, 2);
    NVIC_SetPriority(EXTI2_IRQn, 2);
    NVIC_SetPriority(EXTI3_IRQn, 2);
    NVIC_SetPriority(EXTI4_IRQn, 2);

    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_EnableIRQ(EXTI2_IRQn);
    NVIC_EnableIRQ(EXTI3_IRQn);
    NVIC_EnableIRQ(EXTI4_IRQn);
}

/* KEY_UP (PA0) 中断处理 */
void EXTI0_IRQHandler(void)
{
    static uint32_t last_tick = 0;
    uint32_t now = delay_get_tick();

    if ((now - last_tick) >= 20U)   /* 消抖：20ms 内只响应一次 */
    {
        last_tick = now;
        exti_key_up_flag = 1;
    }

    EXTI->PR = (1U << 0);   /* 清除 pending 位（写 1 清零） */
}

/* KEY2 (PE2) 中断处理 */
void EXTI2_IRQHandler(void)
{
    static uint32_t last_tick = 0;
    uint32_t now = delay_get_tick();

    if ((now - last_tick) >= 20U)
    {
        last_tick = now;
        exti_key2_flag = 1;
    }

    EXTI->PR = (1U << 2);
}

/* KEY1 (PE3) 中断处理 */
void EXTI3_IRQHandler(void)
{
    static uint32_t last_tick = 0;
    uint32_t now = delay_get_tick();

    if ((now - last_tick) >= 20U)
    {
        last_tick = now;
        exti_key1_flag = 1;
    }

    EXTI->PR = (1U << 3);
}

/* KEY0 (PE4) 中断处理 */
void EXTI4_IRQHandler(void)
{
    static uint32_t last_tick = 0;
    uint32_t now = delay_get_tick();

    if ((now - last_tick) >= 20U)
    {
        last_tick = now;
        exti_key0_flag = 1;
    }

    EXTI->PR = (1U << 4);
}
