#include "qmk_wrapper.h"
#include "keybuffer.h"
#include "utils.h"
#include "sequence_transform.h"
#include "sequence_transform_data.h"
#include "sequence_transform_test.h"
#include "tester_utils.h"
#include "tester.h"
#include "sim_output_buffer.h"

//////////////////////////////////////////////////////////////////
static const char *test_pass_str = "[\033[0;32mpass\033[0m]";
static const char *test_fail_str = "[\033[0;31mfail\033[0m]";

//////////////////////////////////////////////////////////////////
static st_test_info_t rule_tests[] = {
    { test_perform,     "st_perform",           { false, 0 } },
    { test_backspace,   "st_handle_backspace",  { false, 0 } },
    { test_find_rule,   "st_find_missed_rule",  { false, 0 } },
    { 0,                0,                      { false, 0 } }
};

//////////////////////////////////////////////////////////////////////
int test_rule(const st_test_rule_t *rule, bool print_all)
{
    // Call all the tests and gather results
    bool all_pass = true;
    for (int i = 0; rule_tests[i].func; ++i) {
        st_test_result_t *res = &rule_tests[i].res;
        rule_tests[i].func(rule, res);
        all_pass = all_pass && res->pass;
    }    
    if (all_pass && !print_all) {
        return all_pass;
    }
    // Print test results
    char seq_str[256];
    keycodes_to_utf8_str(rule->seq_keycodes, seq_str);    
    printf("[rule] %s ⇒ %s\n", seq_str, rule->transform_str);
    for (int i = 0; rule_tests[i].func; ++i) {
        const st_test_info_t *test = &rule_tests[i];
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
int test_all_rules(const st_test_options_t *options)
{
    int rules = 0, pass = 0;
    for (; st_test_rules[rules].transform_str; ++rules) {
        pass += test_rule(&st_test_rules[rules], options->print_all);
    }
    const int fail = rules - pass;
    if (!fail) {
        printf("\n[\033[0;32mAll %d tests passed!\033[0m]\n", rules);
    } else {
        printf("\n[\033[0;31m%d/%d tests failed!\033[0m]\n", fail, rules);
    }
    return fail;
}
