// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "keycodes.h"
#include "progmem.h"
#include "keybuffer.h"
#include "key_stack.h"
#include "trie.h"
#include "utils.h"
#include "sequence_transform.h"
#include "sequence_transform_data.h"
#include "sequence_transform_test.h"
#ifdef WIN32
#include <windows.h>
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
uint16_t char_to_keycode(char *c)
{
    for (int i = 0; i < sizeof(st_magic_chars); ++i) {
        if (*c == st_magic_chars[i]) {
            return SPECIAL_KEY_TRIECODE_0 + i;
        }
    }
    if (*c == st_wordbreak_char) {
        *c = ' ';
        return KC_SPACE;
    }
    return st_char_to_keycode(*c);
}
//////////////////////////////////////////////////////////////////
void send_keys(const char *str, bool reset, bool print, bool rule_search)
{
    st_key_buffer_t *buf = st_get_key_buffer();
    // we don't use st_key_buffer_reset(buf) here because
    // we don't nec want a space at the start of the buffer
    if (reset) {
        buf->context_len = 0;
    }
    for (; *str; ++str) {
        char c = *str;
        const uint16_t keycode = char_to_keycode(&c);
        st_key_buffer_push(buf, keycode);
        if (print) {
            st_key_buffer_print(buf);
        }
        // If st_perform doesn't do anything special with this key,
        // add it to our virtual output buffer
        if (!st_perform()) {
            if (rule_search) {
                st_find_missed_rule();
            } else {
                output_push(c);
            }
        }
    }
}
//////////////////////////////////////////////////////////////////////
bool test_st_perform(const char *sequence, const char *transform)
{
    output_reset();
    send_keys(sequence, true, false, false);
    // ignore spaces at the start of output
    char *output = output_buffer;
    while (*output == ' ') {
        output++;
    }
    bool match = !strcmp(output, transform);
    if (match) {
        printf("[\033[0;32mPASS\033[0m] %s -> %s\n", sequence, output_buffer);
    } else {
        printf("[\033[0;31mFAIL\033[0m] %s -> %s (expected: %s)\n", sequence, output_buffer, transform);
    }    
    return match;
}
//////////////////////////////////////////////////////////////////////
void sequence_transform_on_missed_rule_user(const st_trie_rule_t *rule)
{
    printf("    Missed rule! %s -> %s\n", rule->sequence, rule->transform);
}
//////////////////////////////////////////////////////////////////////
void test_st_find_missed_rule(const char *user_string)
{
    printf("-----\nTyping: %s\n", user_string);
    send_keys(user_string, false, true, true);
}
//////////////////////////////////////////////////////////////////////
int main()
{
#ifdef WIN32
    // Set UTF8 code page for Windows console
    SetConsoleOutputCP(CP_UTF8);
#endif
    int rules = 0, fail = 0;
    for (; *st_rule_strings[rules]; ++rules) {
        if (!test_st_perform(st_rule_strings[rules][0],
                             st_rule_strings[rules][1])) {
            ++fail;
        }
    }
    if (!fail) {
        printf("\n[\033[0;32mAll %d tests passed!\033[0m]\n", rules);
    } else {
        printf("\n[\033[0;31m%d/%d tests failed!\033[0m]\n", fail, rules);
    }
    // Test rule search
    st_key_buffer_t *buf = st_get_key_buffer();
    st_key_buffer_reset(buf);
    output_reset();
    for (int i = 0; i < 5; ++i) {
        /* test_st_find_missed_rule("^the");
        test_st_find_missed_rule("^*m");
        test_st_find_missed_rule("^time");
        test_st_find_missed_rule("^judgment");*/
        test_st_find_missed_rule("^first");
    }
    return fail ? 1 : 0;
}
