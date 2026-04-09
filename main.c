#include "stm32f4xx.h"
#include "sys.h"
#include "beep.h"
#include "delay.h"

int main(void)
{
    sys_clock_init();   /* 必须最先调用，切换到 168MHz */
    delay_init();
    beep_init();

    while (1)
    {
        /* 测试 beep_on / beep_off */
        beep_on();
        delay_ms(2000);
        beep_off();
        delay_ms(2000);

        /* 测试 beep_toggle：蜂鸣器间隔 1s 连续切换 6 次 */
        for (int i = 0; i < 6; i++)
        {
            beep_toggle();
            delay_ms(1000);
        }

        /* 确保循环结束后蜂鸣器处于关闭状态 */
        beep_off();
        delay_ms(2000);
    }
}
