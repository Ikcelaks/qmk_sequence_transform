// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "st_defaults.h"
#include "qmk_wrapper.h"
#include "sequence_transform.h"
#include "tester.h"
#include "tester_utils.h"

// Stack containing cursor vout triecodes
static uint8_t cursor_stack_buffer[256] = {0};
static st_key_stack_t cursor_vout = {
    cursor_stack_buffer,
    256,
    0
};

//////////////////////////////////////////////////////////////////////
void get_cursor_virtual_output(st_key_stack_t *key_stack)
{
    st_key_stack_reset(key_stack);
    st_cursor_t *cursor = st_get_cursor();
    // init cursor for virtual output
    if (!st_cursor_init(cursor, 0, true)) {
        return;
    }
    for (; !st_cursor_at_end(cursor); st_cursor_next(cursor)) {
        const uint8_t code = st_cursor_get_triecode(cursor);
        st_key_stack_push(key_stack, code);
        if (!code) {
            break;
        }
    }
}
//////////////////////////////////////////////////////////////////////
void test_virtual_output(const st_test_rule_t *rule, st_test_result_t *res)
{
    char sim_str[256] = {0}, vout_str[256] = {0};
    sim_st_perform(rule->sequence);
    get_cursor_virtual_output(&cursor_vout);
    if (st_key_stack_cmp(&cursor_vout, &sim_output, false)) {
        st_key_stack_to_utf8(&sim_output, sim_str);
        st_key_stack_to_utf8(&cursor_vout, vout_str);
        RES_FAIL("mismatch! virt: |%s| sim: |%s|", vout_str, sim_str);
    }
}
