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
    st_log_time(st_find_longest_chain(cursor, &res->trie_match, 0));
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
    // 0b NFFB BBBB CCCC CCCC
    payload->func_code = (code_byte1 >> 5) & 0x03;
    payload->num_backspaces = code_byte1 & 0x1f;
    payload->completion_len = code_byte2;
    payload->completion_index = completion_index;
}
//////////////////////////////////////////////////////////////////
void st_get_node_info(const st_trie_t *trie, st_trie_node_info_t *node_info, uint16_t *offset)
{
    // node info is bit-backed into one or two bytes:
    // (N: node type, F: func, B: backspaces, C: completion length)
    // if chain_check_count is less than 16, it will be one byte
    // 0b NNM0 CCCC
    // if chain_check_count is 16 or greater, it will be two bytes
    // 0b NNM1 CCCC CCCC CCCC
    const uint8_t byte1 = TDATA(trie, (*offset)++);
    st_debug(ST_DBG_SEQ_MATCH, "Node Info %#04X (%#04X): ", *offset-1, byte1);
    node_info->has_match = byte1 & TRIE_MATCH_BIT;
    node_info->has_branch = byte1 & TRIE_BRANCH_BIT;
    node_info->has_unchained_match = byte1 & TRIE_UNCHAINED_MATCH_BIT;
    node_info->chain_check_count = byte1 & TRIE_CHAIN_CHECK_COUNT_MASK;
    if (byte1 & TRIE_EXTENDED_HEADER_BIT) {
        node_info->chain_check_count = (node_info->chain_check_count << 8) + TDATA(trie, (*offset)++);
    }
    st_debug(ST_DBG_SEQ_MATCH, "has_match %d, has_branch %d, has_unchained_match %d, chain_match_count %d\n",
                    node_info->has_match, node_info->has_branch, node_info->has_unchained_match, node_info->chain_check_count);
}

//////////////////////////////////////////////////////////////////////
bool find_branch_offset(const st_trie_t *trie, st_cursor_t * cursor, uint16_t *offset)
{
    uint8_t key_triecode = st_cursor_get_triecode(cursor);
    if (!key_triecode) {
        return false;
    }
    for (uint8_t code = TDATA(trie, *offset); code; *offset += 3, code = TDATA(trie, *offset)) {
        st_debug(ST_DBG_SEQ_MATCH, " B Offset: %d; Code: %#04X; Key: %#04X\n", *offset, code, key_triecode);
        if (st_match_triecode(code, key_triecode)) {
            // 16bit offset to child node is built from next uint16_t
            *offset = st_get_trie_data_word(trie, *offset + 1);
            return true;
        }
    }
    return false;
}
//////////////////////////////////////////////////////////////////////
bool follow_multi_branches(const st_trie_t *trie, st_cursor_t *cursor, st_trie_match_t *longest_match, uint16_t offset)
{
    st_trie_match_type_t match_type = ST_NO_MATCH;
    uint8_t key_triecode = st_cursor_get_triecode(cursor);
    if (!key_triecode) {
        return false;
    }
    st_cursor_next(cursor);
    st_cursor_pos_t pos = st_cursor_save(cursor);
    for (uint8_t code = TDATA(trie, offset); code; offset += 3, code = TDATA(trie, offset)) {
        st_debug(ST_DBG_SEQ_MATCH, " Multi-B Offset: %d; Code: %#04X; Key: %#04X\n", offset, code, key_triecode);
        if (st_match_triecode(code, key_triecode)) {
            // 16bit offset to child node is built from next uint16_t
            st_debug(ST_DBG_SEQ_MATCH, " Multi-B MATCH Offset: %d; Code: %#04X; Key: %#04X\n", offset, code, key_triecode);
            const uint16_t child_offset = st_get_trie_data_word(trie, offset + 1);
            match_type = st_find_longest_chain(cursor, longest_match, child_offset);
            if (match_type == ST_FINAL_MATCH) {
                return ST_FINAL_MATCH;
            }
            st_cursor_restore(cursor, &pos);
        }
    }
    return match_type;
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
st_trie_match_type_t st_find_longest_chain(st_cursor_t *cursor, st_trie_match_t *longest_match, uint16_t offset)
{
    const st_trie_t *trie = cursor->trie;
    st_trie_match_type_t match_type = ST_NO_MATCH;
    do {
        st_assert(TDATA(trie, offset), "Unexpected null code! Offset: %d", offset);
        st_trie_node_info_t node_info;
        st_get_node_info(trie, &node_info, &offset);

        uint16_t match_index = st_cursor_get_matched_rule(cursor);
        if (match_index != ST_DEFAULT_KEY_ACTION) {
            // We can no longer match a chained rule. Convert to an output cursor
            // and continue looking for an anchor rule
            st_cursor_convert_to_output(cursor);
        }

        // Match Node if bit 15 is set
        if (node_info.has_match) {
            if (node_info.has_unchained_match) {
                st_debug(ST_DBG_SEQ_MATCH, "New Match found: (%d, %d) %d\n",
                    cursor->pos.index, cursor->pos.sub_index, cursor->pos.segment_len);
                st_debug(ST_DBG_SEQ_MATCH, "Previous Match: (%d, %d) %d\n",
                    longest_match->seq_match_pos.index, longest_match->seq_match_pos.sub_index, longest_match->seq_match_pos.segment_len);
                // record this if it is the longest match
                if (st_cursor_longer_than(cursor, &longest_match->seq_match_pos)) {
                    match_type = ST_MATCH;
                    longest_match->trie_match_index = offset;
                    longest_match->seq_match_pos = st_cursor_save(cursor);
                }
                offset += TRIE_MATCH_SIZE;
            }
            if (match_index != ST_DEFAULT_KEY_ACTION) {
                if (node_info.chain_check_count > 0) {
                    st_debug(ST_DBG_SEQ_MATCH, "Checking for sub-rule matching %#06X\n", match_index);
                    for (int i = 0; i < node_info.chain_check_count; i++) {
                        const uint16_t sub_rule_match_index = st_get_trie_data_word(trie, offset);
                        st_debug(ST_DBG_SEQ_MATCH, "  sub-rule %#06X\n", sub_rule_match_index);
                        if (match_index == sub_rule_match_index) {
                            // This sub-rule was previously matched. This chained rule
                            // must be the longest match, so we record it and return immediately
                            // The match index is at offset + 2
                            // (sub-rule-byte1 sub-rule-byte2 match-byte1 match-byte2 match-byte3 match-byte4)
                            longest_match->trie_match_index = offset + 2;
                            longest_match->seq_match_pos = st_cursor_save(cursor);
                            longest_match->is_chained_match = true;
                            return ST_FINAL_MATCH;
                        }
                        offset += TRIE_CHAINED_MATCH_SIZE;
                    }
                }
            } else {
                // The currently focused key was not a match, so no sub-rule couled possibly match
                // Skip over all the chain rule checks (each is 6 bytes long)
                offset += TRIE_CHAINED_MATCH_SIZE * node_info.chain_check_count;
            }
            // If bit 14 is also set, there is a child node after the completion string
            if (node_info.has_branch) {
                // move offset to next child node and continue walking the trie
                // offset += 4;
                st_debug(ST_DBG_SEQ_MATCH, "  Looking for more: offset %d; code %d\n",
                    offset, TDATA(trie, offset));
            } else {
                // No more matches; return
                return match_type;
            }
        } else if (node_info.has_branch) {
            // Branch Node (with multiple children) if bit 14 is set
            // st_debug(ST_DBG_SEQ_MATCH, "Branching Offset: %d; Code: %#04X", offset, code);
            // code = TDATA(trie, ++offset);
            // Find child key that matches the search buffer at the current depth
            if (node_info.is_multibranch) {
                // It is possible for a key to match multiple branches, so we recursively
                // follow all matches
                return follow_multi_branches(trie, cursor, longest_match, offset) || match_type;
            }
            if (!find_branch_offset(trie, cursor, &offset)) {
                // Couldn't go deeper; return.
                return match_type;
            }
            st_cursor_next(cursor);
        } else {
            // No high bits set, so this is a chain node
            // Travel down chain until we reach a zero byte, or we no longer match our buffer
            uint8_t key_triecode;
            uint8_t code;
            while ((code = TDATA(trie, offset++)) && (key_triecode = st_cursor_get_triecode(cursor))) {
                st_debug(ST_DBG_SEQ_MATCH, "Chaining Offset: %d; Code: %#04X; Key: %#04X\n", offset, code, key_triecode);
                if (!key_triecode || !st_match_triecode(code, key_triecode))
                    return match_type;
                st_cursor_next(cursor);
            }
            if (!key_triecode) {
                return match_type;
            }
            // After a chain, there should be a match or branch
        }
    } while (true);
}
//////////////////////////////////////////////////////////////////////
void st_completion_to_str(const st_trie_t *trie,
                          const st_trie_payload_t *payload,
                          uint8_t *str)
{
    const uint16_t completion_end = payload->completion_index + payload->completion_len;
    for (uint16_t i = payload->completion_index; i < completion_end; ++i) {
        *str++ = CDATA(trie, i);
    }
    *str = '\0';
}
