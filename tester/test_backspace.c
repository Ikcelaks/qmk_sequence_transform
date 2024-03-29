#include "qmk_wrapper.h"
#include "sequence_transform.h"
#include "sequence_transform_data.h"
#include "sequence_transform_test.h"
#include "sim_output_buffer.h"
#include "tester.h"

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
void test_backspace(const st_test_rule_t *rule, st_test_result_t *res)
{
    sim_st_perform(rule->seq_keycodes);
    // Make sure enhanced backspace handling leaves us with an empty
    // output buffer if we send one backspace for every key sent
    sim_st_enhanced_backspace(rule->seq_keycodes);
    const int out_size = sim_output_get_size();
    if (out_size != 0) {
        RES_FAIL("left %d keys in buffer!", out_size);
    }
}
