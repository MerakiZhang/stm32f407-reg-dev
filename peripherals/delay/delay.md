# 延时驱动说明

## 实现方式

基于 SysTick 定时器中断实现精确毫秒延时，精度 1ms，不受编译器优化影响。

## SysTick 配置说明

**第一步：调用 SysTick_Config**

传入 `SystemCoreClock / 1000` 作为重装载值，配置 SysTick 每 1ms 产生一次中断。时钟源为 AHB 时钟（即 SystemCoreClock）。

**第二步：在中断服务函数中计数**

SysTick_Handler 每次触发将全局计数变量 `systick_ms` 加 1，记录系统运行的毫秒数。

**第三步：延时等待**

`delay_ms` 记录调用时的 `systick_ms` 初始值，循环等待直到计数差值达到目标毫秒数。

## 注意事项

- `delay_init` 必须在 `sys_clock_init` 之后调用，确保 `SystemCoreClock` 已更新为正确的系统时钟频率，否则 SysTick 重装载值计算错误，延时不准。
- SysTick_Handler 已在启动文件中声明为弱定义，delay.c 中的实现会自动覆盖。

## 函数说明

### delay_init

初始化函数。调用 `SysTick_Config(SystemCoreClock / 1000)`，配置 SysTick 每 1ms 中断一次，使能 SysTick 定时器和中断。

### delay_ms

毫秒延时函数。传入需要延时的毫秒数，函数阻塞等待直到 `systick_ms` 计数差值达到目标值后返回。
