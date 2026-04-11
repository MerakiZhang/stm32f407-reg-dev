#include "stm32f4xx.h"
#include "sys.h"
#include "delay.h"
#include "led.h"
#include "beep.h"
#include "exti.h"
#include "timer.h"

/*
 * 通用定时器测试：
 *   TIM3 每 500ms 产生更新中断，LED0 翻转（1Hz 闪烁）
 *
 * EXTI 按键测试（与定时器并行运行）：
 *   KEY_UP (PA0) → LED0、LED1 同时翻转
 *   KEY0   (PE4) → LED0 翻转
 *   KEY1   (PE3) → LED1 翻转
 *   KEY2   (PE2) → 蜂鸣器翻转
 */

int main(void)
{
    sys_clock_init();
    delay_init();
    led_init();
    beep_init();
    exti_key_init();
    timer3_init(8399, 4999);   /* 84MHz / 8400 / 5000 = 2Hz → 每 500ms 中断一次 */

    while (1)
    {
        if (timer3_flag)
        {
            timer3_flag = 0;
            led_toggle(LED0);
        }

        if (exti_key_up_flag)
        {
            exti_key_up_flag = 0;
            led_toggle(LED0);
            led_toggle(LED1);
        }

        if (exti_key0_flag)
        {
            exti_key0_flag = 0;
            led_toggle(LED0);
        }

        if (exti_key1_flag)
        {
            exti_key1_flag = 0;
            led_toggle(LED1);
        }

        if (exti_key2_flag)
        {
            exti_key2_flag = 0;
            beep_toggle();
        }
    }
}
