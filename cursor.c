// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "qmk_wrapper.h"
#include "keybuffer.h"
#include "key_stack.h"
#include "trie.h"
#include "utils.h"
#include "cursor.h"
#include "st_assert.h"

//////////////////////////////////////////////////////////////////
bool cursor_advance_to_valid_output(st_cursor_t *cursor)
{
    const st_trie_payload_t *action = st_cursor_get_action(cursor);
    if (cursor->pos.sub_index < action->completion_len) {
        // current sub-index is valid; no need to advance
        return true;
    }
    // we have exceeded the length of the completion string
    // advance to the next key that contains output
    int backspaces = action->num_backspaces;
    while (true) {
        // move to next key in buffer
        ++cursor->pos.index;
        if (st_cursor_at_end(cursor)) {
            return false;
        }
        cursor->cache_valid = false;
        const st_key_action_t *keyaction = st_key_buffer_get(cursor->buffer, cursor->pos.index);
        st_assert(keyaction, "reached the end without finding the next output key");
        if (keyaction->action_taken == ST_DEFAULT_KEY_ACTION) {
            if (backspaces == 0) {
                // This is a real keypress and no more backspaces to consume
                cursor->pos.sub_index = 0;
                return true;
            }
            // consume one backspace
            --backspaces;
            continue;
        }
        // Load payload of key that performed action
        action = st_cursor_get_action(cursor);
        if (backspaces < action->completion_len) {
            // This action contains the next output key. Find it's sub_pos and return true
            cursor->pos.sub_index = backspaces;
            return true;
        }
        backspaces -= action->completion_len - action->num_backspaces;
    }
}
//////////////////////////////////////////////////////////////////
bool st_cursor_init(st_cursor_t *cursor, int history, uint8_t as_output)
{
    cursor->pos.index = history;
    cursor->pos.as_output = as_output;
    cursor->pos.sub_index = as_output ? 0 : 255;
    cursor->pos.segment_len = 1;
    cursor->cache_valid = false;
    if (as_output && !cursor_advance_to_valid_output(cursor)) {
        // This is crazy, but it is theoretically possible that the
        // entire buffer is full of backspaces such that no valid
        // output key exists in the buffer!
        // Set the cursor_pos to the `end` position and return false
        cursor->pos.index = cursor->buffer->size;
        cursor->pos.sub_index = 0;
        return false;
    }
    // TODO: add an assert that the buffer isn't empty, or maybe
    // that should be done in the key_buffer code
    return true;
}
//////////////////////////////////////////////////////////////////
uint8_t st_cursor_get_triecode(st_cursor_t *cursor)
{
    const st_key_action_t *keyaction = st_key_buffer_get(cursor->buffer, cursor->pos.index);
    if (!keyaction) {
        return '\0';
    }
    if (!cursor->pos.as_output
            || keyaction->action_taken == ST_DEFAULT_KEY_ACTION) {
        // we need the actual key that was pressed
        return keyaction->triecode;
    }
    // This is an output cursor focused on rule matching keypress
    // get the character at the sub_indax of the transform completion
    const st_trie_payload_t *action = st_cursor_get_action(cursor);
    int completion_char_index = action->completion_index;
    completion_char_index += action->completion_len - 1 - cursor->pos.sub_index;
    return CDATA(cursor->trie, completion_char_index);
}
//////////////////////////////////////////////////////////////////
// DO NOT USE externally when cursor is initialized to act
// as a virtual output. Behavior is not stable in the presence
// of `st_cursor_get_keycode` in virtual output mode
const st_trie_payload_t *st_cursor_get_action(st_cursor_t *cursor)
{
    st_trie_payload_t *action = &cursor->cached_action;
    if (cursor->cache_valid) {
        return action;
    }
    const st_key_action_t *keyaction = st_key_buffer_get(cursor->buffer, cursor->pos.index);
    if (!keyaction) {
        return NULL;
    }
    if (keyaction->action_taken == ST_DEFAULT_KEY_ACTION) {
        action->completion_index = ST_DEFAULT_KEY_ACTION;
        action->completion_len = 1;
        action->num_backspaces = 0;
        action->func_code = 0;
    } else {
        st_get_payload_from_match_index(cursor->trie, action, keyaction->action_taken);
    }
    cursor->cache_valid = true;
    return action;
}
//////////////////////////////////////////////////////////////////
bool st_cursor_at_end(const st_cursor_t *cursor)
{
    return cursor->pos.index >= cursor->buffer->size;
}
//////////////////////////////////////////////////////////////////
bool st_cursor_next(st_cursor_t *cursor)
{
    if (!cursor->pos.as_output) {
        ++cursor->pos.index;
        cursor->cache_valid = false;
        if (st_cursor_at_end(cursor)) {
            // leave `index` at the End position
            cursor->pos.index = cursor->buffer->size;
            return false;
        }
        ++cursor->pos.segment_len;
        return true;
    }
    // Continue processing if simulating output buffer
    const st_key_action_t *keyaction = st_key_buffer_get(cursor->buffer, cursor->pos.index);
    if (!keyaction) {
        return false;
    }
    if (keyaction->action_taken == ST_DEFAULT_KEY_ACTION) {
        // This is a normal keypress to consume
        ++cursor->pos.index;
        if (st_cursor_at_end(cursor)) {
            return false;
        }
        cursor->cache_valid = false;
        cursor->pos.sub_index = 0;
        ++cursor->pos.segment_len;
        return true;
    }
    // This is a key with an action and completion, increment the sub_index
    // and advance to the next key in the key buffer if we exceeded the completion length
    ++cursor->pos.sub_index;
    if (cursor_advance_to_valid_output(cursor)) {
        ++cursor->pos.segment_len;
        return true;
    }
    return false;
}
//////////////////////////////////////////////////////////////////
st_cursor_pos_t st_cursor_save(const st_cursor_t *cursor)
{
    return cursor->pos;
}
//////////////////////////////////////////////////////////////////
void st_cursor_restore(st_cursor_t *cursor, st_cursor_pos_t *cursor_pos)
{
    cursor->pos = *cursor_pos;
    cursor->cache_valid = false;
}
//////////////////////////////////////////////////////////////////
bool st_cursor_longer_than(const st_cursor_t *cursor, const st_cursor_pos_t *past_pos)
{
    const int cur_pos = (cursor->pos.index << 8)
        + cursor->pos.sub_index;
    const int old_pos = (past_pos->index << 8)
        + past_pos->sub_index;
    return cur_pos > old_pos;
}
//////////////////////////////////////////////////////////////////
void st_cursor_print(st_cursor_t *cursor)
{
    st_cursor_pos_t cursor_pos = st_cursor_save(cursor);
    uprintf("cursor: |");
    while (!st_cursor_at_end(cursor)) {
        uprintf("%c", st_triecode_to_char(st_cursor_get_triecode(cursor)));
        st_cursor_next(cursor);
    }
    uprintf("| (%d:%d)\n", cursor->buffer->size, cursor->pos.segment_len);
    st_cursor_restore(cursor, &cursor_pos);
}
//////////////////////////////////////////////////////////////////
bool st_cursor_push_to_stack(st_cursor_t *cursor,
                             st_key_stack_t *key_stack,
                             int count)
{
    key_stack->size = 0;
    for (; count > 0; --count, st_cursor_next(cursor)) {
        const uint16_t keycode = st_cursor_get_triecode(cursor);
        if (!keycode) {
            return false;
        }
        st_key_stack_push(key_stack, keycode);
    }
    return true;
}
