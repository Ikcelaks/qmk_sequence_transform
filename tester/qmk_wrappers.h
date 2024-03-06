#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifndef ST_TESTER

#include "quantum.h"
#include "print.h"
#include "send_string.h"

#ifdef SEQUENCE_TRANSFORM_LOG_TIME
#   define st_log_time(F) { \
               const int t = timer_read32(); \
               F; \
               uprintf(#F##" time: %d\n", timer_elapsed32(t)); \
           }            
#else
#   define st_log_time(F) F;
#endif

#else

// These includes don't require QMK objects to be linked with tester
#include <stdio.h>
#include <string.h>
#include "quantum_keycodes.h"
#include "progmem.h"

#define uprintf printf
#define st_log_time(F) F;

#define TAPPING_TERM 200

extern const uint8_t ascii_to_shift_lut[16];
extern const uint8_t ascii_to_keycode_lut[128];

uint8_t get_oneshot_mods(void);
void    set_oneshot_mods(uint8_t mods);
uint8_t get_mods(void);
void    tap_code16(uint16_t k);

#endif // ST_TESTER
