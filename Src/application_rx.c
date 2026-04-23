#include "application_rx.h"

#include "stm32f0xx_hal.h"
#include "uart.h"

void application_rx_init(void)
{
    uart_write_string("PROGRAM STARTED\r\n");
    uart_write_string("MODE: RX APPLICATION\r\n");
    uart_write_string("OOK receive path is not implemented yet.\r\n");
}

void application_rx_run(void)
{
    while (1) {
        __WFI();
    }
}
