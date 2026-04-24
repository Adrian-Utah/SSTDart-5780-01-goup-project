#pragma once

#include <stdint.h>

#define PACKET_TYPE_START 0x10u
#define PACKET_TYPE_DATA  0x20u
#define PACKET_TYPE_MASK  0xF0u
#define PACKET_LENGTH_MASK 0x0Fu

uint8_t packet_crc8(const uint8_t *data, uint16_t length);
void packet_send_message(const uint8_t *payload, uint16_t length);
