#pragma once

#include <stdint.h>

void board_init(void);
void board_led_on(void);
void board_led_off(void);
uint8_t board_button_is_pressed(void);
void board_wait_for_button_press(void);
void board_wait_for_button_release(void);
