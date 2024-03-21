// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "st_defaults.h"
#include "qmk_wrapper.h"
#include "sequence_transform.h"
#include "triecodes.h"
#include "utils.h"
#include "tester_utils.h"
#include "tester.h"

//////////////////////////////////////////////////////////////////////
// (partial) simulation of process_sequence_transform/post_process
int test_ascii_string(const st_test_options_t *options)
{
    const int len = strlen(options->user_str);
    if (!len) {
        return 0;
    }
    st_key_stack_reset(&sim_output);
    // we don't use st_key_buffer_reset(buf) here because
    // we don't nec want a space at the start of the buffer
    st_key_buffer_t *buf = st_get_key_buffer();
    buf->size = 0;
    for (int i = 0; i < len; ++i) {
        const char c = options->user_str[i];
        printf("--- str[%d]: %c ---\n", i, c);
        const uint16_t key = c == '<' ? KC_BSPC : st_test_ascii_to_keycode(c);
         // handle backspace (ST does not call st_perform in this case)
        if (key == KC_BSPC) {
            tap_code16(key);
            st_handle_backspace();
            st_key_buffer_print(buf);
            st_key_stack_print(&sim_output);
            continue;
        }
        // send they key to the input buffer
        st_key_buffer_push(buf, st_keycode_to_triecode(key, TEST_KC_SEQ_TOKEN_0));
        st_key_buffer_print(buf);
        // let sequence transform do its thing!
        if (st_perform()) {
            // st_perform sent a transform to the output buffer
            st_key_stack_print(&sim_output);
            continue;
        }
        // st_perform didn't do anything special with this key,
        // so we must add it to the output buffer
        tap_code16(key);
        st_key_stack_print(&sim_output);
        // check for missed rule
        missed_rule_seq[0] = 0;
        missed_rule_transform[0] = 0;
        st_find_missed_rule();
        if (strlen(missed_rule_seq)) {
            printf("Missed rule: %s ⇒ %s\n",
                    missed_rule_seq, missed_rule_transform);
        }
    }
    return 0;
}
