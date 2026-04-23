#include "app_config.h"
#include "application_rx.h"
#include "application_tx.h"
#include "board.h"
#include "stm32f0xx_hal.h"
#include "uart.h"

int main(void)
{
    HAL_Init();
    board_init();
    uart_init();

#if ACTIVE_APPLICATION_ROLE == APPLICATION_ROLE_TX
    application_tx_init();
    application_tx_run();
#else
    application_rx_init();
    application_rx_run();
#endif

    while (1) {
    }
}
