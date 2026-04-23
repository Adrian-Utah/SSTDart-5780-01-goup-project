#include "application_tx.h"

#include "app_config.h"
#include "dds.h"
#include "ook.h"
#include "reflectometry.h"
#include "stm32f0xx_hal.h"
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
#elif ACTIVE_TX_MODE == TX_MODE_CARRIER_TEST
    uart_write_string("MODE: CARRIER TEST\r\n");
    dds_set_amplitude(OOK_TEST_AMPLITUDE);
    dds_set_frequency(OOK_TEST_FREQUENCY_HZ);
    dds_output_off();
    uart_write_string("TX will alternate carrier ON/OFF in long windows.\r\n");
#elif ACTIVE_TX_MODE == TX_MODE_UART_HEARTBEAT
    uart_write_string("MODE: UART HEARTBEAT\r\n");
    uart_write_string("Writing a UART heartbeat once per second.\r\n");
#elif ACTIVE_TX_MODE == TX_MODE_PATTERN_TEST
    uart_write_string("MODE: PATTERN TEST\r\n");
    dds_set_amplitude(OOK_TEST_AMPLITUDE);
    dds_set_frequency(OOK_TEST_FREQUENCY_HZ);
    dds_output_off();
    uart_write_string("TX will repeatedly send 0xAA as OOK bits.\r\n");
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
#elif ACTIVE_TX_MODE == TX_MODE_CARRIER_TEST
    while (1) {
        dds_output_on();
        HAL_Delay(OOK_CARRIER_TEST_ON_MS);
        dds_output_off();
        HAL_Delay(OOK_CARRIER_TEST_OFF_MS);
    }
#elif ACTIVE_TX_MODE == TX_MODE_UART_HEARTBEAT
    while (1) {
        uart_write_string("UART heartbeat\r\n");
        HAL_Delay(UART_HEARTBEAT_PERIOD_MS);
    }
#elif ACTIVE_TX_MODE == TX_MODE_PATTERN_TEST
    while (1) {
        ook_send_byte(OOK_PATTERN_TEST_BYTE, OOK_PATTERN_SYMBOL_MS * 8000u);
    }
#else
    reflectometry_run_guarded_transmit(1u);
#endif
}
