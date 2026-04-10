# 延时驱动说明

## 实现方式

基于 ARM Cortex-M4 内核的 SysTick 定时器，配置为每 1ms 产生一次中断，在中断服务函数中累加毫秒计数，实现精确阻塞延时。

---

## SysTick 寄存器说明

SysTick 是 ARM Cortex-M4 内核内置的 24 位递减计数器，共有 4 个寄存器：

### CTRL（控制和状态寄存器）

| 位   | 名称      | 说明 |
|------|-----------|------|
| 位 0 | ENABLE    | 置 1 使能 SysTick 计数器，置 0 停止 |
| 位 1 | TICKINT   | 置 1 使能计数到 0 时触发 SysTick 中断 |
| 位 2 | CLKSOURCE | 置 1 使用处理器时钟（AHB），置 0 使用 AHB/8 |
| 位 16 | COUNTFLAG | 计数器从 1 递减到 0 后该位置 1，读取后自动清零 |

### LOAD（重装载值寄存器）

| 位域    | 名称   | 说明 |
|---------|--------|------|
| 位 [23:0] | RELOAD | 计数器递减到 0 后自动重装载的值 |

计数器从 RELOAD 开始向下递减，到 0 后重新装载并触发中断（如果 TICKINT 已使能）。

### VAL（当前值寄存器）

| 位域    | 名称    | 说明 |
|---------|---------|------|
| 位 [23:0] | CURRENT | 计数器当前值，写任意值清零，同时清除 CTRL.COUNTFLAG |

### CALIB（校准值寄存器）

提供参考校准值，一般不需要手动操作。

---

## 配置步骤

### 第一步：计算重装载值

目标是每 1ms 触发一次中断，使用处理器时钟（AHB = SystemCoreClock）作为时钟源：

```
RELOAD = SystemCoreClock / 1000 - 1
```

以 168MHz 为例：RELOAD = 168000000 / 1000 - 1 = 167999

> 注意：RELOAD 最大值为 0xFFFFFF（16777215），168MHz 下每 1ms 的值为 167999，满足要求。

### 第二步：调用 SysTick_Config 完成配置

CMSIS 提供的 `SysTick_Config(ticks)` 函数自动完成以下操作：
1. 将 ticks - 1 写入 LOAD 寄存器
2. 清零 VAL 寄存器
3. 设置 CTRL：CLKSOURCE = 1（处理器时钟），TICKINT = 1（使能中断），ENABLE = 1（启动计数器）

```
SysTick_Config(SystemCoreClock / 1000);
```

### 第三步：中断服务函数计数

每次 SysTick 计数到 0 时触发 `SysTick_Handler`，将全局变量 `systick_ms` 加 1：

```c
static volatile uint32_t systick_ms = 0;

void SysTick_Handler(void)
{
    systick_ms++;
}
```

`volatile` 修饰确保编译器每次都从内存读取该变量，防止优化导致主循环中读取到缓存旧值。

### 第四步：延时等待

记录进入 `delay_ms` 时的 `systick_ms` 初始值，轮询差值是否达到目标毫秒数：

```c
void delay_ms(uint32_t ms)
{
    uint32_t start = systick_ms;
    while ((systick_ms - start) < ms);
}
```

使用差值计算（而非直接比较绝对值）可以正确处理 `systick_ms` 溢出回绕的情况。

---

## 注意事项

- `delay_init` 必须在 `sys_clock_init` 之后调用。`SysTick_Config` 使用 `SystemCoreClock` 计算重装载值，若时钟未切换到 168MHz，`SystemCoreClock` 仍为 16MHz，则 RELOAD 值偏小，中断频率偏高，延时偏短。
- `SysTick_Handler` 已在启动文件中以弱定义（`.weak`）声明，`delay.c` 中定义的同名函数会自动覆盖弱定义。

---

## 函数说明

### delay_init

初始化函数。调用 `SysTick_Config(SystemCoreClock / 1000)`，配置 SysTick 时钟源为处理器时钟，重装载值为 `SystemCoreClock / 1000`，使能计数器和中断，实现每 1ms 触发一次中断。

### delay_ms(uint32_t ms)

毫秒延时函数。传入需要延时的毫秒数，记录当前 `systick_ms` 值后阻塞等待，直到 `systick_ms` 累加差值达到目标值后返回。
