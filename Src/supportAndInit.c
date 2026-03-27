/*
this file is for support and iniinitialization code
code in this file is intended to replace use of HAL with register level manipulations
this code is only authorized for use without permision by:
Adrian Webb
Jackson Brough
Sam Makin
all others must request authorization
*/

#include <stdint.h>
#include <stm32f072xb.h>
#include <stm32f0xx_hal.h>

// above includes are used for shortcut names and so it can know about the registors under manipulation


/* PrepRCCLED - enable LED RCC for use.
*
* no peram
*
* no return
*
*/
void PrepRCCLED(){
RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
}
/* PrepRCCOscillator - enable, and config Oscillator RCC for use.
* this is a register level verson of a prev code that relied on HAL
*
* ******NOTE: DO NOT SWITCH HSI IN ANY OTHER SECTION IT CAN AND WILL CAUSE ISSUES******
* ***Note: use PLL if a higher speed is desired and HPER for lower speeds***
* no peram
*
* no return
*
*/
void PrepRCCAndConfigOscillator(){
    //Enable High Speed oscillator
    RCC -> CR |= RCC_CR_HSION;
    //wait for HSI
    while (!(RCC->CR & RCC_CR_HSIRDY));


    //set calibration, clear, then set
    RCC -> CR &= ~(RCC_CR_HSITRIM); 
    RCC -> CR |= RCC_HSICALIBRATION_DEFAULT << RCC_CR_HSITRIM_BitNumber;

    //set flash latency to 0
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= FLASH_ACR_LATENCY_0;

    //set to DVI1
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE);

    //set HSI as system clock
    RCC->CFGR &= ~RCC_CFGR_SW;

    //Wait for the clock switch
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);
}
/* PrepRCCUART - enable UART RCC for use.
*
* no peram
*
* no return
*
*/
void PrepRCCUART(){
  RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
}
/* PrepConfigLED - prep LEDs for use
* no peram
*
* no return
*
*/
void PrepConfigLED(){
    //clear then set MODER of LEDs to Output
    GPIOC->MODER &= ~(0xFF << 12);
    GPIOC->MODER |= (0x55 << 12);

    //set to low speed
    GPIOC->OSPEEDR &= ~(0xFF << 12);

    //set to floating (no up or down)
    GPIOC->PUPDR &= ~(0xFF << 12);

}
/* PrepConfigUART - configure UART for use.
*
* no peram
*
* no return
*
*/
void PrepConfigUART(){
    //set MODER to alt function;
    GPIOC->MODER |= GPIO_MODER_MODER10_1;
    GPIOC->MODER |= GPIO_MODER_MODER11_1;
    GPIOC->MODER &= ~GPIO_MODER_MODER10_0;
    GPIOC->MODER &= ~GPIO_MODER_MODER11_0;
    //set OTYPER to push pull (redundent to the default)
    GPIOC->OTYPER &= ~(GPIO_OTYPER_OT_10 | GPIO_OTYPER_OT_11);
    //set OSPEEDR to low (redundent to the default)
    GPIOC->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEEDR10 | GPIO_OSPEEDR_OSPEEDR11);
    //set to No Pull-up/Pull-down (redundent to the default)
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPDR10 | GPIO_PUPDR_PUPDR11);
    //set Alternate Function to AF1
    GPIOC->AFR[1] &= ~((0xF << 8) | (0xF << 12));
    GPIOC->AFR[1] |=  ((1 << 8) | (1 << 12));

    //set baud rate
    USART3->BRR = 0x45;

    //Enable TX, Enable RX, and Enable RX Interrupt
    USART3->CR1 |= USART_CR1_TE;
    USART3->CR1 |= USART_CR1_RE;
    USART3->CR1 |= USART_CR1_RXNEIE;
    //enable
    USART3->CR1 |= USART_CR1_UE;

    //NVIC Setup
    NVIC->IP[29] = (1 << 6);
    NVIC->ISER[0] = (1 << 29);

}

/* writep - write a pin from target GPIO (mostly for LEDs)
*
* ****WARNING NOT MANY GUARDRAILS ONLY SET WHAT YOU ARE CERTIAN YOU WANT TO SET****
*
* GPIOx: whitch GPIO to target
* GPIO_Pin: pin to write
* GPIO_PinState: state to write (on, off, Ect.)
*
* no return
*
*/
void writep(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState){
    // if s
    if (PinState == GPIO_PIN_SET) { //IE 1
        //use first half to "set" on (our use case)
        GPIOx->BSRR = (uint32_t)GPIO_Pin;
    } else {
        //use second half to "reset" off (our use case)
        GPIOx->BSRR = (uint32_t)GPIO_Pin << 16U;
    }
}

/* togglep - toggle a pin from target GPIO (mostly for LEDs)
*
* GPIOx: whitch GPIO to target
* GPIO_Pin: pin to toggle
*
* no return
*
*/
void togglep(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin){
    //use Output Data Register to get the state of the specified GPIO
    uint32_t odr = GPIOx->ODR;
    //toggle Bit Set Reset Register of specified pin
    GPIOx->BSRR = ((odr & GPIO_Pin) << 16U) | (~odr & GPIO_Pin);
}