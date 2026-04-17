#include <stdio.h>
#include "supportAndInit.h"
#include "main.h"

/*
DDS wiring for the current known-good setup:
PA5  -> DDS CLK
PB7  -> DDS DAT
PB6  -> DDS FSY
PA4  -> DDS CS (amplitude control, optional)
3V3  -> DDS VCC
GND  -> DDS GND

UART3 wiring for serial logging:
PC10 -> USB-UART RX
PC11 -> USB-UART TX
GND  -> USB-UART GND
3V3 logic only, 115200 baud

ADC input for reflectometry sweep measurements:
PA1  -> Detector / IF filter output
GND  -> Measurement ground
Input range must stay within 0V to 3.3V

Note for Adrian:
- Drive the DDS with Set_Frequency(freq_hz).
- In reflectometry mode, press the user button to run the sweep from 0 Hz to 12 MHz.
- During the sweep, UART3 prints "frequency magnitude" lines using averaged ADC
  readings from PA1 after the IF filter settles.
- If amplitude control causes trouble on a bench setup, comment out
  AD5227_Set_Amplitude(...) below and leave DDS CS disconnected.

Additional bench notes:
- OOK testing uses the same DDS wiring plus the helper functions added in
  supportAndInit.c.
- Switch ACTIVE_SYSTEM_STATE below to choose the overall firmware behavior.

System state selection:
- Set ACTIVE_SYSTEM_STATE to SYSTEM_STATE_CAPTURE_SWEEPS to capture the
  four reflectometry sweeps in this order: OPEN, SHORT, MATCH, ANTENNA.
  This mode is for logging and plotting comparison data only.
- Set ACTIVE_SYSTEM_STATE to SYSTEM_STATE_CHECK_THEN_TRANSMIT to run one
  antenna health sweep first. If the measured antenna curve stays within
  the allowed tolerance band of the built-in expected curve, the firmware
  unlocks the existing OOK transmit path. If it fails, transmission stays
  locked and the user can retry the health sweep with the button.
- Set ACTIVE_SYSTEM_STATE to SYSTEM_STATE_BYPASS_TO_TRANSMIT to skip the
  antenna health check entirely and immediately allow the transmit code to
  run. This is the quickest mode for radio-only team testing.

To bypass the antenna check:
- change "#define ACTIVE_SYSTEM_STATE SYSTEM_STATE_CAPTURE_SWEEPS"
  to "#define ACTIVE_SYSTEM_STATE SYSTEM_STATE_BYPASS_TO_TRANSMIT"
  and rebuild/flash the board.

pins for click:

FSN -> PB6
CS -> PA4
SCK -> PA5
SDI -> PB7
OEN -> PC0
3V3 -> 3V
GND -> GND
*/

#define LED_PIN 7u
#define USER_BUTTON_PIN 0u
#define SWEEP_START_HZ 0u
#define SWEEP_END_HZ 12000000u
#define SWEEP_STEPS 100u
#define SWEEP_DELAY_MS 370u
#define SWEEP_COUNT 4u
#define ADC_SAMPLES_PER_STEP 16u
#define ADC_INPUT_PIN GPIO_PIN_1
#define ADC_INPUT_CHANNEL ADC_CHANNEL_1
#define SYSTEM_STATE_CAPTURE_SWEEPS 0u
#define SYSTEM_STATE_CHECK_THEN_TRANSMIT 1u
#define SYSTEM_STATE_BYPASS_TO_TRANSMIT 2u
#define ACTIVE_SYSTEM_STATE SYSTEM_STATE_CAPTURE_SWEEPS
#define REFLECTOMETRY_AMPLITUDE 63u
#define OOK_TEST_AMPLITUDE 40u
#define OOK_TEST_FREQUENCY_HZ 800000u
#define OOK_TEST_CHARACTER 'E'
#define OOK_TEST_PERIOD 1u
#define HEALTH_TOLERANCE_PERCENT 5u
#define HEALTH_MIN_TOLERANCE_COUNTS 3u
#define HEALTH_MAX_ALLOWED_FAILS 10u

UART_HandleTypeDef huart3;
ADC_HandleTypeDef hadc;

volatile uint8_t rx_data = 0;
volatile uint8_t rx_flag = 0;

static void led_init(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC->MODER &= ~(3u << (LED_PIN * 2));
    GPIOC->MODER |=  (1u << (LED_PIN * 2));
}

static void led_on(void)
{
    GPIOC->BSRR = (1u << LED_PIN);
}

static void led_off(void)
{
    GPIOC->BSRR = (1u << (LED_PIN + 16u));
}

static void button_init(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER &= ~(3u << (USER_BUTTON_PIN * 2u));
    GPIOA->PUPDR &= ~(3u << (USER_BUTTON_PIN * 2u));
}

static uint8_t button_is_pressed(void)
{
    return (GPIOA->IDR & (1u << USER_BUTTON_PIN)) != 0u;
}

static void usart3_init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_10 | GPIO_PIN_11;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF1_USART3;
    HAL_GPIO_Init(GPIOC, &gpio);

    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 115200;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);

    USART3->CR1 |= USART_CR1_RXNEIE;
    HAL_NVIC_SetPriority(USART3_4_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART3_4_IRQn);
}

static void transmit_char(char c)
{
    while (!(USART3->ISR & USART_ISR_TXE)) {
    }
    USART3->TDR = c;
}

static void transmit_string(const char *str)
{
    while (*str != '\0') {
        transmit_char(*str++);
    }
}

static void transmit_measurement(uint32_t freq_hz, uint32_t magnitude)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%lu %lu\r\n",
        (unsigned long)freq_hz,
        (unsigned long)magnitude);
    transmit_string(buf);
}

static void transmit_health_summary(uint8_t passed, uint32_t fail_count)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "ANTENNA_%s FAIL_POINTS=%lu\r\n",
        passed ? "OK" : "BAD",
        (unsigned long)fail_count);
    transmit_string(buf);
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
    char buf[48];
    const char *label = load_label_for_sweep(sweep_index);

    snprintf(buf, sizeof(buf), "SET LOAD TO %s, THEN PRESS BUTTON (%lu/%u)\r\n",
        label, (unsigned long)sweep_index, SWEEP_COUNT);
    transmit_string(buf);
}

static void transmit_sweep_start(uint32_t sweep_index)
{
    char buf[48];
    const char *label = load_label_for_sweep(sweep_index);

    snprintf(buf, sizeof(buf), "START %lu %s\r\n", (unsigned long)sweep_index, label);
    transmit_string(buf);
}

static void transmit_sweep_end(uint32_t sweep_index)
{
    char buf[48];
    const char *label = load_label_for_sweep(sweep_index);

    snprintf(buf, sizeof(buf), "END %lu %s\r\n", (unsigned long)sweep_index, label);
    transmit_string(buf);
}

static void adc1_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = ADC_INPUT_PIN;
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

    ADC_ChannelConfTypeDef config = {0};
    config.Channel = ADC_INPUT_CHANNEL;
    config.Rank = ADC_RANK_CHANNEL_NUMBER;
    config.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc, &config);

    HAL_ADCEx_Calibration_Start(&hadc);
}

static uint32_t adc1_read_average(uint32_t sample_count)
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

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    oscillator.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    oscillator.HSIState            = RCC_HSI_ON;
    oscillator.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    oscillator.PLL.PLLState        = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
        while (1) {
        }
    }

    RCC_ClkInitTypeDef clock = {0};
    clock.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    clock.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    clock.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clock.APB1CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&clock, FLASH_LATENCY_0) != HAL_OK) {
        while (1) {
        }
    }
}

static void wait_for_button_release(void)
{
    while (button_is_pressed()) {
        HAL_Delay(10);
    }
}

static void wait_for_button_press(void)
{
    wait_for_button_release();

    while (!button_is_pressed()) {
        HAL_Delay(10);
    }

    HAL_Delay(40);
    while (!button_is_pressed()) {
        HAL_Delay(10);
    }
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
        uint16_t left_mag = anchor_magnitudes[i];
        uint16_t right_mag = anchor_magnitudes[i + 1u];

        if (step_index <= right_step) {
            uint32_t span = right_step - left_step;
            uint32_t offset = step_index - left_step;
            int32_t delta = (int32_t)right_mag - (int32_t)left_mag;

            return (uint16_t)(left_mag + ((delta * (int32_t)offset) / (int32_t)span));
        }
    }

    return anchor_magnitudes[(sizeof(anchor_magnitudes) / sizeof(anchor_magnitudes[0])) - 1u];
}

static uint32_t collect_sweep(uint16_t magnitudes[SWEEP_STEPS], uint8_t emit_uart, uint32_t sweep_index)
{
    uint32_t step_hz = (SWEEP_END_HZ - SWEEP_START_HZ) / (SWEEP_STEPS - 1u);

    led_on();
    if (emit_uart) {
        transmit_sweep_start(sweep_index);
    }

    for (uint32_t i = 0u; i < SWEEP_STEPS; i++) {
        uint32_t freq_hz = SWEEP_START_HZ + (i * step_hz);

        if (i == (SWEEP_STEPS - 1u)) {
            freq_hz = SWEEP_END_HZ;
        }

        Set_Frequency(freq_hz);
        HAL_Delay(SWEEP_DELAY_MS);
        magnitudes[i] = (uint16_t)adc1_read_average(ADC_SAMPLES_PER_STEP);

        if (emit_uart) {
            transmit_measurement(freq_hz, magnitudes[i]);
        }
    }

    if (emit_uart) {
        transmit_sweep_end(sweep_index);
    }
    led_off();

    return step_hz;
}

static void run_four_sweep_session(void)
{
    uint16_t magnitudes[SWEEP_STEPS];

    for (uint32_t sweep_index = 1u; sweep_index <= SWEEP_COUNT; sweep_index++) {
        transmit_sweep_prompt(sweep_index);
        wait_for_button_press();
        collect_sweep(magnitudes, 1u, sweep_index);
        wait_for_button_release();
    }

    transmit_string("SESSION COMPLETE\r\n");
}

static uint8_t antenna_health_is_ok(void)
{
    uint16_t measured[SWEEP_STEPS];
    uint32_t fail_count = 0u;
    collect_sweep(measured, 1u, 4u);

    for (uint32_t i = 0u; i < SWEEP_STEPS; i++) {
        uint16_t expected = expected_health_magnitude_for_step(i);
        uint16_t tolerance = HEALTH_MIN_TOLERANCE_COUNTS;
        uint16_t measured_value = measured[i];
        uint16_t difference = (measured_value > expected)
            ? (measured_value - expected)
            : (expected - measured_value);

        if (((uint32_t)expected * HEALTH_TOLERANCE_PERCENT) / 100u > tolerance) {
            tolerance = (uint16_t)(((uint32_t)expected * HEALTH_TOLERANCE_PERCENT) / 100u);
        }

        if (difference > tolerance) {
            fail_count++;
        }
    }

    transmit_health_summary(fail_count <= HEALTH_MAX_ALLOWED_FAILS, fail_count);
    return fail_count <= HEALTH_MAX_ALLOWED_FAILS;
}

static void run_guarded_transmit_mode(uint8_t bypass_health_check)
{
    uint8_t antenna_ok = bypass_health_check;

    if (bypass_health_check) {
        AD5227_Set_Amplitude(OOK_TEST_AMPLITUDE);
        Set_Frequency(OOK_TEST_FREQUENCY_HZ);
        transmit_string("ANTENNA CHECK BYPASSED\r\n");
        transmit_string("TRANSMIT ENABLED\r\n");
        led_on();
    } else {
        AD5227_Set_Amplitude(REFLECTOMETRY_AMPLITUDE);
        Set_Frequency(SWEEP_START_HZ);
        transmit_string("PRESS BUTTON TO RUN ANTENNA HEALTH CHECK\r\n");
    }

    while (1) {
        if (!antenna_ok) {
            wait_for_button_press();
            transmit_string("RUNNING ANTENNA HEALTH SWEEP\r\n");
            AD5227_Set_Amplitude(REFLECTOMETRY_AMPLITUDE);
            Set_Frequency(SWEEP_START_HZ);
            antenna_ok = antenna_health_is_ok();
            wait_for_button_release();

            if (antenna_ok) {
                AD5227_Set_Amplitude(OOK_TEST_AMPLITUDE);
                Set_Frequency(OOK_TEST_FREQUENCY_HZ);
                transmit_string("TRANSMIT ENABLED\r\n");
                led_on();
            } else {
                transmit_string("TRANSMIT LOCKED - FIX ANTENNA AND PRESS BUTTON TO RETRY\r\n");
                led_off();
            }
        } else {
            DDSSendCharOOK(OOK_TEST_CHARACTER, OOK_TEST_PERIOD);
        }
    }
}

int main(void)
{
    HAL_Init();
    //SystemClock_Config();
    PrepRCCAndConfigOscillator();
    PrepRCCGPIOAnB();
    PrepRCCLED();
    led_init();
    button_init();
    usart3_init();
    PrepConfigSPI();
    //PrepConfigOEN();

#if ACTIVE_SYSTEM_STATE == SYSTEM_STATE_CAPTURE_SWEEPS
    adc1_init();
    transmit_string("PROGRAM STARTED\r\n");
    transmit_string("FOUR-SWEEP SESSION READY: OPEN, SHORT, MATCH, ANTENNA\r\n");
    transmit_sweep_prompt(1u);
    AD5227_Set_Amplitude(REFLECTOMETRY_AMPLITUDE);
    Set_Frequency(SWEEP_START_HZ);
    led_off();
#elif ACTIVE_SYSTEM_STATE == SYSTEM_STATE_CHECK_THEN_TRANSMIT
    adc1_init();
    transmit_string("PROGRAM STARTED\r\n");
    transmit_string("MODE: CHECK ANTENNA THEN TRANSMIT\r\n");
    AD5227_Set_Amplitude(REFLECTOMETRY_AMPLITUDE);
    Set_Frequency(SWEEP_START_HZ);
    led_off();
#else
    transmit_string("PROGRAM STARTED\r\n");
    transmit_string("MODE: BYPASS ANTENNA CHECK\r\n");
#endif

    while (1) {
#if ACTIVE_SYSTEM_STATE == SYSTEM_STATE_CAPTURE_SWEEPS
        run_four_sweep_session();
#elif ACTIVE_SYSTEM_STATE == SYSTEM_STATE_CHECK_THEN_TRANSMIT
        run_guarded_transmit_mode(0u);
#else
        run_guarded_transmit_mode(1u);
#endif
    }
}
