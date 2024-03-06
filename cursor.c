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

#define CDATA(L) pgm_read_byte(&cursor->trie->completions[L])

//////////////////////////////////////////////////////////////////
uint16_t st_cursor_current_keycode(const st_cursor_t *cursor)
{
    const st_key_action_t *keyaction = st_key_buffer_get(cursor->buffer, cursor->cursor_pos);
    if (!keyaction) {
        return KC_NO;
    }
    if (cursor->as_output_buffer && keyaction->action_taken != ST_DEFAULT_KEY_ACTION) {
        st_trie_payload_t payload = {0, 0, 0, 0};
        st_get_payload_from_match_index(cursor->trie, &payload, keyaction->action_taken);
        return ascii_to_keycode_lut[CDATA(payload.completion_index + payload.completion_len - 1 - cursor->sub_pos)];
    } else {
        return keyaction->keypressed;
    }
}
//////////////////////////////////////////////////////////////////
bool st_cursor_next(st_cursor_t *cursor)
{
    if (!cursor->as_output_buffer) {
        ++cursor->cursor_pos;
        return cursor->cursor_pos < cursor->buffer->context_len;
    }
    // Continue processing if simulating output buffer
    st_key_action_t *keyaction = st_key_buffer_get(cursor->buffer, cursor->cursor_pos);
    if (!keyaction) {
        return false;
    }
    if (keyaction->action_taken == ST_IGNORE_KEY_ACTION) {
        // skip fake key and try again
        ++cursor->cursor_pos;
        return st_cursor_next(cursor);
    }
    if (keyaction->action_taken == ST_DEFAULT_KEY_ACTION) {
        // This is a normal keypress to consume
        ++cursor->cursor_pos;
        cursor->sub_pos = 0;
        return cursor->cursor_pos < cursor->buffer->context_len;
    }
    st_trie_payload_t payload = {0, 0, 0, 0};
    st_get_payload_from_match_index(cursor->trie, &payload, keyaction->action_taken);
    if (cursor->sub_pos < payload.completion_len - 1) {
        ++cursor->sub_pos;
        return true;
    }
    // We have exhausted the key_action at the current buffer index
    // check if we need to fast-forward over any backspaced chars
    int backspaces = payload.num_backspaces;
    while (true) {
        // move to next key in buffer
        ++cursor->cursor_pos;
        keyaction = st_key_buffer_get(cursor->buffer, 0);
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
                cursor->sub_pos = 0;
                return true;
            }
            // consume one backspace
            --backspaces;
            continue;
        }
        // Load payload of key that performed action
        st_get_payload_from_match_index(cursor->trie, &payload, keyaction->action_taken);
        if (backspaces < payload.completion_len) {
            // This action contains the next output key. Find it's sub_pos and return true
            cursor->sub_pos = backspaces;
            return true;
        }
        backspaces -= payload.completion_len;
    }
}
//////////////////////////////////////////////////////////////////
void st_cursor_print(const st_cursor_t *cursor)
{
    st_cursor_t temp_cursor = *cursor;
    uprintf("cursor: |");
    while (temp_cursor.cursor_pos < temp_cursor.buffer->context_len) {
        uint16_t key = st_cursor_current_keycode(&temp_cursor);
        uprintf("%c", st_keycode_to_char(key));
        st_cursor_next(&temp_cursor);
    }
    uprintf("| (%d)\n", temp_cursor.buffer->context_len);
}
