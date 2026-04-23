#include "application_tx.h"

#include "app_config.h"
#include "dds.h"
#include "reflectometry.h"
#include "uart.h"

void application_tx_init(void)
{
    dds_init();

    uart_write_string("PROGRAM STARTED\r\n");

#if ACTIVE_TX_MODE == TX_MODE_CAPTURE_SWEEPS
    reflectometry_init();
    uart_write_string("MODE: CAPTURE SWEEPS\r\n");
    reflectometry_prepare_capture_session();
#elif ACTIVE_TX_MODE == TX_MODE_CHECK_THEN_TRANSMIT
    reflectometry_init();
    uart_write_string("MODE: CHECK ANTENNA THEN TRANSMIT\r\n");
    reflectometry_prepare_guarded_transmit(0u);
#else
    uart_write_string("MODE: BYPASS ANTENNA CHECK\r\n");
    reflectometry_prepare_guarded_transmit(1u);
#endif
}

void application_tx_run(void)
{
#if ACTIVE_TX_MODE == TX_MODE_CAPTURE_SWEEPS
    while (1) {
        reflectometry_run_capture_session();
    }
#elif ACTIVE_TX_MODE == TX_MODE_CHECK_THEN_TRANSMIT
    reflectometry_run_guarded_transmit(0u);
#else
    reflectometry_run_guarded_transmit(1u);
#endif
}
