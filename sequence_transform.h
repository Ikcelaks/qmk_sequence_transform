#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "action.h"

//////////////////////////////////////////////////////////////////
// Public API
bool process_context_magic(uint16_t keycode, keyrecord_t *record, uint16_t special_key_start);

#if MAGIC_IDLE_TIMEOUT > 0
void context_magic_task(void);
#else
static inline void context_magic_task(void) {}
#endif

//////////////////////////////////////////////////////////////////
// Internal
typedef void (*trie_fallback)(void);
typedef struct
{
    int             data_size;
    const uint16_t  *data;
    int             completions_size;
    const uint8_t   *completions;
} trie_t;

typedef struct
{
    uint8_t func_code;
    uint8_t num_backspaces;
    uint8_t complete_len;
    int     completion_offset;
} trie_search_result_t;

// trie_t *get_trie(uint16_t keycode);
bool process_check(uint16_t *keycode, keyrecord_t *record, uint8_t *mods);
void enqueue_keycode(uint16_t keycode);
void reset_buffer(void);
void dequeue_keycodes(uint8_t num);
bool find_longest_chain(trie_t *trie, trie_search_result_t *res, int offset, int depth);
void record_send_key(uint16_t keycode);
void repeat_key_fallback(void);
void handle_repeat_key(void);
void handle_result(trie_t *trie, trie_search_result_t *res);
bool perform_magic(void);
