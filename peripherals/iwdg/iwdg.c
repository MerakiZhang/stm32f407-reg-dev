#include "iwdg.h"

/*
 * 独立看门狗 (IWDG) 配置
 *
 * 时钟：LSI 内部 RC，标称 32 kHz（实际误差约 ±20%，不适合精确计时）。
 * IWDG 挂载在 APB1，但时钟源独立，不受系统时钟影响。
 *
 * 寄存器说明：
 *   KR  [写]  键值：0x5555 解锁 PR/RLR 写访问
 *                    0xAAAA 重载计数器（喂狗）
 *                    0xCCCC 启动 IWDG
 *   PR  [2:0] 预分频：0→/4, 1→/8, 2→/16, 3→/32, 4→/64, 5→/128, 6→/256
 *   RLR [11:0] 重载值：0 ~ 4095
 *   SR  [0] PVU  预分频更新中（写 PR 后置 1，更新完成自动清 0）
 *       [1] RVU  重载值更新中（写 RLR 后置 1，更新完成自动清 0）
 *
 * 超时计算（LSI = 32000 Hz）：
 *   T = (RLR + 1) × prescaler / 32000   [秒]
 *   prescaler = 4 << PR
 *
 *   例：timeout_ms=1000, PR=1(/8), RLR=3999
 *       T = 4000 × 8 / 32000 = 1.000 s
 */

void iwdg_init(uint32_t timeout_ms)
{
    /* 1. 根据 timeout_ms 自动选择最小预分频，使 RLR 不超过 4095 */
    /*    RLR + 1 = timeout_ms × 32 / prescaler（向上取整，保证不短于预期）*/
    uint8_t  pr  = 0;
    uint32_t div = 4U;
    uint32_t rlr;

    while (pr < 6U)
    {
        rlr = (timeout_ms * 32U + div - 1U) / div;
        if (rlr <= 4096U) break;
        pr++;
        div <<= 1;
    }

    rlr = (timeout_ms * 32U + div - 1U) / div;
    if (rlr > 4096U) rlr = 4096U;          /* 超出最大范围，截断为最大超时 */
    rlr = (rlr > 0U) ? (rlr - 1U) : 0U;   /* 换算为 RLR 寄存器值 */

    /* 2. 解锁 PR / RLR 写访问 */
    IWDG->KR = 0x5555U;

    /* 3. 等待上次写操作同步完成（SR.PVU | SR.RVU 清零） */
    while (IWDG->SR & 0x3U) {}

    /* 4. 写预分频和重载值 */
    IWDG->PR  = pr;
    IWDG->RLR = rlr;

    /* 5. 重载计数器，使新的 RLR 值立即生效 */
    IWDG->KR = 0xAAAAU;

    /* 6. 启动 IWDG（一旦启动，软件无法关闭） */
    IWDG->KR = 0xCCCCU;
}

void iwdg_feed(void)
{
    IWDG->KR = 0xAAAAU;   /* 重载计数器，防止超时复位 */
}
