#pragma once

#include <stdint.h>

void dds_init(void);
void dds_set_frequency(uint32_t freq_hz);
void dds_set_amplitude(uint8_t target);
void dds_output_on(void);
void dds_output_off(void);
