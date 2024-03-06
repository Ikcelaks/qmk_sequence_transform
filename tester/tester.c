// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "qmk_wrappers.h"
#include "keybuffer.h"
#include "key_stack.h"
#include "trie.h"
#include "utils.h"
#include "sequence_transform.h"
#include "sequence_transform_data.h"
#include "sequence_transform_test.h"
#include "tester_utils.h"
#include "sim_output_buffer.h"
#ifdef WIN32
#include <windows.h>
#endif

#ifndef SEQUENCE_TRANSFORM_GENERATOR_VERSION_3
#  error "sequence_transform_data.h was generated with an incompatible version of the generator script"
#endif

//////////////////////////////////////////////////////////////////
// simulate sending a key to system by adding it to output buffer
void tap_code16(uint16_t keycode)
{
    switch (keycode) {
        case KC_BSPC:
            sim_output_pop(1);
            break;
        default:
            sim_output_push(st_keycode_to_char(keycode));
    }   
}
//////////////////////////////////////////////////////////////////
// (partial) simulation of process_sequence_transform logic
// to test st_perform
void sim_st_perform(const uint16_t *keycodes)
{
    // we don't use st_key_buffer_reset(buf) here because
    // we don't nec want a space at the start of the buffer
    st_key_buffer_t *buf = st_get_key_buffer();
    buf->context_len = 0;
    for (uint16_t key = *keycodes; key; key = *++keycodes) {
        st_key_buffer_push(buf, key);
        // If st_perform doesn't do anything special with this key,
        // add it to our virtual output buffer
        if (!st_perform()) {
            const char c = key == KC_SPACE ? ' ' : st_keycode_to_char(key);
            sim_output_push(c);
        }
    }
}
//////////////////////////////////////////////////////////////////
// (partial) simulation of process_sequence_transform logic
// to test enhanced backspace
void sim_st_enhanced_backspace(const uint16_t *keycodes)
{
    for (uint16_t key = *keycodes; key; key = *++keycodes) {
        tap_code16(KC_BSPC);
        st_handle_backspace();
    }
}
//////////////////////////////////////////////////////////////////////
void sequence_transform_on_missed_rule_user(const st_trie_rule_t *rule)
{
    // TODO
}
//////////////////////////////////////////////////////////////////////
bool test_rule(const st_test_rule_t *rule)
{
    char seq_str[256];
    sim_output_reset();
    sim_st_perform(rule->seq_keycodes);
    // Ignore spaces at the start of output
    char *output = sim_output_get(true);
    // Check if our output buffer matches the expected transform string
    const bool match = !strcmp(output, rule->transform_str);
    keycodes_to_utf8_str(rule->seq_keycodes, seq_str);
    bool res = match;
    if (match) {
        printf("[\033[0;32mPASS\033[0m] %s ⇒ %s\n", seq_str, output);
        // Make sure enhanced backspace handling leaves us with an empty
        // output buffer if we send one backspace for every key sent
        sim_st_enhanced_backspace(rule->seq_keycodes);
        const int out_size = sim_output_get_size();
        if (out_size) {
            printf("[\033[0;31mFAIL\033[0m] Output buffer size after backspaces: %d\n", out_size);
            res = false;
        }
    } else {
        printf("[\033[0;31mFAIL\033[0m] %s ⇒ %s (expected: %s)\n", seq_str, output, rule->transform_str);
    }    
    return res;
}
//////////////////////////////////////////////////////////////////////
int main()
{
#ifdef WIN32
    // Set UTF8 code page for Windows console
    SetConsoleOutputCP(CP_UTF8);
#endif
    int rules = 0, fail = 0;
    for (; st_test_rules[rules].transform_str; ++rules) {
        if (!test_rule(&st_test_rules[rules])) {
            ++fail;
        }
    }
    if (!fail) {
        printf("\n[\033[0;32mAll %d tests passed!\033[0m]\n", rules);
    } else {
        printf("\n[\033[0;31m%d/%d tests failed!\033[0m]\n", fail, rules);
    }
    return fail ? 1 : 0;
}
