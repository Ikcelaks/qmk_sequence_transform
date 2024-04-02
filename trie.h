// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
#pragma once

//////////////////////////////////////////////////////////////////
// Public API

#ifdef ST_TESTER
#   define TDATAW(trie, L) st_get_trie_data_word(trie, L)
#   define TDATA(trie, L)  st_get_trie_data_byte(trie, L)
#   define CDATA(trie, L)  st_get_trie_completion_byte(trie, L)
#else
#   define TDATAW(trie, L) ((pgm_read_byte(&trie->data[L]) << 8) + pgm_read_byte(&trie->data[L + 1]))
#   define TDATA(trie, L)  pgm_read_byte(&trie->data[L])
#   define CDATA(trie, L)  pgm_read_byte(&trie->completions[L])
#endif

#define TRIE_MATCH_BIT              0x80
#define TRIE_BRANCH_BIT             0x40
#define TRIE_UNCHAINED_MATCH_BIT    0x20
#define TRIE_EXTENDED_HEADER_BIT    0x10
#define TRIE_CHAIN_CHECK_COUNT_MASK 0x0F
#define TRIE_MATCH_SIZE             4
#define TRIE_CHAINED_MATCH_SIZE     6

typedef enum {
    ST_NO_MATCH = 0,
    ST_MATCH = 1,
    ST_FINAL_MATCH = 2
} st_trie_match_type_t;

typedef struct
{
    int completion_index;   // index to start of completion string in trie_t.completions
    int completion_len;     // length of completion string
    int num_backspaces;     // number of backspaces to send before the completion string
    int func_code;          // special function code
} st_trie_payload_t;

typedef struct
{
    bool has_match;             // true if node has a match
    bool has_branch;            // true if node is a branch or there are longer
    union {                         // the 5th bit is overloaded depending on the context
        bool has_unchained_match;   // true if unchained match is present
        bool is_multibranch;        // true if the branch contains metacharacters
    };
    int  chain_check_count;     // number chained rules that can match here
} st_trie_node_info_t;

typedef struct
{
    int     index;          // buffer index of cursor position
    int     sub_index;      // Sub-position within the current buffer position
    int     segment_len;    // Number of elements traversed
    uint8_t as_output;      // True if buffer traversing the simulated output
} st_cursor_pos_t;

typedef struct
{
    int            data_size;          // size in words of data buffer
    const uint8_t  *data;              // serialized trie node data
    int            completions_size;   // size in bytes of completions data buffer
    const uint8_t  *completions;       // packed completions strings buffer
    int            completion_max_len; // max len of all completion strings
    int            max_backspaces;     // max backspaces for all completions
} st_trie_t;

typedef struct
{
    const st_key_buffer_t * const buffer;           // input buffer this cursor traverses
    const st_trie_t * const       trie;             // trie used for traversing virtual output buffer
    st_cursor_pos_t               pos;              // Contains all position info for the cursor
    st_trie_payload_t             cached_action;
    uint8_t                       cache_valid;
    int                           seq_ref_index;
} st_cursor_t;

typedef struct
{
    st_trie_payload_t payload;
    char * const      sequence;
    char * const      transform;
} st_trie_rule_t;

typedef struct
{
    uint16_t            trie_match_index;
    st_cursor_pos_t     seq_match_pos;
    bool                is_chained_match;
} st_trie_match_t;

typedef struct
{
    st_trie_match_t     trie_match;
    st_trie_payload_t   trie_payload;
} st_trie_search_result_t;

bool st_trie_get_completion(st_cursor_t *cursor, st_trie_search_result_t *res);

uint16_t st_get_trie_data_word(const st_trie_t *trie, int index);
uint8_t  st_get_trie_data_byte(const st_trie_t *trie, int index);
uint8_t  st_get_trie_completion_byte(const st_trie_t *trie, int index);

//////////////////////////////////////////////////////////////////
// Internal

typedef struct
{
    const st_trie_t * const         trie;               // trie to search in
    const st_key_buffer_t * const   key_buffer;         // key buffer to search with
    st_key_stack_t * const          key_stack;          // stack for recording visited sequences
    int                             search_end_ridx;    // reverse index to end of search window
    int                             search_max_seq_len; // length of longest matching sequence
    int                             skip_levels;	    // number of trie levels to 'skip' when searching
    st_trie_rule_t * const          result;             // pointer to result to be filled with best match
} st_trie_search_t;

void st_get_payload_from_match_index(const st_trie_t *trie, st_trie_payload_t *payload, uint16_t trie_match_index);
void st_get_payload_from_code(st_trie_payload_t *payload, uint8_t code_byte1, uint8_t code_byte2, uint16_t completion_index);
st_trie_match_type_t st_find_longest_chain(st_cursor_t *cursor, st_trie_match_t *longest_match, uint16_t offset);
