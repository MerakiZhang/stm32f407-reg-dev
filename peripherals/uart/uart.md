# uart — USART1 串口通信驱动

## 功能概述

使用 STM32F407 的 USART1 外设实现与 PC 的串口通信：

- **TX**：轮询 TXE 标志，阻塞发送字节 / 数组 / 字符串。
- **RX**：RXNE 中断将数据写入**环形缓冲区**，用户通过 `uart1_recv_byte` 逐字节读取，互不干扰。
- **printf 重定向**：实现 `_write`，`printf / puts` 等标准 IO 自动输出至 USART1。

## 引脚与时钟

| 项目 | 值 |
|------|----|
| TX 引脚 | PA9，复用 AF7（USART1_TX） |
| RX 引脚 | PA10，复用 AF7（USART1_RX） |
| USART1 时钟 | APB2 = 84MHz |
| GPIOA 时钟 | RCC->AHB1ENR bit0 |
| USART1 时钟使能 | RCC->APB2ENR bit4 |

## 波特率计算

OVER8=0（16 倍过采样）：

```
BRR = fAPB2 / BaudRate = 84MHz / BaudRate（四舍五入）
```

| 波特率 | BRR | 实际误差 |
|--------|-----|----------|
| 9600   | 8750 | < 0.01% |
| 115200 | 729  | < 0.02% |

## 环形缓冲区设计

```
rx_buf[UART1_RX_BUF_SIZE]
         ↑ rx_head（ISR 写入，每收到一字节后移）
         ↑ rx_tail（用户读取，每读一字节后移）
```

- 判满：`(rx_head + 1) & MASK == rx_tail`（保留一格区分满/空）
- 判空：`rx_head == rx_tail`
- 缓冲区大小 `UART1_RX_BUF_SIZE` 必须为 **2 的幂**（默认 64）

## API

```c
/* 初始化 */
void uart1_init(uint32_t baudrate);

/* 发送 */
void uart1_send_byte(uint8_t byte);               /* 发送 1 字节（阻塞） */
void uart1_send_buf(const uint8_t *buf, uint16_t len);  /* 发送字节数组 */
void uart1_send_string(const char *str);          /* 发送字符串 */

/* 接收 */
uint8_t uart1_recv_byte(uint8_t *data);    /* 非阻塞；有数据返回 1，否则 0 */
uint8_t uart1_recv_available(void);        /* 返回缓冲区中可读字节数 */
```

## 使用示例

```c
#include "uart.h"
#include <stdio.h>

uart1_init(115200);
printf("boot ok\r\n");

while (1) {
    uint8_t ch;
    if (uart1_recv_byte(&ch)) {
        /* 收到字符后回显 */
        uart1_send_byte(ch);
    }
}
```

批量读取示例：

```c
uint8_t frame[16];
uint8_t idx = 0;

while (uart1_recv_available() && idx < sizeof(frame)) {
    uart1_recv_byte(&frame[idx++]);
}
```

## 注意事项

- `printf` 重定向依赖 `--specs=nano.specs`（newlib-nano）。使用 `%f` 等浮点格式需额外链接选项 `-u _printf_float`。
- 缓冲区满时新数据被丢弃，已有未读内容不受影响。
- `uart1_recv_byte` 只在主循环中调用（非 ISR），无需关中断。
- NVIC 优先级设为 2，与 EXTI 相同，低于 SysTick（0）和 TIM3（1）。
