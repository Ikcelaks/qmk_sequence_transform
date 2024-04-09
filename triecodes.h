// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
#pragma once

bool        st_is_seq_token_keycode(uint16_t keycode, uint16_t kc_seq_token_0);
bool        st_is_seq_token_triecode(uint8_t triecode);
bool        st_is_seq_metachar_triecode(uint8_t triecode);
bool        st_is_trans_seq_ref_triecode(uint8_t triecode);
char        st_get_seq_token_ascii(uint8_t triecode);
char        st_triecode_to_ascii( uint8_t triecode);
uint8_t     st_keycode_to_triecode(uint16_t keycode, uint16_t kc_seq_token_0);
char        st_keycode_to_ascii(uint16_t keycode);
uint16_t    st_ascii_to_keycode(uint8_t triecode);
bool        st_match_triecode(uint8_t triecode, uint8_t key_triecode);
int         st_get_seq_ref_triecode_pos(uint8_t triecode);

#ifdef ST_TESTER
uint8_t     st_get_metachar_example_triecode(uint8_t triecode);
uint16_t    st_triecode_to_keycode(uint8_t triecode, uint16_t kc_seq_token_0);
const char *st_get_seq_token_utf8(uint8_t triecode);
const char *st_get_trans_token_utf8(uint8_t triecode);
uint16_t    st_test_ascii_to_keycode(char c);
#endif
