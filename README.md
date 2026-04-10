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
- the user button starts a sweep from `5 MHz` to `10 MHz`
- the sweep uses `64` points with `0.5 s` dwell per point
- UART3 sends `frequency magnitude` lines during the sweep
- the magnitude values are currently fake placeholder data for integration work

Note for Adrian:

- use `Set_Frequency(freq_hz)` to drive the DDS directly for radio-side testing
- if you want the existing sweep, press the user button on the STM32 board
- UART output is there now so the radio/host side can consume the same frequency-tagged stream while real measurement hardware is still being integrated

Bench note from 4/10/26:

- we reached a working DDS sweep and UART reporting path today
- output amplitude is still the main blocker for the downstream RF chain
- we are waiting on a proper RF attenuator so the amplifier/splitter/coupler/mixer path can be tested at controlled levels without overdriving the next stages
