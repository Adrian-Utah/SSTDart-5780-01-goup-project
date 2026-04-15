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

Note for Adrian:
- Drive the DDS with Set_Frequency(freq_hz).
- Press the user button to run the sweep from 5 MHz to 10 MHz.
- During the sweep, UART3 prints "frequency magnitude" lines with fake magnitude
  values as placeholders for the radio-side processing path.
- If amplitude control causes trouble on a bench setup, comment out
  AD5227_Set_Amplitude(63u) below and leave DDS CS disconnected.


Note by Adrian: 
commented out sweep for OOK tests.

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
#define SWEEP_START_HZ 5000000u
#define SWEEP_END_HZ 10000000u
#define SWEEP_STEPS 64u
#define SWEEP_DELAY_MS 500u

UART_HandleTypeDef huart3;

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

static void run_sweep(void)
{
    uint32_t step_hz = (SWEEP_END_HZ - SWEEP_START_HZ) / (SWEEP_STEPS - 1u);

    led_on();

    for (uint32_t i = 0u; i < SWEEP_STEPS; i++) {
        uint32_t freq_hz = SWEEP_START_HZ + (i * step_hz);
        uint32_t magnitude = i * (4095u / (SWEEP_STEPS - 1u));

        if (i == (SWEEP_STEPS - 1u)) {
            freq_hz = SWEEP_END_HZ;
            magnitude = 4095u;
        }

        Set_Frequency(freq_hz);
        transmit_measurement(freq_hz, magnitude);
        HAL_Delay(SWEEP_DELAY_MS);
    }

    transmit_string("END\r\n");
    led_off();
}

int main(void)
{
    uint8_t was_pressed = 0u;

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

    AD5227_Set_Amplitude(40u); //NOTE: SET to 40u for OOK if not potental pin fry.
    //Set_Frequency(SWEEP_START_HZ);
    led_on();
    Set_Frequency(800000u);

    while (1) {
        //uint8_t pressed = button_is_pressed();

        //set_delay(2);
        DDSSendCharOOK('E',1);

        /*
        if (pressed && !was_pressed) {
            // HAL_Delay(40);
            
            
            if (button_is_pressed()) {
                run_sweep();

                while (button_is_pressed()) {
                    HAL_Delay(10);
                }
            }
                
        }
        

        was_pressed = pressed;
        HAL_Delay(10);
        */
    }
}
