#include "delay.h"

static volatile uint32_t systick_ms = 0;

void SysTick_Handler(void)
{
    systick_ms++;
}

void delay_init(void)
{
    /* 配置 SysTick 每 1ms 产生一次中断，时钟源为 SystemCoreClock */
    SysTick_Config(SystemCoreClock / 1000);
}

void delay_ms(uint32_t ms)
{
    uint32_t start = systick_ms;
    while ((systick_ms - start) < ms);
}

uint32_t delay_get_tick(void)
{
    return systick_ms;
}
