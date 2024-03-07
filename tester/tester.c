// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "qmk_wrapper.h"
#include "utils.h"
#include "keybuffer.h"
#include "key_stack.h"
#include "trie.h"
#include "sequence_transform_data.h"
#include "sequence_transform_test.h"
#include "tester.h"
#include "sim_output_buffer.h"
#ifdef WIN32
#include <windows.h>
#endif

#ifndef SEQUENCE_TRANSFORM_GENERATOR_VERSION_3
#  error "sequence_transform_data.h was generated with an incompatible version of the generator script"
#endif

//////////////////////////////////////////////////////////////////
#define ACTION_TEST_ALL_RULES       0
#define ACTION_TEST_ASCII_STRING    1

//////////////////////////////////////////////////////////////////
static st_test_action_t actions[] = {
    [ACTION_TEST_ALL_RULES] = test_all_rules,
    [ACTION_TEST_ASCII_STRING] = test_ascii_string,
    0
};

//////////////////////////////////////////////////////////////////////
char missed_rule_seq[SEQUENCE_MAX_LENGTH + 1] = {0};
char missed_rule_transform[TRANSFORM_MAX_LEN + 1] = {0};
// rule search callback
// (overriden function)
void sequence_transform_on_missed_rule_user(const st_trie_rule_t *rule)
{
    missed_rule_seq[0] = 0;
    missed_rule_transform[0] = 0;
    strncat(missed_rule_seq, rule->sequence, SEQUENCE_MAX_LENGTH);
    strncat(missed_rule_transform, rule->transform, TRANSFORM_MAX_LEN);
}
//////////////////////////////////////////////////////////////////
// simulate sending a key to system by adding it to output buffer
// (overriden function)
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
void init_options(int argc, char **argv, st_test_options_t *options)
{
    options->action = ACTION_TEST_ALL_RULES;
    options->user_str = 0;
    options->print_all = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-p")) {
            options->print_all = true;
        } else if (!strcmp(argv[i], "-s") && i+1 < argc) {
            options->user_str = argv[i+1];
            options->action = ACTION_TEST_ASCII_STRING;
        }
    }
}
//////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
#ifdef WIN32
    // Set UTF8 code page for Windows console
    SetConsoleOutputCP(CP_UTF8);
#endif
    st_test_options_t options;
    init_options(argc, argv, &options);
    return actions[options.action](&options);
}
