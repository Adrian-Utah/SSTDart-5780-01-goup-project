#include "application_rx.h"

#include <stdio.h>

#include "app_config.h"
#include "board.h"
#include "stm32f0xx_hal.h"
#include "uart.h"

#define RX_TEST_ADC_INPUT_PIN GPIO_PIN_1
#define RX_TEST_ADC_INPUT_CHANNEL ADC_CHANNEL_1

static ADC_HandleTypeDef hadc;
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
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    ADC_ChannelConfTypeDef config = {0};

    gpio.Pin = RX_TEST_ADC_INPUT_PIN;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    hadc.Instance = ADC1;
    hadc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
    hadc.Init.Resolution = ADC_RESOLUTION_12B;
    hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
    hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc.Init.LowPowerAutoWait = DISABLE;
    hadc.Init.LowPowerAutoPowerOff = DISABLE;
    hadc.Init.ContinuousConvMode = DISABLE;
    hadc.Init.DiscontinuousConvMode = DISABLE;
    hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc.Init.DMAContinuousRequests = DISABLE;
    hadc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    HAL_ADC_Init(&hadc);

    config.Channel = RX_TEST_ADC_INPUT_CHANNEL;
    config.Rank = ADC_RANK_CHANNEL_NUMBER;
    config.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc, &config);

    HAL_ADCEx_Calibration_Start(&hadc);
}

static uint16_t rx_test_adc_read_once(void)
{
    HAL_ADC_Start(&hadc);
    HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY);
    uint16_t value = (uint16_t)HAL_ADC_GetValue(&hadc);
    HAL_ADC_Stop(&hadc);
    return value;
}

void application_rx_init(void)
{
    rx_test_adc_init();
    uart_write_string("PROGRAM STARTED\r\n");
    uart_write_string("MODE: RX APPLICATION\r\n");
    uart_write_string("Waiting for a gap, then center-sampling 8 OOK bits on PA1.\r\n");
    rx_wait_for_confirmation();
}

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
