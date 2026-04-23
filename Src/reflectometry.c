#include "reflectometry.h"

#include <stdio.h>

#include "app_config.h"
#include "board.h"
#include "dds.h"
#include "ook.h"
#include "stm32f0xx_hal.h"
#include "uart.h"

#define REFLECTOMETRY_ADC_INPUT_PIN GPIO_PIN_1
#define REFLECTOMETRY_ADC_INPUT_CHANNEL ADC_CHANNEL_1

static ADC_HandleTypeDef hadc;

static void transmit_measurement(uint32_t freq_hz, uint32_t magnitude)
{
    char buffer[32];

    snprintf(buffer, sizeof(buffer), "%lu %lu\r\n",
        (unsigned long)freq_hz,
        (unsigned long)magnitude);
    uart_write_string(buffer);
}

static void transmit_health_summary(uint8_t passed, uint32_t fail_count)
{
    char buffer[64];

    snprintf(buffer, sizeof(buffer), "ANTENNA_%s FAIL_POINTS=%lu\r\n",
        passed ? "OK" : "BAD",
        (unsigned long)fail_count);
    uart_write_string(buffer);
}

static const char *load_label_for_sweep(uint32_t sweep_index)
{
    if (sweep_index == 1u) {
        return "OPEN";
    }
    if (sweep_index == 2u) {
        return "SHORT";
    }
    if (sweep_index == 3u) {
        return "MATCH";
    }
    if (sweep_index == 4u) {
        return "ANTENNA";
    }

    return "UNKNOWN";
}

static void transmit_sweep_prompt(uint32_t sweep_index)
{
    char buffer[48];

    snprintf(buffer, sizeof(buffer), "SET LOAD TO %s, THEN PRESS BUTTON (%lu/%u)\r\n",
        load_label_for_sweep(sweep_index),
        (unsigned long)sweep_index,
        REFLECTOMETRY_SWEEP_COUNT);
    uart_write_string(buffer);
}

static void transmit_sweep_start(uint32_t sweep_index)
{
    char buffer[48];

    snprintf(buffer, sizeof(buffer), "START %lu %s\r\n",
        (unsigned long)sweep_index,
        load_label_for_sweep(sweep_index));
    uart_write_string(buffer);
}

static void transmit_sweep_end(uint32_t sweep_index)
{
    char buffer[48];

    snprintf(buffer, sizeof(buffer), "END %lu %s\r\n",
        (unsigned long)sweep_index,
        load_label_for_sweep(sweep_index));
    uart_write_string(buffer);
}

static uint32_t adc_read_average(uint32_t sample_count)
{
    uint32_t total = 0u;

    for (uint32_t i = 0u; i < sample_count; i++) {
        HAL_ADC_Start(&hadc);
        HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY);
        total += HAL_ADC_GetValue(&hadc);
        HAL_ADC_Stop(&hadc);
    }

    return total / sample_count;
}

static uint16_t expected_health_magnitude_for_step(uint32_t step_index)
{
    static const uint32_t anchor_steps[] = {
        0u, 30u, 32u, 36u, 41u, 50u, 60u, 75u, 99u
    };
    static const uint16_t anchor_magnitudes[] = {
        0u, 0u, 24u, 50u, 53u, 45u, 31u, 20u, 15u
    };

    for (uint32_t i = 0u; i < ((sizeof(anchor_steps) / sizeof(anchor_steps[0])) - 1u); i++) {
        uint32_t left_step = anchor_steps[i];
        uint32_t right_step = anchor_steps[i + 1u];
        uint16_t left_magnitude = anchor_magnitudes[i];
        uint16_t right_magnitude = anchor_magnitudes[i + 1u];

        if (step_index <= right_step) {
            uint32_t span = right_step - left_step;
            uint32_t offset = step_index - left_step;
            int32_t delta = (int32_t)right_magnitude - (int32_t)left_magnitude;

            return (uint16_t)(left_magnitude + ((delta * (int32_t)offset) / (int32_t)span));
        }
    }

    return anchor_magnitudes[(sizeof(anchor_magnitudes) / sizeof(anchor_magnitudes[0])) - 1u];
}

static void collect_sweep(uint16_t magnitudes[REFLECTOMETRY_SWEEP_STEPS], uint8_t emit_uart, uint32_t sweep_index)
{
    uint32_t step_hz =
        (REFLECTOMETRY_SWEEP_END_HZ - REFLECTOMETRY_SWEEP_START_HZ) /
        (REFLECTOMETRY_SWEEP_STEPS - 1u);

    board_led_on();
    if (emit_uart != 0u) {
        transmit_sweep_start(sweep_index);
    }

    for (uint32_t i = 0u; i < REFLECTOMETRY_SWEEP_STEPS; i++) {
        uint32_t freq_hz = REFLECTOMETRY_SWEEP_START_HZ + (i * step_hz);

        if (i == (REFLECTOMETRY_SWEEP_STEPS - 1u)) {
            freq_hz = REFLECTOMETRY_SWEEP_END_HZ;
        }

        dds_set_frequency(freq_hz);
        HAL_Delay(REFLECTOMETRY_SWEEP_DELAY_MS);
        magnitudes[i] = (uint16_t)adc_read_average(REFLECTOMETRY_ADC_SAMPLES_PER_STEP);

        if (emit_uart != 0u) {
            transmit_measurement(freq_hz, magnitudes[i]);
        }
    }

    if (emit_uart != 0u) {
        transmit_sweep_end(sweep_index);
    }
    board_led_off();
}

static uint8_t antenna_health_is_ok(void)
{
    uint16_t measured[REFLECTOMETRY_SWEEP_STEPS];
    uint32_t fail_count = 0u;

    collect_sweep(measured, 1u, 4u);

    for (uint32_t i = 0u; i < REFLECTOMETRY_SWEEP_STEPS; i++) {
        uint16_t expected = expected_health_magnitude_for_step(i);
        uint16_t tolerance = HEALTH_MIN_TOLERANCE_COUNTS;
        uint16_t measured_value = measured[i];
        uint16_t difference = (measured_value > expected)
            ? (measured_value - expected)
            : (expected - measured_value);

        if ((((uint32_t)expected * HEALTH_TOLERANCE_PERCENT) / 100u) > tolerance) {
            tolerance = (uint16_t)(((uint32_t)expected * HEALTH_TOLERANCE_PERCENT) / 100u);
        }

        if (difference > tolerance) {
            fail_count++;
        }
    }

    transmit_health_summary(fail_count <= HEALTH_MAX_ALLOWED_FAILS, fail_count);
    return fail_count <= HEALTH_MAX_ALLOWED_FAILS;
}

void reflectometry_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    ADC_ChannelConfTypeDef config = {0};

    gpio.Pin = REFLECTOMETRY_ADC_INPUT_PIN;
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

    config.Channel = REFLECTOMETRY_ADC_INPUT_CHANNEL;
    config.Rank = ADC_RANK_CHANNEL_NUMBER;
    config.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc, &config);

    HAL_ADCEx_Calibration_Start(&hadc);
}

void reflectometry_prepare_capture_session(void)
{
    uart_write_string("FOUR-SWEEP SESSION READY: OPEN, SHORT, MATCH, ANTENNA\r\n");
    transmit_sweep_prompt(1u);
    dds_set_amplitude(REFLECTOMETRY_AMPLITUDE);
    dds_set_frequency(REFLECTOMETRY_SWEEP_START_HZ);
    board_led_off();
}

void reflectometry_run_capture_session(void)
{
    uint16_t magnitudes[REFLECTOMETRY_SWEEP_STEPS];

    for (uint32_t sweep_index = 1u; sweep_index <= REFLECTOMETRY_SWEEP_COUNT; sweep_index++) {
        transmit_sweep_prompt(sweep_index);
        board_wait_for_button_press();
        collect_sweep(magnitudes, 1u, sweep_index);
        board_wait_for_button_release();
    }

    uart_write_string("SESSION COMPLETE\r\n");
}

void reflectometry_prepare_guarded_transmit(uint8_t bypass_health_check)
{
    if (bypass_health_check != 0u) {
        dds_set_amplitude(OOK_TEST_AMPLITUDE);
        dds_set_frequency(OOK_TEST_FREQUENCY_HZ);
        uart_write_string("ANTENNA CHECK BYPASSED\r\n");
        uart_write_string("TRANSMIT ENABLED\r\n");
        board_led_on();
    } else {
        dds_set_amplitude(REFLECTOMETRY_AMPLITUDE);
        dds_set_frequency(REFLECTOMETRY_SWEEP_START_HZ);
        uart_write_string("PRESS BUTTON TO RUN ANTENNA HEALTH CHECK\r\n");
        board_led_off();
    }
}

void reflectometry_run_guarded_transmit(uint8_t bypass_health_check)
{
    uint8_t antenna_ok = bypass_health_check;

    while (1) {
        if (antenna_ok == 0u) {
            board_wait_for_button_press();
            uart_write_string("RUNNING ANTENNA HEALTH SWEEP\r\n");
            dds_set_amplitude(REFLECTOMETRY_AMPLITUDE);
            dds_set_frequency(REFLECTOMETRY_SWEEP_START_HZ);
            antenna_ok = antenna_health_is_ok();
            board_wait_for_button_release();

            if (antenna_ok != 0u) {
                dds_set_amplitude(OOK_TEST_AMPLITUDE);
                dds_set_frequency(OOK_TEST_FREQUENCY_HZ);
                uart_write_string("TRANSMIT ENABLED\r\n");
                board_led_on();
            } else {
                uart_write_string("TRANSMIT LOCKED - FIX ANTENNA AND PRESS BUTTON TO RETRY\r\n");
                board_led_off();
            }
        } else {
            ook_send_byte(OOK_TEST_CHARACTER, OOK_SYMBOL_PERIOD_TICKS);
        }
    }
}
