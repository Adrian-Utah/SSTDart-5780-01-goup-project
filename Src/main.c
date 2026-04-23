#include "app_config.h"
#include "application_rx.h"
#include "application_tx.h"
#include "board.h"
#include "stm32f0xx_hal.h"
#include "uart.h"

/*
  DDS wiring for the current known-good setup:
  PA5  -> DDS CLK
  PB7  -> DDS DAT
  PB6  -> DDS FSY
  PA4  -> DDS CS (amplitude control, optional)
  3V3  -> DDS VCC
  GND  -> DDS GND
  
  UART3 wiring for serial logging:
  PC10 -> USB-UART RX
  PC11 -> USB-UART TX
  GND  -> USB-UART GND
  3V3 logic only, 115200 baud
  
  ADC input for reflectometry sweep measurements:
  PA1  -> Detector / IF filter output
  GND  -> Measurement ground
  Input range must stay within 0V to 3.3V
  
  Note for Adrian:
  - Drive the DDS with Set_Frequency(freq_hz).
  - In reflectometry mode, press the user button to run the sweep from 0 Hz to 12 MHz.
  - During the sweep, UART3 prints "frequency magnitude" lines using averaged ADC
  readings from PA1 after the IF filter settles.
  - If amplitude control causes trouble on a bench setup, comment out
  AD5227_Set_Amplitude(...) below and leave DDS CS disconnected.
  
  Additional bench notes:
  - OOK testing uses the same DDS wiring plus the helper functions added in
  supportAndInit.c.
  - Switch ACTIVE_SYSTEM_STATE below to choose the overall firmware behavior.
  
  System state selection:
  - Set ACTIVE_SYSTEM_STATE to SYSTEM_STATE_CAPTURE_SWEEPS to capture the
  four reflectometry sweeps in this order: OPEN, SHORT, MATCH, ANTENNA.
  This mode is for logging and plotting comparison data only.
  - Set ACTIVE_SYSTEM_STATE to SYSTEM_STATE_CHECK_THEN_TRANSMIT to run one
  antenna health sweep first. If the measured antenna curve stays within
  the allowed tolerance band of the built-in expected curve, the firmware
  unlocks the existing OOK transmit path. If it fails, transmission stays
  locked and the user can retry the health sweep with the button.
  - Set ACTIVE_SYSTEM_STATE to SYSTEM_STATE_BYPASS_TO_TRANSMIT to skip the
  antenna health check entirely and immediately allow the transmit code to
  run. This is the quickest mode for radio-only team testing.
  
  To bypass the antenna check:
  - change "#define ACTIVE_SYSTEM_STATE SYSTEM_STATE_CAPTURE_SWEEPS"
  to "#define ACTIVE_SYSTEM_STATE SYSTEM_STATE_BYPASS_TO_TRANSMIT"
  and rebuild/flash the board.
  
  
  OOk period is 188u for the 2ms req
  OOk freq is 250000 //best for ADC
  pins for click:
  
  FSN -> PB6
  CS -> PA4
  SCK -> PA5
  SDI -> PB7
  OEN -> PC0
  3V3 -> 3V
  GND -> GND
*/

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
