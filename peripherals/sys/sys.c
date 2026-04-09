#include "sys.h"

/*
 * 系统时钟配置：HSE 8MHz → PLL → SYSCLK 168MHz
 *
 * PLL 参数：
 *   PLLM = 8   → VCO 输入 = 8MHz / 8   = 1MHz
 *   PLLN = 336 → VCO 输出 = 1MHz * 336 = 336MHz
 *   PLLP = 2   → SYSCLK  = 336MHz / 2  = 168MHz
 *   PLLQ = 7   → USB     = 336MHz / 7  = 48MHz
 *
 * 总线分频：
 *   AHB  = SYSCLK / 1 = 168MHz
 *   APB1 = SYSCLK / 4 = 42MHz  (最大 42MHz)
 *   APB2 = SYSCLK / 2 = 84MHz  (最大 84MHz)
 */

void sys_clock_init(void)
{
    /* 1. 使能 HSE */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    /* 2. 使能 PWR 时钟，设置电压调节器为 Scale 1 模式（支持 168MHz） */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;

    /* 3. 配置 AHB / APB1 / APB2 分频 */
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;    /* AHB = SYSCLK / 1  */
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;   /* APB1 = AHB / 4    */
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;   /* APB2 = AHB / 2    */

    /* 4. 配置 PLL：时钟源 HSE，PLLM=25，PLLN=336，PLLP=2，PLLQ=7 */
    RCC->PLLCFGR = (8U  <<  0) |         /* PLLM = 8 */
                   (336U <<  6) |         /* PLLN = 336 */
                   (0U  << 16) |         /* PLLP = 2（00 表示 2） */
                   RCC_PLLCFGR_PLLSRC_HSE |
                   (7U  << 24);          /* PLLQ = 7 */

    /* 5. 使能 PLL，等待就绪 */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    /* 6. 配置 Flash：使能预取，设置 5 个等待周期（168MHz @ 3.3V） */
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_5WS;

    /* 7. 切换系统时钟到 PLL */
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    /* 8. 更新 SystemCoreClock 全局变量 */
    SystemCoreClock = 168000000;
}
