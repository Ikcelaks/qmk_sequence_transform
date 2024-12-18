// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "qmk_wrapper.h"
#include "triecodes.h"
#include "keybuffer.h"
#include "key_stack.h"
#include "trie.h"
#include "triecodes.h"
#include "utils.h"
#include "cursor.h"
#include "st_assert.h"
#include <ctype.h>

//////////////////////////////////////////////////////////////////
bool cursor_advance_to_valid_output(st_cursor_t *cursor)
{
    const st_trie_payload_t *action = st_cursor_get_action(cursor);
    if (!action) {
        return false;
    }
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
        st_key_buffer_advance_seq_ref_index(cursor->buffer, &cursor->seq_ref_index);
        if (st_cursor_at_end(cursor)) {
            return false;
        }
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
            for (cursor->pos.sub_index = 0; cursor->pos.sub_index < backspaces; ++cursor->pos.sub_index) {
                const int completion_char_index = action->completion_index + action->completion_len - 1 - cursor->pos.sub_index;
                const uint8_t triecode = CDATA(cursor->trie, completion_char_index);
                if (st_is_trans_seq_ref_triecode(triecode)) {
                    // This is a seq_ref, increment the seq_ref_index
                    ++cursor->seq_ref_index;
                }
            }
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
    cursor->pos.sub_index = 0;
    cursor->pos.segment_len = 1;
    cursor->cache_valid = 255;
    cursor->seq_ref_index = 0;
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
    const int completion_char_index_delta = action->completion_len - 1 - cursor->pos.sub_index;
    const int completion_char_index = action->completion_index + completion_char_index_delta;
    st_assert(completion_char_index >= 0, "Invalid completion_char_index: %d at Cursor Pos: %d, %d; %d",
                completion_char_index, cursor->pos.index, cursor->pos.sub_index, cursor->buffer->size);
    const uint8_t triecode = CDATA(cursor->trie, completion_char_index);
    if (st_is_trans_seq_ref_triecode(triecode)) {
        return st_key_buffer_get_seq_ref(cursor->buffer, cursor->seq_ref_index);
    }
    if ((keyaction->key_flags & ST_KEY_FLAG_IS_FULL_SHIFT) ||
            (completion_char_index_delta == 0 && (keyaction->key_flags & ST_KEY_FLAG_IS_ONE_SHOT_SHIFT))) {
        return toupper(triecode);
    }
    return triecode;
}
//////////////////////////////////////////////////////////////////
uint16_t st_cursor_get_matched_rule(st_cursor_t *cursor)
{
    const st_key_action_t *keyaction = st_key_buffer_get(cursor->buffer, cursor->pos.index);
    if (cursor->pos.as_output || !keyaction) {
        return ST_DEFAULT_KEY_ACTION;
    }
    return keyaction->action_taken;
}
//////////////////////////////////////////////////////////////////
// DO NOT USE externally when cursor is initialized to act
// as a virtual output. Behavior is not stable in the presence
// of `st_cursor_get_keycode` in virtual output mode
const st_trie_payload_t *st_cursor_get_action(st_cursor_t *cursor)
{
    st_trie_payload_t *action = &cursor->cached_action;
    if (cursor->cache_valid == cursor->pos.index) {
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
    cursor->cache_valid = cursor->pos.index;
    return action;
}
//////////////////////////////////////////////////////////////////
uint8_t st_cursor_get_shift_of_nth(st_cursor_t *cursor, int nth)
{
    st_cursor_pos_t original_pos = st_cursor_save(cursor);
    st_cursor_init(cursor, 1, true);
    for (int i = 0; i < nth - 1; ++i) {
        if (!st_cursor_next(cursor)) {
            st_cursor_restore(cursor, &original_pos);
            return 0;
        }
    }
    const st_trie_payload_t *action = st_cursor_get_action(cursor);
    uint8_t key_flags = st_key_buffer_get(cursor->buffer, cursor->pos.index)->key_flags;
    key_flags &= ~ST_KEY_FLAG_IS_ANCHOR_MATCH;
    if (action->completion_len > cursor->pos.sub_index + 1) {
        key_flags &= ~ST_KEY_FLAG_IS_ONE_SHOT_SHIFT;
    }
    st_cursor_restore(cursor, &original_pos);
    return key_flags;
}
//////////////////////////////////////////////////////////////////
uint8_t st_cursor_get_seq_ascii(st_cursor_t *cursor, uint8_t triecode)
{
    if (!st_is_trans_seq_ref_triecode(triecode)) {
        return triecode;
    }
    int nth = st_get_seq_ref_triecode_pos(triecode);
    st_cursor_pos_t original_pos = st_cursor_save(cursor);
    cursor->pos.as_output = false;
    cursor->pos.sub_index = 0;
    while (nth > 0) {
        if (st_cursor_at_end(cursor)) {
            // nth character in the sequence is not currently available
            st_cursor_restore(cursor, &original_pos);
            return 0;
        }
        --nth;
        if (!cursor->pos.as_output && (st_key_buffer_get(cursor->buffer, cursor->pos.index)->key_flags & ST_KEY_FLAG_IS_ANCHOR_MATCH)) {
            // reached the anchor of the sequence, move past the match
            // and get the rest of the sequence from the virtual output
            st_cursor_next(cursor);
            st_cursor_convert_to_output(cursor);
            cursor_advance_to_valid_output(cursor);
        } else {
            st_cursor_next(cursor);
        }
    }
    triecode = st_cursor_get_triecode(cursor);
    st_cursor_restore(cursor, &original_pos);
    return triecode;
}
//////////////////////////////////////////////////////////////////
bool st_cursor_at_end(const st_cursor_t *cursor)
{
    return cursor->pos.index >= cursor->buffer->size || cursor->seq_ref_index >= cursor->buffer->seq_ref_capacity;
}
//////////////////////////////////////////////////////////////////
bool st_cursor_next(st_cursor_t *cursor)
{
    if (!cursor->pos.as_output) {
        ++cursor->pos.index;
        st_key_buffer_advance_seq_ref_index(cursor->buffer, &cursor->seq_ref_index);
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
        st_key_buffer_advance_seq_ref_index(cursor->buffer, &cursor->seq_ref_index);
        cursor->pos.sub_index = 0;
        if (!cursor_advance_to_valid_output(cursor)) {
            cursor->pos.index = cursor->buffer->size;
            cursor->pos.sub_index = 0;
            return false;
        }
        ++cursor->pos.segment_len;
        return true;
    }
    // This is a key with an action and completion, increment the sub_index
    // and advance to the next key in the key buffer if we exceeded the completion length
    ++cursor->pos.sub_index;
    const st_trie_payload_t *action = st_cursor_get_action(cursor);
    if (action->completion_len > cursor->pos.sub_index) {
        const int completion_char_index = action->completion_index + action->completion_len - 1 - cursor->pos.sub_index;
        const uint8_t triecode = CDATA(cursor->trie, completion_char_index);
        if (st_is_trans_seq_ref_triecode(triecode)) {
            // This is a seq_ref, increment the seq_ref_index
            ++cursor->seq_ref_index;
        }
    }
    if (cursor_advance_to_valid_output(cursor)) {
        ++cursor->pos.segment_len;
        return true;
    }
    cursor->pos.index = cursor->buffer->size;
    cursor->pos.sub_index = 0;
    return false;
}
//////////////////////////////////////////////////////////////////
bool st_cursor_convert_to_output(st_cursor_t *cursor)
{
    if (cursor->pos.as_output) {
        return true;
    }
    cursor->pos.as_output = true;
    return cursor_advance_to_valid_output(cursor);
}
//////////////////////////////////////////////////////////////////
st_cursor_pos_t st_cursor_save(const st_cursor_t *cursor)
{
    return cursor->pos;
}
//////////////////////////////////////////////////////////////////
void st_cursor_restore(st_cursor_t *cursor, const st_cursor_pos_t *cursor_pos)
{
    cursor->pos = *cursor_pos;
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
#ifndef NO_PRINT
    st_cursor_pos_t cursor_pos = st_cursor_save(cursor);
    uprintf("cursor: |");
    while (!st_cursor_at_end(cursor)) {
        const uint8_t code = st_cursor_get_triecode(cursor);
        uprintf("%c", st_triecode_to_ascii(code));
        st_cursor_next(cursor);
    }
    uprintf("| (%d:%d)\n", cursor->buffer->size, cursor->pos.segment_len);
    st_cursor_restore(cursor, &cursor_pos);
#endif
}
//////////////////////////////////////////////////////////////////
bool st_cursor_push_to_stack(st_cursor_t *cursor,
                             st_key_stack_t *key_stack,
                             int count)
{
    key_stack->size = 0;
    for (; count > 0; --count, st_cursor_next(cursor)) {
        const uint8_t triecode = st_cursor_get_triecode(cursor);
        if (!triecode) {
            return false;
        }
        st_key_stack_push(key_stack, triecode);
    }
    return true;
}
