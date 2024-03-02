// Copyright 2021 Google LLC
// Copyright 2021 @filterpaper
// Copyright 2023 Pablo Martinez (@elpekenin) <elpekenin@elpekenin.dev>
// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
// Original source/inspiration: https://getreuer.info/posts/keyboards/autocorrection
#pragma once

//////////////////////////////////////////////////////////////////
// Public API

#define IS_ALPHA_KEYCODE(code) ((code) >= KC_A && (code) <= KC_Z)

uint16_t    st_char_to_keycode(char c);
char        st_keycode_to_char(uint16_t keycode);
void        st_multi_tap(uint16_t keycode, int count);
void        st_send_key(uint16_t keycode);
int         st_max(int a, int b);
int         st_min(int a, int b);
