#pragma once

#include <stdint.h>
#include "stm32f0xx.h"

void dac_init(void);
void dac_write(uint16_t value);