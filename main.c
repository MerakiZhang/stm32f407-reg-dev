#include "stm32f4xx.h"
#include "sys.h"
#include "delay.h"
#include "led.h"
#include "beep.h"
#include "key.h"

/*
 * 按键测试：
 *   KEY_UP → LED0、LED1 同时翻转
 *   KEY0   → LED0 翻转
 *   KEY1   → LED1 翻转
 *   KEY2   → 蜂鸣器翻转
 */

int main(void)
{
    sys_clock_init();
    delay_init();
    led_init();
    beep_init();
    key_init();

    while (1)
    {
        uint8_t key = key_scan(0);

        switch (key)
        {
            case KEY_UP_VAL:
                led_toggle(LED0);
                led_toggle(LED1);
                break;
            case KEY0_VAL:
                led_toggle(LED0);
                break;
            case KEY1_VAL:
                led_toggle(LED1);
                break;
            case KEY2_VAL:
                beep_toggle();
                break;
            default:
                break;
        }

        delay_ms(10);
    }
}
