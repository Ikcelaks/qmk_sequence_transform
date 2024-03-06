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
#ifdef WIN32
#include <windows.h>
#endif

#ifndef SEQUENCE_TRANSFORM_GENERATOR_VERSION_3
#  error "sequence_transform_data.h was generated with an incompatible version of the generator script"
#endif

#define OUTPUT_BUFFER_CAPACITY 256
char output_buffer[OUTPUT_BUFFER_CAPACITY];
int  output_buffer_size = 0;

//////////////////////////////////////////////////////////////////
void output_reset(void)
{
    output_buffer_size = 0;
    output_buffer[0] = 0;
}
//////////////////////////////////////////////////////////////////
void output_push(char c)
{
    if (output_buffer_size < OUTPUT_BUFFER_CAPACITY - 1) {
        output_buffer[output_buffer_size++] = c;
        output_buffer[output_buffer_size] = 0;
    }       
}
//////////////////////////////////////////////////////////////////
void output_pop(int n)
{
    output_buffer_size = st_max(0, output_buffer_size - n);
    output_buffer[output_buffer_size] = 0;
}
//////////////////////////////////////////////////////////////////
// simulate sending a key to system by adding it to output buffer
void tap_code16(uint16_t keycode)
{
    switch (keycode) {
        case KC_BSPC:
            output_pop(1);
            break;
        default:
            output_push(st_keycode_to_char(keycode));
    }   
}
//////////////////////////////////////////////////////////////////
void send_keycodes(const uint16_t *keycodes, bool reset, bool print, bool rule_search)
{
    st_key_buffer_t *buf = st_get_key_buffer();
    // we don't use st_key_buffer_reset(buf) here because
    // we don't nec want a space at the start of the buffer
    if (reset) {
        buf->context_len = 0;
    }
    for (uint16_t key = *keycodes; key; key = *++keycodes) {
        st_key_buffer_push(buf, key);
        if (print) {
            st_key_buffer_print(buf);
        }
        // If st_perform doesn't do anything special with this key,
        // add it to our virtual output buffer
        if (!st_perform()) {
            if (rule_search) {
                st_find_missed_rule();
            } else {
                const char c = key == KC_SPACE ? ' '
                    : st_keycode_to_char(key);
                output_push(c);
            }
        }
    }
}
//////////////////////////////////////////////////////////////////////
bool test_st_perform(const st_test_rule_t *rule)
{
    char seq_str[256];
    output_reset();
    send_keycodes(rule->seq_keycodes, true, false, false);
    // Ignore spaces at the start of output
    char *output = output_buffer;
    while (*output++ == ' ');
    --output;
    // Check if our output buffer matches the expected transform string
    const bool match = !strcmp(output, rule->transform_str);
    keycodes_to_utf8_str(rule->seq_keycodes, seq_str);
    if (match) {
        printf("[\033[0;32mPASS\033[0m] %s ⇒ %s\n", seq_str, output);
    } else {
        printf("[\033[0;31mFAIL\033[0m] %s ⇒ %s (expected: %s)\n", seq_str, output, rule->transform_str);
    }    
    return match;
}
//////////////////////////////////////////////////////////////////////
void sequence_transform_on_missed_rule_user(const st_trie_rule_t *rule)
{
    printf("    Missed rule! %s ⇒ %s\n", rule->sequence, rule->transform);
}
//////////////////////////////////////////////////////////////////////
void test_st_find_missed_rule(const char *user_string)
{
    printf("-----\nTyping: %s\n", user_string);
    //send_keys(user_string, false, true, true);
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
        if (!test_st_perform(&st_test_rules[rules])) {
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
