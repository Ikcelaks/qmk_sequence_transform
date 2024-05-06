// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "st_defaults.h"
#include "qmk_wrapper.h"
#include "utils.h"
#include "sequence_transform.h"
#include "triecodes.h"
#include "tester_utils.h"
#include "tester.h"

#define TRIECODE_AT(i) st_key_buffer_get_triecode(buf, (i))

//////////////////////////////////////////////////////////////////////
// Setup input buffer from rule transform so that
// it's ready for the st_find_missed_rule call.
// Fills chained_transform with an ascii string of the actual transform
// that will be tested.
// returns false if rule is untestable, true otherwise
bool setup_input_from_transform(const st_test_rule_t *rule, char *chained_transform)
{
    st_key_buffer_t *buf = st_get_key_buffer();
    // send input rule seq so we can get output transform to test
    sim_st_perform(rule->sequence);

    // Check if sequence contains an unexpanded token
    if (!st_key_buffer_has_unexpanded_seq(buf)) {
        // 'normal' rule, so we can use the transform as is
        st_triecodes_to_ascii_str(rule->transform, chained_transform);
        // send the output into input buffer
        // to simulate user typing it directly
        buf->size = 0;
        for (int i = 0; i < sim_output.size; ++i) {
            const uint8_t code = sim_output.buffer[i];
            st_key_buffer_push(buf, code, 0);
        }
        return true;
    }
    // Rule contains a sequence token before the end so
    // rule_search won't be able to find this rule
    // if we use the expanded transform.
    // We must instead convert it to a form that it could find.
    // Example: ^d@r -> developer
    // seq_prefix: ^d@
    // trans_prefix: develop
    // chained_transform: ^d@er

    // find seq_prefix/trans_prefix by backspacing non seq tokens
    uint8_t key = 0;
    do {
        tap_code16(KC_BSPC);
        st_handle_backspace();
        key = TRIECODE_AT(0);
    } while (key && !st_is_seq_token_triecode(key));
    if (!key) {
        //printf("empty seq_prefix!\n");
        return false;
    }
    // input now contains seq_prefix (^d@)
    // output now contains trans_prefix (develop)
    sim_output.buffer[sim_output.size] = 0;
    const uint8_t *trans_prefix = &sim_output.buffer[0];
    if (!strstr((char*)rule->transform, (char*)trans_prefix)) {
        // If trans_prefix is not in rule->transform,
        // this is an untestable rule.
        //char trans_prefix_str[128] = {0};
        //st_triecodes_to_ascii_str(trans_prefix, trans_prefix_str);
        //printf("trans_prefix %s is not in rule->transform!\n", trans_prefix_str);
        return false;
    }
    // add removed chars (er) to new end of sequence in input buffer
    const int out_len = strlen((char*)trans_prefix);
    const int rule_trans_len = strlen((char*)rule->transform);
    for (int i = out_len; i < rule_trans_len; ++i) {
        const uint8_t code = rule->transform[i];
        st_key_buffer_push(buf, code, 0);
    }
    // input should now contain ^d@er and is ready for searching
    st_key_buffer_to_ascii_str(buf, chained_transform);
    //printf("chained_transform: %s\n", chained_transform);
    if (!strcmp(chained_transform, (char*)rule->transform)) {
        // transform already a chained expression,
        // so nothing to test!
        //printf("transform already a chained expression!\n");
        return false;
    }
    return true;
}
//////////////////////////////////////////////////////////////////////
void test_find_rule(const st_test_rule_t *rule, st_test_result_t *res)
{
    char seq_ascii[256] = {0};
    char chained_transform[256] = {0};
    // rule search starts looking from the last space in the buffer
    // so if there is a space in the rule transform,
    // this rule is untestable (except if space is at the start or end)
    char *space = strchr((char*)rule->transform, ' ');
    if (space) {
        const int transform_len = strlen((char*)rule->transform);
        const int space_idx = space - (char*)rule->transform;
        if (space_idx && space_idx < transform_len - 1) {
            RES_WARN("untestable rule (space in transform)!");
            return;
        }
    }
    // setup input buffer from rule transform so that
    // it's ready for the st_find_missed_rule call
    if (!setup_input_from_transform(rule, chained_transform)) {
        RES_WARN("untestable rule!");
        return;
    }
    // from this new input buffer, find missed rule
    missed_rule_seq[0] = 0;
    missed_rule_transform[0] = 0;
    st_find_missed_rule();
    // Check if found rule matches ours
    st_triecodes_to_ascii_str(rule->sequence, seq_ascii);
    const int missed_rule_seq_len = strlen(missed_rule_seq);
    const int seq_ascii_len = strlen(seq_ascii);
    if (!missed_rule_seq_len) {
        RES_FAIL("found nothing!");
        return;
    }
    const int seq_dif = strcmp(missed_rule_seq, seq_ascii);
    const int trans_dif = strcmp(missed_rule_transform, chained_transform);
    if (!trans_dif && missed_rule_seq_len < seq_ascii_len) {
        RES_WARN("found shorter sequence rule: %s ⇒ %s",
                 missed_rule_seq, missed_rule_transform);
        return;
    }
    if (!trans_dif && seq_dif) {
        RES_WARN("found diff sequence for same transform: %s ⇒ %s",
                 missed_rule_seq, missed_rule_transform);
        return;
    }
    if (seq_dif || trans_dif) {
        //printf("(%s -> %s)\n", seq_ascii, chained_transform);
        RES_FAIL("found: %s ⇒ %s",
                 missed_rule_seq, missed_rule_transform);
        return;
    }
}
