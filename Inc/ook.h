#pragma once

#include <stdint.h>

void ook_send_bit(uint8_t bit, uint32_t period_ticks);
void ook_send_byte(uint8_t data, uint32_t period_ticks);
void ook_send_bytes(const uint8_t *data, uint32_t length, uint32_t period_ticks);
