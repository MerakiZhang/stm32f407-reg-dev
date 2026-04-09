#include "led.h"

/* LED0: PF9, LED1: PF10, 低电平点亮 */

void led_init(void)
{
    /* 使能 GPIOF 时钟 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;

    /* 配置 PF9、PF10 为通用输出模式（MODER = 01） */
    GPIOF->MODER &= ~((3U << (9 * 2)) | (3U << (10 * 2)));
    GPIOF->MODER |=  ((1U << (9 * 2)) | (1U << (10 * 2)));

    /* 配置为推挽输出（OTYPER = 0） */
    GPIOF->OTYPER &= ~((1U << 9) | (1U << 10));

    /* 配置为低速（OSPEEDR = 00） */
    GPIOF->OSPEEDR &= ~((3U << (9 * 2)) | (3U << (10 * 2)));

    /* 无上下拉（PUPDR = 00） */
    GPIOF->PUPDR &= ~((3U << (9 * 2)) | (3U << (10 * 2)));

    /* 默认熄灭：PF9、PF10 输出高电平 */
    GPIOF->BSRR = (1U << 9) | (1U << 10);
}

void led_on(uint8_t led)
{
    if (led == 0)
        GPIOF->BSRR = (1U << (9 + 16));   /* BR9：PF9 输出低电平 */
    else if (led == 1)
        GPIOF->BSRR = (1U << (10 + 16));  /* BR10：PF10 输出低电平 */
}

void led_off(uint8_t led)
{
    if (led == 0)
        GPIOF->BSRR = (1U << 9);    /* BS9：PF9 输出高电平 */
    else if (led == 1)
        GPIOF->BSRR = (1U << 10);   /* BS10：PF10 输出高电平 */
}

void led_toggle(uint8_t led)
{
    if (led == 0)
        GPIOF->ODR ^= (1U << 9);
    else if (led == 1)
        GPIOF->ODR ^= (1U << 10);
}
