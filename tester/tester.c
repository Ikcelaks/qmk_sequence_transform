// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "qmk_wrapper.h"
#include "utils.h"
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
