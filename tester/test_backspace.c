// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "st_defaults.h"
#include "qmk_wrapper.h"
#include "sequence_transform.h"
#include "tester.h"

//////////////////////////////////////////////////////////////////
// (partial) simulation of process_sequence_transform logic
// to test enhanced backspace
void sim_st_enhanced_backspace(const uint8_t *triecodes)
{
    while (*triecodes++) {
        tap_code16(KC_BSPC);
        st_handle_backspace();
    }
}
//////////////////////////////////////////////////////////////////////
void test_backspace(const st_test_rule_t *rule, st_test_result_t *res)
{
    sim_st_perform(rule->sequence);
    // Make sure enhanced backspace handling leaves us with an empty
    // output buffer if we send one backspace for every key sent
    sim_st_enhanced_backspace(rule->sequence);
    if (sim_output.size != 0) {
        RES_FAIL("left %d keys in buffer!", sim_output.size);
    }
}
