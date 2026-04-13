# rng — 硬件随机数发生器（RNG）

## 概述

STM32F407 内置硬件随机数发生器（True Random Number Generator，TRNG），基于模拟噪声源产生真随机熵，每 40 个 RNG 时钟周期输出一个 32 位随机数。本模块封装了 RNG 的初始化、原始数据读取以及范围限制采样。

---

## 硬件资源

| 资源 | 说明 |
|------|------|
| 基地址 | `0x50060800`（AHB2 外设） |
| 时钟源 | PLLQ → 48MHz（必须在 40~48MHz 之间） |
| 时钟使能 | `RCC->AHB2ENR` bit6（`RNGEN`） |
| 中断 | `RNG_IRQn`（本模块不使用，采用轮询） |

> **本项目 PLL 参数**：VCO = 336MHz，PLLQ = 7，RNG 时钟 = 336/7 = **48MHz**，满足要求。

---

## 寄存器说明

### CR — 控制寄存器

| 位 | 名称 | 说明 |
|----|------|------|
| 2 | RNGEN | RNG 使能；置 1 后开始连续产生随机数 |
| 3 | IE    | 中断使能（本模块不使用） |

### SR — 状态寄存器

| 位 | 名称 | 说明 |
|----|------|------|
| 0 | DRDY | 数据就绪；读取 DR 后硬件自动清零 |
| 1 | CECS | 时钟错误当前状态（实时，只读） |
| 2 | SECS | 种子错误当前状态（实时，只读） |
| 5 | CEIS | 时钟错误中断标志（需软件写 0 清除） |
| 6 | SEIS | 种子错误中断标志（需软件写 0 清除） |

### DR — 数据寄存器

| 位 | 说明 |
|----|------|
| 31:0 | 32 位随机数；须在 `DRDY=1` 后读取，读取后 DRDY 自动清零 |

---

## 配置步骤

1. `RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN` — 使能 AHB2 RNG 时钟
2. `RNG->CR |= RNG_CR_RNGEN` — 使能 RNG，开始产生随机数
3. 轮询 `RNG->SR & RNG_SR_DRDY` — 等待数据就绪
4. 读取 `RNG->DR` — 获取 32 位随机值

---

## 错误恢复

RM0090 建议：检测到 CEIS 或 SEIS 时，清除标志、关闭再重新使能 RNG，等待下一个有效数据。

```c
if (RNG->SR & (RNG_SR_CEIS | RNG_SR_SEIS)) {
    RNG->SR &= ~(RNG_SR_CEIS | RNG_SR_SEIS);
    RNG->CR &= ~RNG_CR_RNGEN;
    RNG->CR |=  RNG_CR_RNGEN;
    while (!(RNG->SR & RNG_SR_DRDY)) {}
}
```

---

## 范围采样算法

`rng_get_range(max)` 使用**拒绝采样**（rejection sampling）消除模偏差：

- 若直接用 `val % 101` 映射 `[0, 100]`，因 2³² 不能被 101 整除，低端值出现概率略高。
- 拒绝采样：丢弃使末尾区间不完整的值，保证输出均匀分布。

```
range = max + 1 = 101
limit = 0xFFFFFFFF - (0xFFFFFFFF % 101) = 0xFFFFFF99
仅接受 val ∈ [0, 0xFFFFFF99]，约占全范围的 99.6%
平均采样次数 ≈ 1.004 次
```

---

## API

```c
void rng_init(void);
```
使能 RNG 时钟并启动随机数生成器。须在 `sys_clock_init()` 之后调用（依赖 PLLQ 48MHz）。

```c
uint32_t rng_get(void);
```
阻塞等待 DRDY，返回 32 位硬件随机数。含错误检测与自动恢复。

```c
uint8_t rng_get_range(uint8_t max);
```
返回 `[0, max]` 范围内均匀分布的随机整数，使用拒绝采样消除模偏差。

---

## 使用示例

```c
#include "rng.h"
#include "uart.h"
#include <stdio.h>

rng_init();

/* 产生 0~100 随机数并通过串口发送 */
uint8_t n = rng_get_range(100);
char buf[32];
snprintf(buf, sizeof(buf), "RNG: %u\r\n", (unsigned)n);
uart1_send_string(buf);
```

---

## 注意事项

1. **时钟频率要求**：RNG 时钟必须在 40~48MHz 之间，否则触发时钟错误（CEIS）。本项目 PLLQ = 48MHz，满足要求。
2. **启动延迟**：使能 RNGEN 后需约 40 个 RNG 时钟周期（约 1µs）才产生第一个有效数据，`rng_get()` 内已通过轮询 DRDY 处理。
3. **阻塞特性**：`rng_get()` 在等待 DRDY 期间阻塞，正常情况下等待时间极短（< 1µs），不影响主循环实时性。
