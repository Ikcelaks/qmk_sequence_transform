// Copyright 2021 Google LLC
// Copyright 2021 @filterpaper
// Copyright 2023 Pablo Martinez (@elpekenin) <elpekenin@elpekenin.dev>
// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
// Original source/inspiration: https://getreuer.info/posts/keyboards/autocorrection

#include "qmk_wrapper.h"
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "keybuffer.h"
#include "key_stack.h"
#include "cursor.h"
#include "trie.h"
#include "utils.h"

#define TRIE_MATCH_BIT      0x8000
#define TRIE_BRANCH_BIT     0x4000
#define TRIE_CODE_MASK      0x3FFF

#define TDATA(L) pgm_read_word(&trie->data[L])
#define CDATA(L) pgm_read_byte(&trie->completions[L])

//////////////////////////////////////////////////////////////////
bool st_trie_get_completion(st_trie_t *trie, st_key_buffer_t *search, st_trie_search_result_t *res)
{
    st_cursor_init(trie, search, 0, false);
    st_find_longest_chain(trie, &res->trie_match, 0);
#ifdef SEQUENCE_TRANSFORM_ENABLE_FALLBACK_BUFFER
    st_cursor_init(trie, search, 0, true);

#ifdef SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS
    st_cursor_print(trie);
#endif
    st_find_longest_chain(trie, &res->trie_match, 0);
#endif
    if (res->trie_match.seq_match_pos.segment_len > 0) {
        st_get_payload_from_match_index(trie, &res->trie_payload, res->trie_match.trie_match_index);
        return true;
    }
    return false;
}
//////////////////////////////////////////////////////////////////
void st_get_payload_from_match_index(const st_trie_t *trie, st_trie_payload_t *payload, uint16_t match_index)
{
    st_get_payload_from_code(payload, TDATA(match_index), TDATA(match_index+1));
}
//////////////////////////////////////////////////////////////////
void st_get_payload_from_code(st_trie_payload_t *payload, uint16_t code, uint16_t completion_index)
{
    // Payload data is bit-backed into 16bits:
    // (N: node type, F: func, B: backspackes, C: completion index)
    // 0b NNFF FBBB BCCC CCCC
    payload->func_code = (code >> 11) & 7;
    payload->num_backspaces = (code >> 7) & 15;
    payload->completion_len = code & 127;
    payload->completion_index = completion_index;
}

//////////////////////////////////////////////////////////////////////
bool find_branch_offset(st_trie_t *trie, uint16_t *offset, uint16_t code, uint16_t cur_key)
{
    for (; code; *offset += 2, code = TDATA(*offset)) {
        if (code == cur_key) {
            // 16bit offset to child node is built from next uint16_t
            *offset = TDATA(*offset+1);
            return true;
        }
    }
    return 0;
}

/**
 * @brief Find longest chain in trie matching the key_buffer. (recursive)
 *
 * @param trie   trie_t struct containing trie data/size
 * @param res    result containing payload for longest context match
 * @param offset current offset in trie data
 * @param depth  current depth in trie
 * @return       true if match found
 */
bool st_find_longest_chain(st_trie_t *trie, st_trie_match_t *longest_match, uint16_t offset)
{
    bool longer_match_found = false;
    do {
#ifdef SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS
        if (offset >= trie->data_size) {
            uprintf("find_longest_chain() ERROR: tried reading outside trie data! Offset: %d\n", offset);
            return false;
        }
#endif
        uint16_t code = TDATA(offset);
#ifdef SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS
        if (!code) {
            uprintf("find_longest_chain() ERROR: unexpected null code! Offset: %d\n", offset);
            return false;
        }
        if (code & TRIE_MATCH_BIT) {
            uprintf("find_longest_chain() ERROR: match found at top of loop! Offset: %d\n", offset);
            return false;
        }
#endif
        // Branch Node (with multiple children) if bit 14 is set
        if (code & TRIE_BRANCH_BIT) {
#ifdef SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS
            uprintf("Branching Offset: %d; Code: %#04X\n", offset, code);
#endif
            code &= TRIE_CODE_MASK;
            // Find child key that matches the search buffer at the current depth
            const uint16_t cur_key = st_cursor_get_keycode(trie);
            if (!cur_key) { // exhausted buffer; return
                return longer_match_found;
            }
            if (!find_branch_offset(trie, &offset, code, cur_key)) {
                // Couldn't go deeper; return.
                return longer_match_found;
            }
        } else {
            // No high bits set, so this is a chain node
            // Travel down chain until we reach a zero byte, or we no longer match our buffer
            do {
#ifdef SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS
                uprintf("Chaining Offset: %d; Code: %#04X\n", offset, code);
#endif
                if (code != st_cursor_get_keycode(trie))
                    return longer_match_found;
            } while ((code = TDATA(++offset)) && st_cursor_next(trie));
            // After a chain, there should be a match or branch
            ++offset;
        }
        // Traversed one (or more) buffer keys, check if we are at a match
        code = TDATA(offset);
        // Match Node if bit 15 is set
        if (code & TRIE_MATCH_BIT) {
#ifdef SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS
            uprintf("New Match found: (%d, %d) %d\n", trie->cursor->cursor_pos.pos, trie->cursor->cursor_pos.sub_pos, trie->cursor->cursor_pos.segment_len);
            uprintf("Previous Match: (%d, %d) %d\n", longest_match->seq_match_pos.pos, longest_match->seq_match_pos.sub_pos, longest_match->seq_match_pos.segment_len);
#endif
            // record this if it is the longest match
            if (st_cursor_longer_than(trie, &longest_match->seq_match_pos)) {
                longer_match_found = true;
                longest_match->trie_match_index = offset;
                longest_match->seq_match_pos = st_cursor_save(trie);
            }
            // If bit 14 is also set, there is a child node after the completion string
            if (code & TRIE_BRANCH_BIT) {
                // move offset to next child node and continue walking the trie
                offset += 2;
                code = TDATA(offset);
            } else {
                // No more matches; return
                return longer_match_found;
            }
        }
    } while (st_cursor_next(trie));
    return longer_match_found;
}

//////////////////////////////////////////////////////////////////////
int st_trie_get_rule(st_trie_t              *trie,
                     const st_key_buffer_t  *key_buffer,
                     int                    search_len_start,
                     st_trie_rule_t         *res)
{
    st_trie_search_t search;
    search.trie = trie;
    search.key_buffer = key_buffer;
    search.result = res;
    search.max_transform_len = 0;
    const int max_skip_levels = 1 + trie->max_backspaces;
    for (int len = search_len_start; len < key_buffer->context_len; ++len) {
        // For every base search_len, increase skip_levels until we find a match.
        // Each skip level allows for a magic key or deleted char.
        for (search.skip_levels = 1; search.skip_levels <= max_skip_levels; ++search.skip_levels) {
            search.search_len = len + search.skip_levels;
            if (search.search_len > key_buffer->context_len)
                break;
            //uprintf("  searching from len %d, skips: %d\n", len, search.skip_levels);
            trie->key_stack->size = 0;
            st_find_rule(&search, 0);
            if (search.max_transform_len) {
                return len + res->payload.completion_len;
            }
        }
    }
    return search_len_start;
}
//////////////////////////////////////////////////////////////////////
bool st_find_rule(st_trie_search_t *search, uint16_t offset)
{
// Simulate future buffer keys by offsetting buffer access
#define OFFSET_BUFFER_VAL st_key_buffer_get_keycode(key_buffer, key_stack->size - search->search_len)
    st_trie_t *trie = search->trie;
    const st_key_buffer_t *key_buffer = search->key_buffer;
    st_key_stack_t *key_stack = trie->key_stack;
    uint16_t code = TDATA(offset);
    // Match Node if bit 15 is set
    if (code & TRIE_MATCH_BIT) {
        // If bit 14 is also set, there's a child node after the completion string
        if ((code & TRIE_BRANCH_BIT) && st_find_rule(search, offset + 2)) {
            return true;
        }
        // If no better match found deeper,
        // inspect this payload to see if the rule would match
        st_trie_payload_t payload;
        st_get_payload_from_code(&payload, code, TDATA(offset + 1));
        // Make sure skip_levels matches 1 magic key + backspaces,
        // and that removing those wouldn't leave us with an empty context
        const int skips = 1 + payload.num_backspaces;
        if (search->skip_levels != skips || key_stack->size <= skips) {
            return false;
        }
        st_check_rule_match(&payload, search);
        // Found a match so return true!
        return true;
    }
    // BRANCH node if bit 14 is set
    if (code & TRIE_BRANCH_BIT) {
        if (key_stack->size >= search->search_len) {
            return false;
        }
        code &= TRIE_CODE_MASK;
        bool res = false;
        const bool check = key_stack->size >= search->skip_levels;
        const uint16_t cur_key = check ? OFFSET_BUFFER_VAL : 0;
        // find child that matches our current buffer location
        // (if this is a skip level, we go down all children)
        for (; code; offset += 2, code = TDATA(offset)) {
            if (!check || cur_key == code) {
                // Get 16bit offset to child node
                const uint16_t child_offset = TDATA(offset + 1);
                // Traverse down child node
                st_key_stack_push(key_stack, code);
                res = st_find_rule(search, child_offset) || res;
                st_key_stack_pop(key_stack);
                if (check) {
                    return res;
                }
            }
        }
        return res;
    }
    // No high bits set, so this is a chain node
    // Travel down chain until we reach a zero code, or we no longer match our buffer
    const int prev_stack_size = key_stack->size;
    for (; code; code = TDATA(++offset)) {
        if (key_stack->size >= search->search_len) {
            key_stack->size = prev_stack_size;
            return false;
        }
        const bool check = key_stack->size >= search->skip_levels;
        const uint16_t cur_key = check ? OFFSET_BUFFER_VAL : 0;
        if (check && cur_key != code) {
            key_stack->size = prev_stack_size;
            return false;
        }
        st_key_stack_push(key_stack, code);
    }
    // After a chain, there should be a leaf or branch
    const bool res = st_find_rule(search, offset+1);
    key_stack->size = prev_stack_size;
    return res;
}
//////////////////////////////////////////////////////////////////////
void st_check_rule_match(const st_trie_payload_t *payload, st_trie_search_t *search)
{
    st_trie_t *trie = search->trie;
    st_trie_rule_t *res = search->result;
    //uprintf("checking match at index %d\n", payload->completion_index);
    // Early return if:
    // 1. potential transform len is smaller than our current best
    // 2. potential transform len is bigger than search buffer
    int clen = search->search_len - search->skip_levels;
    const int transform_len = clen + payload->completion_len;
    if (transform_len < search->max_transform_len ||
        transform_len > search->key_buffer->context_len) {
        return;
    }
    //uprintf("  testing completion: ");
    // Check if completed string matches what comes next in our search buffer
    const int completion_end = payload->completion_index + payload->completion_len;
    for (int i = payload->completion_index; i < completion_end; ++i, ++clen) {
        const char ascii_code = CDATA(i);
        const uint16_t keycode = st_char_to_keycode(tolower(ascii_code));
        const uint16_t buffer_keycode = st_key_buffer_get_keycode(search->key_buffer, -(clen+1));
        //uprintf("[%02X, %02X] ", keycode, buffer_keycode);
        if (keycode != buffer_keycode) {
            //uprintf("  no match.\n");
            return;
        }
    }
    // Save payload data and max_transform_len in result
    search->max_transform_len = clen;
    res->payload = *payload;
    // Fill the result sequence and start of transform
    char *seq = res->sequence;
    char *transform = res->transform;
    for (int i = trie->key_stack->size - 1; i >= 0; --i) {
        const uint16_t keycode = trie->key_stack->buffer[i];
        const char c = st_keycode_to_char(keycode);
        *seq++ = c;
        if (i >= 1 + payload->num_backspaces &&
            !(i == trie->key_stack->size - 1 && keycode == KC_SPACE)) {
            *transform++ = c;
        }
    }
    *seq = 0;
    // Finish writing transform
    for (int i = payload->completion_index; i < completion_end; ++i) {
        *transform++ = CDATA(i);
    }
    *transform = 0;
}
