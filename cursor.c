// Copyright 2021 Google LLC
// Copyright 2021 @filterpaper
// Copyright 2023 Pablo Martinez (@elpekenin) <elpekenin@elpekenin.dev>
// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
// Original source/inspiration: https://getreuer.info/posts/keyboards/autocorrection

#include "tester/qmk_wrappers.h"
#include <stdint.h>
#include "cursor.h"
#include "keybuffer.h"
#include "key_stack.h"
#include "trie.h"
#include "utils.h"

#define CDATA(L) pgm_read_byte(&trie->completions[L])

//////////////////////////////////////////////////////////////////
void st_cursor_init(const st_trie_t *trie, st_key_buffer_t *buf, int history, uint8_t as_output_buffer)
{
    trie->cursor->buffer = buf;
    trie->cursor->cursor_pos.pos = history;
    trie->cursor->cursor_pos.as_output_buffer = as_output_buffer;
    trie->cursor->cursor_pos.sub_pos = as_output_buffer ? 0 : 255;
    trie->cursor->cursor_pos.segment_len = 1;
    trie->cursor->cache_valid = false;
}
//////////////////////////////////////////////////////////////////
uint16_t st_cursor_get_keycode(const st_trie_t *trie)
{
    const st_key_action_t *keyaction = st_key_buffer_get(trie->cursor->buffer, trie->cursor->cursor_pos.pos);
    if (!keyaction) {
        return KC_NO;
    }
    if (trie->cursor->cursor_pos.as_output_buffer && keyaction->action_taken != ST_DEFAULT_KEY_ACTION && keyaction->action_taken != ST_IGNORE_KEY_ACTION) {
        const st_trie_payload_t *action = st_cursor_get_action(trie);
        return st_char_to_keycode(CDATA(action->completion_index + action->completion_len - 1 - trie->cursor->cursor_pos.sub_pos));
    } else {
        return keyaction->keypressed;
    }
}
//////////////////////////////////////////////////////////////////
st_trie_payload_t *st_cursor_get_action(const st_trie_t *trie)
{
    if (trie->cursor->cache_valid) {
        return &trie->cursor->cached_action;
    }
    const st_key_action_t *keyaction = st_key_buffer_get(trie->cursor->buffer, trie->cursor->cursor_pos.pos);
    if (!keyaction) {
        return NULL;
    }
    if (keyaction->action_taken == ST_DEFAULT_KEY_ACTION) {
        trie->cursor->cached_action.completion_index = ST_DEFAULT_KEY_ACTION;
        trie->cursor->cached_action.completion_len = 1;
        trie->cursor->cached_action.num_backspaces = 0;
        trie->cursor->cached_action.func_code = 0;
    } else if (keyaction->action_taken == ST_IGNORE_KEY_ACTION) {
        trie->cursor->cached_action.completion_index = ST_DEFAULT_KEY_ACTION;
        trie->cursor->cached_action.completion_len = 0;
        trie->cursor->cached_action.num_backspaces = 0;
        trie->cursor->cached_action.func_code = 0;
    } else {
        st_get_payload_from_match_index(trie, &trie->cursor->cached_action, keyaction->action_taken);
    }
    trie->cursor->cache_valid = true;
    return &trie->cursor->cached_action;
}
//////////////////////////////////////////////////////////////////
bool st_cursor_next(const st_trie_t *trie)
{
    st_cursor_t *cursor = trie->cursor;
    if (!cursor->cursor_pos.as_output_buffer) {
        ++cursor->cursor_pos.pos;
        ++cursor->cursor_pos.segment_len;
        return cursor->cursor_pos.pos < cursor->buffer->context_len;
    }
    // Continue processing if simulating output buffer
    st_key_action_t *keyaction = st_key_buffer_get(cursor->buffer, cursor->cursor_pos.pos);
    if (!keyaction) {
        return false;
    }
    ++cursor->cursor_pos.segment_len;
    if (keyaction->action_taken == ST_IGNORE_KEY_ACTION) {
        // skip fake key and try again
        ++cursor->cursor_pos.pos;
        cursor->cache_valid = false;
        cursor->cursor_pos.sub_pos = 0;
        return st_cursor_next(trie);
    }
    if (keyaction->action_taken == ST_DEFAULT_KEY_ACTION) {
        // This is a normal keypress to consume
        ++cursor->cursor_pos.pos;
        cursor->cache_valid = false;
        cursor->cursor_pos.sub_pos = 0;
        ++cursor->cursor_pos.segment_len;
        // TODO: Handle caching
        return cursor->cursor_pos.pos < cursor->buffer->context_len;
    }
    st_trie_payload_t *action = st_cursor_get_action(trie);
    if (cursor->cursor_pos.sub_pos < action->completion_len - 1) {
        ++cursor->cursor_pos.sub_pos;
        ++cursor->cursor_pos.segment_len;
        return true;
    }
    // We have exhausted the key_action at the current buffer index
    // check if we need to fast-forward over any backspaced chars
    int backspaces = action->num_backspaces;
    while (true) {
        // move to next key in buffer
        ++cursor->cursor_pos.pos;
        cursor->cache_valid = false;
        keyaction = st_key_buffer_get(cursor->buffer, cursor->cursor_pos.pos);
        if (!keyaction) {
            // We reached the end without finding the next output key
            return false;
        }
        if (keyaction->action_taken == ST_IGNORE_KEY_ACTION) {
            // skip fake key
            continue;
        }
        if (keyaction->action_taken == ST_DEFAULT_KEY_ACTION) {
            if (backspaces == 0) {
                // This is a real keypress and no more backspaces to consume
                cursor->cursor_pos.sub_pos = 0;
                return true;
            }
            // consume one backspace
            --backspaces;
            continue;
        }
        // Load payload of key that performed action
        action = st_cursor_get_action(trie);
        if (backspaces < action->completion_len) {
            // This action contains the next output key. Find it's sub_pos and return true
            cursor->cursor_pos.sub_pos = backspaces;
            return true;
        }
        backspaces -= action->completion_len;
    }
}
//////////////////////////////////////////////////////////////////
bool st_cursor_move_to_history(const st_trie_t *trie, int history)
{
    st_cursor_t *cursor = trie->cursor;
    cursor->cache_valid = false;    // invalidate cache
    cursor->cursor_pos.pos = history;
    cursor->cursor_pos.sub_pos = 0;
    return history < cursor->buffer->context_len;
}
//////////////////////////////////////////////////////////////////
st_cursor_pos_t st_cursor_save(const st_trie_t *trie)
{
    return trie->cursor->cursor_pos;
}
//////////////////////////////////////////////////////////////////
void st_cursor_restore(const st_trie_t *trie, st_cursor_pos_t *cursor_pos)
{
    trie->cursor->cursor_pos = *cursor_pos;
    trie->cursor->cache_valid = false;
}
//////////////////////////////////////////////////////////////////
bool st_cursor_longer_than(const st_trie_t *trie, st_cursor_pos_t *past_pos)
{
    return (trie->cursor->cursor_pos.pos << 8) + trie->cursor->cursor_pos.sub_pos
        > (past_pos->pos << 8) + past_pos->pos;
}
//////////////////////////////////////////////////////////////////
void st_cursor_print(const st_trie_t *trie)
{
    st_cursor_pos_t cursor_pos = st_cursor_save(trie);
    uprintf("cursor: |");
    while (trie->cursor->cursor_pos.pos < trie->cursor->buffer->context_len) {
        uint16_t key = st_cursor_get_keycode(trie);
        uprintf("%c", st_keycode_to_char(key));
        st_cursor_next(trie);
    }
    uprintf("| (%d)\n", trie->cursor->buffer->context_len);
    st_cursor_restore(trie, &cursor_pos);
}
//////////////////////////////////////////////////////////////////
bool st_cursor_push_to_stack(const st_trie_t *trie, int count)
{
    trie->key_stack->size = 0;
    for (; count > 0; --count, st_cursor_next(trie)) {
        const uint16_t keycode = st_cursor_get_keycode(trie);
        if (!keycode) {
            return false;
        }
        st_key_stack_push(trie->key_stack, keycode);
    }
    return true;
}
