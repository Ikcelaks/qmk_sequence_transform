// Copyright 2021 Google LLC
// Copyright 2021 @filterpaper
// Copyright 2023 Pablo Martinez (@elpekenin) <elpekenin@elpekenin.dev>
// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
// Original source/inspiration: https://getreuer.info/posts/keyboards/autocorrection
#pragma once

#include "st_defaults.h"
#include <stdint.h>
#include <stdbool.h>
#include "action.h"
#include "keybuffer.h"
#include "key_stack.h"
#include "trie.h"
#include "cursor.h"

//////////////////////////////////////////////////////////////////
// Public API

bool process_sequence_transform(uint16_t keycode, keyrecord_t *record, uint16_t sequence_token_start);
void sequence_transform_on_missed_rule_user(const st_trie_rule_t *rule);
void post_process_sequence_transform(void);
uint16_t sequence_transform_past_keycode(int index);

#if SEQUENCE_TRANSFORM_IDLE_TIMEOUT > 0
void sequence_transform_task(void);
#else
static inline void sequence_transform_task(void) {}
#endif

//////////////////////////////////////////////////////////////////
// Internal

bool st_process_check(uint16_t *keycode, const keyrecord_t *record, uint8_t *mods);
void st_handle_repeat_key(void);
void st_handle_result(const st_trie_t *trie, const st_trie_search_result_t *res);
bool st_perform(void);
void st_handle_backspace(void);

#ifdef ST_TESTER
const st_trie_t *st_get_trie(void);
st_key_buffer_t *st_get_key_buffer(void);
st_cursor_t     *st_get_cursor(void);
#endif
