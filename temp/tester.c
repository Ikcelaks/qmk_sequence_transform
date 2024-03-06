// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "keycodes.h"
#include "progmem.h"
#include "keybuffer.h"
//#include "key_stack.h"
#include "trie.h"
#include "utils.h"
#include "sequence_transform.h"
#include "sequence_transform_data.h"

//////////////////////////////////////////////////////////////////
void tap_code16(uint16_t k)
{

}

/*
#ifndef SEQUENCE_TRANSFORM_GENERATOR_VERSION_0_1_0
    #error "sequence_transform_data.h was generated with an incompatible version of the generator script"
#endif

// todo get from generated h
const char special_chars[] = {'@', '*'};

//////////////////////////////////////////////////////////////////
// Key history buffer
#define KEY_BUFFER_CAPACITY SEQUENCE_MAX_LENGTH + COMPLETION_MAX_LENGTH
static st_key_action_t key_buffer_data[KEY_BUFFER_CAPACITY] = {{KC_SPC, ST_DEFAULT_KEY_ACTION}};
static st_key_buffer_t key_buffer = {
    key_buffer_data,
    KEY_BUFFER_CAPACITY,
    1
};
//////////////////////////////////////////////////////////////////
// Trie key stack
static uint16_t trie_key_stack_data[SEQUENCE_MAX_LENGTH] = {0};
static st_key_stack_t trie_stack = {
    trie_key_stack_data,
    SEQUENCE_MAX_LENGTH,
    0
};
//////////////////////////////////////////////////////////////////
// Trie node and completion data
static st_trie_t trie = {
    DICTIONARY_SIZE,
    sequence_transform_data,
    COMPLETIONS_SIZE,
    sequence_transform_completions_data,
    COMPLETION_MAX_LENGTH,
    MAX_BACKSPACES,
    &trie_stack
};*/

//////////////////////////////////////////////////////////////////////

/*
//////////////////////////////////////////////////////////////////////
uint16_t char_to_keycode(char c)
{
    for (int i = 0; i < sizeof(special_chars); ++i)
        if (c == special_chars[i])
            return SPECIAL_KEY_TRIECODE_0 + i;
    return st_char_to_keycode(c);
}
//////////////////////////////////////////////////////////////////////
void set_buffer(const char *str)
{
    st_key_buffer_reset(&key_buffer);
    for (; *str; ++str) {
        const uint16_t keycode = char_to_keycode(*str);
        st_key_buffer_push(&key_buffer, keycode);
    }
}
//////////////////////////////////////////////////////////////////////
int find_missed_rule(int search_len_start)
{
    char sequence_str[SEQUENCE_MAX_LENGTH + 1];
    char completion_str[COMPLETION_MAX_LENGTH + 1];
    st_trie_rule_t result;
    result.sequence = sequence_str;
    result.completion = completion_str;
    const int next_start = st_trie_get_rule(&trie, &key_buffer, search_len_start, &result);
    if (next_start != search_len_start) {
        printf("     rule: %s -> %s (new start: %d)\n", sequence_str, completion_str, next_start);
        // Next time, start searching from after completion
        return next_start;
    }
    return search_len_start;
}
//////////////////////////////////////////////////////////////////////
void find_rules_from(const char *str)
{
    printf("Looking for rules from: %s\n", str);
    st_key_buffer_reset(&key_buffer);
    for (int start_len = 1; *str; ++str) {
        //printf("start_len: %d, clen: %d\n", start_len, key_buffer.context_len);
        const uint16_t keycode = char_to_keycode(*str);
        st_key_buffer_push(&key_buffer, keycode);        
        if (keycode == KC_SPACE) {
            start_len = key_buffer.context_len - 1;
            continue;
        }
        // when the buffer is full, it starts rolling, so dec search len
        if (key_buffer.context_len == key_buffer.size) {
            start_len = st_max(1, start_len - 1);
        }        
        start_len = find_missed_rule(start_len);
    }
}*/
//////////////////////////////////////////////////////////////////////
int main()
{
    /*find_rules_from("something");
    find_rules_from("them");    
    find_rules_from("boat");
    find_rules_from("obviously");
    find_rules_from("judgment");
    find_rules_from("treatment");
    find_rules_from("I'm");
    find_rules_from("true");
    find_rules_from("obviously the time obviously the time");
    find_rules_from("test test test test test test");
    find_rules_from("the the the the the the");*/
}
