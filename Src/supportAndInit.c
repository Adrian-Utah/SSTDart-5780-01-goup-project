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
void PrepRCCLED(void){
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
}

/* PrepRCCGPIOAnB - enable GPIOA/GPIOB and SPI1 RCC for use.
*
* no peram
*
* no return
*
*/
void PrepRCCGPIOAnB(void){
    RCC->AHBENR |= (RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN);
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
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
void PrepRCCAndConfigOscillator(void){
    // Enable High Speed oscillator
    RCC->CR |= RCC_CR_HSION;

    // wait for HSI
    while (!(RCC->CR & RCC_CR_HSIRDY));

    // set calibration, clear, then set
    RCC->CR &= ~(RCC_CR_HSITRIM);
    RCC->CR |= (RCC_HSICALIBRATION_DEFAULT << RCC_CR_HSITRIM_BitNumber);

    // set flash latency to 0 wait states
    FLASH->ACR &= ~FLASH_ACR_LATENCY;

    // set to DIV1
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE);

    // set HSI as system clock
    RCC->CFGR &= ~RCC_CFGR_SW;

    // Wait for the clock switch
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);
}

/* PrepRCCUART - enable UART RCC for use.
*
* no peram
*
* no return
*
*/
void PrepRCCUART(void){
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
}

/* PrepConfigLED - prep LEDs for use
* no peram
*
* no return
*
*/
void PrepConfigLED(void){
    // clear then set MODER of LEDs to Output
    GPIOC->MODER &= ~(0xFF << 12);
    GPIOC->MODER |= (0x55 << 12);

    // set to low speed
    GPIOC->OSPEEDR &= ~(0xFF << 12);

    // set to floating (no up or down)
    GPIOC->PUPDR &= ~(0xFF << 12);
}

/* PrepConfigUART - configure UART for use.
*
* no peram
*
* no return
*
*/
void PrepConfigUART(void){
    // set MODER to alt function
    GPIOC->MODER |= GPIO_MODER_MODER10_1;
    GPIOC->MODER |= GPIO_MODER_MODER11_1;
    GPIOC->MODER &= ~GPIO_MODER_MODER10_0;
    GPIOC->MODER &= ~GPIO_MODER_MODER11_0;

    // set OTYPER to push pull
    GPIOC->OTYPER &= ~(GPIO_OTYPER_OT_10 | GPIO_OTYPER_OT_11);

    // set OSPEEDR to low
    GPIOC->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEEDR10 | GPIO_OSPEEDR_OSPEEDR11);

    // set to No Pull-up/Pull-down
    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPDR10 | GPIO_PUPDR_PUPDR11);

    // set Alternate Function to AF1
    GPIOC->AFR[1] &= ~((0xF << 8) | (0xF << 12));
    GPIOC->AFR[1] |=  ((1 << 8) | (1 << 12));

    // set baud rate
    USART3->BRR = 0x45;

    // Enable TX, Enable RX, and Enable RX Interrupt
    USART3->CR1 |= USART_CR1_TE;
    USART3->CR1 |= USART_CR1_RE;
    USART3->CR1 |= USART_CR1_RXNEIE;

    // enable UART
    USART3->CR1 |= USART_CR1_UE;

    // NVIC Setup
    NVIC->IP[29] = (1 << 6);
    NVIC->ISER[0] = (1 << 29);
}

/* PrepConfigSPI - configure SPI.
*
* no peram
*
* no return
*
*/
void PrepConfigSPI(void)
{
    // PA4 = amplitude CS output
    // PA5 = CLK output
    GPIOA->MODER &= ~(GPIO_MODER_MODER4 | GPIO_MODER_MODER5);
    GPIOA->MODER |= (GPIO_MODER_MODER4_0 | GPIO_MODER_MODER5_0);

    // PB6 = FSY output
    // PB7 = DAT output
    GPIOB->MODER &= ~(GPIO_MODER_MODER6 | GPIO_MODER_MODER7);
    GPIOB->MODER |= (GPIO_MODER_MODER6_0 | GPIO_MODER_MODER7_0);

    // Idle states
    GPIOA->BSRR = (1u << 4);   // amplitude CS high
    GPIOA->BSRR = (1u << 5);   // CLK high
    GPIOB->BRR  = (1u << 7);   // DAT low
    GPIOB->BSRR = (1u << 6);   // FSY high
}

/* PrepConfigSPI - configure OEN PIN -PC0.
*
* no peram
*
* no return
*
*/
void PrepConfigOEN() {
    // Set PC0 to Output
    GPIOC->MODER &= ~GPIO_MODER_MODER0;
    GPIOC->MODER |= GPIO_MODER_MODER0_0;
    
    // Start with output OFF (Low)
    GPIOC->BRR = (1u << 0); 
}

/* short_delay - delay for short time.
*
* no peram
*
* no return
*
*/
void short_delay(){
    for (volatile int i = 0; i < 20; i++) {
        __asm__("nop");
    }
}

/* short_delay - delay for short time.
*
* time - time to delay for
*
* no return
*
*/
void set_delay(uint32_t time){
    for(volatile int i = 0; i > time;){
        short_delay();
    }
}

/* AD9833_Write - writ to dds.
*
* data - data to send to DDS
*
* no return
*
*/

void AD9833_Write(uint16_t data)
{
    // Assume:
    // PA5 = CLK / SCLK
    // PB6 = FSY / FSYNC
    // PB7 = DAT / SDATA

    // Make sure clock is high before FSYNC goes low
    GPIOA->BSRR = (1u << 5);   // CLK high
    GPIOB->BSRR = (1u << 6);   // FSY high
    short_delay();

    // Start frame
    GPIOB->BRR = (1u << 6);    // FSY low
    short_delay();

    // Send 16 bits MSB first
    for (int i = 15; i >= 0; i--) {
        if (data & (1u << i)) {
            GPIOB->BSRR = (1u << 7);   // DAT high
        } else {
            GPIOB->BRR = (1u << 7);    // DAT low
        }

        short_delay();

        // Data shifts in on falling edge
        GPIOA->BRR = (1u << 5);        // CLK low
        short_delay();
        GPIOA->BSRR = (1u << 5);       // CLK high
        short_delay();
    }

    // End frame
    GPIOB->BSRR = (1u << 6);   // FSY high
    short_delay();
}

/* AD5227_Set_Amplitude - configure amplitude chip.
*
* target - target V mod
*
* no return
*
*/
void AD5227_Set_Amplitude(uint8_t target) {
    if (target > 63) target = 63;

    // New DDS module wiring:
    // PA4 = CS for amplitude control
    // PA5 = CLK
    // PB7 = DAT / U-D
    // PB6 = FSY, keep high while changing amplitude
    GPIOB->BSRR = (1u << 6);

    GPIOA->BRR = (1u << 4); // CS Low

    // Walk to Zero (Down)
    GPIOB->BRR = (1u << 7); // DAT/UD Low
    for (int i = 0; i < 64; i++) {
        GPIOA->BSRR = (1u << 5); // CLK High
        for (volatile int d = 0; d < 10; d++);
        GPIOA->BRR = (1u << 5);  // CLK Low
    }

    // Walk up to Target
    GPIOB->BSRR = (1u << 7); // DAT/UD High
    for (int i = 0; i < target; i++) {
        GPIOA->BSRR = (1u << 5); // CLK High
        for (volatile int d = 0; d < 10; d++);
        GPIOA->BRR = (1u << 5);  // CLK Low
    }

    GPIOA->BSRR = (1u << 4); // CS High (Lock)
    GPIOA->BSRR = (1u << 5); // CLK idle high for AD9833
    GPIOB->BRR = (1u << 7);  // DAT idle low
}

/* writep - write a pin from target GPIO (mostly for LEDs)
*
* ****WARNING NOT MANY GUARDRAILS ONLY SET WHAT YOU ARE CERTIAN YOU WANT TO SET****
*
* GPIOx: whitch GPIO to target]
]]
]

* GPIO_Pin: pin to write
* GPIO_PinState: state to write (on, off, Ect.)
*
* no return
*
*/
void writep(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState){
    if (PinState == GPIO_PIN_SET) {
        GPIOx->BSRR = (uint32_t)GPIO_Pin;
    } else {
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
    uint32_t odr = GPIOx->ODR;
    GPIOx->BSRR = ((odr & GPIO_Pin) << 16U) | (~odr & GPIO_Pin);
}




void Set_Frequency(uint32_t freq_hz)
{
    uint32_t freq_reg = (uint32_t)(((uint64_t)freq_hz << 28) / 25000000ULL);

    AD9833_Write(0x2100);                               // B28 = 1, RESET = 1
    AD9833_Write(0x4000 | (freq_reg & 0x3FFF));         // FREQ0 LSB 14 bits
    AD9833_Write(0x4000 | ((freq_reg >> 14) & 0x3FFF)); // FREQ0 MSB 14 bits
    AD9833_Write(0x2000);                               // leave reset, sine output

}

/* DDSOutOn - turn DDS on for OOK
*
* no peram
*
* no return
*
*/
void DDSOutOn(){
    AD9833_Write(0x2000);
    //GPIOC->BSRR = (1u << 0);
}

/* DDSOutOff - turn DDS off for OOK
*
* no peram
*
* no return
*
*/
void DDSOutOff(){
    //GPIOC->BRR = (1u << 0);
    AD9833_Write(0x2100);
}

/* DDSSendCharOOK - send char over
*
* data: char to send
* period: length of the period to use
*
* no return
*
*/
void DDSSendCharOOK(uint8_t data, uint16_t period){
    //send char as an OOK MSB first
    for(int i = 7; i>= 0; i--){
        if(data & (1<<i)){
            DDSOutOn();
        }
        else{
            DDSOutOff();
        }
        set_delay(period*8000);
    }
   

    
}
