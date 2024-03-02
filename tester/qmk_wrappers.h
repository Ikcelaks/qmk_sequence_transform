#pragma once

#include <stdio.h>
#include "quantum_keycodes.h"
#include "progmem.h"

#define uprintf printf

extern const uint8_t ascii_to_shift_lut[16];
extern const uint8_t ascii_to_keycode_lut[128];

uint8_t get_oneshot_mods(void);
void    set_oneshot_mods(uint8_t mods);
uint8_t get_mods(void);
void    tap_code16(uint16_t k);
