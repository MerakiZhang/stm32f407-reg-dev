#include "stm32f407xx.h"
#include "led.h"

static void delay(volatile uint32_t count)
{
    while (count--)
    {
        __NOP();
    }
}

int main(void)
{
    led_init();

    while (1)
    {
        led_on(LED0);
        delay(2000000);
        led_off(LED0);
        led_on(LED1);
        delay(2000000);
        led_off(LED1);
    }
}
