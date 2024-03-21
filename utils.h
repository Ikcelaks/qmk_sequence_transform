// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
#pragma once

//////////////////////////////////////////////////////////////////
// Public API

#define IS_ALPHA_KEYCODE(code) ((code) >= KC_A && (code) <= KC_Z)

void        st_multi_tap(uint16_t keycode, int count);
void        st_send_key(uint16_t keycode);
int         st_max(int a, int b);
int         st_min(int a, int b);
int         st_clamp(int val, int min_val, int max_val);
