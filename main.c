#include "stm32f4xx.h"
#include "sys.h"
#include "delay.h"
#include "key.h"
#include "pwm.h"
#include "uart.h"
#include "timer.h"
#include <stdio.h>

/*
 * 功能：
 *   1. LED0（PF9）呼吸灯 —— TIM14 PWM，占空比 0→100→0 循环渐变，非阻塞
 *      单程 100 步 × 20ms = 2s，完整一个呼吸周期约 4s
 *
 *   2. KEY_UP（PA0）高电平脉宽检测 —— TIM5 输入捕获
 *      上升沿记录起始，下降沿计算差值，精度 1µs，量程约 4295s
 *
 *   3. 检测结果通过 USART1（PA9/PA10，115200 bps）发送到 PC
 *
 * 初始化顺序：
 *   key_init()    先配置 PA0 内部下拉（GPIO 输入）
 *   tim5ic_init() 后将 PA0 切换为 AF2（TIM5_CH1），下拉保留
 */

int main(void)
{
    sys_clock_init();
    delay_init();
    key_init();        /* 配置 PA0 内部下拉，供 tim5ic_init() 依赖 */
    pwm_init();        /* TIM14 PWM → LED0 呼吸灯 */
    uart1_init(115200);
    tim5ic_init();     /* TIM5 输入捕获 → KEY_UP 脉宽，须在 key_init() 后调用 */

    uart1_send_string("=== STM32F407 Ready ===\r\n");
    uart1_send_string("LED0: breathing | Hold KEY_UP and release to see pulse width\r\n");

    uint8_t  breath_duty = 0;
    int8_t   breath_dir  = 1;
    uint32_t breath_tick = 0;

    while (1)
    {
        /* ------------------------------------------------
         * 1. LED0 呼吸灯（非阻塞，每 20ms 更新一次占空比）
         * ------------------------------------------------ */
        if (delay_get_tick() - breath_tick >= 20U)
        {
            breath_tick = delay_get_tick();
            if (breath_duty == 100) breath_dir = -1;
            if (breath_duty == 0)   breath_dir =  1;
            breath_duty = (uint8_t)(breath_duty + breath_dir);
            pwm_set_duty(breath_duty);
        }

        /* ------------------------------------------------
         * 2. TIM5 脉宽就绪 → 串口打印
         * ------------------------------------------------ */
        if (tim5_pulse_ready)
        {
            tim5_pulse_ready = 0;
            char buf[48];
            snprintf(buf, sizeof(buf), "KEY_UP held: %lu us\r\n",
                     (unsigned long)tim5_pulse_us);
            uart1_send_string(buf);
        }
    }
}
