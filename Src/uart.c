#include "uart.h"

#include <stm32f072xb.h>

#define UART_RX_BUFFER_SIZE 128u

static volatile uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint16_t uart_rx_head = 0u;
static volatile uint16_t uart_rx_tail = 0u;

void uart_init(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;

    GPIOC->MODER |= GPIO_MODER_MODER10_1;
    GPIOC->MODER |= GPIO_MODER_MODER11_1;
    GPIOC->MODER &= ~GPIO_MODER_MODER10_0;
    GPIOC->MODER &= ~GPIO_MODER_MODER11_0;

    // set OTYPER to push pull
    GPIOC->OTYPER &= ~(GPIO_OTYPER_OT_10 | GPIO_OTYPER_OT_11);
    // set OSPEEDR to low
    GPIOC->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEEDR10 | GPIO_OSPEEDR_OSPEEDR11);
    // set to No Pull-up/Pull-down
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPDR10 | GPIO_PUPDR_PUPDR11);

    // set Alternate Function to AF1
    GPIOC->AFR[1] &= ~((0xFu << 8) | (0xFu << 12));
    GPIOC->AFR[1] |= ((1u << 8) | (1u << 12));

    // set baud rate
    USART3->BRR = 0x45;
    // Enable TX, Enable RX, and Enable RX Interrupt
    USART3->CR1 |= USART_CR1_TE;
    USART3->CR1 |= USART_CR1_RE;
    USART3->CR1 |= USART_CR1_RXNEIE;
    // enable UART
    USART3->CR1 |= USART_CR1_UE;

    // NVIC Setup
    NVIC->IP[29] = (1u << 6);
    NVIC->ISER[0] = (1u << 29);
}

void uart_write_char(char c)
{
    while ((USART3->ISR & USART_ISR_TXE) == 0u) {
    }
    USART3->TDR = c;
}

void uart_write_string(const char *str)
{
    while (*str != '\0') {
        uart_write_char(*str++);
    }
}

int uart_try_read_char(uint8_t *out)
{
    if (uart_rx_head == uart_rx_tail) {
        return 0;
    }

    *out = uart_rx_buffer[uart_rx_tail];
    uart_rx_tail = (uint16_t)((uart_rx_tail + 1u) % UART_RX_BUFFER_SIZE);
    return 1;
}

uint8_t uart_read_char_blocking(void)
{
    uint8_t byte = 0u;

    while (!uart_try_read_char(&byte)) {
    }

    return byte;
}

uint32_t uart_read_line(char *buffer, uint32_t max_length)
{
    uint32_t length = 0u;

    if (max_length == 0u) {
        return 0u;
    }

    while (length + 1u < max_length) {
        uint8_t byte = uart_read_char_blocking();

        if ((byte == '\r') || (byte == '\n')) {
            break;
        }

        buffer[length++] = (char)byte;
    }

    buffer[length] = '\0';
    return length;
}

void uart_irq_handler(void)
{
    if ((USART3->ISR & USART_ISR_RXNE) != 0u) {
        uint8_t received = (uint8_t)USART3->RDR;
        uint16_t next_head = (uint16_t)((uart_rx_head + 1u) % UART_RX_BUFFER_SIZE);

        if (next_head != uart_rx_tail) {
            uart_rx_buffer[uart_rx_head] = received;
            uart_rx_head = next_head;
        }
    }
}
