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

#define ST_DEFAULT_KEY_ACTION 0xffff

typedef struct
{
    uint16_t keypressed;
    uint16_t action_taken;
} st_key_action_t;

typedef struct
{
    st_key_action_t         *data;       // array of keycodes
    uint8_t                 size;        // buffer size
    uint8_t                 context_len; // number of current keys in buffer
    uint8_t                 cur_pos;
} st_key_buffer_t;

st_key_action_t         *st_key_buffer_get(st_key_buffer_t *buf, int index);
uint16_t                st_key_buffer_get_keycode(st_key_buffer_t *buf, int index);
void                    st_key_buffer_reset(st_key_buffer_t *buf);
void                    st_key_buffer_push(st_key_buffer_t *buf, uint16_t keycode);
void                    st_key_buffer_pop(st_key_buffer_t *buf, uint8_t num);
void                    st_key_buffer_print(st_key_buffer_t *buf);
void                    st_key_buffer_get_context_string(st_key_buffer_t *buf, char* output_string, uint8_t context_len);
