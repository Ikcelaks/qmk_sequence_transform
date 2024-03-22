// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
#pragma once

#define ST_PBIT_NONALPHA 0x01
#define ST_PBIT_DIGIT 0x02
#define ST_PBIT_ALPHA 0x04
#define ST_PBIT_UPPERALPHA 0x08
#define ST_PBIT_PUNCT_TERMINILNAL 0x10
#define ST_PBIT_PUNCT_CONNECTING 0x20

bool        st_is_seq_token_keycode(uint16_t keycode, uint16_t kc_seq_token_0);
bool        st_is_seq_token_triecode(uint8_t triecode);
char        st_get_seq_token_ascii(uint8_t triecode);
bool        st_is_seq_pred_triecode(uint8_t triecode);
char        st_triecode_to_ascii( uint8_t triecode);
uint8_t     st_keycode_to_triecode(uint16_t keycode, uint16_t kc_seq_token_0);
char        st_keycode_to_ascii(uint16_t keycode);
uint16_t    st_ascii_to_keycode(uint8_t triecode);
bool        st_match_triecode(uint8_t triecode, st_key_t key);

#ifdef ST_TESTER
uint16_t    st_triecode_to_keycode(uint8_t triecode, uint16_t kc_seq_token_0);
const char *st_get_seq_token_utf8(uint8_t triecode);
const char *st_get_seq_pred_utf8(uint8_t triecode);
uint16_t    st_test_ascii_to_keycode(char c);
#endif
