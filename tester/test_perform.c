#include "st_defaults.h"
#include "qmk_wrapper.h"
#include "sequence_transform.h"
#include "sim_output_buffer.h"
#include "tester.h"
#include "utils.h"

//////////////////////////////////////////////////////////////////
// (partial) simulation of process_sequence_transform logic
// to test st_perform
void sim_st_perform(const uint8_t *triecodes)
{
    // we don't use st_key_buffer_reset(buf) here because
    // we don't nec want a space at the start of the buffer
    st_key_buffer_t *buf = st_get_key_buffer();
    buf->size = 0;
    sim_output_reset();
    for (uint8_t key = *triecodes; key; key = *++triecodes) {
        st_key_buffer_push(buf, key);
        // If st_perform doesn't do anything special with this key,
        // add it to our virtual output buffer
        if (!st_perform()) {
            tap_code16(st_triecode_to_keycode(key, TEST_KC_SEQ_TOKEN_0));
        }
    }
}
//////////////////////////////////////////////////////////////////////
void test_perform(const st_test_rule_t *rule, st_test_result_t *res)
{
    sim_st_perform(rule->seq_triecodes);
    // Ignore spaces at the start of output
    const char *output = sim_output_get(true);
    // Check if our output buffer matches the expected transform string
    if (strcmp(output, rule->transform_str)) {
        RES_FAIL("output: %s", output);
    }
}
