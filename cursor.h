// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
#pragma once

//////////////////////////////////////////////////////////////////
// Public API

bool                    st_cursor_configure(const st_trie_t * const trie, st_key_buffer_t * const buffer);
bool                    st_cursor_init(st_cursor_t *cursor, uint8_t as_output);
uint8_t                 st_cursor_get_triecode(st_cursor_t *cursor);
uint16_t                st_cursor_get_matched_rule(st_cursor_t *cursor);
const st_trie_payload_t *st_cursor_get_action(st_cursor_t *cursor);
uint8_t                 st_cursor_get_shift_of_nth(st_cursor_t *cursor, int nth);
uint8_t                 st_cursor_get_seq_ascii(uint8_t seq_ref_index);
bool                    st_cursor_at_end(const st_cursor_t *cursor);
bool                    st_cursor_next(st_cursor_t *cursor);
bool                    st_cursor_next_key(st_cursor_t *cursor);
bool                    st_cursor_convert_to_output(st_cursor_t *cursor);
st_cursor_t             st_cursor_save(const st_cursor_t *cursor);
void                    st_cursor_restore(st_cursor_t *cursor, const st_cursor_t *cursor_new);
bool                    st_cursor_longer_than(const st_cursor_t *cursor, const st_cursor_t *past_pos);
bool                    st_cursor_push_to_stack(st_cursor_t *cursor, st_key_stack_t *key_stack, int count);
void                    st_cursor_print(st_cursor_t *cursor);
