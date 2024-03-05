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
#include "keybuffer.h"
#include "cursor.h"
#include "trie.h"
#include "print.h"

#define TRIE_MATCH_BIT      0x8000
#define TRIE_BRANCH_BIT     0x4000
#define TRIE_CODE_MASK      0x3FFF

#define TDATA(L) pgm_read_word(&trie->data[L])

//////////////////////////////////////////////////////////////////
bool st_trie_get_completion(st_trie_t *trie, st_key_buffer_t *search, st_trie_search_result_t *res)
{
    st_cursor_t cursor = {search, trie, 0, 0, false};
    st_cursor_t output_cursor = {search, trie, 0, 0, true};
    st_cursor_print(&output_cursor);
    st_find_longest_chain_cursor(&cursor, &res->trie_match, 0, 0);
    st_find_longest_chain_cursor(&output_cursor, &res->trie_match, 0, 0);
    if (res->trie_match.seq_match_len > 0) {
        st_get_payload_from_match_index(trie, &res->trie_payload, res->trie_match.trie_match_index);
        return true;
    }
    return false;
}
//////////////////////////////////////////////////////////////////
void st_get_payload_from_match_index(st_trie_t *trie, st_trie_payload_t *payload, uint16_t match_index)
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
bool st_find_longest_chain(st_trie_t *trie, st_key_buffer_t *search, st_trie_match_t *longest_match, uint16_t offset, uint8_t depth)
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
        // record this as the new longest match
        longest_match->trie_match_index = offset;
        longest_match->seq_match_len = depth + 1;
        // If bit 14 is also set, there is a child node after the completion string
        if ((code & TRIE_BRANCH_BIT) && st_find_longest_chain(trie, search, longest_match, offset+2, depth+1))
            return true;
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
                return st_find_longest_chain(trie, search, longest_match, child_offset, depth+1);
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
	return st_find_longest_chain(trie, search, longest_match, offset+1, depth);
}


bool st_find_longest_chain_cursor(st_cursor_t *cursor, st_trie_match_t *longest_match, uint16_t offset, uint8_t depth)
{
    const st_trie_t *trie = cursor->trie;
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
        // record this if it is the longest match
        if (depth >= longest_match->seq_match_len) {
            longest_match->trie_match_index = offset;
            longest_match->seq_match_len = depth + 1;
        }
        // If bit 14 is also set, there is a child node after the completion string
        if ((code & TRIE_BRANCH_BIT) && st_find_longest_chain_cursor(cursor, longest_match, offset+2, depth+1))
            return true;
        // Found a match so return true!
        return true;
	}
	// Branch Node (with multiple children) if bit 14 is set
	if (code & TRIE_BRANCH_BIT) {
		code &= TRIE_CODE_MASK;
        // Find child key that matches the search buffer at the current depth
        const uint16_t cur_key = st_cursor_current_keycode(cursor);
        if (!cur_key) {
            return false;
        }
		for (; code; offset += 2, code = TDATA(offset)) {
            if (code == cur_key) {
                // 16bit offset to child node is built from next uint16_t
                const uint16_t child_offset = TDATA(offset+1);
                // Traverse down child node
                st_cursor_next(cursor);
                return st_find_longest_chain_cursor(cursor, longest_match, child_offset, depth+1);
            }
        }
        // Couldn't go deeper, so return false.
        return false;
	}
    // No high bits set, so this is a chain node
	// Travel down chain until we reach a zero byte, or we no longer match our buffer
	for (; code; depth++, st_cursor_next(cursor), code = TDATA(++offset)) {
		if (code != st_cursor_current_keycode(cursor))
			return false;
	}
	// After a chain, there should be a leaf or branch
	return st_find_longest_chain_cursor(cursor, longest_match, offset+1, depth);
}
