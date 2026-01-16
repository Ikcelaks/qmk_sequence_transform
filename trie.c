// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "st_defaults.h"
#include "qmk_wrapper.h"
#include "st_debug.h"
#include "st_assert.h"
#include "triecodes.h"
#include "keybuffer.h"
#include "key_stack.h"
#include "trie.h"
#include "cursor.h"

//////////////////////////////////////////////////////////////////////
uint8_t st_get_trie_data_byte(const st_trie_t * const trie, int index)
{
    st_assert(0 <= index && index < trie->data_size,
        "Tried reading outside trie data! index: %d, size: %d",
        index, trie->data_size);
    return pgm_read_byte(&trie->data[index]);
}
//////////////////////////////////////////////////////////////////////
uint16_t st_get_trie_data_word(const st_trie_t * const trie, int index)
{
    st_assert(0 <= index && index + 1 < trie->data_size,
        "Tried reading outside trie data! index: %d, size: %d",
        index, trie->data_size);
    return (pgm_read_byte(&trie->data[index]) << 8) + pgm_read_byte(&trie->data[index + 1]);
}
//////////////////////////////////////////////////////////////////////
uint8_t st_get_trie_completion_byte(const st_trie_t * const trie, int index)
{
    st_assert(0 <= index && index < trie->completions_size,
        "Tried reading outside completion data! index: %d, size: %d",
        index, trie->completions_size);
    return pgm_read_byte(&trie->completions[index]);
}
//////////////////////////////////////////////////////////////////
void st_get_payload_from_match_index(const st_trie_t * const trie,
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
    // 0b NFFB BBBB CCCC CCCC
    payload->func_code = (code_byte1 >> 5) & 0x03;
    payload->num_backspaces = code_byte1 & 0x1f;
    payload->completion_len = code_byte2;
    payload->completion_index = completion_index;
}
//////////////////////////////////////////////////////////////////
st_trie_node_type_t st_read_node_type(const st_trie_t * const trie, uint16_t *offset)
{
    // node info is bit-backed into one or two bytes:
    // (M: match flag, B: branch flag, A: anchor match / multi-match flag, S: sup_rule_count)
    // if sup_rule_count is less than 16, it will be one byte
    // 0b MBA0 SSSS
    // if sup_rule_count is 16 or greater, it will be two bytes
    // 0b MBA1 SSSS SSSS SSSS
    return TDATA(trie, (*offset)++);
}
//////////////////////////////////////////////////////////////////
uint16_t st_read_node_sup_rule_count(const st_trie_t * const trie, st_trie_node_type_t node_type, uint16_t *offset)
{
    // node info is bit-backed into one or two bytes:
    // (M: match flag, B: branch flag, A: anchor match / multi-match flag, S: sup_rule_count)
    // if sup_rule_count is less than 16, it will be one byte
    // 0b MBA0 SSSS
    // if sup_rule_count is 16 or greater, it will be two bytes
    // 0b MBA1 SSSS SSSS SSSS
    const uint16_t sup_rule_count = node_type & TRIE_SUP_RULE_COUNT_MASK;
    if (node_type & TRIE_EXTENDED_HEADER_BIT) {
        return (sup_rule_count << 8) + TDATA(trie, (*offset)++);
    }
    return sup_rule_count;
}
//////////////////////////////////////////////////////////////////
uint8_t st_get_node_has_match(st_trie_node_type_t node_type)
{
    return node_type & TRIE_MATCH_BIT;
}
//////////////////////////////////////////////////////////////////
uint8_t st_get_node_has_branch(st_trie_node_type_t node_type)
{
    return node_type & TRIE_BRANCH_BIT;
}
//////////////////////////////////////////////////////////////////
uint8_t st_get_node_is_multi_branch(st_trie_node_type_t node_type)
{
    return node_type & TRIE_MULTI_BRANCH_BIT;
}
//////////////////////////////////////////////////////////////////
uint8_t st_get_node_has_anchor_match(st_trie_node_type_t node_type)
{
    return node_type & TRIE_ANCHOR_MATCH_BIT;
}

//////////////////////////////////////////////////////////////////////
uint16_t st_trie_next_matching_branch_offset(const st_trie_t * const trie, uint8_t key_triecode, uint16_t *offset)
{
    for (uint8_t code = TDATA(trie, *offset); code; *offset += 3, code = TDATA(trie, *offset)) {
        st_debug(ST_DBG_SEQ_MATCH, " B Offset: %d; Code: %#04X; Key: %#04X\n", *offset, code, key_triecode);
        if (st_match_triecode(code, key_triecode)) {
            // 16bit offset to child node is built from next uint16_t
            // First advance offset to next branch for future processing
            *offset += 3;
            return TDATAW(trie, *offset - 2);
        }
    }
    return 0;
}
//////////////////////////////////////////////////////////////////////
st_trie_progress_t st_trie_match_next_single_chain(const st_trie_t * const trie, uint8_t key_triecode, uint16_t *offset)
{
    const uint8_t triecode = TDATA(trie, (*offset)++);
    st_debug(ST_DBG_SEQ_MATCH, " SC Offset: %d; Code: %#04X; Key: %#04X\n", *offset, triecode, key_triecode);
    if (!triecode) {
        return ST_SUCCESS;
    }
    if (st_match_triecode(triecode, key_triecode)) {
        return ST_CONTINUE;
    }
    return ST_FAILED;
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
st_trie_match_type_t st_find_longest_chain(const st_trie_t * const trie, st_cursor_t *cursor, st_trie_match_t *longest_match, uint16_t offset)
{
    st_trie_match_type_t match_type = ST_NO_MATCH;
    while (true) {
        uint8_t node_type = st_read_node_type(trie, &offset);

        uint16_t match_index = st_cursor_get_matched_rule(cursor);
        if (match_index != ST_DEFAULT_KEY_ACTION) {
            // We can no longer match a chained rule. Convert to an output cursor
            // and continue looking for an anchor rule
            st_cursor_convert_to_output(cursor);
        }

        // Node contains potential anchor and/or sup-rule matches
        if (st_get_node_has_match(node_type)) {
            const uint16_t sup_rule_count = st_read_node_sup_rule_count(trie, node_type, &offset);
            if (match_index != ST_DEFAULT_KEY_ACTION) {
                if (sup_rule_count > 0) {
                    st_debug(ST_DBG_SEQ_MATCH, "Checking for sup-rule matching %#06X\n", match_index);
                    for (int i = 0; i < sup_rule_count; i++) {
                        const uint16_t sup_rule_match_index = TDATAW(trie, offset);
                        st_debug(ST_DBG_SEQ_MATCH, "  sup-rule %#06X\n", sup_rule_match_index);
                        if (match_index == sup_rule_match_index) {
                            // This sup-rule was previously matched. This chained rule
                            // must be the longest match, so we record it and return immediately
                            // The match index is at offset + 2
                            // (sup-rule-byte1 sup-rule-byte2 match-byte1 match-byte2 match-byte3 match-byte4)
                            longest_match->trie_match_index = offset + 2;
                            longest_match->seq_match_pos = st_cursor_save(cursor);
                            longest_match->match_type = ST_SUB_MATCH;
                            return ST_SUB_MATCH;
                        }
                        offset += TRIE_CHAINED_MATCH_SIZE;
                    }
                }
            } else {
                // The currently focused key was not a match, so no sup-rule couled possibly match
                // Skip over all the chain rule checks (each is 6 bytes long)
                offset += TRIE_CHAINED_MATCH_SIZE * sup_rule_count;
            }
            if (st_get_node_has_anchor_match(node_type)) {
                st_debug(ST_DBG_SEQ_MATCH, "New Match found: (%d, %d)\n",
                    cursor->index, cursor->sub_index);
                st_debug(ST_DBG_SEQ_MATCH, "Previous Match: (%d, %d)\n",
                    longest_match->seq_match_pos.index, longest_match->seq_match_pos.sub_index);
                // record this if it is the longest match
                if (st_cursor_longer_than(cursor, &longest_match->seq_match_pos)) {
                    match_type = ST_ANCHOR_MATCH;
                    longest_match->trie_match_index = offset;
                    longest_match->seq_match_pos = st_cursor_save(cursor);
                    longest_match->match_type = ST_ANCHOR_MATCH;
                }
                offset += TRIE_MATCH_SIZE;
            }
            if (!st_get_node_has_branch(node_type)) {
                // No more matches; return
                return match_type;
            }
            st_debug(ST_DBG_SEQ_MATCH, "  Looking for more: offset %d\n", offset);
            continue;
        }

        uint8_t key_triecode = st_cursor_get_triecode(cursor);
        if (st_get_node_has_branch(node_type)) {
            // Branch Node (with multiple children)
            if (st_get_node_is_multi_branch(node_type)) {
                // It is possible for a key to match multiple branches, so we recursively
                // follow all matches
                st_cursor_next(cursor);
                st_cursor_t pos = st_cursor_save(cursor);
                for (uint16_t child_offset; (child_offset = st_trie_next_matching_branch_offset(trie, key_triecode, &offset));) {
                    switch (st_find_longest_chain(trie, cursor, longest_match, child_offset)) {
                        case ST_SUB_MATCH:
                            return ST_SUB_MATCH;
                        case ST_ANCHOR_MATCH:
                            match_type = ST_ANCHOR_MATCH;
                            break;
                        default:;
                    }
                    st_cursor_restore(cursor, &pos);
                }
                return match_type;
            }
            offset = st_trie_next_matching_branch_offset(trie, key_triecode, &offset);
            if (!offset) {
                return match_type;
            }
            st_cursor_next(cursor);
            continue;
        }
        // Single-child string
        // Travel down chain until we reach a zero byte, or we no longer match our buffer
        st_trie_progress_t progress;
        while ((progress = st_trie_match_next_single_chain(trie, key_triecode, &offset)) == ST_CONTINUE) {
            st_cursor_next(cursor);
            key_triecode = st_cursor_get_triecode(cursor);
        }
        if (progress == ST_FAILED) {
            return match_type;
        }
    }
}
//////////////////////////////////////////////////////////////////
bool st_trie_get_completion(const st_trie_t * const trie, st_trie_search_result_t *res)
{
    st_cursor_t cursor;
    st_cursor_init(&cursor, false);
    st_trie_match_type_t match_type;
    st_log_time_with_result(st_find_longest_chain(trie, &cursor, &res->trie_match, 0), &match_type);
    if (match_type == ST_NO_MATCH) {
        return false;
    }
    st_get_payload_from_match_index(trie, &res->trie_payload, res->trie_match.trie_match_index);
    st_debug(ST_DBG_SEQ_MATCH, "completion search res: index: %d, len: %d, bspaces: %d, func: %d\n",
        res->trie_payload.completion_index,
        res->trie_payload.completion_len,
        res->trie_payload.num_backspaces,
        res->trie_payload.func_code);
    return true;
}
