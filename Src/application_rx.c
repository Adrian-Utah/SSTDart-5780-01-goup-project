#include "application_rx.h"

#include <stdio.h>
#include <string.h>

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
#define RX_PACKET_ERR_FORMAT 2
#define RX_PACKET_ERR_CRC 3
#define RX_PACKET_ERR_TIMEOUT 4

typedef struct {
    uint8_t type;
    uint8_t length;
    uint8_t payload[OOK_FRAGMENT_MAX_PAYLOAD];
    uint8_t received_crc;
    uint8_t expected_crc;
} rx_packet_t;

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

static void rx_write_sanitized_bytes(const uint8_t *data, uint16_t length)
{
    for (uint16_t i = 0u; i < length; i++) {
        uint8_t byte = data[i];
        uart_write_char(((byte >= 0x20u) && (byte < 0x7Fu)) ? (char)byte : '.');
    }
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

static int rx_wait_for_pattern_start_until(uint32_t timeout_ms)
{
    uint32_t wait_start_tick = HAL_GetTick();
    uint32_t idle_start_tick = 0u;

    while ((HAL_GetTick() - wait_start_tick) < timeout_ms) {
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

    if ((HAL_GetTick() - wait_start_tick) >= timeout_ms) {
        return RX_PACKET_ERR_TIMEOUT;
    }

    while ((HAL_GetTick() - wait_start_tick) < timeout_ms) {
        uint16_t span = rx_test_measure_span_for_ms(RX_SYNC_POLL_MS);

        if (span >= RX_TEST_SPAN_THRESHOLD_COUNTS) {
            return RX_PACKET_OK;
        }
    }

    return RX_PACKET_ERR_TIMEOUT;
}

static int rx_receive_packet(rx_packet_t *packet, uint32_t timeout_ms)
{
    packet->type = 0u;
    packet->length = 0u;
    packet->received_crc = 0u;
    packet->expected_crc = 0u;

    if (timeout_ms == 0u) {
        rx_wait_for_pattern_start();
    } else {
        int wait_result = rx_wait_for_pattern_start_until(timeout_ms);
        if (wait_result != RX_PACKET_OK) {
            return wait_result;
        }
    }

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

    uint8_t control = rx_read_byte_at(&next_sample_tick);
    uint8_t type = (uint8_t)(control & PACKET_TYPE_MASK);
    uint8_t length = (uint8_t)(control & PACKET_LENGTH_MASK);
    uint8_t crc_input[3u + OOK_FRAGMENT_MAX_PAYLOAD];

    packet->type = type;
    crc_input[0] = control;

    if (type == PACKET_TYPE_START) {
        if (length != 0u) {
            return RX_PACKET_ERR_FORMAT;
        }

        crc_input[1] = rx_read_byte_at(&next_sample_tick);
        crc_input[2] = rx_read_byte_at(&next_sample_tick);
        packet->received_crc = rx_read_byte_at(&next_sample_tick);
        packet->expected_crc = packet_crc8(crc_input, 3u);

        if (packet->received_crc != packet->expected_crc) {
            return RX_PACKET_ERR_CRC;
        }

        packet->length = 2u;
        packet->payload[0] = crc_input[1];
        packet->payload[1] = crc_input[2];
        return RX_PACKET_OK;
    }

    if ((type != PACKET_TYPE_DATA) || (length == 0u) || (length > OOK_FRAGMENT_MAX_PAYLOAD)) {
        return RX_PACKET_ERR_FORMAT;
    }

    packet->length = length;
    for (uint8_t i = 0u; i < length; i++) {
        uint8_t byte = rx_read_byte_at(&next_sample_tick);
        crc_input[1u + i] = byte;
        packet->payload[i] = byte;
    }

    packet->received_crc = rx_read_byte_at(&next_sample_tick);
    packet->expected_crc = packet_crc8(crc_input, (uint16_t)(1u + length));
    if (packet->received_crc != packet->expected_crc) {
        return RX_PACKET_ERR_CRC;
    }

    return RX_PACKET_OK;
}

void application_rx_run(void)
{
    rx_packet_t packet;
    uint8_t message_buffer[RX_MESSAGE_BUFFER_SIZE];
    uint16_t expected_total_length = 0u;
    uint16_t bytes_received = 0u;
    uint8_t message_active = 0u;

    while (1) {
        int result = rx_receive_packet(&packet, message_active != 0u ? RX_MESSAGE_TIMEOUT_MS : 0u);

        switch (result) {
        case RX_PACKET_OK:
            if (packet.type == PACKET_TYPE_START) {
                expected_total_length = (uint16_t)((uint16_t)packet.payload[0]
                    | ((uint16_t)packet.payload[1] << 8));

                if (message_active != 0u) {
                    uart_write_string("RX error: message aborted by new start.\r\n");
                }

                if ((expected_total_length == 0u) || (expected_total_length > RX_MESSAGE_BUFFER_SIZE)) {
                    uart_write_string("RX error: message too large.\r\n");
                    message_active = 0u;
                    bytes_received = 0u;
                    expected_total_length = 0u;
                    break;
                }

                message_active = 1u;
                bytes_received = 0u;
                memset(message_buffer, 0, sizeof(message_buffer));
                uart_write_string("RX start: expecting ");
                {
                    char count_buffer[16];
                    snprintf(count_buffer, sizeof(count_buffer), "%u", expected_total_length);
                    uart_write_string(count_buffer);
                }
                uart_write_string(" bytes.\r\n");
                break;
            }

            if (message_active == 0u) {
                uart_write_string("RX error: data without active message.\r\n");
                break;
            }

            if ((uint16_t)(bytes_received + packet.length) > expected_total_length) {
                uart_write_string("RX error: fragment exceeds announced length.\r\n");
                message_active = 0u;
                bytes_received = 0u;
                expected_total_length = 0u;
                break;
            }

            memcpy(&message_buffer[bytes_received], packet.payload, packet.length);
            bytes_received = (uint16_t)(bytes_received + packet.length);

            uart_write_string("RX fragment: ");
            rx_write_sanitized_bytes(packet.payload, packet.length);
            uart_write_string("\r\n");

            if (bytes_received == expected_total_length) {
                uart_write_string("RX complete: ");
                rx_write_sanitized_bytes(message_buffer, bytes_received);
                uart_write_string("\r\n");
                message_active = 0u;
                bytes_received = 0u;
                expected_total_length = 0u;
            }
            break;
        case RX_PACKET_ERR_SYNC:
            uart_write_string("RX error: sync not found.\r\n");
            message_active = 0u;
            bytes_received = 0u;
            expected_total_length = 0u;
            break;
        case RX_PACKET_ERR_FORMAT:
            uart_write_string("RX error: bad packet format.\r\n");
            message_active = 0u;
            bytes_received = 0u;
            expected_total_length = 0u;
            break;
        case RX_PACKET_ERR_CRC:
            uart_write_string("RX error: CRC mismatch. type=");
            rx_print_hex_byte(packet.type);
            uart_write_string(" crc_rx=");
            rx_print_hex_byte(packet.received_crc);
            uart_write_string(" crc_calc=");
            rx_print_hex_byte(packet.expected_crc);
            uart_write_string("\r\n");
            message_active = 0u;
            bytes_received = 0u;
            expected_total_length = 0u;
            break;
        case RX_PACKET_ERR_TIMEOUT:
            uart_write_string("RX error: message timeout.\r\n");
            message_active = 0u;
            bytes_received = 0u;
            expected_total_length = 0u;
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
