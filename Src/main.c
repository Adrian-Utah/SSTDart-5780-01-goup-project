#include "main.h"
#include "dac.h"

#define LED_PIN 9u   // green LED

void SystemClock_Config(void)
{
}

static void delay_loop(volatile uint32_t t)
{
    while (t--) {
        __asm__("nop");
    }
}

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

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    // Initialize LED and DAC hardware
    green_led_init();
    dac_init();

    // Blink twice to show program loaded
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

    // Leave LED on after successful startup
    green_led_on();

    // NOTE: need to verify DAC is working
    while (1) {
        dac_write(512);
        delay_loop(200000);

        dac_write(2048);
        delay_loop(200000);

        dac_write(3584);
        delay_loop(200000);
    }
}