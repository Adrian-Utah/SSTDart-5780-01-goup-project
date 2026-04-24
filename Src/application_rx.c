#include "application_rx.h"

#include <stdio.h>

#include "app_config.h"
#include "board.h"
#include "packet.h"
#include "stm32f0xx_hal.h"
#include "uart.h"

#define RX_TEST_ADC_INPUT_PIN GPIO_PIN_1
#define RX_TEST_ADC_INPUT_CHANNEL ADC_CHANNEL_1

static uint16_t rx_test_adc_read_once(void);

static uint16_t rx_test_measure_span_for_ms(uint32_t duration_ms)
{
    uint16_t min_value = 0x0FFFu;
    uint16_t max_value = 0u;
    uint32_t start_tick = HAL_GetTick();

    while ((HAL_GetTick() - start_tick) < duration_ms) {
        uint16_t sample = rx_test_adc_read_once();

        if (sample < min_value) {
            min_value = sample;
        }
        if (sample > max_value) {
            max_value = sample;
        }
    }

    return (uint16_t)(max_value - min_value);
}

static void rx_wait_for_pattern_start(void)
{
    uint32_t idle_start_tick = 0u;

    uart_write_string("Waiting for long carrier-off gap.\r\n");

    while (1) {
        uint16_t span = rx_test_measure_span_for_ms(RX_SYNC_POLL_MS);

        if (span < RX_TEST_SPAN_THRESHOLD_COUNTS) {
            if (idle_start_tick == 0u) {
                idle_start_tick = HAL_GetTick();
            }

            if ((HAL_GetTick() - idle_start_tick) >= RX_SYNC_IDLE_MS) {
                break;
            }
        } else {
            idle_start_tick = 0u;
        }
    }

    uart_write_string("Gap detected. Waiting for carrier start.\r\n");

    while (1) {
        uint16_t span = rx_test_measure_span_for_ms(RX_SYNC_POLL_MS);

        if (span >= RX_TEST_SPAN_THRESHOLD_COUNTS) {
            uart_write_string("Carrier start detected. Sampling byte.\r\n");
            return;
        }
    }
}

static void rx_wait_for_confirmation(void)
{
    char buffer[8];

    while (1) {
        uart_write_string("Type y and press Enter to start RX sampling.\r\n");
        if (uart_read_line(buffer, sizeof(buffer)) == 0u) {
            continue;
        }

        if ((buffer[0] == 'y') || (buffer[0] == 'Y')) {
            uart_write_string("Starting RX sampling.\r\n");
            return;
        }

        uart_write_string("RX start cancelled. Waiting again.\r\n");
    }
}

static void rx_test_adc_init(void)
{
  // Enable Peripherals Clocks (RCC)
  RCC->AHBENR |= RCC_AHBENR_GPIOAEN; // Enable GPIOA clock
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN; // Enable ADC1 clock
  
  // Configure GPIOA Pin for Analog Mode
  // Assuming RX_TEST_ADC_INPUT_PIN is Pin X, we set MODER bits to 11 (Analog)
  GPIOA->MODER |= (3U << (RX_TEST_ADC_INPUT_PIN * 2)); 
  
  // ADC Clock and Basic Configuration
  // ADC_CLOCK_ASYNC_DIV1 usually means using the HSI16 or internal RC oscillator
  ADC1->CFGR2 &= ~ADC_CFGR2_CKMODE;      // Asynchronous clock mode
  
  // Configure ADC Features (CFGR1)
  ADC1->CFGR1 &= ~(ADC_CFGR1_RES |       // 12-bit Resolution (00)
                   ADC_CFGR1_ALIGN |     // Right alignment (0)
                   ADC_CFGR1_CONT |      // Single conversion mode (0)
                   ADC_CFGR1_EXTEN);     // Software trigger (00)
  
  ADC1->CFGR1 |= ADC_CFGR1_OVRMOD;       // Overrun: Data preserved (Keep old data)
  ADC1->CFGR1 |= ADC_CFGR1_SCANDIR;      // Scan direction: Forward
  
  // Configure Channel and Sampling Time
  // Select the channel in the CHSELR register
  ADC1->CHSELR |= (1U << RX_TEST_ADC_INPUT_CHANNEL); 
  
  // Set Sampling Time (SMPR) - 0x7 corresponds to 239.5 cycles
  ADC1->SMPR |= (0x7U << ADC_SMPR_SMP_Pos); 
  
  //Calibration
  if ((ADC1->CR & ADC_CR_ADEN) != 0) {
    ADC1->CR |= ADC_CR_ADDIS;          // Ensure ADC is disabled for calibration
  }
  while ((ADC1->CR & ADC_CR_ADEN) != 0); // Wait for disable
  
  ADC1->CR |= ADC_CR_ADCAL;              // Start calibration
  while ((ADC1->CR & ADC_CR_ADCAL) != 0); // Wait for calibration to finish
}

static uint16_t rx_test_adc_read_once(void)
{
    // Enable the ADC if it isn't already
    if ((ADC1->CR & ADC_CR_ADEN) == 0) {
        ADC1->ISR |= ADC_ISR_ADRDY;      // Clear the ADRDY flag by writing 1
        ADC1->CR |= ADC_CR_ADEN;         // Enable the ADC
        while (!(ADC1->ISR & ADC_ISR_ADRDY)); // Wait until ADC is ready
    }

    // Start the conversion
    ADC1->CR |= ADC_CR_ADSTART;

    // Poll for conversion completion (EOC flag)
    while (!(ADC1->ISR & ADC_ISR_EOC));

    //Read the value (This also clears the EOC flag automatically)
    uint16_t value = (uint16_t)ADC1->DR;

    // Stop the ADC
    if ((ADC1->CR & ADC_CR_ADSTART) != 0) {
        ADC1->CR |= ADC_CR_ADSTP;
        while (ADC1->CR & ADC_CR_ADSTP); // Wait for stop to complete
    }

    return value;
}

void application_rx_init(void)
{
    rx_test_adc_init();
    uart_write_string("PROGRAM STARTED\r\n");
#if ACTIVE_RX_MODE == RX_MODE_MESSAGE
    uart_write_string("MODE: MESSAGE\r\n");
    uart_write_string("Waiting for a packet, then printing the decoded message.\r\n");
#else
    uart_write_string("MODE: PATTERN TEST\r\n");
    uart_write_string("Waiting for a gap, then center-sampling 8 OOK bits on PA1.\r\n");
#endif
    rx_wait_for_confirmation();
}

#if ACTIVE_RX_MODE == RX_MODE_MESSAGE

#define RX_PACKET_OK 0
#define RX_PACKET_ERR_SYNC 1
#define RX_PACKET_ERR_LENGTH 2
#define RX_PACKET_ERR_CRC 3

static void rx_wait_until(uint32_t target_tick)
{
    while ((int32_t)(target_tick - HAL_GetTick()) > 0) {
    }
}

static uint8_t rx_read_bit_at(uint32_t *next_sample_tick)
{
    rx_wait_until(*next_sample_tick);
    uint16_t span = rx_test_measure_span_for_ms(RX_CENTER_SAMPLE_MS);
    uint8_t bit = (span >= RX_TEST_SPAN_THRESHOLD_COUNTS) ? 1u : 0u;

    if (bit != 0u) {
        board_led_on();
    } else {
        board_led_off();
    }

    *next_sample_tick += OOK_PATTERN_SYMBOL_MS;
    return bit;
}

static uint8_t rx_read_byte_at(uint32_t *next_sample_tick)
{
    uint8_t value = 0u;
    for (uint8_t i = 0u; i < 8u; i++) {
        value = (uint8_t)((value << 1) | rx_read_bit_at(next_sample_tick));
    }
    return value;
}

static void rx_print_hex_byte(uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    char buffer[3];
    buffer[0] = hex[(value >> 4) & 0xFu];
    buffer[1] = hex[value & 0xFu];
    buffer[2] = '\0';
    uart_write_string(buffer);
}

static int rx_receive_packet(uint8_t *out_buffer, uint8_t out_capacity, uint8_t *out_length,
                             uint8_t *out_received_crc, uint8_t *out_expected_crc)
{
    *out_length = 0u;
    *out_received_crc = 0u;
    *out_expected_crc = 0u;

    rx_wait_for_pattern_start();
    uint32_t next_sample_tick = HAL_GetTick() + (OOK_PATTERN_SYMBOL_MS / 2u);

    uint8_t shift_register = 0u;
    uint32_t max_sync_bits = (uint32_t)(OOK_PACKET_PREAMBLE_LENGTH + 4u) * 8u;

    for (uint32_t i = 0u; i < max_sync_bits; i++) {
        shift_register = (uint8_t)((shift_register << 1) | rx_read_bit_at(&next_sample_tick));
        if (shift_register == OOK_PACKET_SYNC_BYTE) {
            break;
        }
        if (i + 1u == max_sync_bits) {
            return RX_PACKET_ERR_SYNC;
        }
    }

    uint8_t length = rx_read_byte_at(&next_sample_tick);
    if ((length == 0u) || (length > out_capacity)) {
        *out_length = length;
        return RX_PACKET_ERR_LENGTH;
    }
    *out_length = length;

    uint8_t header_and_payload[1u + OOK_PACKET_MAX_PAYLOAD];
    header_and_payload[0] = length;
    for (uint8_t i = 0u; i < length; i++) {
        header_and_payload[1u + i] = rx_read_byte_at(&next_sample_tick);
        out_buffer[i] = header_and_payload[1u + i];
    }

    *out_received_crc = rx_read_byte_at(&next_sample_tick);
    *out_expected_crc = packet_crc8(header_and_payload, (uint16_t)(1u + length));
    if (*out_received_crc != *out_expected_crc) {
        return RX_PACKET_ERR_CRC;
    }

    return RX_PACKET_OK;
}

static void rx_print_payload_hex(const uint8_t *payload, uint8_t length)
{
    for (uint8_t i = 0u; i < length; i++) {
        rx_print_hex_byte(payload[i]);
        uart_write_string(" ");
    }
}

void application_rx_run(void)
{
    uint8_t payload[OOK_PACKET_MAX_PAYLOAD];
    char print_buffer[OOK_PACKET_MAX_PAYLOAD + 1u];

    while (1) {
        uint8_t length = 0u;
        uint8_t received_crc = 0u;
        uint8_t expected_crc = 0u;
        int result = rx_receive_packet(payload, sizeof(payload), &length, &received_crc, &expected_crc);

        switch (result) {
        case RX_PACKET_OK:
            for (uint8_t i = 0u; i < length; i++) {
                uint8_t byte = payload[i];
                print_buffer[i] = ((byte >= 0x20u) && (byte < 0x7Fu)) ? (char)byte : '.';
            }
            print_buffer[length] = '\0';
            uart_write_string("RX: ");
            uart_write_string(print_buffer);
            uart_write_string("\r\n");
            break;
        case RX_PACKET_ERR_SYNC:
            uart_write_string("RX error: sync not found.\r\n");
            break;
        case RX_PACKET_ERR_LENGTH:
            uart_write_string("RX error: bad length=");
            rx_print_hex_byte(length);
            uart_write_string("\r\n");
            break;
        case RX_PACKET_ERR_CRC:
            uart_write_string("RX error: CRC mismatch. len=");
            rx_print_hex_byte(length);
            uart_write_string(" crc_rx=");
            rx_print_hex_byte(received_crc);
            uart_write_string(" crc_calc=");
            rx_print_hex_byte(expected_crc);
            uart_write_string(" payload=");
            rx_print_payload_hex(payload, length);
            uart_write_string("\r\n");
            break;
        default:
            uart_write_string("RX error: unknown.\r\n");
            break;
        }
    }
}

#else

void application_rx_run(void)
{
    char bit_buffer[9];

    while (1) {
        rx_wait_for_pattern_start();
        HAL_Delay(OOK_PATTERN_SYMBOL_MS / 2u);

        for (uint32_t bit_index = 0u; bit_index < 8u; bit_index++) {
            uint16_t span = rx_test_measure_span_for_ms(RX_CENTER_SAMPLE_MS);
            uint8_t carrier_present = (span >= RX_TEST_SPAN_THRESHOLD_COUNTS) ? 1u : 0u;

            if (carrier_present != 0u) {
                board_led_on();
            } else {
                board_led_off();
            }

            bit_buffer[bit_index] = carrier_present != 0u ? '1' : '0';

            if (bit_index != 7u) {
                HAL_Delay(OOK_PATTERN_SYMBOL_MS - RX_CENTER_SAMPLE_MS);
            }
        }

        bit_buffer[8] = '\0';
        uart_write_string(bit_buffer);
        uart_write_string("\r\n");
    }
}

#endif
