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

static void packet_send_byte(uint8_t value)
{
    for (int bit_index = 7; bit_index >= 0; bit_index--) {
        if (((value >> bit_index) & 0x1u) != 0u) {
            dds_output_on();
        } else {
            dds_output_off();
        }
        HAL_Delay(OOK_PATTERN_SYMBOL_MS);
    }
}

void packet_send(const uint8_t *payload, uint8_t length)
{
    uint8_t header_and_payload[1u + OOK_PACKET_MAX_PAYLOAD];
    header_and_payload[0] = length;
    for (uint8_t i = 0u; i < length; i++) {
        header_and_payload[1u + i] = payload[i];
    }
    uint8_t crc = packet_crc8(header_and_payload, (uint16_t)(1u + length));

    for (uint8_t i = 0u; i < OOK_PACKET_PREAMBLE_LENGTH; i++) {
        packet_send_byte(OOK_PACKET_PREAMBLE_BYTE);
    }
    packet_send_byte(OOK_PACKET_SYNC_BYTE);
    packet_send_byte(length);
    for (uint8_t i = 0u; i < length; i++) {
        packet_send_byte(payload[i]);
    }
    packet_send_byte(crc);

    dds_output_off();
    HAL_Delay(OOK_PACKET_INTER_GAP_MS);
}
