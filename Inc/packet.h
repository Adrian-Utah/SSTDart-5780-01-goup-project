#pragma once

#include <stdint.h>

uint8_t packet_crc8(const uint8_t *data, uint16_t length);
void packet_send(const uint8_t *payload, uint8_t length);
