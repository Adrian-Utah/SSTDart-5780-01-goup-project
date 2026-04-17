# SSTDart-5780-01-goup-project

Adrian Webb -Commit <- i am still dumb

Jackson Brough

Sam Makin

3/23/26 edits:
# SSTDart STM32 Bring-Up

The project can now be built and flashed to the STM32 board.

A simple LED startup sequence has been added to verify that the board was programmed successfully and that the code is running after startup.

Initial DAC setup has also been added, but the DAC output still needs to be tested on hardware.

4/10/26 updates:
# SSTDart DDS status

Current DDS wiring used for the working bench setup:

- `PA5 -> DDS CLK`
- `PB7 -> DDS DAT`
- `PB6 -> DDS FSY`
- `PA4 -> DDS CS` (only needed when forcing the module amplitude setting)
- `3V3 -> DDS VCC`
- `GND -> DDS GND`

Current firmware behavior:

- the DDS is responding to `Set_Frequency(...)`
- `ACTIVE_SYSTEM_STATE` in [Src/main.c](/C:/Users/hotdo/OneDrive/Desktop/ECE/5780_project/SSTDart-5780-01-goup-project/Src/main.c) selects among the three top-level firmware modes described below
- reflectometry sweep capture uses `100` points from `0 Hz` to `12 MHz` with `0.37 s` dwell per point
- UART3 sends `frequency magnitude` lines during sweeps
- the magnitude values come from averaged ADC readings on `PA1`

Firmware mode selection:

- `SYSTEM_STATE_CAPTURE_SWEEPS` captures four reflectometry sweeps in this order: `OPEN`, `SHORT`, `MATCH`, `ANTENNA`
- `SYSTEM_STATE_CHECK_THEN_TRANSMIT` runs one antenna-health sweep, compares the measured curve against the built-in expected antenna curve with a `5%` tolerance band, and only unlocks the transmit path if the sweep passes
- `SYSTEM_STATE_BYPASS_TO_TRANSMIT` skips the antenna-health check and immediately allows the team transmit path to run
- change `ACTIVE_SYSTEM_STATE` near the top of `Src/main.c` to switch modes, then rebuild and flash
- to bypass the antenna check for transmitter-only testing, set `ACTIVE_SYSTEM_STATE` to `SYSTEM_STATE_BYPASS_TO_TRANSMIT`

Note for Adrian:

- use `Set_Frequency(freq_hz)` to drive the DDS directly for radio-side testing
- if you want the sweep capture session, set `ACTIVE_SYSTEM_STATE` to `SYSTEM_STATE_CAPTURE_SWEEPS`
- if you want the guarded transmit test, set `ACTIVE_SYSTEM_STATE` to `SYSTEM_STATE_CHECK_THEN_TRANSMIT`
- if you want to bypass reflectometry hardware and let the transmit code run immediately, set `ACTIVE_SYSTEM_STATE` to `SYSTEM_STATE_BYPASS_TO_TRANSMIT`
- UART output is there now so the radio/host side can consume the same frequency-tagged stream during reflectometry testing

Bench note from 4/10/26:

- we reached a working DDS sweep and UART reporting path today
- output amplitude is still the main blocker for the downstream RF chain
- we are waiting on a proper RF attenuator so the amplifier/splitter/coupler/mixer path can be tested at controlled levels without overdriving the next stages
