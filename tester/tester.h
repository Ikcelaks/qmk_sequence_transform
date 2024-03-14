#pragma once

extern char missed_rule_seq[SEQUENCE_MAX_LENGTH + 1];
extern char missed_rule_transform[TRANSFORM_MAX_LEN + 1];

#define TEST_FAIL   0
#define TEST_WARN   1
#define TEST_OK     2

// Utility macros to set result code/message
#define RES_WARN(...) { res->code = TEST_WARN; \
    snprintf(res->message, sizeof(res->message), __VA_ARGS__); }

#define RES_FAIL(...) { res->code = TEST_FAIL; \
    snprintf(res->message, sizeof(res->message), __VA_ARGS__); }

typedef struct {
    int     code;
    char    message[512];
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
    bool    print_all;
} st_test_options_t;

typedef int (*st_test_action_t)(const st_test_options_t *);

//      Internal
void    sim_st_perform(const uint16_t *keycodes);

//      Rule tests
void    test_perform(const st_test_rule_t *rule, st_test_result_t *res);
void    test_virtual_output(const st_test_rule_t *rule, st_test_result_t *res);
void    test_cursor(const st_test_rule_t *rule, st_test_result_t *res);
void    test_backspace(const st_test_rule_t *rule, st_test_result_t *res);
void    test_find_rule(const st_test_rule_t *rule, st_test_result_t *res);
int     test_rule(const st_test_rule_t *rule, bool print_all);

//      Test Actions
int     test_all_rules(const st_test_options_t *options);
int     test_ascii_string(const st_test_options_t *options);
