// Copyright 2021 Google LLC
// Copyright 2021 @filterpaper
// Copyright 2023 Pablo Martinez (@elpekenin) <elpekenin@elpekenin.dev>
// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
// Original source/inspiration: https://getreuer.info/posts/keyboards/autocorrection
#pragma once

#include "keybuffer.h"
#include "trie.h"

//////////////////////////////////////////////////////////////////
// Public API

typedef struct
{
    st_key_buffer_t * const buffer;             // buffer
    st_trie_t               *trie;
    int                     cursor_pos;         // buffer index of cursor position
    int                     sub_pos;            // Sub-position within the current buffer position
                                                //   represented as a index from start of completion
    const bool              as_output_buffer;   // True if buffer traversing the simulated output
} st_cursor_t;

uint16_t                st_cursor_current_keycode(const st_cursor_t *cursor);
bool                    st_cursor_next(st_cursor_t *cursor);
