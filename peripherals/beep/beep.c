#include "beep.h"

/* BEEP: PF8，高电平打开，低电平关闭 */

void beep_init(void)
{
    /* 使能 GPIOF 时钟 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;

    /* 配置 PF8 为通用输出模式（MODER = 01） */
    GPIOF->MODER &= ~(3U << (8 * 2));
    GPIOF->MODER |=  (1U << (8 * 2));

    /* 配置为推挽输出（OTYPER = 0） */
    GPIOF->OTYPER &= ~(1U << 8);

    /* 配置为低速（OSPEEDR = 00） */
    GPIOF->OSPEEDR &= ~(3U << (8 * 2));

    /* 无上下拉（PUPDR = 00） */
    GPIOF->PUPDR &= ~(3U << (8 * 2));

    /* 默认关闭：PF8 输出低电平 */
    GPIOF->BSRR = (1U << (8 + 16));
}

void beep_on(void)
{
    /* BS8：PF8 输出高电平，蜂鸣器打开 */
    GPIOF->BSRR = (1U << 8);
}

void beep_off(void)
{
    /* BR8：PF8 输出低电平，蜂鸣器关闭 */
    GPIOF->BSRR = (1U << (8 + 16));
}

void beep_toggle(void)
{
    GPIOF->ODR ^= (1U << 8);
}
