#include "qmk_wrapper.h"
#include "keybuffer.h"
#include "sequence_transform.h"
#include "sequence_transform_data.h"
#include "sequence_transform_test.h"
#include "sim_output_buffer.h"
#include "tester.h"

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
//////////////////////////////////////////////////////////////////////
void test_perform(const st_test_rule_t *rule, st_test_result_t *res)
{
    sim_st_perform(rule->seq_keycodes);
    // Ignore spaces at the start of output
    char *output = sim_output_get(true);
    // Check if our output buffer matches the expected transform string
    if (strcmp(output, rule->transform_str)) {
        RES_FAIL("output: %s", output);
    }
}
