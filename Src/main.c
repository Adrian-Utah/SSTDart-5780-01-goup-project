#include <stdio.h>
#include "supportAndInit.h"
#include "main.h"
#include "dac.h"

#define LED_PIN         8u
#define SWEEP_START_HZ  1000u
#define SWEEP_END_HZ    100000u
#define SWEEP_STEPS     64u

// --- LED ---

static void green_led_init(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC->MODER &= ~(3u << (LED_PIN * 2));
    GPIOC->MODER |=  (1u << (LED_PIN * 2));
}

static void green_led_on(void)
{
    GPIOC->BSRR = (1u << LED_PIN);
}

static void green_led_off(void)
{
    GPIOC->BSRR = (1u << (LED_PIN + 16u));
}

static void delay_loop(volatile uint32_t t)
{
    while (t--) {
        __asm__("nop");
    }
}

// --- Clock ---

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

// --- UART ---

UART_HandleTypeDef huart3;

volatile uint8_t rx_data = 0;
volatile uint8_t rx_flag = 0;

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

static char receive_char(void)
{
    while (!rx_flag) {
    }
    rx_flag = 0;
    return (char)rx_data;
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

static void transmit_measurement(uint32_t freq, uint32_t mag)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%lu %lu\r\n", (unsigned long)freq, (unsigned long)mag);
    transmit_string(buf);
}

// --- Sweep ---

static void sweep(void)
{
    uint32_t step = (SWEEP_END_HZ - SWEEP_START_HZ) / (SWEEP_STEPS - 1u);

    for (uint32_t i = 0; i < SWEEP_STEPS; i++) {
        uint32_t freq = SWEEP_START_HZ + i * step;
        uint32_t mag  = i * (4095u / (SWEEP_STEPS - 1u));

        Set_Frequency(freq);
        delay_loop(50000);

        transmit_measurement(freq, mag);
    }

    transmit_string("END\r\n");
}

// --- Main ---

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    green_led_init();
    dac_init();
    usart3_init();

    PrepRCCGPIOAnC();
    PrepConfigSPI();

    green_led_off();
    delay_loop(300000);
    green_led_on();
    delay_loop(300000);
    green_led_off();
    delay_loop(300000);
    green_led_on();
    delay_loop(300000);
    green_led_off();
    delay_loop(300000);

    AD5227_Set_Amplitude(63);
    Set_Frequency(1000000);

    green_led_on();

    while (1) {
        switch (receive_char()) {
            case 's':
                sweep();
                break;

            default:
                transmit_string("Error: unknown command\r\n");
                break;
        }
    }
}