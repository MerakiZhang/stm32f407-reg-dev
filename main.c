#include "stm32f4xx.h"
#include "sys.h"
#include "delay.h"
#include "led.h"
#include "beep.h"
#include "exti.h"

/*
 * 外部中断按键测试：
 *   KEY_UP (PA0) → LED0、LED1 同时翻转
 *   KEY0   (PE4) → LED0 翻转
 *   KEY1   (PE3) → LED1 翻转
 *   KEY2   (PE2) → 蜂鸣器翻转
 *
 * 按键事件由 EXTI 中断驱动，主循环只检查标志位，无需轮询。
 */

int main(void)
{
    sys_clock_init();
    delay_init();
    led_init();
    beep_init();
    exti_key_init();

    while (1)
    {
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
