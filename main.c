#include "stm32f4xx.h"
#include "sys.h"
#include "delay.h"
#include "led.h"
#include "beep.h"
#include "key.h"
#include "uart.h"

/*
 * 按键 → 发送字符（通知 PC）：
 *   KEY_UP → 'a'
 *   KEY0   → 'b'
 *   KEY1   → 'c'
 *   KEY2   → 'd'
 *
 * PC 发送字符 → 控制外设：
 *   'a' → LED0  切换
 *   'b' → LED1  切换
 *   'c' → BEEP  切换
 *   'd' → LED0 / LED1 / BEEP 全部关闭
 */

int main(void)
{
    sys_clock_init();
    delay_init();
    led_init();
    beep_init();
    key_init();
    uart1_init(115200);

    uart1_send_string("STM32 ready. Press KEY to send / send a-d to control.\r\n");

    while (1)
    {
        /* ------------------------------------------------
         * 1. 按键扫描 → 发送字符到 PC
         *    key_scan 内部消抖 10ms（阻塞），期间 UART RX
         *    中断照常工作，数据存入环形缓冲区不丢失。
         * ------------------------------------------------ */
        uint8_t key = key_scan(0);
        switch (key)
        {
            case KEY_UP_VAL:
                uart1_send_string("a\r\n");
                break;
            case KEY0_VAL:
                uart1_send_string("b\r\n");
                break;
            case KEY1_VAL:
                uart1_send_string("c\r\n");
                break;
            case KEY2_VAL:
                uart1_send_string("d\r\n");
                break;
            default:
                break;
        }

        /* ------------------------------------------------
         * 2. 接收 PC 发来的字符 → 控制 LED / BEEP
         *    uart1_recv_byte 非阻塞，有数据立即处理。
         * ------------------------------------------------ */
        uint8_t ch;
        while (uart1_recv_byte(&ch))
        {
            switch (ch)
            {
                case 'a':
                    led_toggle(LED0);
                    uart1_send_string("LED0 toggled\r\n");
                    break;
                case 'b':
                    led_toggle(LED1);
                    uart1_send_string("LED1 toggled\r\n");
                    break;
                case 'c':
                    beep_toggle();
                    uart1_send_string("BEEP toggled\r\n");
                    break;
                case 'd':
                    led_off(LED0);
                    led_off(LED1);
                    beep_off();
                    uart1_send_string("All off\r\n");
                    break;
                default:
                    break;
            }
        }
    }
}
