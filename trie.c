// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "st_defaults.h"
#include "qmk_wrapper.h"
#include "st_debug.h"
#include "st_assert.h"
#include <ctype.h>
#include "triecodes.h"
#include "keybuffer.h"
#include "key_stack.h"
#include "trie.h"
#include "cursor.h"
#include "utils.h"

//////////////////////////////////////////////////////////////////////
uint8_t st_get_trie_data_byte(const st_trie_t *trie, int index)
{
    st_assert(0 <= index && index < trie->data_size,
        "Tried reading outside trie data! index: %d, size: %d",
        index, trie->data_size);
    return pgm_read_byte(&trie->data[index]);
}
//////////////////////////////////////////////////////////////////////
uint16_t st_get_trie_data_word(const st_trie_t *trie, int index)
{
    st_assert(0 <= index && index + 1 < trie->data_size,
        "Tried reading outside trie data! index: %d, size: %d",
        index, trie->data_size);
    return (pgm_read_byte(&trie->data[index]) << 8) + pgm_read_byte(&trie->data[index + 1]);
}
//////////////////////////////////////////////////////////////////////
uint8_t st_get_trie_completion_byte(const st_trie_t *trie, int index)
{
    st_assert(0 <= index && index < trie->completions_size,
        "Tried reading outside completion data! index: %d, size: %d",
        index, trie->completions_size);
    return pgm_read_byte(&trie->completions[index]);
}
//////////////////////////////////////////////////////////////////
bool st_trie_get_completion(st_cursor_t *cursor, st_trie_search_result_t *res)
{
    st_cursor_init(cursor, 0, false);
    st_find_longest_chain(cursor, &res->trie_match, 0);
#if SEQUENCE_TRANSFORM_FALLBACK_BUFFER
    if (st_cursor_init(cursor, 0, true)) {
        st_find_longest_chain(cursor, &res->trie_match, 0);
    }
#endif
    if (res->trie_match.seq_match_pos.segment_len > 0) {
        st_get_payload_from_match_index(cursor->trie, &res->trie_payload, res->trie_match.trie_match_index);
        st_debug(ST_DBG_SEQ_MATCH, "completion search res: index: %d, len: %d, bspaces: %d, func: %d\n",
            res->trie_payload.completion_index,
            res->trie_payload.completion_len,
            res->trie_payload.num_backspaces,
            res->trie_payload.func_code);
        return true;
    }
    return false;
}
//////////////////////////////////////////////////////////////////
void st_get_payload_from_match_index(const st_trie_t *trie,
                                     st_trie_payload_t *payload,
                                     uint16_t match_index)
{
    st_get_payload_from_code(payload,
        TDATA(trie, match_index),
        TDATA(trie, match_index+1),
        TDATAW(trie, match_index+2));
}
//////////////////////////////////////////////////////////////////
void st_get_payload_from_code(st_trie_payload_t *payload, uint8_t code_byte1, uint8_t code_byte2, uint16_t completion_index)
{
    // Payload data is bit-backed into 16bits:
    // (N: node type, F: func, B: backspaces, C: completion length)
    // 0b NNFF BBBB CCCC CCCC
    payload->func_code = (code_byte1 >> 4) & 3;
    payload->num_backspaces = code_byte1 & 15;
    payload->completion_len = code_byte2;
    payload->completion_index = completion_index;
}

//////////////////////////////////////////////////////////////////////
bool find_branch_offset(const st_trie_t *trie, uint16_t *offset, uint8_t code, uint8_t cur_key)
{
    for (; code; *offset += 3, code = TDATA(trie, *offset)) {
        st_debug(ST_DBG_SEQ_MATCH, " B Offset: %d; Code: %#04X; Key: %#04X\n", *offset, code, cur_key);
        if (code == cur_key) {
            // 16bit offset to child node is built from next uint16_t
            *offset = st_get_trie_data_word(trie, *offset + 1);
            return true;
        }
    }
    return 0;
}

/**
 * @brief Find longest chain in trie matching the key_buffer. (recursive)
 *
 * @param trie   trie_t struct containing trie data/size
 * @param res    result containing payload for longest sequence match
 * @param offset current offset in trie data
 * @param depth  current depth in trie
 * @return       true if match found
 */
bool st_find_longest_chain(st_cursor_t *cursor, st_trie_match_t *longest_match, uint16_t offset)
{
    const st_trie_t *trie = cursor->trie;
    bool longer_match_found = false;
    do {
        uint8_t code = TDATA(trie, offset);
        st_assert(code, "Unexpected null code! Offset: %d", offset);
        st_assert(!(code & TRIE_MATCH_BIT), "Match found at top of loop! Offset: %d", offset);

        // Branch Node (with multiple children) if bit 14 is set
        if (code & TRIE_BRANCH_BIT) {
            st_debug(ST_DBG_SEQ_MATCH, "Branching Offset: %d; Code: %#04X", offset, code);
            code = TDATA(trie, ++offset);
            // Find child key that matches the search buffer at the current depth
            const uint8_t cur_key = st_cursor_get_triecode(cursor);
            if (!cur_key || !find_branch_offset(trie, &offset, code, cur_key)) {
                // Couldn't go deeper; return.
                return longer_match_found;
            }
        } else {
            // No high bits set, so this is a chain node
            // Travel down chain until we reach a zero byte, or we no longer match our buffer
            code = TDATA(trie, ++offset);
            do {
                st_debug(ST_DBG_SEQ_MATCH, "Chaining Offset: %d; Code: %#04X\n", offset, code);
                if (code != st_cursor_get_triecode(cursor))
                    return longer_match_found;
            } while ((code = TDATA(trie, ++offset)) && st_cursor_next(cursor));
            // After a chain, there should be a match or branch
            ++offset;
        }
        // Traversed one (or more) buffer keys, check if we are at a match
        code = TDATA(trie, offset);
        // Match Node if bit 15 is set
        if (code & TRIE_MATCH_BIT) {
            st_debug(ST_DBG_SEQ_MATCH, "New Match found: (%d, %d) %d\n",
                cursor->pos.index, cursor->pos.sub_index, cursor->pos.segment_len);
            st_debug(ST_DBG_SEQ_MATCH, "Previous Match: (%d, %d) %d\n",
                longest_match->seq_match_pos.index, longest_match->seq_match_pos.sub_index, longest_match->seq_match_pos.segment_len);
            // record this if it is the longest match
            if (st_cursor_longer_than(cursor, &longest_match->seq_match_pos)) {
                longer_match_found = true;
                longest_match->trie_match_index = offset;
                longest_match->seq_match_pos = st_cursor_save(cursor);
            }
            // If bit 14 is also set, there is a child node after the completion string
            if (code & TRIE_BRANCH_BIT) {
                // move offset to next child node and continue walking the trie
                offset += 4;
                st_debug(ST_DBG_SEQ_MATCH, "  Looking for more: offset %d; code %d\n",
                    offset, TDATA(trie, offset));
            } else {
                // No more matches; return
                return longer_match_found;
            }
        }
    } while (st_cursor_next(cursor));
    return longer_match_found;
}

/**
 * @brief Performs a series of searches in the trie to try to find
 *        a rule that could have been used to get what is currently
 *        the last word in the input key buffer.
 *
 * @param trie              trie_t struct containing trie data/size
 * @param key_buffer        current user input key buffer
 * @param key_stack         stack used to record visited sequences
 * @param word_start_idx    index to space before last word in buffer
 * @param rule              pointer to rule result to fill if match found
 * @return                  true if match found that reaches end of key_buffer
 */
bool st_trie_do_rule_searches(const st_trie_t       *trie,
                              const st_key_buffer_t *key_buffer,
                              st_key_stack_t        *key_stack,
                              int                   word_start_idx,
                              st_trie_rule_t        *rule)
{
    st_debug(ST_DBG_RULE_SEARCH,
        "START OF RULE SEARCH - word_start_idx: %d\n", word_start_idx);
    // Convert word_start_index to reverse index
    const int search_base_ridx = st_clamp(key_buffer->size - word_start_idx,
                                          1, key_buffer->size - 1);
    st_trie_search_t search = {trie, key_buffer, key_stack, 0, 0, 0, rule};
    const int max_skip_levels = st_min(1 + trie->max_backspaces,
                                       SEQUENCE_TRANSFORM_RULE_SEARCH_MAX_SKIP);
    for (int i = search_base_ridx; i < key_buffer->size; ++i) {
        search.search_max_seq_len = 0;
        // For every base search_base_ridx, increase skip_levels until we find a match.
        // Each skip level allows for a rule trigger key or deleted char.
        for (search.skip_levels = 1; search.skip_levels <= max_skip_levels; ++search.skip_levels) {
            search.search_end_ridx = i + search.skip_levels;
            if (search.search_end_ridx > key_buffer->size + 1) {
                break;
            }
            st_debug(ST_DBG_RULE_SEARCH, "searching from ridx %d, skips: %d\n", i, search.skip_levels);
            key_stack->size = 0;
            if (st_trie_rule_search(&search, 0)) {
                return true;
            }
        }
    }
    return false;
}
//////////////////////////////////////////////////////////////////////
// Utility function to print debug info about a potential rule match,
// done here so we don't make a mess in st_check_rule_match and
// read stack/completion strings before we actually need to.
void debug_rule_match(const st_trie_payload_t *payload,
                      const st_trie_search_t *search,
                      uint16_t offset)
{
#if SEQUENCE_TRANSFORM_DEBUG
    const st_trie_t *trie = search->trie;
    const st_key_buffer_t *key_buffer = search->key_buffer;
    const st_key_stack_t *key_stack = search->key_stack;
    char stackstr[key_stack->size + 1];
    char compstr[payload->completion_len + 1];
    st_completion_to_str(trie, payload, compstr);
    st_key_stack_to_str(key_stack, stackstr);
    const int seq_skips = 1 + payload->num_backspaces;
    const int search_base_ridx = search->search_end_ridx - seq_skips;
    const int transform_end_ridx = search_base_ridx + payload->completion_len;
    st_debug(ST_DBG_RULE_SEARCH,
        "  checking match @%d, transform_end_ridx: %d (%send), stack: |%s|, comp: |%s|(%d bs)\n",
        offset,
        transform_end_ridx,
        transform_end_ridx != key_buffer->size ? "!" : "",
        stackstr,
        compstr,
        payload->num_backspaces);
#endif
}
//////////////////////////////////////////////////////////////////////
// Recursive trie search function used by st_trie_do_rule_searches
bool st_trie_rule_search(st_trie_search_t *search, uint16_t offset)
{
// Simulate future buffer keys by offsetting buffer access
#define OFFSET_BUFFER_VAL st_key_buffer_get_triecode(key_buffer, key_stack->size - search->search_end_ridx)
    const st_trie_t *trie = search->trie;
    const st_key_buffer_t *key_buffer = search->key_buffer;
    st_key_stack_t *key_stack = search->key_stack;
    uint8_t code = TDATA(trie, offset);
    // Match Node if bit 7 is set
    if (code & TRIE_MATCH_BIT) {
        // If bit 6 is also set, there's a child node after the completion string
        if ((code & TRIE_BRANCH_BIT) && st_trie_rule_search(search, offset + 4)) {
            return true;
        }
        // If we found a longer sequence match (but not a full rule match)
        // we don't need to test any shorter sequence matches
        if (key_stack->size < search->search_max_seq_len) {
            return false;
        }
        // If no better match found deeper,
        // inspect this payload to see if the rule would match
        st_trie_payload_t payload;
        st_get_payload_from_code(&payload, code, TDATA(trie, offset+1), TDATAW(trie, offset+2));
        // Bail if removing trigger key + backspaces would result in empty seq
        const int skips = 1 + payload.num_backspaces;
        if (key_stack->size <= skips) {
            return false;
        }
        if (st_debug_check(ST_DBG_RULE_SEARCH)) {
            debug_rule_match(&payload, search, offset);
        }
        return st_check_rule_match(&payload, search);
    }
    // BRANCH node if bit 6 is set
    if (code & TRIE_BRANCH_BIT) {
        if (key_stack->size >= search->search_end_ridx) {
            return false;
        }
        code = TDATA(trie, ++offset);
        bool res = false;
        const bool check = key_stack->size >= search->skip_levels;
        const uint8_t cur_key = check ? OFFSET_BUFFER_VAL : 0;
        // find child that matches our current buffer location
        // (if this is a skip level, we go down all children)
        for (; code; offset += 3, code = TDATA(trie, offset)) {
            if (!check || cur_key == code) {
                // Get 16bit offset to child node
                const uint16_t child_offset = TDATAW(trie, offset+1);
                // Traverse down child node
                st_key_stack_push(key_stack, code);
                res = st_trie_rule_search(search, child_offset) || res;
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
    code = TDATA(trie, ++offset);
    for (; code; code = TDATA(trie, ++offset)) {
        if (key_stack->size >= search->search_end_ridx) {
            key_stack->size = prev_stack_size;
            return false;
        }
        const bool check = key_stack->size >= search->skip_levels;
        const uint8_t cur_key = check ? OFFSET_BUFFER_VAL : 0;
        if (check && cur_key != code) {
            key_stack->size = prev_stack_size;
            return false;
        }
        st_key_stack_push(key_stack, code);
    }
    // After a chain, there should be a leaf or branch
    const bool res = st_trie_rule_search(search, offset+1);
    key_stack->size = prev_stack_size;
    return res;
}
//////////////////////////////////////////////////////////////////////
bool st_check_rule_match(const st_trie_payload_t *payload, st_trie_search_t *search)
{
    const st_trie_t *trie = search->trie;
    const st_key_stack_t *key_stack = search->key_stack;
    const st_key_buffer_t *key_buffer = search->key_buffer;
    st_trie_rule_t *res = search->result;
    // After removing 'skipped' keys (trigger and backspaces),
    // check that the stack matches the input buffer.
    const int seq_skips = 1 + payload->num_backspaces;
    const int search_base_ridx = search->search_end_ridx - seq_skips;
    st_debug(ST_DBG_RULE_SEARCH, "    testing stack:");
    for (int i = seq_skips, j = 0; i < key_stack->size; ++i, ++j) {
        const uint8_t stack_key = key_stack->buffer[i];
        const int buf_ridx = search_base_ridx - j;
        const uint8_t buf_key = st_key_buffer_get_triecode(key_buffer, -buf_ridx);
        st_debug(ST_DBG_RULE_SEARCH, " [%c, %c]",
            st_triecode_to_ascii(stack_key), st_triecode_to_ascii(buf_key));
        if (stack_key != buf_key) {
            st_debug(ST_DBG_RULE_SEARCH, " no match.\n");
            return false;
        }
    }
    // Record search_max_seq_len so we can avoid reporting
    // false positives during this run that have shorter sequences
    st_debug(ST_DBG_RULE_SEARCH, " potential match! seq_len: %d\n", key_stack->size);
    search->search_max_seq_len = key_stack->size;
    // Early return if potential transform doesn't reach end of search buffer
    const int transform_end_ridx = search_base_ridx + payload->completion_len;
    if (transform_end_ridx != key_buffer->size) {
        return false;
    }
    // If stack contains an un-expanded sequence, and this rule
    // requires backspaces, we cannot properly check this rule
    if (st_stack_has_unexpanded_seq(key_stack) && seq_skips > 1) {
        st_debug(ST_DBG_RULE_SEARCH, "    unexpanded seq!\n");
        return false;
    }
    // Check if completed string matches what comes next in our search buffer
    st_debug(ST_DBG_RULE_SEARCH, "    testing completion:");
    const int completion_end = payload->completion_index + payload->completion_len;
    for (int i = payload->completion_index, j = search_base_ridx; i < completion_end; ++i, ++j) {
        const char ascii_code = CDATA(trie, i);
        const uint8_t buf_key = st_key_buffer_get_triecode(key_buffer, -(j+1));
        st_debug(ST_DBG_RULE_SEARCH, " [%c, %c]",
            ascii_code, st_triecode_to_ascii(buf_key));
        if (ascii_code != buf_key) {
            st_debug(ST_DBG_RULE_SEARCH, " no match.\n");
            return false;
        }
    }
    st_debug(ST_DBG_RULE_SEARCH, " match!\n");
    // Save payload data
    res->payload = *payload;
    // Fill the result sequence and start of transform
    char *seq = res->sequence;
    char *transform = res->transform;
    for (int i = key_stack->size - 1; i >= 0; --i) {
        const uint8_t keycode = key_stack->buffer[i];
        const char c = st_triecode_to_ascii(keycode);
        *seq++ = c;
        if (i >= seq_skips) {
            *transform++ = c;
        }
    }
    *seq = 0;
    // Finish writing transform
    st_completion_to_str(trie, payload, transform);
    return true;
}
//////////////////////////////////////////////////////////////////////
void st_completion_to_str(const st_trie_t *trie,
                          const st_trie_payload_t *payload,
                          char *str)
{
    const uint16_t completion_end = payload->completion_index + payload->completion_len;
    for (uint16_t i = payload->completion_index; i < completion_end; ++i) {
        *str++ = CDATA(trie, i);
    }
    *str = '\0';
}
