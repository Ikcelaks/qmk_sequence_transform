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
#include "key_stack.h"
#include "trie.h"

//////////////////////////////////////////////////////////////////
// Public API

void                    st_cursor_init(st_cursor_t *cursor, st_trie_t *trie, st_key_buffer_t *buf, int history, bool as_output_buffer);
uint16_t                st_cursor_current_keycode(const st_cursor_t *cursor);
bool                    st_cursor_next(st_cursor_t *cursor);
void                    st_cursor_print(const st_cursor_t *cursor);
