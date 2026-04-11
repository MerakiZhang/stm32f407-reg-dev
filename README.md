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
    └── pwm/                      # PWM 输出驱动（TIM14）
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
