// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifndef ST_TESTER

// If a .c file needs a QMK include, this should add it here,
// and include "qmk_wrapper.h" instead.
#include "quantum.h"
#include "print.h"
#include "send_string.h"

#if SEQUENCE_TRANSFORM_LOG_TIME
#   define st_log_time(F) { \
               const uint32_t t = timer_read32(); \
               F; \
               uprintf("%s time: %lu\n", #F, timer_elapsed32(t)); \
           }
#   define st_log_time_with_result(F, R) { \
               const uint32_t t = timer_read32(); \
               *R = F; \
               uprintf("%s time: %lu\n", #F, timer_elapsed32(t)); \
           }
#else
#   define st_log_time(F) F;
#   define st_log_time_with_result(F, R) { \
                *R = F; \
            }
#endif

#else

// These includes don't require QMK objects to be linked with tester
#include <stdio.h>
#include <string.h>
#include "quantum_keycodes.h"
#include "progmem.h"

#define TEST_KC_SEQ_TOKEN_0 0x7E40

#define uprintf printf
#define st_log_time(F) F;
#define st_log_time_with_result(F, R) { \
                *R = F; \
        }

#define TAPPING_TERM 200
#define FAST_TIMER_T_SIZE 0

// WARNING: these do not prevent double eval of args!
// Only use these in preproc statements!
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

extern const uint8_t ascii_to_shift_lut[16];
extern const uint8_t ascii_to_keycode_lut[128];

uint8_t get_oneshot_mods(void);
void    set_oneshot_mods(uint8_t mods);
void    clear_oneshot_mods(void);
uint8_t get_mods(void);
void    tap_code16(uint16_t k);

#endif // ST_TESTER
