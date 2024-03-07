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
#include "tester.h"
#include "sim_output_buffer.h"
#ifdef WIN32
#include <windows.h>
#endif

#ifndef SEQUENCE_TRANSFORM_GENERATOR_VERSION_3
#  error "sequence_transform_data.h was generated with an incompatible version of the generator script"
#endif

//////////////////////////////////////////////////////////////////
static const char *test_pass_str = "[\033[0;32mpass\033[0m]";
static const char *test_fail_str = "[\033[0;31mfail\033[0m]";

//////////////////////////////////////////////////////////////////
static st_test_info_t tests[] = {
    { test_perform,     "st_perform",           { false, 0 } },
    { test_backspace,   "st_handle_backspace",  { false, 0 } },
    { test_find_rule,   "st_find_missed_rule",  { false, 0 } },
    { 0,                0,                      { false, 0 } }
};

//////////////////////////////////////////////////////////////////
// simulate sending a key to system by adding it to output buffer
void tap_code16(uint16_t keycode)
{
    switch (keycode) {
        case KC_BSPC:
            sim_output_pop(1);
            break;
        case KC_SPACE:
            // we don't want st_keycode_to_char's translation
            // of KC_SPACE to st_wordbreak_ascii here
            sim_output_push(' ');
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
    sim_output_reset();
    for (uint16_t key = *keycodes; key; key = *++keycodes) {
        st_key_buffer_push(buf, key);
        // If st_perform doesn't do anything special with this key,
        // add it to our virtual output buffer
        if (!st_perform()) {
            tap_code16(key);
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
static char missed_rule_seq[SEQUENCE_MAX_LENGTH + 1] = {0};
static char missed_rule_transform[TRANSFORM_MAX_LEN + 1] = {0};
void sequence_transform_on_missed_rule_user(const st_trie_rule_t *rule)
{
    strcpy_s(missed_rule_seq, sizeof(missed_rule_seq), rule->sequence);
    strcpy_s(missed_rule_transform, sizeof(missed_rule_transform), rule->transform);
}
//////////////////////////////////////////////////////////////////////
// (partial) simulation of process_sequence_transform logic
// to test st_find_missed_rule
void sim_st_find_missed_rule(const uint16_t *keycodes)
{
    missed_rule_seq[0] = 0;
    missed_rule_transform[0] = 0;
    st_key_buffer_t *buf = st_get_key_buffer();
    // reset search_len_from_space by first sending a single space
    st_key_buffer_push(buf, KC_SPACE);
    st_find_missed_rule();
    // send input rule seq so we can get output transform to test
    sim_st_perform(keycodes);
    buf->context_len = 0;
    // send the output into input buffer
    // to simulate user typing it directly
    char *output = sim_output_get(false);
    for (char c = *output; c; c = *++output) {
        const uint16_t key = ascii_to_keycode(&c);
        st_key_buffer_push(buf, key);
    }
    // from this new input buffer, find missed rule
    st_find_missed_rule();
}
//////////////////////////////////////////////////////////////////////
void test_perform(const st_test_rule_t *rule, st_test_result_t *res)
{
    static char message[256];
    res->message = message;
    // Test #1: st_perform
    sim_st_perform(rule->seq_keycodes);
    // Ignore spaces at the start of output
    char *output = sim_output_get(true);
    // Check if our output buffer matches the expected transform string
    res->pass = !strcmp(output, rule->transform_str);
    if (res->pass) {
        sprintf_s(message, sizeof(message), "OK!");
    } else {
        sprintf_s(message, sizeof(message), "output: %s", output);
    }
}
//////////////////////////////////////////////////////////////////////
void test_backspace(const st_test_rule_t *rule, st_test_result_t *res)
{
    static char message[256];
    res->message = message;
    // Test #2: st_handle_backspace
    // Make sure enhanced backspace handling leaves us with an empty
    // output buffer if we send one backspace for every key sent
    sim_st_enhanced_backspace(rule->seq_keycodes);
    const int out_size = sim_output_get_size();
    res->pass = out_size == 0;
    if (res->pass) {
        sprintf_s(message, sizeof(message), "OK!");
    } else {
        sprintf_s(message, sizeof(message), "left %d keys in buffer!", out_size);
    }
}
//////////////////////////////////////////////////////////////////////
void test_find_rule(const st_test_rule_t *rule, st_test_result_t *res)
{
    static char message[256];
    res->message = message;
    // Test #3: st_find_missed_rule
    sim_st_find_missed_rule(rule->seq_keycodes);
    res->pass = !strcmp(missed_rule_transform, rule->transform_str);
    if (res->pass) {
        sprintf_s(message, sizeof(message), "OK!");
    } else {
        if (strlen(missed_rule_seq)) {
            sprintf_s(message, sizeof(message), "found: %s ⇒ %s",
                missed_rule_seq, missed_rule_transform);
        } else {
            sprintf_s(message, sizeof(message), "found nothing!");
        }
    }
}
//////////////////////////////////////////////////////////////////////
int test_rule(const st_test_rule_t *rule, bool print_all)
{
    // Call all the tests and gather results
    bool all_pass = true;
    for (int i = 0; tests[i].func; ++i) {
        st_test_result_t *res = &tests[i].res;
        tests[i].func(rule, res);
        all_pass = all_pass && res->pass;
    }    
    if (all_pass && !print_all) {
        return all_pass;
    }
    // Print test results
    char seq_str[256];
    keycodes_to_utf8_str(rule->seq_keycodes, seq_str);    
    printf("[rule] %s ⇒ %s\n", seq_str, rule->transform_str);
    for (int i = 0; tests[i].func; ++i) {
        const st_test_info_t *test = &tests[i];
        const bool pass = test->res.pass;
        if (print_all || !pass) {
            printf("%s %s() %s\n",
                   pass ? test_pass_str : test_fail_str,
                   test->name,
                   test->res.message);
        }        
    }
    puts("");
    return all_pass ? 1 : 0;
}
//////////////////////////////////////////////////////////////////////
int main()
{
#ifdef WIN32
    // Set UTF8 code page for Windows console
    SetConsoleOutputCP(CP_UTF8);
#endif
    int rules = 0, pass = 0;
    for (; st_test_rules[rules].transform_str; ++rules) {
        pass += test_rule(&st_test_rules[rules], true);
    }
    const int fail = rules - pass;
    if (!fail) {
        printf("\n[\033[0;32mAll %d tests passed!\033[0m]\n", rules);
    } else {
        printf("\n[\033[0;31m%d/%d tests failed!\033[0m]\n", fail, rules);
    }
    return fail;
}
