// Copyright 2021 Google LLC
// Copyright 2021 @filterpaper
// Copyright 2023 Pablo Martinez (@elpekenin) <elpekenin@elpekenin.dev>
// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
// Original source/inspiration: https://getreuer.info/posts/keyboards/autocorrection
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "keybuffer.h"
#include "key_stack.h"
#include "trie.h"
#include "utils.h"
#include "print.h"

#define TRIE_MATCH_BIT      0x8000
#define TRIE_BRANCH_BIT     0x4000
#define TRIE_CODE_MASK      0x3FFF

#define TDATA(L) pgm_read_word(&trie->data[L])
#define CDATA(L) pgm_read_byte(&trie->completions[L])

//////////////////////////////////////////////////////////////////
bool st_trie_get_completion(st_trie_t *trie, st_key_buffer_t *search, st_trie_payload_t *res)
{
    return st_find_longest_chain(trie, search, res, 0, 0);
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

/**
 * @brief Find longest chain in trie matching the key_buffer. (recursive)
 *
 * @param trie   trie_t struct containing trie data/size
 * @param search key buffer to search for
 * @param res    result containing payload for longest context match
 * @param offset current offset in trie data
 * @param depth  current depth in trie
 * @return       true if match found
 */
bool st_find_longest_chain(st_trie_t *trie, st_key_buffer_t *search, st_trie_payload_t *res, uint16_t offset, uint8_t depth)
{
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
#endif
	// Match Node if bit 15 is set
	if (code & TRIE_MATCH_BIT) {
        // match nodes are side attachments, so decrease depth
        depth--;
        // If bit 14 is also set, there is a child node after the completion string
        if ((code & TRIE_BRANCH_BIT) && st_find_longest_chain(trie, search, res, offset+2, depth+1))
            return true;
        // If no better match found deeper, so recordd the payload result!
        st_get_payload_from_code(res, code, TDATA(offset + 1));
        res->context_match_len = depth + 1;
#ifdef SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS
         // bounds check completion data
        if (res->completion_index + res->completion_len > trie->completions_size) {
            uprintf("find_longest_chain() ERROR: trying to read past end of completion data buffer! index: %d, len: %d\n",
                res->completion_index, res->completion_len);
            return false;
        }
#endif
        // Found a match so return true!
        return true;
	}
	// Branch Node (with multiple children) if bit 14 is set
	if (code & TRIE_BRANCH_BIT) {
        if (depth > search->context_len)
			return false;
		code &= TRIE_CODE_MASK;
        // Find child key that matches the search buffer at the current depth
        const uint16_t cur_key = st_key_buffer_get_keycode(search, depth);
		for (; code; offset += 2, code = TDATA(offset)) {
            if (code == cur_key) {
                // 16bit offset to child node is built from next uint16_t
                const uint16_t child_offset = TDATA(offset+1);
                // Traverse down child node
                return st_find_longest_chain(trie, search, res, child_offset, depth+1);
            }
        }
        // Couldn't go deeper, so return false.
        return false;
	}
    // No high bits set, so this is a chain node
	// Travel down chain until we reach a zero byte, or we no longer match our buffer
	for (; code; depth++, code = TDATA(++offset)) {
		if (depth > search->context_len ||
            code != st_key_buffer_get_keycode(search, depth))
			return false;
	}
	// After a chain, there should be a leaf or branch
	return st_find_longest_chain(trie, search, res, offset+1, depth);
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
    search.max_completed_chars = 0;    
    const int max_skip_levels = 1 + trie->max_backspaces;
    for (int len = search_len_start; len < key_buffer->context_len; ++len) {
        // For every base search_len, increase skip_levels until we find a match.
        // Each skip level allows for a magic key or deleted char.
        for (search.skip_levels = 1; search.skip_levels <= max_skip_levels; ++search.skip_levels) {
            search.search_len = len + search.skip_levels;
            trie->key_stack->size = 0;
            st_find_rule(&search, 0);
            if (search.max_completed_chars) {
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
    // 1. potential completed len is smaller than our current best
    // 2. potential completed len overflows search buffer
    int clen = search->search_len - search->skip_levels;
    const int clen_end = clen + payload->completion_len;
    if (clen_end < search->max_completed_chars ||
        clen_end > search->key_buffer->context_len) {
        return;
    }
    //uprintf("  testing completion: ");
    // Check if completed string matches what comes next in our search buffer
    const int completion_end = payload->completion_index + payload->completion_len;
    char *completion_str = res->completion;
    for (int i = payload->completion_index; i < completion_end; ++i, ++clen) {
        const char ascii_code = CDATA(i);
        // Save to ascii completion string at the same time
        *completion_str++ = ascii_code;       
        const uint16_t keycode = st_char_to_keycode(tolower(ascii_code));
        const uint16_t buffer_keycode = st_key_buffer_get_keycode(search->key_buffer, -(clen+1));
        //uprintf("[%02X, %02X] ", keycode, buffer_keycode);
        if (keycode != buffer_keycode) {
            //uprintf("  no match.\n");
            return;
        }
    }
    *completion_str = 0;
    //uprintf("  match found!!!\n");
    // Mark winning info in search struct, and write the rule string
    search->max_completed_chars = clen;
    res->payload = *payload;
    st_key_stack_to_str(trie->key_stack, res->rule);
 }
