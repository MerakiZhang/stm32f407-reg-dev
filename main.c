#include "stm32f4xx.h"
#include "sys.h"
#include "delay.h"
#include "led.h"
#include "beep.h"
#include "key.h"
#include "uart.h"
#include "timer.h"
#include "rng.h"
#include <stdio.h>
#include <string.h>

/*
 * 功能：
 *   1. LED0（PF9）呼吸灯 —— TIM14 PWM，占空比 0→100→0 循环渐变，非阻塞
 *      单程 100 步 × 20ms = 2s，完整一个呼吸周期约 4s
 *
 *   2. KEY_UP（PA0）高电平脉宽检测 —— TIM5 输入捕获
 *      上升沿记录起始，下降沿计算差值，精度 1µs，量程约 4295s
 *      结果通过 USART1（PA9/PA10，115200 bps）发送到 PC
 *
 *   3. 电容按键（PA5）按下检测 —— TIM2 充放电时间测量
 *      按下时 LED1（PF10）点亮，松开时熄灭
 *      松开后将按压持续时间（ms）通过 USART1 发送到 PC
 *
 *   4. KEY0（PE4）按下 —— 硬件 RNG 产生 0~100 随机数，通过串口发送到 PC
 *
 *   5. USART1 串口指令 —— 控制 LED1 / BEEP
 *      格式：led1 on / led1 off / beep on / beep off（行尾 \r\n 或 \n）
 *
 * 初始化顺序：
 *   key_init()      先配置 PA0 内部下拉（GPIO 输入）
 *   tim5ic_init()   后将 PA0 切换为 AF2（TIM5_CH1），下拉保留
 *   tim2cap_init()  PA5 独立，无顺序依赖；内部自动校准基线（须无触摸）
 */

int main(void)
{
    sys_clock_init();
    delay_init();
    led_init();
    beep_init();
    key_init();        /* 配置 PA0 内部下拉，供 tim5ic_init() 依赖 */
    pwm_init();        /* TIM14 PWM → LED0 呼吸灯 */
    uart1_init(115200);
    tim5ic_init();     /* TIM5 输入捕获 → KEY_UP 脉宽，须在 key_init() 后调用 */
    tim2cap_init();    /* TIM2 电容按键检测，初始化时自动校准基线（须无触摸） */
    rng_init();        /* 硬件 RNG，使用 PLLQ 48MHz 时钟 */

    uart1_send_string("=== STM32F407 Ready ===\r\n");
    uart1_send_string("LED0: breathing | KEY_UP: pulse width | CAP_KEY: LED1+dur | KEY0: random\r\n");
    uart1_send_string("CMD: led1 on/off | beep on/off\r\n");

    /* ---- 呼吸灯状态 ---- */
    uint8_t  breath_duty = 0;
    int8_t   breath_dir  = 1;
    uint32_t breath_tick = 0;

    /* ---- 电容按键状态 ---- */
    uint8_t  prev_cap_state  = 0;
    uint32_t cap_press_tick  = 0;   /* 按下时刻（ms） */
    uint32_t cap_scan_tick   = 0;   /* 上次扫描时刻（ms） */

    /* ---- KEY0 扫描时钟 ---- */
    uint32_t key_scan_tick   = 0;

    /* ---- 串口命令接收缓冲区 ---- */
    char     cmd_buf[32];
    uint8_t  cmd_len = 0;

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

        /* ------------------------------------------------
         * 3. 电容按键检测（每 5ms 扫描一次）
         *    按下 → LED1 点亮；松开 → LED1 熄灭，发送按压时长
         * ------------------------------------------------ */
        if (delay_get_tick() - cap_scan_tick >= 5U)
        {
            cap_scan_tick = delay_get_tick();
            uint8_t cap = tim2cap_scan();

            if (cap && !prev_cap_state)
            {
                /* 下降沿：检测到按下 */
                led_on(LED1);
                cap_press_tick = delay_get_tick();
            }
            else if (!cap && prev_cap_state)
            {
                /* 上升沿：检测到松开 */
                led_off(LED1);
                uint32_t dur = delay_get_tick() - cap_press_tick;
                char buf[48];
                snprintf(buf, sizeof(buf), "CAP_KEY held: %lu ms\r\n",
                         (unsigned long)dur);
                uart1_send_string(buf);
            }

            prev_cap_state = cap;
        }

        /* ------------------------------------------------
         * 4. KEY0 按下 → 硬件 RNG 产生 0~100 随机数，串口发送
         *    每 20ms 扫描一次（key_scan 内含 10ms 消抖延时）
         * ------------------------------------------------ */
        if (delay_get_tick() - key_scan_tick >= 20U)
        {
            key_scan_tick = delay_get_tick();
            if (key_scan(0) == KEY0_VAL)
            {
                uint8_t rnd = rng_get_range(100);
                char buf[32];
                snprintf(buf, sizeof(buf), "RNG: %u\r\n", (unsigned)rnd);
                uart1_send_string(buf);
            }
        }

        /* ------------------------------------------------
         * 5. 串口命令接收：led1 on/off | beep on/off
         *    以 '\n' 为行终止符，忽略 '\r'
         * ------------------------------------------------ */
        {
            uint8_t byte;
            while (uart1_recv_byte(&byte))
            {
                if (byte == '\r') continue;
                if (byte == '\n' || cmd_len >= (uint8_t)(sizeof(cmd_buf) - 1))
                {
                    cmd_buf[cmd_len] = '\0';
                    cmd_len = 0;

                    if      (strcmp(cmd_buf, "led1 on")  == 0) { led_on(LED1);  uart1_send_string("LED1 ON\r\n");  }
                    else if (strcmp(cmd_buf, "led1 off") == 0) { led_off(LED1); uart1_send_string("LED1 OFF\r\n"); }
                    else if (strcmp(cmd_buf, "beep on")  == 0) { beep_on();     uart1_send_string("BEEP ON\r\n");  }
                    else if (strcmp(cmd_buf, "beep off") == 0) { beep_off();    uart1_send_string("BEEP OFF\r\n"); }
                    else if (cmd_buf[0] != '\0')               { uart1_send_string("ERR: unknown cmd\r\n"); }
                }
                else
                {
                    cmd_buf[cmd_len++] = (char)byte;
                }
            }
        }
    }
}
