#ifndef __UART_H
#define __UART_H

#include "stm32f4xx.h"

/*
 * USART1 — PA9(TX) / PA10(RX)，AF7
 *
 * 时钟来源：APB2（84MHz）
 * 配置：8-N-1，无硬件流控
 *
 * TX：轮询 TXE，阻塞发送。
 * RX：RXNE 中断写入环形缓冲区，用户通过 uart1_recv_byte /
 *     uart1_recv_available 读取，无需关心缓冲区指针。
 */

#define UART1_RX_BUF_SIZE   64      /* 必须为 2 的幂，便于掩码取模 */

void    uart1_init(uint32_t baudrate);

/* 发送 */
void    uart1_send_byte(uint8_t byte);
void    uart1_send_buf(const uint8_t *buf, uint16_t len);
void    uart1_send_string(const char *str);

/* 接收 */
uint8_t uart1_recv_byte(uint8_t *data);   /* 非阻塞；有数据返回 1，否则返回 0 */
uint8_t uart1_recv_available(void);       /* 返回环形缓冲区中可读字节数 */

#endif /* __UART_H */
