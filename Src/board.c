#include "board.h"

#include "stm32f0xx_hal.h"

#define BOARD_LED_PIN 7u
#define BOARD_USER_BUTTON_PIN 0u

static void board_clock_init(void)
{
    // Enable high speed oscillator
    RCC->CR |= RCC_CR_HSION;

    // Wait for HSI
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) {
    }

    // Set calibration, clear, then set
    RCC->CR &= ~(RCC_CR_HSITRIM);
    RCC->CR |= (RCC_HSICALIBRATION_DEFAULT << RCC_CR_HSITRIM_BitNumber);

    // Set flash latency to 0, wait states
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    // Set to DIV1
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE);
    // Set HSI as system clock
    RCC->CFGR &= ~RCC_CFGR_SW;

    // Wait for the clock switch
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {
    }
}

static void board_led_init(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    // clear then set MODER of LEDs to Output
    GPIOC->MODER &= ~(3u << (BOARD_LED_PIN * 2u));
    GPIOC->MODER |= (1u << (BOARD_LED_PIN * 2u));
    // set to low speed
    GPIOC->OSPEEDR &= ~(0xFF << 12);
    // set to floating (no up or down)
    GPIOC->PUPDR &= ~(0xFF << 12);
}

static void board_button_init(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER &= ~(3u << (BOARD_USER_BUTTON_PIN * 2u));
    GPIOA->PUPDR &= ~(3u << (BOARD_USER_BUTTON_PIN * 2u));
}

void board_init(void)
{
    board_clock_init();
    board_led_init();
    board_button_init();
    board_led_off();
}

void board_led_on(void)
{
    GPIOC->BSRR = (1u << BOARD_LED_PIN);
}

void board_led_off(void)
{
    GPIOC->BSRR = (1u << (BOARD_LED_PIN + 16u));
}

uint8_t board_button_is_pressed(void)
{
    return (GPIOA->IDR & (1u << BOARD_USER_BUTTON_PIN)) != 0u;
}

void board_wait_for_button_release(void)
{
    while (board_button_is_pressed()) {
        HAL_Delay(10);
    }
}

void board_wait_for_button_press(void)
{
    board_wait_for_button_release();

    while (!board_button_is_pressed()) {
        HAL_Delay(10);
    }

    HAL_Delay(40);
    while (!board_button_is_pressed()) {
        HAL_Delay(10);
    }
}
