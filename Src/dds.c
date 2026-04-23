#include "dds.h"

#include <stm32f072xb.h>

static void dds_short_delay(void)
{
    for (volatile int i = 0; i < 20; i++) {
        __asm__("nop");
    }
}

/*
 * Write to dds.
 *
 * data - data to send to DDS
 * no return
 */
static void dds_write_word(uint16_t data)
{
    // Assume:
    // PA5 = CLK / SCLK
    // PB6 = FSY / FSYNC
    // PB7 = DAT / SDATA

    // Make sure clock is high before FSYNC goes low
    GPIOA->BSRR = (1u << 5);
    GPIOB->BSRR = (1u << 6);
    dds_short_delay();

    // Start frame
    GPIOB->BRR = (1u << 6); // FSY low
    dds_short_delay();

    // Send 16 bits MSB first
    for (int i = 15; i >= 0; i--) {
        if ((data & (1u << i)) != 0u) {
            GPIOB->BSRR = (1u << 7); // DAT high
        } else {
            GPIOB->BRR = (1u << 7); // DAT low
        }

        // Data shifts in on falling edge
        dds_short_delay();
        GPIOA->BRR = (1u << 5); // CLK low
        dds_short_delay();
        GPIOA->BSRR = (1u << 5); // CLK high
        dds_short_delay();
    }

    // End frame
    GPIOB->BSRR = (1u << 6); // FSY high
    dds_short_delay();
}

void dds_init(void)
{
    RCC->AHBENR |= (RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN);

    GPIOA->MODER &= ~(GPIO_MODER_MODER4 | GPIO_MODER_MODER5);
    GPIOA->MODER |= (GPIO_MODER_MODER4_0 | GPIO_MODER_MODER5_0);

    GPIOB->MODER &= ~(GPIO_MODER_MODER6 | GPIO_MODER_MODER7);
    GPIOB->MODER |= (GPIO_MODER_MODER6_0 | GPIO_MODER_MODER7_0);

    GPIOA->BSRR = (1u << 4);
    GPIOA->BSRR = (1u << 5);
    GPIOB->BRR  = (1u << 7);
    GPIOB->BSRR = (1u << 6);
}

void dds_set_frequency(uint32_t freq_hz)
{
    uint32_t freq_reg = (uint32_t)(((uint64_t)freq_hz << 28) / 25000000ULL);

    dds_write_word(0x2100);
    dds_write_word(0x4000 | (freq_reg & 0x3FFFu));
    dds_write_word(0x4000 | ((freq_reg >> 14) & 0x3FFFu));
    dds_write_word(0x2000);
}

/* configure amplitude chip.
 *
 * target - target V mod
 *
 * no return
 */
void dds_set_amplitude(uint8_t target)
{
    if (target > 63u) {
        target = 63u;
    }

    // New DDS module wiring:
    // PA4 = CS for amplitude control
    // PA5 = CLK
    // PB7 = DAT / U-D
    // PB6 = FSY, keep high while changing amplitude
    GPIOB->BSRR = (1u << 6);
    GPIOA->BRR = (1u << 4);

    GPIOB->BRR = (1u << 7); // CS low
    for (int i = 0; i < 64; i++) {
        GPIOA->BSRR = (1u << 5); // CLK high
        for (volatile int delay = 0; delay < 10; delay++) {
        }
        GPIOA->BRR = (1u << 5); // CLK low
    }

    // Walk up to target
    GPIOB->BSRR = (1u << 7);
    // DAT/UD high
    for (int i = 0; i < target; i++) {
        GPIOA->BSRR = (1u << 5); // CLK high
        for (volatile int delay = 0; delay < 10; delay++) {
        }
        GPIOA->BRR = (1u << 5); // CLK low
    }

    GPIOA->BSRR = (1u << 4); // CS high (lock)
    GPIOA->BSRR = (1u << 5); // CLK idle high for AD9833
    GPIOB->BRR = (1u << 7); // DAT idle low
}

void dds_output_on(void)
{
    dds_write_word(0x2000);
}

void dds_output_off(void)
{
    dds_write_word(0x2100);
}
