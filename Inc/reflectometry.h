#pragma once

#include <stdint.h>

void reflectometry_init(void);
void reflectometry_prepare_capture_session(void);
void reflectometry_run_capture_session(void);
void reflectometry_prepare_guarded_transmit(uint8_t bypass_health_check);
void reflectometry_run_guarded_transmit(uint8_t bypass_health_check);
