// Copyright 2021 Google LLC
// Copyright 2021 @filterpaper
// Copyright 2023 Pablo Martinez (@elpekenin) <elpekenin@elpekenin.dev>
// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
// Original source/inspiration: https://getreuer.info/posts/keyboards/autocorrection

#include "qmk_wrapper.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "keybuffer.h"
#include "key_stack.h"
#include "trie.h"
#include "cursor.h"
#include "utils.h"

#define TRIE_MATCH_BIT      0x8000
#define TRIE_BRANCH_BIT     0x4000
#define TRIE_CODE_MASK      0x3FFF

#define BYTE_TRIE_MATCH_BIT      0x80
#define BYTE_TRIE_BRANCH_BIT     0x40
#define BYTE_TRIE_CODE_MASK      0x3F

#ifndef SEQUENCE_TRANSFORM_RULE_SEARCH_MAX_SKIP
#define SEQUENCE_TRANSFORM_RULE_SEARCH_MAX_SKIP 4
#endif

#define TDATA(L) pgm_read_word(&trie->data[L])
#define TBYTEDATA(L) pgm_read_byte(&trie->data[L])
#define CDATA(L) pgm_read_byte(&trie->completions[L])

#define SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS_2

//////////////////////////////////////////////////////////////////
bool st_trie_get_completion(st_transform_trie_t const * const transform_trie, st_cursor_t *cursor, st_trie_search_result_t *res)
{
    st_cursor_init(cursor, 0, false);
    st_find_longest_chain(cursor, &res->trie_match, 0);
    st_cursor_print(cursor);
    st_trie_match_t longest_match = {0, {0,0, 0, 0}};
    st_cursor_init(cursor, 0, true);
    st_find_longest_transform(transform_trie, cursor, &longest_match, 0);
#ifdef SEQUENCE_TRANSFORM_ENABLE_FALLBACK_BUFFER
    if (st_cursor_init(cursor, 0, true)) {
        st_find_longest_chain(cursor, &res->trie_match, 0);
    }
#endif
    if (res->trie_match.seq_match_pos.segment_len > 0) {
        st_get_payload_from_match_index(cursor->trie, &res->trie_payload, res->trie_match.trie_match_index);
        return true;
    }
    return false;
}
//////////////////////////////////////////////////////////////////
bool st_trie_check_for_unused_rule(st_transform_trie_t const * const transform_trie, st_cursor_t *cursor)
{
    st_trie_match_t longest_match = {0, {0,0, 0, 0}};
    st_cursor_init(cursor, 0, true);
    st_cursor_print(cursor);
    return st_find_longest_transform(transform_trie, cursor, &longest_match, 0);
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
uint16_t get_uint16_from_uint8s(st_transform_trie_t const * const trie, uint16_t offset)
{
    return ((uint16_t)TBYTEDATA(offset) << 8) + (uint16_t)TBYTEDATA(offset + 1);
}

//////////////////////////////////////////////////////////////////////
bool find_transform_branch_offset(st_transform_trie_t const * const trie, uint16_t *offset, uint8_t cur_key)
{
    for (uint8_t code = TBYTEDATA(++*offset); code; *offset += 3, code = TBYTEDATA(*offset)) {
        printf(" B Offset: %d; Code: %#04X; Cur_key: %#04X\n", *offset, code, cur_key);
        if (code == cur_key) {
            // 16bit offset to child node is built from next two uint8_t
            *offset = get_uint16_from_uint8s(trie, *offset+1);
            printf(" Found B Child Offset: %d; Code: %#04X; Cur_key: %#04X\n", *offset, code, cur_key);
            return true;
        }
    }
    return false;
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
bool st_find_longest_transform(st_transform_trie_t const * const trie, st_cursor_t * const cursor, st_trie_match_t *longest_match, uint16_t offset)
{
    bool longer_match_found = false;
    do {
#ifdef SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS_2
        if (offset >= trie->data_size) {
            printf("find_longest_chain() ERROR: tried reading outside trie data! Offset: %d\n", offset);
            return false;
        }
#endif
        uint8_t code = TBYTEDATA(offset);
#ifdef SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS_2
        if (!code) {
            printf("find_longest_chain() ERROR: unexpected null code! Offset: %d\n", offset);
            return false;
        }
        if (code & TRIE_MATCH_BIT) {
            printf("find_longest_chain() ERROR: match found at top of loop! Offset: %d\n", offset);
            return false;
        }
#endif
        // Branch Node (with multiple children) if bit 14 is set
        if (code & BYTE_TRIE_BRANCH_BIT) {
            code &= BYTE_TRIE_CODE_MASK;
#ifdef SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS_2
            // TODO: st_debug(ST_LOG_TRIE_SEARCH_BIT, "Branching Offset: %d; Code: %#04X\n", offset, code);
            printf("Branching Offset: %d; Code: %#04X\n", offset, code);
#endif
            // Find child key that matches the search buffer at the current depth
            const uint8_t cur_key = st_keycode_to_char_real(st_cursor_get_keycode(cursor));
            if (!cur_key || !find_transform_branch_offset(trie, &offset, cur_key)) {
                // Couldn't go deeper; return.
                return longer_match_found;
            }
        } else {
            // No high bits set, so this is a chain node
            // Travel down chain until we reach a zero byte, or we no longer match our buffer
            code = TBYTEDATA(++offset);
            do {
                uint8_t cur_key = st_keycode_to_char_real(st_cursor_get_keycode(cursor));
#ifdef SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS_2
                // TODO: st_debug(ST_LOG_TRIE_SEARCH_BIT, "Chaining Offset: %d; Code: %#04X\n", offset, code);
                printf("Chaining Offset: %d; Code: %#04X; Cur_key: %#04X\n", offset, code, cur_key);
#endif
                if (code != cur_key) {
                    printf("Chaining Miss Offset: %d; Code: %#04X; Cur_key: %#04X\n", offset, code, cur_key);
                    return longer_match_found;
                }
            } while ((code = TBYTEDATA(++offset)) && st_cursor_next(cursor));
            printf("Chaining End: %d; Code: %#04X; Cur_keycode: %#04X: cursor_pos: (%d, %d)\n", offset, code, st_cursor_get_keycode(cursor), cursor->cursor_pos.index, cursor->cursor_pos.sub_index);
            // After a chain, there should be a match or branch
            ++offset;
        }
        // Traversed one (or more) buffer keys, check if we are at a match
        code = TBYTEDATA(offset);
        printf("Checking for match Offset: %d; Code: %#04X\n", offset, code);
        // Match Node if bit 15 is set
        if (code & BYTE_TRIE_MATCH_BIT) {
#ifdef SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS_2
            // TODO: st_debug(ST_LOG_TRIE_SEARCH_BIT,...
            printf("New Match found: (%d, %d) %d\n", cursor->cursor_pos.index, cursor->cursor_pos.sub_index, cursor->cursor_pos.segment_len);
            printf("Previous Match: (%d, %d) %d\n", longest_match->seq_match_pos.index, longest_match->seq_match_pos.sub_index, longest_match->seq_match_pos.segment_len);
#endif
            uint16_t prefix_check_offset = get_uint16_from_uint8s(trie, offset + 3);
            uint16_t prefix_node = cursor->trie->data[prefix_check_offset];
            bool valid_match = false;
            if (prefix_node & TRIE_BRANCH_BIT) {
                printf("Possible prefix collision at match_node (%d): %#04X\n", prefix_check_offset, prefix_node);
                st_cursor_pos_t cur_pos = st_cursor_save(cursor);
                st_cursor_next(cursor);
                st_trie_match_t prefix_match = {0, {0, 0, 0, 0}};
                bool prefix_matched = st_find_longest_chain(cursor, &prefix_match, prefix_check_offset + 2);
                if (prefix_matched) {
                    printf("Conflicting prefix matched; false rule match\n");
                } else {
                    printf("No prefix matched; true rule match\n");
                    valid_match = true;
                }
                st_cursor_restore(cursor, &cur_pos);
            } else {
                printf("No prefix collision at match_node (%d): %#04X\n", prefix_check_offset, prefix_node);
                valid_match = true;
            }
            uint8_t expected_key_count = code & BYTE_TRIE_CODE_MASK;
            // record this if it is the longest match
            if (valid_match && (expected_key_count < cursor->cursor_pos.index + 1 || cursor->nondefault_action_count == 0)) {
                printf("Found rule (%d): expected keys %d; actual keys %d\n", get_uint16_from_uint8s(trie, offset + 1), expected_key_count, cursor->cursor_pos.index + 1);
                longer_match_found = true;
                longest_match->trie_match_index = offset;
                longest_match->seq_match_pos = st_cursor_save(cursor);
            } else if (valid_match) {
                printf("Good job! You used rule (%d) (%d;%d)\n", get_uint16_from_uint8s(trie, offset + 1), expected_key_count, cursor->cursor_pos.index + 1);
            }
            // If bit 14 is also set, there is a child node after the completion string
            if (code & BYTE_TRIE_BRANCH_BIT) {
                // move offset to next child node and continue walking the trie
                offset += 5;
                code = TBYTEDATA(offset);
            } else {
                // No more matches; return
                return longer_match_found;
            }
        }
    } while (st_cursor_next(cursor));
    return longer_match_found;
}

//////////////////////////////////////////////////////////////////////
bool find_branch_offset(const st_trie_t *trie, uint16_t *offset, uint16_t code, uint16_t cur_key)
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
bool st_find_longest_chain(st_cursor_t *cursor, st_trie_match_t *longest_match, uint16_t offset)
{
    const st_trie_t *trie = cursor->trie;
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
            // TODO: st_debug(ST_LOG_TRIE_SEARCH_BIT, "Branching Offset: %d; Code: %#04X\n", offset, code);
            uprintf("Branching Offset: %d; Code: %#04X\n", offset, code);
#endif
            code &= TRIE_CODE_MASK;
            // Find child key that matches the search buffer at the current depth
            const uint16_t cur_key = st_cursor_get_keycode(cursor);
            if (!cur_key || !find_branch_offset(trie, &offset, code, cur_key)) {
                // Couldn't go deeper; return.
                return longer_match_found;
            }
        } else {
            // No high bits set, so this is a chain node
            // Travel down chain until we reach a zero byte, or we no longer match our buffer
            do {
#ifdef SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS
                // TODO: st_debug(ST_LOG_TRIE_SEARCH_BIT, "Chaining Offset: %d; Code: %#04X\n", offset, code);
                uprintf("Chaining Offset: %d; Code: %#04X\n", offset, code);
#endif
                if (code != st_cursor_get_keycode(cursor))
                    return longer_match_found;
            } while ((code = TDATA(++offset)) && st_cursor_next(cursor));
            // After a chain, there should be a match or branch
            ++offset;
        }
        // Traversed one (or more) buffer keys, check if we are at a match
        code = TDATA(offset);
        // Match Node if bit 15 is set
        if (code & TRIE_MATCH_BIT) {
#ifdef SEQUENCE_TRANSFORM_TRIE_SANITY_CHECKS
            // TODO: st_debug(ST_LOG_TRIE_SEARCH_BIT,...
            uprintf("New Match found: (%d, %d) %d\n", cursor->cursor_pos.index, cursor->cursor_pos.sub_index, cursor->cursor_pos.segment_len);
            uprintf("Previous Match: (%d, %d) %d\n", longest_match->seq_match_pos.index, longest_match->seq_match_pos.sub_index, longest_match->seq_match_pos.segment_len);
#endif
            // record this if it is the longest match
            if (st_cursor_longer_than(cursor, &longest_match->seq_match_pos)) {
                longer_match_found = true;
                longest_match->trie_match_index = offset;
                longest_match->seq_match_pos = st_cursor_save(cursor);
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
 * @param word_start_idx    index to space before last word in buffer
 * @param rule              pointer to rule result to fill if match found
 * @return                  true if match found that reaches end of key_buffer
 */
bool st_trie_do_rule_searches(st_trie_t              *trie,
                              const st_key_buffer_t  *key_buffer,
                              int                    word_start_idx,
                              st_trie_rule_t         *rule)
{
    // Convert word_start_index to reverse index
    const int search_base_ridx = st_clamp(key_buffer->context_len - word_start_idx,
                                          1, key_buffer->context_len - 1);
    st_trie_search_t search;
    search.trie = trie;
    search.key_buffer = key_buffer;
    search.result = rule;
    const int max_skip_levels = st_min(1 + trie->max_backspaces,
                                       SEQUENCE_TRANSFORM_RULE_SEARCH_MAX_SKIP);
    for (int i = search_base_ridx; i < key_buffer->context_len; ++i) {
        // For every base search_base_ridx, increase skip_levels until we find a match.
        // Each skip level allows for a rule trigger key or deleted char.
        for (search.skip_levels = 1; search.skip_levels <= max_skip_levels; ++search.skip_levels) {
            search.search_end_ridx = i + search.skip_levels;
            //printf("searching from ridx %d, skips: %d\n", i, search.skip_levels);
            trie->key_stack->size = 0;
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
// TODO: wrap this and its call around ST_DEBUG ifdef
void debug_rule_match(const st_trie_payload_t *payload,
                      const st_trie_search_t *search,
                      uint16_t offset)
{
    st_trie_t *trie = search->trie;
    const st_key_stack_t *key_stack = trie->key_stack;
    char stackstr[key_stack->size + 1];
    char compstr[payload->completion_len + 1];
    st_completion_to_str(trie, payload, compstr);
    st_key_stack_to_str(key_stack, stackstr);
    const int search_base_ridx = search->search_end_ridx - search->skip_levels;
    const int transform_end_ridx = search_base_ridx + payload->completion_len;
    const int backspaces = payload->num_backspaces;
    uprintf("  checking match @%d, transform_end_ridx: %d, stack: |%s|, comp: |%s|(%d bs)\n",
            offset, transform_end_ridx, stackstr, compstr, backspaces);
}
//////////////////////////////////////////////////////////////////////
// Recursive trie search function used by st_trie_do_rule_searches
bool st_trie_rule_search(st_trie_search_t *search, uint16_t offset)
{
// Simulate future buffer keys by offsetting buffer access
#define OFFSET_BUFFER_VAL st_key_buffer_get_keycode(key_buffer, key_stack->size - search->search_end_ridx)
    st_trie_t *trie = search->trie;
    const st_key_buffer_t *key_buffer = search->key_buffer;
    st_key_stack_t *key_stack = trie->key_stack;
    uint16_t code = TDATA(offset);
    // Match Node if bit 15 is set
    if (code & TRIE_MATCH_BIT) {
        // If bit 14 is also set, there's a child node after the completion string
        if ((code & TRIE_BRANCH_BIT) && st_trie_rule_search(search, offset + 2)) {
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
        // TODO: wrap around ST_DEBUG ifdef
        //debug_rule_match(&payload, search, offset);
        return st_check_rule_match(&payload, search);
    }
    // BRANCH node if bit 14 is set
    if (code & TRIE_BRANCH_BIT) {
        if (key_stack->size >= search->search_end_ridx) {
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
    for (; code; code = TDATA(++offset)) {
        if (key_stack->size >= search->search_end_ridx) {
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
    const bool res = st_trie_rule_search(search, offset+1);
    key_stack->size = prev_stack_size;
    return res;
}
//////////////////////////////////////////////////////////////////////
bool stack_contains_unexpanded_seq(const st_key_stack_t *s)
{
    for (int i = 1; i < s->size; ++i) {
        const uint16_t key = s->buffer[i];
        if (st_is_seq_token_keycode(key))
            return true;
    }
    return false;
}
//////////////////////////////////////////////////////////////////////
bool st_check_rule_match(const st_trie_payload_t *payload, st_trie_search_t *search)
{
    st_trie_t *trie = search->trie;
    const st_key_stack_t *key_stack = trie->key_stack;
    const st_key_buffer_t *key_buffer = search->key_buffer;
    st_trie_rule_t *res = search->result;
    // Early return if potential transform doesn't reach end of search buffer
    const int search_base_ridx = search->search_end_ridx - search->skip_levels;
    const int transform_end_ridx = search_base_ridx + payload->completion_len;
    if (transform_end_ridx != key_buffer->context_len) {
        return false;
    }
    // If stack contains an un-expanded sequence, and this rule
    // requires backspaces, we cannot properly check this rule
    const int backspaces = payload->num_backspaces;
    if (stack_contains_unexpanded_seq(key_stack) && backspaces) {
        //printf("  untestable rule!\n");
        return false;
    }
    // Check that the stack matches the input buffer
    //printf("  testing stack: ");
    for (int i = 1 + backspaces, j = 0; i < key_stack->size; ++i, ++j) {
        const uint16_t stack_key = key_stack->buffer[i];
        const uint16_t buf_key = st_key_buffer_get_keycode(key_buffer, j+payload->completion_len);
        //printf("[%02X(%c), %02X(%c)] ", stack_key, st_keycode_to_char(stack_key),
        //        buf_key, st_keycode_to_char(buf_key));
        if (stack_key != buf_key) {
            //printf("  no match.\n");
            return false;
        }
    }
    // Check if completed string matches what comes next in our search buffer
    //printf("  testing completion: ");
    const int completion_end = payload->completion_index + payload->completion_len;
    for (int i = payload->completion_index, j = search_base_ridx; i < completion_end; ++i, ++j) {
        const char ascii_code = CDATA(i);
        const uint16_t comp_key = st_char_to_keycode(ascii_code);
        const uint16_t buf_key = st_key_buffer_get_keycode(key_buffer, -(j+1));
        //printf("[%02X(%c), %02X(%c)] ", comp_key, ascii_code,
        //        buf_key, st_keycode_to_char(buf_key));
        if (comp_key != buf_key) {
            //printf("  no match.\n");
            return false;
        }
    }
    //printf("  match!\n");
    // Save payload data
    res->payload = *payload;
    // Fill the result sequence and start of transform
    char *seq = res->sequence;
    char *transform = res->transform;
    for (int i = key_stack->size - 1; i >= 0; --i) {
        const uint16_t keycode = key_stack->buffer[i];
        const char c = st_keycode_to_char(keycode);
        *seq++ = c;
        if (i >= 1 + backspaces &&
            !(i == key_stack->size - 1 && keycode == KC_SPACE)) {
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
        *str++ = CDATA(i);
    }
    *str = '\0';
}
