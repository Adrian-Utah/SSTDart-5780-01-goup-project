#include "dac.h"

void dac_init(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_DACEN;

    GPIOA->MODER &= ~(3u << (4 * 2));
    GPIOA->MODER |=  (3u << (4 * 2));

    DAC->CR |= DAC_CR_EN1;
}

void dac_write(uint16_t value)
{
    value &= 0x0FFF;
    DAC->DHR12R1 = value;
}