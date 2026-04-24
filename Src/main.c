#include "app_config.h"
#include "application_rx.h"
#include "application_tx.h"
#include "board.h"
#include "dds.h"
#include "reflectometry.h"
#include "stm32f0xx_hal.h"
#include "uart.h"

/*
  Combined Reflect+OOK firmware branch.

  Change ACTIVE_APPLICATION_MODE in Inc/app_config.h to choose one of:
  - APPLICATION_MODE_REFLECTOMETRY:
    Runs one reflectometry capture session, then starts the OOK transmit app.
  - APPLICATION_MODE_OOK_TRANSMIT:
    Runs the OOK transmit message path from the OOK branch.
  - APPLICATION_MODE_OOK_RECEIVE:
    Runs the OOK receive/decode message path from the OOK branch.

  DDS wiring:
  PA5 -> DDS CLK
  PB7 -> DDS DAT
  PB6 -> DDS FSY
  PA4 -> DDS CS

  Reflectometry ADC input:
  PA1 -> detector / IF filter output

  UART3 wiring:
  PC10 -> USB-UART RX
  PC11 -> USB-UART TX
  GND  -> USB-UART GND
*/

int main(void)
{
    HAL_Init();
    board_init();
    uart_init();

#if ACTIVE_APPLICATION_MODE == APPLICATION_MODE_REFLECTOMETRY
    dds_init();
    reflectometry_init();
    uart_write_string("PROGRAM STARTED\r\n");
    uart_write_string("MODE: REFLECTOMETRY THEN OOK TRANSMIT\r\n");
    reflectometry_prepare_capture_session();
    reflectometry_run_capture_session();
    uart_write_string("REFLECTOMETRY COMPLETE - STARTING OOK TRANSMIT\r\n");
    application_tx_init();
    application_tx_run();
#elif ACTIVE_APPLICATION_MODE == APPLICATION_MODE_OOK_TRANSMIT
    application_tx_init();
    application_tx_run();
#else
    application_rx_init();
    application_rx_run();
#endif

    while (1) {
    }
}
