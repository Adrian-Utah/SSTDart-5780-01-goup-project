#include "ook.h"

#include "dds.h"

static void ook_delay(uint32_t period_ticks)
{
    for (volatile uint32_t i = 0u; i < period_ticks; i++) {
        __asm__("nop");
    }
}

void ook_send_bit(uint8_t bit, uint32_t period_ticks)
{
    if (bit != 0u) {
        dds_output_on();
    } else {
        dds_output_off();
    }

    ook_delay(period_ticks);
}

void ook_send_byte(uint8_t data, uint32_t period_ticks)
{
    for (int bit_index = 7; bit_index >= 0; bit_index--) {
        ook_send_bit((uint8_t)((data >> bit_index) & 0x1u), period_ticks);
    }
}

void ook_send_bytes(const uint8_t *data, uint32_t length, uint32_t period_ticks)
{
    for (uint32_t i = 0u; i < length; i++) {
        ook_send_byte(data[i], period_ticks);
    }

    dds_output_off();
}
