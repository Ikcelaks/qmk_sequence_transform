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

bool                    st_cursor_init(st_cursor_t *cursor, int history, uint8_t as_output);
uint8_t                st_cursor_get_triecode(st_cursor_t *cursor);
const st_trie_payload_t *st_cursor_get_action(st_cursor_t *cursor);
bool                    st_cursor_at_end(const st_cursor_t *cursor);
bool                    st_cursor_next(st_cursor_t *cursor);
st_cursor_pos_t         st_cursor_save(const st_cursor_t *cursor);
void                    st_cursor_restore(st_cursor_t *cursor, st_cursor_pos_t *cursor_pos);
bool                    st_cursor_longer_than(const st_cursor_t *cursor, const st_cursor_pos_t *past_pos);
bool                    st_cursor_push_to_stack(st_cursor_t *cursor, st_key_stack_t *key_stack, int count);
void                    st_cursor_print(st_cursor_t *cursor);
