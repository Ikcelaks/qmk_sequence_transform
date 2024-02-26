#pragma once

#include <stdint.h>

uint16_t char_to_keycode(char c);
char keycode_to_char(uint16_t keycode);
uint16_t normalize_keycode(uint16_t keycode);
void multi_tap(uint16_t keycode, int count);
