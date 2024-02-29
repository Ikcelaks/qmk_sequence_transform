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
    size_t          data_size;          // size in words of data buffer
    const uint16_t  *data;              // serialized trie node data
    size_t          completions_size;   // size in bytes of completions data buffer
    const uint8_t   *completions;       // packed completions strings buffer
    uint8_t         completion_max_len; // max len of all completion strings
    uint8_t         max_backspaces;     // max backspaces for all completions
} st_trie_t;

typedef struct
{
    uint16_t    completion_index;   // index to start of completion string in trie_t.completions
    uint8_t     completion_len;     // length of completion string
    uint8_t     num_backspaces;     // number of backspaces to send before the completion string
    uint8_t     func_code;          // special function code
    uint8_t     context_match_len;
} st_trie_payload_t;

bool st_trie_get_completion(st_trie_t *trie, st_key_buffer_t *search, st_trie_payload_t *res);

//////////////////////////////////////////////////////////////////
// Internal

void st_get_payload_from_code(st_trie_payload_t *payload, uint16_t code, uint16_t completion_index);
bool st_find_longest_chain(st_trie_t *trie, st_key_buffer_t *search, st_trie_payload_t *res, uint16_t offset, uint8_t depth);
