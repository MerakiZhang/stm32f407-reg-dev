#include "key.h"
#include "delay.h"

/*
 * KEY_UP → PA0，内部下拉，高电平有效（按下为高）
 * KEY0   → PE4，内部上拉，低电平有效（按下为低）
 * KEY1   → PE3，内部上拉，低电平有效（按下为低）
 * KEY2   → PE2，内部上拉，低电平有效（按下为低）
 */

void key_init(void)
{
    /* 使能 GPIOA、GPIOE 时钟 */
    RCC->AHB1ENR |= (1U << 0);   /* GPIOAEN */
    RCC->AHB1ENR |= (1U << 4);   /* GPIOEEN */

    /* PA0：输入模式 */
    GPIOA->MODER &= ~(3U << (0 * 2));

    /* PA0：内部下拉 */
    GPIOA->PUPDR &= ~(3U << (0 * 2));
    GPIOA->PUPDR |=  (2U << (0 * 2));

    /* PE4/PE3/PE2：输入模式 */
    GPIOE->MODER &= ~((3U << (4 * 2)) | (3U << (3 * 2)) | (3U << (2 * 2)));

    /* PE4/PE3/PE2：内部上拉 */
    GPIOE->PUPDR &= ~((3U << (4 * 2)) | (3U << (3 * 2)) | (3U << (2 * 2)));
    GPIOE->PUPDR |=  ((1U << (4 * 2)) | (1U << (3 * 2)) | (1U << (2 * 2)));
}

/*
 * key_scan - 按键扫描
 *
 * mode = 0：单次触发，按住不松开只返回一次
 * mode = 1：连续触发，按住持续返回按键值
 *
 * 返回值：KEY_UP_VAL / KEY0_VAL / KEY1_VAL / KEY2_VAL / 0（无按键）
 *
 * 优先级：KEY_UP > KEY0 > KEY1 > KEY2
 */
uint8_t key_scan(uint8_t mode)
{
    static uint8_t key_up = 1;   /* 按键松开标志，1 表示当前无按键按下 */

    if (mode == 1)
        key_up = 1;   /* 连续触发模式：每次进入都允许检测 */

    if (key_up &&
        ((GPIOA->IDR & (1U << 0)) ||   /* KEY_UP 按下：PA0 高电平 */
         !(GPIOE->IDR & (1U << 4)) ||  /* KEY0 按下：PE4 低电平 */
         !(GPIOE->IDR & (1U << 3)) ||  /* KEY1 按下：PE3 低电平 */
         !(GPIOE->IDR & (1U << 2))))   /* KEY2 按下：PE2 低电平 */
    {
        delay_ms(10);   /* 消抖延时 */
        key_up = 0;

        if (GPIOA->IDR & (1U << 0))    return KEY_UP_VAL;
        if (!(GPIOE->IDR & (1U << 4))) return KEY0_VAL;
        if (!(GPIOE->IDR & (1U << 3))) return KEY1_VAL;
        if (!(GPIOE->IDR & (1U << 2))) return KEY2_VAL;
    }
    else if (!(GPIOA->IDR & (1U << 0)) &&
               (GPIOE->IDR & (1U << 4)) &&
               (GPIOE->IDR & (1U << 3)) &&
               (GPIOE->IDR & (1U << 2)))
    {
        key_up = 1;   /* 所有按键松开，复位标志 */
    }

    return 0;
}
