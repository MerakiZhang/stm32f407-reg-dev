#include "led.h"
#include "stm32f407xx.h"

void led_init(void)
{
    // 使能 GPIOF 时钟
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;

    // 配置 PF9、PF10 为通用输出模式
    GPIOF->MODER &= ~(GPIO_MODER_MODER9 | GPIO_MODER_MODER10);
    GPIOF->MODER |= (GPIO_MODER_MODER9_0 | GPIO_MODER_MODER10_0);

    // 配置为推挽输出
    GPIOF->OTYPER &= ~(GPIO_OTYPER_OT9 | GPIO_OTYPER_OT10);

    // 配置输出速度为高速
    GPIOF->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED9 | GPIO_OSPEEDR_OSPEED10);
    GPIOF->OSPEEDR |= (GPIO_OSPEEDR_OSPEED9_1 | GPIO_OSPEEDR_OSPEED10_1);

    // 配置无上下拉
    GPIOF->PUPDR &= ~(GPIO_PUPDR_PUPD9 | GPIO_PUPDR_PUPD10);

    // 默认输出高电平，让 LED 熄灭
    GPIOF->BSRR = (GPIO_BSRR_BS9 | GPIO_BSRR_BS10);
}

void led_on(uint8_t led)
{
    if (led == LED0)
    {
        // PF9 输出低电平点亮 LED0
        GPIOF->BSRR = GPIO_BSRR_BR9;
    }
    else if (led == LED1)
    {
        // PF10 输出低电平点亮 LED1
        GPIOF->BSRR = GPIO_BSRR_BR10;
    }
}

void led_off(uint8_t led)
{
    if (led == LED0)
    {
        // PF9 输出高电平熄灭 LED0
        GPIOF->BSRR = GPIO_BSRR_BS9;
    }
    else if (led == LED1)
    {
        // PF10 输出高电平熄灭 LED1
        GPIOF->BSRR = GPIO_BSRR_BS10;
    }
}
