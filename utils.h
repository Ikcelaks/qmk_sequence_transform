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

uint16_t char_to_keycode(char c);
char keycode_to_char(uint16_t keycode);
void multi_tap(uint16_t keycode, int count);
void send_key(uint16_t keycode);
