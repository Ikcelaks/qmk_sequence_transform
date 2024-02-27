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

typedef struct
{
    uint16_t    *data;       // array of keycodes
    uint8_t     size;        // buffer size
    uint8_t     context_len; // number of current keys in buffer
} key_buffer_t;

uint16_t    key_buffer_get(key_buffer_t *buf, int index);
void        key_buffer_reset(key_buffer_t *buf);
void        key_buffer_enqueue(key_buffer_t *buf, uint16_t keycode);
void        key_buffer_dequeue(key_buffer_t *buf, uint8_t num);
void        key_buffer_print(key_buffer_t *buf);
