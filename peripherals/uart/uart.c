#include "uart.h"

/*
 * USART1 驱动 — PA9(TX) / PA10(RX)
 *
 * 引脚复用：PA9  → AF7 → USART1_TX
 *           PA10 → AF7 → USART1_RX
 * USART1 时钟：APB2 = 84MHz
 *
 * BRR 计算（OVER8=0，16 倍过采样）：
 *   BRR = fAPB2 / BaudRate（四舍五入）
 *   例：115200bps → BRR = 729，实际误差 < 0.02%
 *
 * RX 环形缓冲区：
 *   rx_head — ISR 写指针（只在中断中修改）
 *   rx_tail — 用户读指针（只在主循环中修改）
 *   缓冲区满时丢弃新数据（保护已有数据），不会覆盖未读内容。
 */

/* ----------------------------------------------------------------
 * 环形缓冲区（内部私有）
 * UART1_RX_BUF_SIZE 必须为 2 的幂，掩码 = SIZE - 1
 * ---------------------------------------------------------------- */
#define RX_MASK  (UART1_RX_BUF_SIZE - 1U)

static volatile uint8_t rx_buf[UART1_RX_BUF_SIZE];
static volatile uint8_t rx_head = 0;   /* ISR 写入位置 */
static volatile uint8_t rx_tail = 0;   /* 用户读取位置 */

/* ----------------------------------------------------------------
 * uart1_init — 初始化 USART1
 * ---------------------------------------------------------------- */
void uart1_init(uint32_t baudrate)
{
    /* --------------------------------------------------------
     * 1. GPIO：PA9(TX) 和 PA10(RX) 配置为 AF7
     * -------------------------------------------------------- */
    RCC->AHB1ENR |= (1U << 0);              /* 使能 GPIOA 时钟 */

    /* PA9 — TX：复用功能，推挽，高速，无上下拉 */
    GPIOA->MODER   &= ~(3U << 18);
    GPIOA->MODER   |=  (2U << 18);          /* AF 模式 */
    GPIOA->OSPEEDR |=  (2U << 18);          /* 高速 */
    GPIOA->OTYPER  &= ~(1U << 9);           /* 推挽 */
    GPIOA->PUPDR   &= ~(3U << 18);          /* 无上下拉 */

    /* PA10 — RX：复用功能，上拉 */
    GPIOA->MODER   &= ~(3U << 20);
    GPIOA->MODER   |=  (2U << 20);          /* AF 模式 */
    GPIOA->PUPDR   &= ~(3U << 20);
    GPIOA->PUPDR   |=  (1U << 20);          /* 上拉 */

    /* AFRH（AFR[1]）：PA9 → bits[7:4] = 7，PA10 → bits[11:8] = 7 */
    GPIOA->AFR[1] &= ~(0xFFU << 4);
    GPIOA->AFR[1] |=  (7U << 4) | (7U << 8);

    /* --------------------------------------------------------
     * 2. USART1 配置
     * -------------------------------------------------------- */
    RCC->APB2ENR |= (1U << 4);              /* 使能 USART1 时钟（APB2ENR bit4） */

    USART1->BRR = (84000000U + baudrate / 2U) / baudrate;

    /* CR1：UE=1 使能，TE=1 发送，RE=1 接收，RXNEIE=1 接收非空中断 */
    USART1->CR1 = (1U << 13) |              /* UE    */
                  (1U << 5)  |              /* RXNEIE */
                  (1U << 3)  |              /* TE    */
                  (1U << 2);               /* RE    */

    /* CR2 / CR3 保持默认（1 停止位，无硬件流控） */

    /* --------------------------------------------------------
     * 3. NVIC：优先级 2（低于 SysTick=0 和 TIM3=1）
     * -------------------------------------------------------- */
    NVIC_SetPriority(USART1_IRQn, 2);
    NVIC_EnableIRQ(USART1_IRQn);
}

/* ----------------------------------------------------------------
 * 发送函数
 * ---------------------------------------------------------------- */

/*
 * uart1_send_byte — 阻塞发送 1 字节
 * 等待 TXE（发送数据寄存器空）后写入 DR。
 */
void uart1_send_byte(uint8_t byte)
{
    while (!(USART1->SR & (1U << 7)));      /* 等待 TXE */
    USART1->DR = byte;
}

/*
 * uart1_send_buf — 阻塞发送任意字节数组
 */
void uart1_send_buf(const uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        uart1_send_byte(buf[i]);
    }
}

/*
 * uart1_send_string — 阻塞发送以 '\0' 结尾的字符串
 */
void uart1_send_string(const char *str)
{
    while (*str)
    {
        uart1_send_byte((uint8_t)*str++);
    }
}

/* ----------------------------------------------------------------
 * 接收函数
 * ---------------------------------------------------------------- */

/*
 * uart1_recv_available — 返回环形缓冲区中可读字节数
 */
uint8_t uart1_recv_available(void)
{
    return (uint8_t)((rx_head - rx_tail) & RX_MASK);
}

/*
 * uart1_recv_byte — 非阻塞读取 1 字节
 * 有数据时将其写入 *data 并返回 1；缓冲区空时返回 0。
 */
uint8_t uart1_recv_byte(uint8_t *data)
{
    if (rx_head == rx_tail)
    {
        return 0;                           /* 缓冲区空 */
    }
    *data   = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1U) & RX_MASK;
    return 1;
}

/* ----------------------------------------------------------------
 * 中断服务程序
 * ---------------------------------------------------------------- */

/*
 * USART1_IRQHandler — RXNE 中断：将新字节写入环形缓冲区
 * 缓冲区满时丢弃新数据，不覆盖已有未读内容。
 */
void USART1_IRQHandler(void)
{
    if (USART1->SR & (1U << 5))             /* RXNE：接收数据就绪 */
    {
        uint8_t data = (uint8_t)(USART1->DR & 0xFFU);   /* 读 DR 同时清 RXNE */
        uint8_t next = (rx_head + 1U) & RX_MASK;
        if (next != rx_tail)                /* 缓冲区未满 */
        {
            rx_buf[rx_head] = data;
            rx_head = next;
        }
    }
}

/* ----------------------------------------------------------------
 * printf 重定向（newlib-nano _write）
 * ---------------------------------------------------------------- */
int _write(int fd, char *ptr, int len)
{
    (void)fd;
    uart1_send_buf((const uint8_t *)ptr, (uint16_t)len);
    return len;
}
