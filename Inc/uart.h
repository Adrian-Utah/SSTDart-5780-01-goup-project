#pragma once

#include <stdint.h>

void uart_init(void);
void uart_write_char(char c);
void uart_write_string(const char *str);
int uart_try_read_char(uint8_t *out);
uint8_t uart_read_char_blocking(void);
uint32_t uart_read_line(char *buffer, uint32_t max_length);
void uart_irq_handler(void);
