# 系统时钟配置说明

## 时钟目标

使用外部高速时钟 HSE（8MHz），经 PLL 倍频后将系统时钟（SYSCLK）配置为 168MHz。

## PLL 倍频计算

```
VCO 输入 = HSE / PLLM = 8MHz / 8 = 1MHz
VCO 输出 = VCO 输入 × PLLN = 1MHz × 336 = 336MHz
SYSCLK   = VCO 输出 / PLLP = 336MHz / 2 = 168MHz
USB 时钟 = VCO 输出 / PLLQ = 336MHz / 7 = 48MHz
```

| 参数  | 值  | 作用 |
|-------|-----|------|
| PLLM  | 8   | 对 HSE 预分频，得到 1MHz VCO 输入（要求 1~2MHz） |
| PLLN  | 336 | VCO 倍频系数（要求输出 100~432MHz） |
| PLLP  | 2   | 对 VCO 输出分频，得到 SYSCLK |
| PLLQ  | 7   | 对 VCO 输出分频，得到 USB/SDIO/RNG 时钟（需为 48MHz） |

## 总线频率

| 总线 | 分频系数 | 频率 | 最大限制 |
|------|----------|------|----------|
| AHB  | ÷1       | 168MHz | 168MHz |
| APB1 | ÷4       | 42MHz  | 42MHz  |
| APB2 | ÷2       | 84MHz  | 84MHz  |

---

## 寄存器配置说明

### 第一步：使能 HSE，等待就绪

寄存器：**RCC_CR**（RCC 时钟控制寄存器）

| 位    | 名称    | 说明 |
|-------|---------|------|
| 位 16 | HSEON   | 置 1 使能 HSE 振荡器 |
| 位 17 | HSERDY  | 只读，HSE 稳定后硬件自动置 1 |

```
RCC->CR |= RCC_CR_HSEON;
while (!(RCC->CR & RCC_CR_HSERDY));   // 等待 HSE 就绪
```

---

### 第二步：使能 PWR，设置电压调节器为 Scale 1 模式

**为什么需要配置电压调节器：**
STM32F407 内部电压调节器支持三种 Scale 模式，决定了允许的最高运行频率。运行 168MHz 必须使用 Scale 1 模式（1.2V 内核电压）。

寄存器：**RCC_APB1ENR**（RCC APB1 外设时钟使能寄存器）

| 位    | 名称  | 说明 |
|-------|-------|------|
| 位 28 | PWREN | 置 1 使能 PWR 外设时钟 |

```
RCC->APB1ENR |= RCC_APB1ENR_PWREN;
```

寄存器：**PWR_CR**（PWR 电源控制寄存器）

| 位域     | 名称 | 说明 |
|----------|------|------|
| 位 [15:14] | VOS  | 00=保留，01=Scale 3，10=Scale 2，11=Scale 1 |

```
PWR->CR |= PWR_CR_VOS;   // VOS = 11，Scale 1 模式
```

---

### 第三步：配置 AHB / APB1 / APB2 分频

寄存器：**RCC_CFGR**（RCC 时钟配置寄存器）

**AHB 分频（HPRE，位 [7:4]）：**

| 位值 | 分频系数 |
|------|----------|
| 0xxx | ÷1（不分频） |
| 1000 | ÷2 |
| 1001 | ÷4 |
| 1010 | ÷8 |
| 1011 | ÷16 |
| 1100 | ÷64 |
| 1101 | ÷128 |
| 1110 | ÷256 |
| 1111 | ÷512 |

设置为 0000（不分频），AHB = SYSCLK = 168MHz：
```
RCC->CFGR |= RCC_CFGR_HPRE_DIV1;
```

**APB1 分频（PPRE1，位 [12:10]）：**

| 位值 | 分频系数 |
|------|----------|
| 0xx  | ÷1 |
| 100  | ÷2 |
| 101  | ÷4 |
| 110  | ÷8 |
| 111  | ÷16 |

设置为 101（÷4），APB1 = 168 / 4 = 42MHz：
```
RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;
```

**APB2 分频（PPRE2，位 [15:13]）：**

位值定义与 PPRE1 相同。设置为 100（÷2），APB2 = 168 / 2 = 84MHz：
```
RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;
```

---

### 第四步：配置 PLL 参数

寄存器：**RCC_PLLCFGR**（RCC PLL 配置寄存器）

| 位域      | 名称    | 说明 |
|-----------|---------|------|
| 位 [5:0]  | PLLM    | HSE/HSI 预分频系数，范围 2~63 |
| 位 [14:6] | PLLN    | VCO 倍频系数，范围 50~432 |
| 位 [17:16] | PLLP   | VCO 输出分频，00=÷2，01=÷4，10=÷6，11=÷8 |
| 位 22     | PLLSRC  | 0=HSI 作为 PLL 源，1=HSE 作为 PLL 源 |
| 位 [27:24] | PLLQ   | USB 时钟分频系数，范围 2~15 |

写入参数（PLLP=2 对应位值 00）：
```
RCC->PLLCFGR = (8U  <<  0) |              // PLLM = 8
               (336U <<  6) |             // PLLN = 336
               (0U  << 16) |              // PLLP = 2（00）
               RCC_PLLCFGR_PLLSRC_HSE |  // 时钟源 = HSE
               (7U  << 24);              // PLLQ = 7
```

---

### 第五步：使能 PLL，等待就绪

寄存器：**RCC_CR**

| 位    | 名称   | 说明 |
|-------|--------|------|
| 位 24 | PLLON  | 置 1 使能主 PLL |
| 位 25 | PLLRDY | 只读，PLL 锁定后硬件自动置 1 |

```
RCC->CR |= RCC_CR_PLLON;
while (!(RCC->CR & RCC_CR_PLLRDY));   // 等待 PLL 锁定
```

---

### 第六步：配置 Flash 等待周期

寄存器：**FLASH_ACR**（Flash 访问控制寄存器）

| 位域    | 名称    | 说明 |
|---------|---------|------|
| 位 [2:0] | LATENCY | Flash 等待周期数 |
| 位 8    | PRFTEN  | 置 1 使能预取缓冲，提升取指效率 |
| 位 9    | ICEN    | 置 1 使能指令缓存 |
| 位 10   | DCEN    | 置 1 使能数据缓存 |

不同频率对应的等待周期（3.3V 供电）：

| SYSCLK 范围   | 等待周期（LATENCY） |
|---------------|---------------------|
| 0 ~ 30MHz     | 0WS |
| 30 ~ 60MHz    | 1WS |
| 60 ~ 90MHz    | 2WS |
| 90 ~ 120MHz   | 3WS |
| 120 ~ 150MHz  | 4WS |
| 150 ~ 168MHz  | 5WS |

168MHz 需要配置为 5 个等待周期，**必须在切换系统时钟之前配置**，否则 CPU 取指速度快于 Flash 读取速度，导致系统异常：
```
FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_5WS;
```

---

### 第七步：切换系统时钟到 PLL

寄存器：**RCC_CFGR**

| 位域    | 名称 | 说明 |
|---------|------|------|
| 位 [1:0] | SW   | 系统时钟源选择：00=HSI，01=HSE，10=PLL |
| 位 [3:2] | SWS  | 只读，当前实际使用的系统时钟源，与 SW 定义相同 |

```
RCC->CFGR |= RCC_CFGR_SW_PLL;
while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);   // 等待切换完成
```

---

### 第八步：更新 SystemCoreClock

`SystemCoreClock` 是 CMSIS 提供的全局变量，记录当前 AHB 时钟频率，供 SysTick 等模块使用。时钟切换后手动更新为 168MHz：
```
SystemCoreClock = 168000000;
```

---

## 函数说明

### sys_clock_init

系统时钟初始化函数。按以上八个步骤完成从 HSE 8MHz 到系统时钟 168MHz 的全部配置。**必须在所有外设初始化（包括 delay_init）之前最先调用。**
