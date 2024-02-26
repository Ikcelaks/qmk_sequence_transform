#pragma once

#include <stdint.h>

#define IS_ALPHA_KEYCODE(code) ((code) >= KC_A && (code) <= KC_Z)

uint16_t char_to_keycode(char c);
char keycode_to_char(uint16_t keycode);
void multi_tap(uint16_t keycode, int count);
void send_key(uint16_t keycode);
