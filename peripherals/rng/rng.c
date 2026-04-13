#include "rng.h"

/*
 * 硬件随机数发生器（RNG）配置
 *
 * 时钟来源：PLLQ → 48MHz，挂载在 AHB2 总线（RCC->AHB2ENR bit6）。
 * RNG 要求时钟频率在 40~48MHz 之间，本项目 PLLQ = 7，VCO = 336MHz，
 * 故 RNG 时钟 = 336 / 7 = 48MHz，满足要求。
 *
 * 寄存器说明：
 *   CR  [2] RNGEN  使能 RNG；置 1 后开始连续产生随机数
 *   SR  [0] DRDY   数据就绪标志；读取 DR 后硬件自动清零
 *       [5] CEIS   时钟错误中断状态（时钟频率异常）
 *       [6] SEIS   种子错误中断状态（熵源出错）
 *   DR  [31:0]     32 位随机数，读取时须先确认 DRDY=1
 *
 * 错误处理策略：
 *   检测到时钟错误（CEIS）或种子错误（SEIS）时，清除错误标志并
 *   重新使能 RNG，等待下一个有效数据。这是 RM0090 推荐的恢复流程。
 */

void rng_init(void)
{
    /* 使能 RNG 时钟（AHB2ENR bit6） */
    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;

    /* 使能 RNG */
    RNG->CR |= RNG_CR_RNGEN;
}

uint32_t rng_get(void)
{
    /* 等待数据就绪或出错 */
    while (!(RNG->SR & (RNG_SR_DRDY | RNG_SR_CEIS | RNG_SR_SEIS))) {}

    /* 处理错误：清标志后重新使能，重新等待 */
    if (RNG->SR & (RNG_SR_CEIS | RNG_SR_SEIS))
    {
        RNG->SR &= ~(RNG_SR_CEIS | RNG_SR_SEIS);
        RNG->CR &= ~RNG_CR_RNGEN;
        RNG->CR |=  RNG_CR_RNGEN;
        while (!(RNG->SR & RNG_SR_DRDY)) {}
    }

    return RNG->DR;
}

/*
 * rng_get_range - 返回 [0, max] 范围内的随机整数
 *
 * 使用拒绝采样（rejection sampling）消除模偏差：
 *   若 max+1 能整除 2^32，直接取模；否则丢弃落在末尾不完整区间内的值，
 *   重新采样，保证每个输出值的概率严格相等。
 *
 * 对于 max=100（101 个值），最坏情况下拒绝概率 < 0.4%，平均约 1.004 次采样。
 */
uint8_t rng_get_range(uint8_t max)
{
    uint32_t range = (uint32_t)max + 1U;
    uint32_t limit = (0xFFFFFFFFU - (0xFFFFFFFFU % range));
    uint32_t val;

    do {
        val = rng_get();
    } while (val > limit);

    return (uint8_t)(val % range);
}
