#include "timer.h"

/* ============================================================
 * TIM3 通用定时器配置：产生周期性更新中断
 *
 * TIM3 挂载在 APB1 总线（42MHz），因 APB1 分频系数 != 1，
 * 定时器时钟 = 2 × APB1 = 84MHz。
 *
 * 中断周期计算公式：
 *   T = (PSC + 1) × (ARR + 1) / 84MHz
 *
 * 示例（500ms）：
 *   PSC = 8399  →  计数频率 = 84MHz / 8400 = 10kHz（0.1ms / tick）
 *   ARR = 4999  →  5000 × 0.1ms = 500ms
 */

volatile uint8_t timer3_flag = 0;

void timer3_init(uint16_t psc, uint16_t arr)
{
    /* 1. 使能 TIM3 时钟（APB1，bit1） */
    RCC->APB1ENR |= (1U << 1);   /* TIM3EN */

    /* 2. 设置预分频和自动重载值 */
    TIM3->PSC = psc;
    TIM3->ARR = arr;

    /* 3. 产生更新事件，将 PSC/ARR 立即装入影子寄存器 */
    TIM3->EGR = (1U << 0);       /* UG：软件产生更新事件 */

    /* 4. 清除 EGR 写 UG 产生的 UIF，避免立即触发中断 */
    TIM3->SR &= ~(1U << 0);      /* 清 UIF */

    /* 5. 使能更新中断（DIER bit0 UIE） */
    TIM3->DIER |= (1U << 0);

    /* 6. 配置 NVIC：优先级 1（低于 SysTick=0，高于 EXTI=2） */
    NVIC_SetPriority(TIM3_IRQn, 1);
    NVIC_EnableIRQ(TIM3_IRQn);

    /* 7. 启动计数器（CR1 bit0 CEN） */
    TIM3->CR1 |= (1U << 0);
}

void TIM3_IRQHandler(void)
{
    if (TIM3->SR & (1U << 0))    /* 检查更新中断标志 UIF */
    {
        TIM3->SR &= ~(1U << 0);  /* 清除 UIF */
        timer3_flag = 1;
    }
}

/* ============================================================
 * TIM5 输入捕获：测量 PA0（KEY_UP）高电平脉宽
 *
 * 硬件映射：PA0 → TIM5_CH1，AF2
 * 时钟：APB1 定时器时钟 = 84 MHz
 * 预分频：PSC = 83 → 计数频率 1 MHz（1 µs / tick）
 * 自动重载：ARR = 0xFFFFFFFF（32 位，满量程约 4295 s）
 *
 * 测量原理：
 *   上升沿捕获 → 记录 CCR1 起始值，切换为下降沿捕获
 *   下降沿捕获 → 脉宽 = CCR1 − 起始值（32 位减法自动处理溢出）
 *
 * 注意：tim5ic_init() 会将 PA0 重新配置为 AF2，
 *       须在 key_init() 之后调用（下拉由 key_init 配置，保持不变）。
 * ============================================================ */

volatile uint32_t tim5_pulse_us    = 0;
volatile uint8_t  tim5_pulse_ready = 0;

static uint32_t tim5_rise_val = 0;
static uint8_t  tim5_edge     = 0;   /* 0 = 等待上升沿，1 = 等待下降沿 */

void tim5ic_init(void)
{
    /* 1. 使能 GPIOA 时钟（key_init 已使能，重复无害） */
    RCC->AHB1ENR |= (1U << 0);

    /* 2. PA0 → 复用功能模式（MODER = 10） */
    GPIOA->MODER &= ~(3U << (0 * 2));
    GPIOA->MODER |=  (2U << (0 * 2));

    /* 3. PA0 → AF2（TIM5_CH1） */
    GPIOA->AFR[0] &= ~(0xFU << (0 * 4));
    GPIOA->AFR[0] |=  (2U   << (0 * 4));

    /* 下拉保留 key_init() 设定，按键未按时 PA0 保持低电平，不误触发 */

    /* 4. 使能 TIM5 时钟（APB1 bit3） */
    RCC->APB1ENR |= (1U << 3);

    /* 5. 预分频：84 MHz / 84 = 1 MHz */
    TIM5->PSC = 83;

    /* 6. 自动重载：32 位最大值 */
    TIM5->ARR = 0xFFFFFFFFU;

    /* 7. CH1 输入捕获：映射到 TI1（CC1S = 01），无滤波，无分频 */
    TIM5->CCMR1 &= ~(0xFFU);
    TIM5->CCMR1 |=  (1U << 0);   /* CC1S[1:0] = 01 */

    /* 8. 捕获极性：上升沿（CC1P = 0，CC1NP = 0） */
    TIM5->CCER &= ~((1U << 1) | (1U << 3));

    /* 9. 使能通道 1 捕获（CC1E = 1） */
    TIM5->CCER |= (1U << 0);

    /* 10. 使能 CC1 捕获中断（CC1IE = 1） */
    TIM5->DIER |= (1U << 1);

    /* 11. 产生更新事件装载影子寄存器，然后清除所有标志 */
    TIM5->EGR = (1U << 0);
    TIM5->SR  = 0;

    /* 12. NVIC：优先级 2（与 EXTI 同级，低于 TIM3） */
    NVIC_SetPriority(TIM5_IRQn, 2);
    NVIC_EnableIRQ(TIM5_IRQn);

    /* 13. 启动计数器 */
    TIM5->CR1 |= (1U << 0);

    tim5_edge = 0;
}

void TIM5_IRQHandler(void)
{
    if (TIM5->SR & (1U << 1))    /* CC1IF */
    {
        TIM5->SR &= ~(1U << 1);  /* 清除 CC1IF */

        if (tim5_edge == 0)
        {
            /* 上升沿：记录起始值，切换为下降沿捕获 */
            tim5_rise_val  = TIM5->CCR1;
            TIM5->CCER    |=  (1U << 1);   /* CC1P = 1 → 下降沿 */
            tim5_edge      = 1;
        }
        else
        {
            /* 下降沿：计算脉宽，32 位减法自动处理计数器溢出 */
            tim5_pulse_us   = TIM5->CCR1 - tim5_rise_val;
            tim5_pulse_ready = 1;
            TIM5->CCER     &= ~(1U << 1);  /* CC1P = 0 → 切回上升沿 */
            tim5_edge       = 0;
        }
    }
}

/* ============================================================
 * TIM2 电容按键检测：PA5（TIM2_CH1，AF1）
 *
 * 检测原理（充放电时间测量法）：
 *   电容按键 + 外部上拉电阻（3.3V）构成 RC 充电回路。
 *   每次检测执行以下步骤：
 *     1. PA5 → 输出低电平，持续 TIM2_DISCHARGE_US，将电容完全放电
 *     2. PA5 → AF1（TIM2_CH1），外部上拉开始对电容充电
 *     3. TIM2 输入捕获 PA5 上升沿，CCR1 = 充电时间（µs）
 *   手指触摸时并联体表电容，等效电容增大，充电时间变长：
 *     充电时间 > 基线（无触摸均值）+ 阈值  →  判定为触摸
 *
 * 初始化时自动校准基线（须在无触摸状态下调用 tim2cap_init）。
 *
 * 参数调整：
 *   TIM2_DISCHARGE_US  — 放电时长，应确保完全放电（与 RC 有关）
 *   TIM2_TIMEOUT_US    — 等待上升沿超时，取决于最大 RC 时间常数
 *   TIM2_CAP_THRESHOLD — 触摸判定阈值，根据实际硬件调试确定
 *
 * 硬件：PA5，外部上拉至 3.3V（PUPDR 配置为浮空）
 *       TIM2 时钟 84MHz，PSC=83 → 1MHz（1 µs/tick），轮询无中断
 * ============================================================ */

#define TIM2_DISCHARGE_US    50U    /* 放电时长（µs） */
#define TIM2_TIMEOUT_US     500U    /* 等待上升沿超时（µs） */
#define TIM2_CAP_THRESHOLD   20U    /* 触摸判定阈值（µs） */
#define TIM2_CAL_SAMPLES     16U    /* 基线校准采样次数 */

static uint32_t  tim2_baseline  = 0;
volatile uint8_t tim2_cap_state = 0;   /* 0=未触摸，1=已触摸 */

/* 单次充放电测量，返回充电时间（µs），超时返回 TIM2_TIMEOUT_US */
static uint32_t tim2cap_measure_once(void)
{
    /* 1. PA5 → 推挽输出，拉低放电 */
    GPIOA->MODER &= ~(3U << (5 * 2));
    GPIOA->MODER |=  (1U << (5 * 2));    /* 输出模式 */
    GPIOA->BSRR   =  (1U << (5 + 16));   /* PA5 = 0，放电 */

    /* 2. 等待放电完成（利用 TIM2->CNT 计 µs） */
    uint32_t t = TIM2->CNT;
    while ((TIM2->CNT - t) < TIM2_DISCHARGE_US)
        ;

    /* 3. 复位计数器，清除捕获标志 */
    TIM2->CNT = 0;
    TIM2->SR  = 0;

    /* 4. PA5 → AF1（TIM2_CH1），上拉电阻开始对电容充电 */
    GPIOA->MODER &= ~(3U << (5 * 2));
    GPIOA->MODER |=  (2U << (5 * 2));    /* 复用功能，AFR 已在 init 设定 */

    /* 5. 等待 TIM2 捕获上升沿（CC1IF），含超时保护 */
    while (!(TIM2->SR & (1U << 1)))
    {
        if (TIM2->CNT >= TIM2_TIMEOUT_US)
            return TIM2_TIMEOUT_US;       /* 超时（上拉未接或阻值过大） */
    }

    return TIM2->CCR1;                    /* 充电时间（µs） */
}

void tim2cap_init(void)
{
    /* 1. 使能 GPIOA 和 TIM2 时钟 */
    RCC->AHB1ENR |= (1U << 0);    /* GPIOAEN */
    RCC->APB1ENR |= (1U << 0);    /* TIM2EN  */

    /* 2. PA5 → AF1（TIM2_CH1）：初始设为 AF 模式，measure_once 内动态切换 */
    GPIOA->MODER  &= ~(3U << (5 * 2));
    GPIOA->MODER  |=  (2U << (5 * 2));   /* 10: AF 模式 */
    GPIOA->AFR[0] &= ~(0xFU << (5 * 4));
    GPIOA->AFR[0] |=  (1U   << (5 * 4)); /* AF1 = TIM2_CH1 */

    /* 3. 外部上拉已提供，PUPDR 设为浮空（00） */
    GPIOA->PUPDR &= ~(3U << (5 * 2));

    /* 4. 预分频：84 MHz / 84 = 1 MHz（1 µs/tick） */
    TIM2->PSC = 83;

    /* 5. 自动重载：32 位最大值 */
    TIM2->ARR = 0xFFFFFFFFU;

    /* 6. CH1 输入捕获：CC1S=01（IC1→TI1），上升沿，无滤波，无分频 */
    TIM2->CCMR1 &= ~(0xFFU);
    TIM2->CCMR1 |=  (1U << 0);                   /* CC1S[1:0] = 01 */
    TIM2->CCER  &= ~((1U << 1) | (1U << 3));     /* CC1P=0, CC1NP=0 → 上升沿 */
    TIM2->CCER  |=  (1U << 0);                   /* CC1E = 1，使能捕获 */

    /* 7. 轮询方式，不使用捕获中断 */
    TIM2->DIER = 0;

    /* 8. 产生更新事件装载影子寄存器，清除标志 */
    TIM2->EGR = (1U << 0);
    TIM2->SR  = 0;

    /* 9. 启动计数器（measure_once 使用 CNT 计时，须先启动） */
    TIM2->CR1 |= (1U << 0);

    /* 10. 基线校准：连续采样 TIM2_CAL_SAMPLES 次，取平均 */
    uint32_t sum = 0;
    for (uint8_t i = 0; i < TIM2_CAL_SAMPLES; i++)
    {
        sum += tim2cap_measure_once();
        /* 两次采样间隔约 200µs，避免连续放电干扰 */
        uint32_t tw = TIM2->CNT;
        while ((TIM2->CNT - tw) < 200U)
            ;
    }
    tim2_baseline  = sum / TIM2_CAL_SAMPLES;
    tim2_cap_state = 0;
}

uint8_t tim2cap_scan(void)
{
    uint32_t charge_time = tim2cap_measure_once();
    tim2_cap_state = (charge_time > tim2_baseline + TIM2_CAP_THRESHOLD) ? 1U : 0U;
    return tim2_cap_state;
}

/* ============================================================
 * TIM14 硬件 PWM — 控制 LED0（PF9）亮度
 *
 * 引脚复用：PF9 → AF9 → TIM14_CH1
 * TIM14 时钟：84MHz（APB1 × 2）
 * PWM 参数：PSC=83，ARR=999 → f_PWM = 84MHz / 84 / 1000 = 1kHz
 *
 * LED0 为低电平点亮（Active-Low）。
 * PWM 模式 1：计数值 < CCR1 时输出高电平，≥ CCR1 时输出低电平。
 * 低电平（LED 亮）占比 = (ARR + 1 - CCR1) / (ARR + 1)
 * 因此：CCR1 = (100 - duty) × 10
 *   duty=0   → CCR1=1000 → 始终高电平 → LED 灭
 *   duty=50  → CCR1=500  → 50% 低电平 → LED 半亮
 *   duty=100 → CCR1=0    → 始终低电平 → LED 全亮
 * ============================================================ */

void pwm_init(void)
{
    /* --------------------------------------------------------
     * 1. GPIO：PF9 配置为复用功能模式，AF9 = TIM14_CH1
     * -------------------------------------------------------- */
    RCC->AHB1ENR |= (1U << 5);              /* 使能 GPIOF 时钟 */

    GPIOF->MODER  &= ~(3U << 18);           /* 清除 PF9 模式位 */
    GPIOF->MODER  |=  (2U << 18);           /* PF9 = 复用功能（AF） */
    GPIOF->OSPEEDR |= (2U << 18);           /* 高速 */
    GPIOF->OTYPER  &= ~(1U << 9);           /* 推挽输出 */
    GPIOF->PUPDR   &= ~(3U << 18);          /* 无上下拉 */

    /* AFRH（AFR[1]）PF9 对应 bits[7:4]，AF9 = 0b1001 */
    GPIOF->AFR[1] &= ~(0xFU << 4);
    GPIOF->AFR[1] |=  (9U   << 4);          /* AF9 = TIM14_CH1 */

    /* --------------------------------------------------------
     * 2. TIM14 PWM 配置
     * -------------------------------------------------------- */
    RCC->APB1ENR |= (1U << 8);              /* 使能 TIM14 时钟（APB1ENR bit8） */

    TIM14->PSC = 83;                         /* 84MHz / 84 = 1MHz 计数频率 */
    TIM14->ARR = 999;                        /* 1MHz / 1000 = 1kHz PWM 频率 */

    /*
     * CCMR1：
     *   OC1M[2:0] = 110（bits[6:4]）→ PWM 模式 1
     *   OC1PE     = 1  （bit 3）    → 输出比较预装载使能
     */
    TIM14->CCMR1 = (6U << 4) | (1U << 3);

    /* CCER：CC1E=1（输出使能），CC1P=0（高电平有效极性，不取反） */
    TIM14->CCER |= (1U << 0);

    /* CR1：ARPE=1（ARR 预装载使能） */
    TIM14->CR1 |= (1U << 7);

    /* 产生更新事件，将 PSC/ARR 立即装入影子寄存器 */
    TIM14->EGR = (1U << 0);                 /* UG */

    /* 初始占空比 0：LED0 熄灭 */
    TIM14->CCR1 = 1000;

    /* 启动计数器 */
    TIM14->CR1 |= (1U << 0);               /* CEN = 1 */
}

/*
 * pwm_set_duty — 设置 LED0 亮度
 *
 * duty: 0（完全熄灭）~ 100（全亮）
 */
void pwm_set_duty(uint8_t duty)
{
    if (duty > 100) duty = 100;
    TIM14->CCR1 = (uint16_t)((100U - duty) * 10U);
}
