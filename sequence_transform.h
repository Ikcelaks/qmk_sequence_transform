#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "action.h"

//////////////////////////////////////////////////////////////////
// Public API
bool process_sequence_transform(uint16_t keycode, keyrecord_t *record, uint16_t special_key_start);

#if SEQUENCE_TRANSFORM_IDLE_TIMEOUT > 0
void sequence_transform_task(void);
#else
static inline void sequence_transform_task(void) {}
#endif

//////////////////////////////////////////////////////////////////
// Internal
typedef struct
{
    int             data_size;
    const uint16_t  *data;
    int             completions_size;
    const uint8_t   *completions;
} trie_t;

typedef struct
{
    uint16_t    completion_offset;
    uint8_t     complete_len;
    uint8_t     num_backspaces;
    uint8_t     func_code;      
} trie_search_result_t;

bool process_check(uint16_t *keycode, keyrecord_t *record, uint8_t *mods);
void enqueue_keycode(uint16_t keycode);
void reset_buffer(void);
void dequeue_keycodes(uint8_t num);
bool find_longest_chain(trie_t *trie, trie_search_result_t *res, uint16_t offset, uint8_t depth);
void record_send_key(uint16_t keycode);
void handle_repeat_key(void);
void handle_result(trie_t *trie, trie_search_result_t *res);
bool perform_sequence_transform(void);
