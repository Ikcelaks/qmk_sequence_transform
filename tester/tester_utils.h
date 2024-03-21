// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
#pragma once

const char  *st_get_seq_token_symbol(uint8_t triecode);
void        keycodes_to_utf8_str(const uint16_t *keycodes, char *str);
void        triecodes_to_utf8_str(const uint8_t *triecodes, char *str);
void        keycodes_to_ascii_str(const uint16_t *keycodes, char *str);
void        triecodes_to_ascii_str(const uint8_t *triecodes, char *str);
uint16_t    ascii_to_keycode(char c);
char        *ltrim_str(char *str);
