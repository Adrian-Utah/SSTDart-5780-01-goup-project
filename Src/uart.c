#include "uart.h"

#include "stm32f0xx_hal.h"

#define UART_RX_BUFFER_SIZE 128u

UART_HandleTypeDef huart3;

static volatile uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint16_t uart_rx_head = 0u;
static volatile uint16_t uart_rx_tail = 0u;

void uart_init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF1_USART3;
    HAL_GPIO_Init(GPIOC, &gpio);

    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);

    USART3->CR1 |= USART_CR1_RXNEIE;
    HAL_NVIC_SetPriority(USART3_4_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART3_4_IRQn);
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
