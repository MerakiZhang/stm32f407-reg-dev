# 外部中断驱动说明

## 功能概述

将按键引脚配置为 EXTI 外部中断输入，按下按键时由硬件触发 ISR，在 ISR 中置位标志变量，主循环检测标志位后执行对应动作。相比轮询方式，主循环无需持续读取 IDR，CPU 占用更低。

---

## 硬件连接

| 按键    | GPIO | 外部连接 | 有效电平 | 内部电阻 |
|---------|------|----------|----------|----------|
| KEY_UP  | PA0  | 一端接 VCC 3.3V | 高（按下为高） | 内部下拉 |
| KEY0    | PE4  | 一端接 GND      | 低（按下为低） | 内部上拉 |
| KEY1    | PE3  | 一端接 GND      | 低（按下为低） | 内部上拉 |
| KEY2    | PE2  | 一端接 GND      | 低（按下为低） | 内部上拉 |

---

## EXTI 中断线映射

STM32F407 的每个 GPIO 引脚编号（Pin0~Pin15）对应同一条 EXTI 线，同一时刻每条线只能连接一个 GPIO 端口，通过 SYSCFG 寄存器选择。

| 按键    | GPIO | EXTI 线 | 触发方式 | IRQ Handler        | IRQn       |
|---------|------|---------|----------|--------------------|------------|
| KEY_UP  | PA0  | EXTI0   | 上升沿   | EXTI0_IRQHandler   | 6          |
| KEY0    | PE4  | EXTI4   | 下降沿   | EXTI4_IRQHandler   | 10         |
| KEY1    | PE3  | EXTI3   | 下降沿   | EXTI3_IRQHandler   | 9          |
| KEY2    | PE2  | EXTI2   | 下降沿   | EXTI2_IRQHandler   | 8          |

---

## 涉及寄存器

### RCC_AHB1ENR — GPIO 时钟使能

| 位   | 名称    | 说明 |
|------|---------|------|
| 位 0 | GPIOAEN | 置 1 使能 GPIOA 时钟 |
| 位 4 | GPIOEEN | 置 1 使能 GPIOE 时钟 |

### RCC_APB2ENR — SYSCFG 时钟使能

| 位    | 名称      | 说明 |
|-------|-----------|------|
| 位 14 | SYSCFGEN  | 置 1 使能 SYSCFG 时钟（配置 EXTICR 前必须先使能） |

### GPIOX_MODER — GPIO 模式寄存器

每个引脚占 2 位，配置为输入模式（`00`）：

```c
GPIOA->MODER &= ~(3U << (0 * 2));   /* PA0 输入 */
GPIOE->MODER &= ~((3U << (4*2)) | (3U << (3*2)) | (3U << (2*2)));  /* PE4/3/2 输入 */
```

### GPIOX_PUPDR — 上下拉寄存器

每个引脚占 2 位：`00` 无上下拉，`01` 上拉，`10` 下拉。

```c
GPIOA->PUPDR |= (2U << (0 * 2));    /* PA0 下拉 */
GPIOE->PUPDR |= (1U << (4*2)) | (1U << (3*2)) | (1U << (2*2));  /* PE4/3/2 上拉 */
```

### SYSCFG_EXTICR[0~3] — GPIO 端口到 EXTI 线的映射

每条 EXTI 线占 4 位，端口编码：PA=0, PB=1, PC=2, PD=3, PE=4, PF=5。
EXTICR[0] 管 EXTI0~3，EXTICR[1] 管 EXTI4~7。

```c
/* EXTICR[0]: EXTI0[3:0]=0x0(PA), EXTI2[11:8]=0x4(PE), EXTI3[15:12]=0x4(PE) */
SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0] & ~0xFF0FU) | (0x4U << 8) | (0x4U << 12);

/* EXTICR[1]: EXTI4[3:0]=0x4(PE) */
SYSCFG->EXTICR[1] = (SYSCFG->EXTICR[1] & ~0x000FU) | (0x4U << 0);
```

### EXTI_RTSR / EXTI_FTSR — 触发沿选择

| 寄存器 | 功能 |
|--------|------|
| RTSR   | 上升沿触发选择，对应位置 1 则该线在信号上升时触发中断 |
| FTSR   | 下降沿触发选择，对应位置 1 则该线在信号下降时触发中断 |

```c
EXTI->RTSR |= (1U << 0);                          /* EXTI0 上升沿（KEY_UP） */
EXTI->FTSR |= (1U << 2) | (1U << 3) | (1U << 4); /* EXTI2/3/4 下降沿（KEY0/1/2） */
```

### EXTI_IMR — 中断屏蔽寄存器

对应位置 1 则允许该 EXTI 线产生中断请求，置 0 则屏蔽。

```c
EXTI->IMR |= (1U << 0) | (1U << 2) | (1U << 3) | (1U << 4);
```

### EXTI_PR — Pending 寄存器

EXTI 线触发后该位置 1，表示有待处理的中断请求。在 ISR 中**写 1 清零**（写 0 无效）：

```c
EXTI->PR = (1U << n);   /* 正确：写 1 清除对应 pending 位 */
```

> 不能用 `EXTI->PR &= ~(1U << n)`，`&=` 操作会先读出寄存器当前值（可能有多个 pending 位为 1），再写入，会意外清除其他 pending 位。

---

## 配置步骤

1. **使能时钟**：AHB1（GPIOA、GPIOE）、APB2（SYSCFG）
2. **配置 GPIO**：输入模式，设置上下拉
3. **SYSCFG EXTICR**：将 PA0 → EXTI0，PE2/3/4 → EXTI2/3/4
4. **EXTI 触发沿**：RTSR 配置 EXTI0 上升沿，FTSR 配置 EXTI2/3/4 下降沿
5. **EXTI IMR**：解除 4 条线的中断屏蔽
6. **NVIC**：设置优先级，使能对应 IRQ

---

## 消抖方案

机械按键按下时触点抖动通常持续 5~20ms，会产生多次电平变化，导致 ISR 被连续触发多次。

在 ISR 内部不能调用 `delay_ms`（会永久阻塞），改用**时间戳消抖**：记录上次有效触发的毫秒时刻，当前触发时刻与之差值不足 20ms 则忽略。

```c
void EXTI0_IRQHandler(void)
{
    static uint32_t last_tick = 0;
    uint32_t now = delay_get_tick();   /* 获取当前 systick_ms 计数 */

    if ((now - last_tick) >= 20U)
    {
        last_tick = now;
        exti_key_up_flag = 1;          /* 置位标志，由主循环处理 */
    }

    EXTI->PR = (1U << 0);              /* 清除 pending 位 */
}
```

---

## 标志位使用方法

ISR 只负责置位，主循环读取标志后先清零再执行动作，确保每次按下只处理一次：

```c
if (exti_key0_flag)
{
    exti_key0_flag = 0;   /* 先清零，防止执行期间再次触发被漏掉 */
    led_toggle(LED0);
}
```

---

## NVIC 优先级说明

SysTick 中断默认优先级为 0（最高），`delay_get_tick` 依赖 SysTick 正常计数。EXTI 中断优先级设为 2，保证 SysTick 不被 EXTI ISR 阻塞，时间戳消抖可以正常工作。

```c
NVIC_SetPriority(EXTI0_IRQn, 2);
NVIC_EnableIRQ(EXTI0_IRQn);
```

---

## 函数说明

### exti_key_init

初始化函数。完成 GPIO 输入配置、SYSCFG EXTICR 映射、EXTI 触发沿及屏蔽配置、NVIC 优先级及使能。需在 `delay_init` 之后调用（消抖依赖 SysTick 计数）。

### EXTI0_IRQHandler

KEY_UP (PA0) 的中断服务函数。时间戳消抖后置位 `exti_key_up_flag`，并清除 EXTI0 pending 位。

### EXTI2_IRQHandler

KEY2 (PE2) 的中断服务函数。时间戳消抖后置位 `exti_key2_flag`，并清除 EXTI2 pending 位。

### EXTI3_IRQHandler

KEY1 (PE3) 的中断服务函数。时间戳消抖后置位 `exti_key1_flag`，并清除 EXTI3 pending 位。

### EXTI4_IRQHandler

KEY0 (PE4) 的中断服务函数。时间戳消抖后置位 `exti_key0_flag`，并清除 EXTI4 pending 位。
