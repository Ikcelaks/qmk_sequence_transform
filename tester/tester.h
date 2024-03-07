#pragma once

typedef struct {
    bool    pass;
    char    *message;
} st_test_result_t;

typedef void (*test_func_t)(const st_test_rule_t *, st_test_result_t *);

typedef struct {
    test_func_t         func;
    const char          *name;
    st_test_result_t    res;
} st_test_info_t;

void    sim_st_perform(const uint16_t *keycodes);
void    sim_st_enhanced_backspace(const uint16_t *keycodes);
void    sim_st_find_missed_rule(const uint16_t *keycodes);

void    test_perform(const st_test_rule_t *rule, st_test_result_t *res);
void    test_backspace(const st_test_rule_t *rule, st_test_result_t *res);
void    test_find_rule(const st_test_rule_t *rule, st_test_result_t *res);

int     test_rule(const st_test_rule_t *rule, bool print_all);
