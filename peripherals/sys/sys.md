# 系统时钟配置说明

## 时钟目标

使用外部高速时钟 HSE（8MHz），经 PLL 倍频后将系统时钟配置为 168MHz。

## PLL 参数

| 参数  | 值  | 说明 |
|-------|-----|------|
| PLLM  | 8   | HSE 8MHz ÷ 8 = 1MHz（VCO 输入） |
| PLLN  | 336 | 1MHz × 336 = 336MHz（VCO 输出） |
| PLLP  | 2   | 336MHz ÷ 2 = 168MHz（SYSCLK） |
| PLLQ  | 7   | 336MHz ÷ 7 = 48MHz（USB） |

## 总线频率

| 总线 | 分频 | 频率 |
|------|------|------|
| AHB  | ÷1   | 168MHz |
| APB1 | ÷4   | 42MHz（最大 42MHz） |
| APB2 | ÷2   | 84MHz（最大 84MHz） |

## 配置步骤

**第一步：使能 HSE，等待就绪**

操作 RCC_CR 寄存器置位 HSEON，轮询 HSERDY 位等待 HSE 稳定。

**第二步：使能 PWR，设置电压调节器为 Scale 1 模式**

使能 RCC_APB1ENR 的 PWREN 位，将 PWR_CR 的 VOS 位设置为 Scale 1，支持 168MHz 运行频率。

**第三步：配置总线分频**

操作 RCC_CFGR 寄存器，设置 AHB 不分频，APB1 4 分频，APB2 2 分频。

**第四步：配置 PLL 参数**

操作 RCC_PLLCFGR 寄存器，设置 PLLM、PLLN、PLLP、PLLQ 参数，选择 HSE 作为 PLL 时钟源。

**第五步：使能 PLL，等待就绪**

操作 RCC_CR 寄存器置位 PLLON，轮询 PLLRDY 位等待 PLL 锁定。

**第六步：配置 Flash 等待周期**

168MHz 主频下需要 5 个等待周期（LATENCY_5WS），同时使能预取缓冲。此步骤必须在切换系统时钟之前完成，否则 CPU 取指错误导致系统异常。

**第七步：切换系统时钟到 PLL**

操作 RCC_CFGR 寄存器将 SW 位设置为 PLL，轮询 SWS 位确认切换完成。

**第八步：更新 SystemCoreClock**

将全局变量 `SystemCoreClock` 更新为 168000000，供 SysTick 等外设初始化使用。

## 函数说明

### sys_clock_init

系统时钟初始化函数。按上述八个步骤完成从 HSE 8MHz 到系统时钟 168MHz 的全部配置，必须在所有外设初始化之前最先调用。
