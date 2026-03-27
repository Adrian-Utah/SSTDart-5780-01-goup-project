#include "stm32f0xx_hal.h"

void HardFault_Handler(void) { while (1) {} }

void SysTick_Handler(void) {
    HAL_IncTick();
}

void HAL_MspInit(void) {
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void system_clock_config(void) {
    RCC_OscInitTypeDef oscillator = {0};
    oscillator.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    oscillator.HSIState            = RCC_HSI_ON;
    oscillator.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    oscillator.PLL.PLLState        = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
        while (1) {}
    }

    RCC_ClkInitTypeDef clock = {0};
    clock.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    clock.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    clock.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clock.APB1CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clock, FLASH_LATENCY_0) != HAL_OK) {
        while (1) {}
    }
}

void led_init(void) {
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
}

int main(void) {
    HAL_Init();
    system_clock_config();
    led_init();

    while (1) {
    }
}
