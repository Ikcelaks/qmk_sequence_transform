// Copyright 2021 Google LLC
// Copyright 2021 @filterpaper
// Copyright 2023 Pablo Martinez (@elpekenin) <elpekenin@elpekenin.dev>
// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
// Original source/inspiration: https://getreuer.info/posts/keyboards/autocorrection
#pragma once

//////////////////////////////////////////////////////////////////
// Public API

typedef struct
{
    uint16_t    completion_index;   // index to start of completion string in trie_t.completions
    int     completion_len;     // length of completion string
    int     num_backspaces;     // number of backspaces to send before the completion string
    int     func_code;          // special function code
} st_trie_payload_t;

typedef struct
{
    int     index;                // buffer index of cursor position
    int     sub_index;            // Sub-position within the current buffer position
    int     segment_len;        // Number of elements traversed
    uint8_t as_output;   // True if buffer traversing the simulated output
} st_cursor_pos_t;

typedef struct
{
    size_t          data_size;          // size in words of data buffer
    const uint16_t  *data;              // serialized trie node data
    size_t          completions_size;   // size in bytes of completions data buffer
    const uint8_t   *completions;       // packed completions strings buffer
    uint8_t         completion_max_len; // max len of all completion strings
    uint8_t         max_backspaces;     // max backspaces for all completions
    st_key_stack_t * const  key_stack;  // key stack used for searches
} st_trie_t;

typedef struct
{
    st_key_buffer_t const * const buffer;           // input buffer this cursor traverses
    st_trie_t const * const       trie;             // trie used for traversing virtual output buffer
    st_cursor_pos_t               cursor_pos;       // Contains all position info for the cursor
    st_trie_payload_t             cached_action;
    uint8_t                       cache_valid;
} st_cursor_t;

typedef struct
{
    st_trie_payload_t   payload;
    char                *sequence;
    char                *transform;
} st_trie_rule_t;

typedef struct
{
    uint16_t            trie_match_index;
    st_cursor_pos_t     seq_match_pos;
} st_trie_match_t;

typedef struct
{
    st_trie_match_t     trie_match;
    st_trie_payload_t   trie_payload;
} st_trie_search_result_t;

bool st_trie_get_completion(st_cursor_t *cursor, st_trie_search_result_t *res);
bool st_trie_do_rule_searches(st_trie_t *trie, const st_key_buffer_t *key_buffer, int word_start_idx, st_trie_rule_t *rule);

//////////////////////////////////////////////////////////////////
// Internal

typedef struct
{
    st_trie_t               *trie;                  // trie to search
    const st_key_buffer_t   *key_buffer;            // search buffer
    int                     search_end_ridx;        // reverse index to end of search window
    int                     skip_levels;	        // number of trie levels to 'skip' when searching
    st_trie_rule_t          *result;                // pointer to result to be filled with best match
} st_trie_search_t;

void st_get_payload_from_match_index(const st_trie_t *trie, st_trie_payload_t *payload, uint16_t trie_match_index);
void st_get_payload_from_code(st_trie_payload_t *payload, uint16_t code, uint16_t completion_index);
bool st_trie_rule_search(st_trie_search_t *search, uint16_t offset);
bool st_find_longest_chain(st_cursor_t *cursor, st_trie_match_t *longest_match, uint16_t offset);
void st_completion_to_str(const st_trie_t *trie, const st_trie_payload_t *payload, char *str);
bool st_check_rule_match(const st_trie_payload_t *payload, st_trie_search_t *search);
