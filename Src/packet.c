#include "packet.h"

#include "app_config.h"
#include "dds.h"
#include "stm32f0xx_hal.h"

uint8_t packet_crc8(const uint8_t *data, uint16_t length)
{
    uint8_t crc = 0x00u;

    for (uint16_t i = 0u; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0u; bit < 8u; bit++) {
            if ((crc & 0x80u) != 0u) {
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }

    return crc;
}

static void packet_wait_until(uint32_t target_tick)
{
    while ((int32_t)(target_tick - HAL_GetTick()) > 0) {
    }
}

static void packet_send_byte_at(uint8_t value, uint32_t *next_edge_tick)
{
    for (int bit_index = 7; bit_index >= 0; bit_index--) {
        packet_wait_until(*next_edge_tick);
        if (((value >> bit_index) & 0x1u) != 0u) {
            dds_output_on();
        } else {
            dds_output_off();
        }
        *next_edge_tick += OOK_PATTERN_SYMBOL_MS;
    }
}

static void packet_send_framed_bytes(const uint8_t *bytes, uint8_t length, uint32_t *next_edge_tick)
{
    for (uint8_t i = 0u; i < OOK_PACKET_PREAMBLE_LENGTH; i++) {
        packet_send_byte_at(OOK_PACKET_PREAMBLE_BYTE, next_edge_tick);
    }
    packet_send_byte_at(OOK_PACKET_SYNC_BYTE, next_edge_tick);
    for (uint8_t i = 0u; i < length; i++) {
        packet_send_byte_at(bytes[i], next_edge_tick);
    }
    packet_send_byte_at(packet_crc8(bytes, length), next_edge_tick);
    packet_wait_until(*next_edge_tick);
    dds_output_off();
    HAL_Delay(OOK_PACKET_INTER_GAP_MS);
}

static void packet_send_start(uint16_t total_length, uint32_t *next_edge_tick)
{
    uint8_t bytes[3];
    bytes[0] = PACKET_TYPE_START;
    bytes[1] = (uint8_t)(total_length & 0xFFu);
    bytes[2] = (uint8_t)((total_length >> 8) & 0xFFu);
    packet_send_framed_bytes(bytes, sizeof(bytes), next_edge_tick);
}

static void packet_send_data(const uint8_t *payload, uint8_t length, uint32_t *next_edge_tick)
{
    uint8_t bytes[1u + OOK_FRAGMENT_MAX_PAYLOAD];
    bytes[0] = (uint8_t)(PACKET_TYPE_DATA | length);
    for (uint8_t i = 0u; i < length; i++) {
        bytes[1u + i] = payload[i];
    }
    packet_send_framed_bytes(bytes, (uint8_t)(1u + length), next_edge_tick);
}

void packet_send_message(const uint8_t *payload, uint16_t length)
{
    uint32_t next_edge_tick = HAL_GetTick();

    packet_send_start(length, &next_edge_tick);

    for (uint16_t offset = 0u; offset < length; offset += OOK_FRAGMENT_MAX_PAYLOAD) {
        uint16_t remaining = (uint16_t)(length - offset);
        uint8_t fragment_length = (remaining > OOK_FRAGMENT_MAX_PAYLOAD)
            ? OOK_FRAGMENT_MAX_PAYLOAD
            : (uint8_t)remaining;
        packet_send_data(&payload[offset], fragment_length, &next_edge_tick);
    }
}
