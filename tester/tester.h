// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
#pragma once

// Strings used to store rule search result from callback
extern char missed_rule_seq[128];
extern char missed_rule_transform[128];
// Virtual output
extern st_key_stack_t sim_output;

typedef enum {
    ACTION_TEST_ALL_RULES,
    ACTION_TEST_ASCII_STRING,
} st_test_action_t;

typedef enum {
    TEST_FAIL,
    TEST_WARN,
    TEST_OK,
} st_result_code_t;

// Utility macros to set result code/message
#define RES_WARN(...) { res->code = TEST_WARN; \
    snprintf(res->message, sizeof(res->message), __VA_ARGS__); }

#define RES_FAIL(...) { res->code = TEST_FAIL; \
    snprintf(res->message, sizeof(res->message), __VA_ARGS__); }

typedef struct {
    // null terminated triecode arrays
    const uint8_t * const   sequence;
    const uint8_t * const   transform;
} st_test_rule_t;

typedef struct {
    st_result_code_t    code;
    char                message[1024];
} st_test_result_t;

typedef void (*st_test_func_t)(const st_test_rule_t *, st_test_result_t *);

typedef struct {
    st_test_func_t      func;
    const char          *name;
    st_test_result_t    res;
} st_test_info_t;

typedef struct {
    int     action;
    char    *user_str;
    char    *tests;
    bool    print_all;
} st_test_options_t;

typedef int (*st_test_action_func_t)(const st_test_options_t *);

void    print_available_tests(void);

//      Internal
void    sim_st_perform(const uint8_t *sequence);

//      Rule tests
void    test_perform(const st_test_rule_t *rule, st_test_result_t *res);
void    test_virtual_output(const st_test_rule_t *rule, st_test_result_t *res);
void    test_cursor(const st_test_rule_t *rule, st_test_result_t *res);
void    test_backspace(const st_test_rule_t *rule, st_test_result_t *res);
void    test_find_rule(const st_test_rule_t *rule, st_test_result_t *res);
int     test_rule(const st_test_rule_t *rule, bool *tests, bool print_all, int *warns);

//      Test Actions
int     test_all_rules(const st_test_options_t *options);
int     test_ascii_string(const st_test_options_t *options);
