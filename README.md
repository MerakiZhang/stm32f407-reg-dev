# stm32f407-reg-dev

STM32F407 寄存器级裸机开发项目，不依赖 HAL/LL 库，直接操作寄存器驱动外设。

## 开发环境

| 工具 | 说明 |
|------|------|
| 编译器 | arm-none-eabi-gcc |
| 构建系统 | CMake + Ninja |
| 调试器 | OpenOCD / ST-Link |

## 硬件平台

| 项目 | 说明 |
|------|------|
| 芯片 | STM32F407ZGTx |
| 内核 | ARM Cortex-M4（FPU） |
| 主频 | 168MHz（HSE 8MHz → PLL） |
| Flash | 1MB |
| RAM | 192KB |

## 开发方式对比

本项目采用 **Claude Code + CMake + 寄存器直接操作** 的开发模式，与传统的 Keil MDK + STM32CubeMX + HAL 库方案在多个维度上存在明显差异。

### 工具链对比

| 维度 | 本项目（现代化方案） | 传统方案 |
|------|---------------------|----------|
| IDE / 编辑器 | VS Code + Claude Code | Keil MDK（商业 IDE） |
| 编译器 | arm-none-eabi-gcc（开源） | ARMCC / Clang（Keil 内置，部分商业授权） |
| 构建系统 | CMake + Ninja（跨平台） | Keil 内置构建（.uvprojx，仅 Windows） |
| 代码生成 | 无，手写寄存器操作 | STM32CubeMX 图形化生成初始化代码 |
| AI 辅助 | Claude Code 全程参与 | 无原生 AI 集成 |
| 版本控制集成 | Git 原生，命令行操作 | Keil 无内置 Git 支持 |
| 跨平台 | Linux / macOS / Windows 均可编译 | 仅 Windows |

### 抽象层级对比

```
本项目（寄存器级）          传统 HAL 库方案
─────────────────           ─────────────────
应用逻辑                    应用逻辑
    │                           │
直接寄存器操作               HAL API（如 HAL_GPIO_WritePin）
（如 GPIOF->ODR |= ...）         │
    │                       LL 驱动 / HAL 内部实现
    │                           │
硬件寄存器                  硬件寄存器
```

- **寄存器开发**：每一行代码都对应数据手册中的具体寄存器位域，执行路径短，无运行时开销，代码体积小。
- **HAL 库开发**：通过统一 API 屏蔽芯片差异，代码可在不同 STM32 系列间移植，但引入额外的抽象层和运行时开销，生成代码量更大。

### Claude Code 辅助开发

传统嵌入式开发中，寄存器级编程对开发者要求较高——需要频繁查阅数据手册、参考手册（RM）和芯片勘误表。Claude Code 在此流程中承担以下角色：

- **数据手册替代查询**：直接描述需求（如"TIM14 输出 PWM，引脚 PF9"），Claude Code 给出寄存器配置步骤和位域说明，减少手动翻阅 RM 的时间。
- **代码生成与审查**：生成驱动代码后可立即进行 code review，指出潜在问题（如 shadow register 加载时序、active-low LED 占空比计算反转等）。
- **文档同步**：每次新增外设驱动，Claude Code 同步更新 README 和模块文档（`.md`），保持代码与文档一致。
- **构建系统维护**：CMakeLists.txt 的源文件列表和头文件路径由 Claude Code 在添加模块时自动维护，避免遗漏。

---

## 项目结构

```
stm32f407-reg-dev/
├── main.c                        # 主程序入口
├── CMakeLists.txt                # 构建配置
├── CMakePresets.json             # Debug / Release 预设
├── cmake/
│   └── arm-none-eabi-gcc.cmake   # 交叉编译工具链配置
├── core/
│   ├── inc/                      # CMSIS 头文件、芯片寄存器定义
│   └── src/
│       └── system_stm32f4xx.c    # 系统初始化（HSE_VALUE = 8MHz）
├── startup/
│   └── startup_stm32f407xx.s    # 启动文件，向量表
├── linker/
│   ├── STM32F407ZGTX_FLASH.ld   # Flash 链接脚本
│   └── STM32F407ZGTX_RAM.ld     # RAM 链接脚本
└── peripherals/
    ├── sys/                      # 系统时钟配置
    ├── delay/                    # SysTick 毫秒延时
    ├── led/                      # LED 驱动
    ├── beep/                     # 蜂鸣器驱动
    ├── key/                      # 按键驱动（轮询）
    ├── exti/                     # 外部中断驱动
    ├── timer/                    # 通用定时器驱动
    ├── pwm/                      # PWM 输出驱动（TIM14）
    └── uart/                     # 串口通信驱动（USART1）
```

## 外设驱动

### sys — 系统时钟

HSE 8MHz 经 PLL 倍频配置系统时钟为 168MHz。

| 参数 | 值 |
|------|----|
| PLLM | 8 |
| PLLN | 336 |
| PLLP | 2 |
| PLLQ | 7 |
| AHB  | 168MHz |
| APB1 | 42MHz |
| APB2 | 84MHz |

### delay — SysTick 延时

基于 SysTick 定时器，每 1ms 中断一次，提供毫秒阻塞延时和计数读取。

```c
delay_init();
delay_ms(100);
uint32_t t = delay_get_tick();   // 获取当前毫秒计数，供 ISR 消抖使用
```

### led — LED 驱动

| 标识 | 引脚 | 有效电平 |
|------|------|----------|
| LED0 | PF9  | 低电平点亮 |
| LED1 | PF10 | 低电平点亮 |

```c
led_init();
led_on(LED0);
led_off(LED1);
led_toggle(LED0);
```

### beep — 蜂鸣器驱动

| 标识 | 引脚 | 有效电平 |
|------|------|----------|
| BEEP | PF8  | 高电平打开 |

```c
beep_init();
beep_on();
beep_off();
beep_toggle();
```

### key — 按键驱动

| 标识   | 引脚 | 有效电平 | 上下拉 |
|--------|------|----------|--------|
| KEY_UP | PA0  | 高电平   | 内部下拉 |
| KEY0   | PE4  | 低电平   | 内部上拉 |
| KEY1   | PE3  | 低电平   | 内部上拉 |
| KEY2   | PE2  | 低电平   | 内部上拉 |

```c
key_init();
uint8_t key = key_scan(0);   // 0: 单次触发，1: 连续触发
```

返回值：`KEY_UP_VAL` / `KEY0_VAL` / `KEY1_VAL` / `KEY2_VAL` / `0`（无按键）

### exti — 外部中断按键

将按键引脚配置为 EXTI 外部中断，按下时由硬件触发 ISR，主循环只需检查标志位，无需持续轮询。ISR 内采用时间戳消抖（20ms）。

| 标识   | 引脚 | EXTI 线 | 触发方式 |
|--------|------|---------|----------|
| KEY_UP | PA0  | EXTI0   | 上升沿 |
| KEY0   | PE4  | EXTI4   | 下降沿 |
| KEY1   | PE3  | EXTI3   | 下降沿 |
| KEY2   | PE2  | EXTI2   | 下降沿 |

```c
exti_key_init();

// 主循环中检查标志位
if (exti_key0_flag) {
    exti_key0_flag = 0;
    // 处理 KEY0
}
```

标志位：`exti_key_up_flag` / `exti_key0_flag` / `exti_key1_flag` / `exti_key2_flag`

### timer — 通用定时器

使用 TIM3 产生周期性更新中断。TIM3 时钟 = 84MHz（2 × APB1），通过 PSC 和 ARR 设定中断周期。

```
T = (PSC + 1) × (ARR + 1) / 84MHz
```

```c
timer3_init(8399, 4999);   /* 500ms 中断周期 */
```

主循环检查标志位：

```c
if (timer3_flag) {
    timer3_flag = 0;
    // 执行定时任务
}
```

标志位：`timer3_flag`

### pwm — PWM 输出（LED0 亮度控制）

使用 TIM14_CH1 在 PF9（LED0）上输出硬件 PWM，无需 ISR，直接操作 CCR1 寄存器调节占空比。

| 参数 | 值 | 说明 |
|------|----|------|
| 引脚 | PF9 (AF9) | TIM14_CH1 复用 |
| PWM 频率 | 1kHz | PSC=83，ARR=999，TIM14 时钟 84MHz |
| 分辨率 | 100 级 | duty 0（灭）~ 100（全亮） |

```c
pwm_init();
pwm_set_duty(50);    /* 半亮 */
pwm_set_duty(100);   /* 全亮 */
```

呼吸灯示例（渐亮渐暗，2s 一周期）：

```c
for (uint8_t d = 0; d <= 100; d++) { pwm_set_duty(d);         delay_ms(10); }
for (int16_t d = 99; d >= 0; d--) { pwm_set_duty((uint8_t)d); delay_ms(10); }
```

### uart — 串口通信（USART1）

使用 USART1 提供串口收发，TX 轮询发送，RX 中断接收，并将 `printf` 重定向至串口。

| 参数 | 值 | 说明 |
|------|----|------|
| TX 引脚 | PA9 (AF7) | USART1_TX |
| RX 引脚 | PA10 (AF7) | USART1_RX |
| 时钟来源 | APB2 = 84MHz | |
| 默认配置 | 8-N-1 | 无硬件流控 |

```c
uart1_init(115200);

/* 发送 */
uart1_send_byte(0xA5);
uart1_send_string("Hello, STM32!\r\n");
uart1_send_buf(buf, len);             /* 发送任意字节数组 */
printf("tick = %lu\r\n", delay_get_tick());   /* printf 重定向至串口 */

/* 接收（非阻塞） */
uint8_t ch;
if (uart1_recv_byte(&ch)) {
    /* 处理收到的字节 ch */
}

/* 批量读取 */
while (uart1_recv_available()) {
    uart1_recv_byte(&ch);
    /* 处理 ch */
}
```

RX 采用环形缓冲区（`UART1_RX_BUF_SIZE` = 64 字节），中断自动填充；缓冲区满时丢弃新数据，已有数据不会被覆盖。

### iwdg — 独立看门狗（IWDG）

由 LSI 内部 RC（约 32 kHz）驱动，与主时钟无关；超时未喂狗则强制复位，用于检测程序跑飞或死锁。

| 参数 | 值 | 说明 |
|------|----|------|
| 时钟源 | LSI ≈ 32 kHz | 精度约 ±20%，与系统时钟无关 |
| 超时范围 | 1 ~ 32760 ms | 自动选择预分频 |
| 启动后 | 不可关闭 | 复位也不停止，断电才停止 |

```c
iwdg_init(1000);   /* 初始化，超时 1000ms，超时后自动复位 */

while (1) {
    iwdg_feed();   /* 喂狗，必须在超时时间内定期调用 */
    /* 业务逻辑... */
}
```

调试时若需要断点暂停而不触发看门狗复位，可在 `iwdg_init` 前配置：

```c
DBGMCU->APB1FZ |= (1U << 12);   /* 调试暂停时冻结 IWDG 计数 */
```

## 构建方法

```bash
# 配置（Debug）
cmake --preset debug

# 编译
cmake --build build/debug

# 配置（Release）
cmake --preset release
cmake --build build/release
```

编译产物位于对应构建目录下：`stm32f407-reg-dev.elf` / `.bin` / `.hex`

## 初始化顺序

```c
sys_clock_init();   // 必须最先调用，切换系统时钟到 168MHz
delay_init();       // 依赖 SystemCoreClock，必须在 sys_clock_init 之后
// 其他外设初始化...
```
