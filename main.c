#include "stm32f4xx.h"
#include "sys.h"
#include "delay.h"
#include "pwm.h"

/*
 * PWM 呼吸灯测试：
 *   LED0 (PF9) — TIM14_CH1 硬件 PWM，亮度在 0~100 之间平滑渐变
 *   每步 10ms，渐亮 101 步 + 渐暗 99 步 = 200 步 × 10ms = 2s 一个周期
 */

int main(void)
{
    sys_clock_init();
    delay_init();
    pwm_init();

    while (1)
    {
        /* 渐亮：0 → 100 */
        for (uint8_t d = 0; d <= 100; d++)
        {
            pwm_set_duty(d);
            delay_ms(10);
        }
        /* 渐暗：99 → 0 */
        for (int16_t d = 99; d >= 0; d--)
        {
            pwm_set_duty((uint8_t)d);
            delay_ms(10);
        }
    }
}
