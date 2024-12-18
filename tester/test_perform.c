// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "st_defaults.h"
#include "qmk_wrapper.h"
#include "triecodes.h"
#include "sequence_transform.h"
#include "tester.h"
#include "tester_utils.h"
#include "utils.h"

//////////////////////////////////////////////////////////////////
// (partial) simulation of process_sequence_transform logic
// to test st_perform
void sim_st_perform(const uint8_t *sequence)
{
    st_key_stack_reset(&sim_output);
    // we don't use st_key_buffer_reset(buf) here because
    // we don't nec want a space at the start of the buffer
    st_key_buffer_t *buf = st_get_key_buffer();
    buf->size = 0;
    for (uint8_t triecode = *sequence; triecode; triecode = *++sequence) {
        triecode = st_get_metachar_example_triecode(triecode);
        st_key_buffer_push(buf, triecode, 0);
        // If st_perform doesn't do anything special with this key,
        // add it to our virtual output buffer
        if (!st_perform()) {
            uint16_t keycode = st_triecode_to_keycode(triecode, TEST_KC_SEQ_TOKEN_0);
            tap_code16(keycode);
        }
    }
}
//////////////////////////////////////////////////////////////////////
void test_perform(const st_test_rule_t *rule, st_test_result_t *res)
{
    char out_str[256] = {0};
    sim_st_perform(rule->sequence);
    // Check if sim output buffer matches the expected transform
    if (st_key_stack_cmp_buf(&sim_output, rule->transform)) {
        st_key_stack_to_utf8(&sim_output, out_str);
        RES_FAIL("output: %s", out_str);
    }
}
