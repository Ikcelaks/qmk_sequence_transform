// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "qmk_wrapper.h"
#include "triecodes.h"
#include "key_stack.h"
#include "utils.h"
#include "st_assert.h"

//////////////////////////////////////////////////////////////////////
void st_key_stack_reset(st_key_stack_t *s)
{
    s->size = 0;
}
//////////////////////////////////////////////////////////////////////
void st_key_stack_push(st_key_stack_t *s, uint8_t key)
{
    st_assert(s->size < s->capacity, "Stack overflow!");
    if (s->size < s->capacity) {
        s->buffer[s->size++] = key;
    }
}
//////////////////////////////////////////////////////////////////////
void st_key_stack_pop(st_key_stack_t *s)
{
    st_assert(s->size, "Stack underflow!");
    s->size = st_max(0, s->size - 1);
}
//////////////////////////////////////////////////////////////////////
void st_key_stack_to_str(const st_key_stack_t *s, char *str)
{
    char *dst = str;
    for (int i = s->size - 1; i >= 0; --i) {
        *dst++ = st_triecode_to_ascii(s->buffer[i]);
    }
    str[s->size] = 0;
}
//////////////////////////////////////////////////////////////////////
// returns true if there are any sequence tokens in the stack
// before the most recent key
bool st_stack_has_unexpanded_seq(const st_key_stack_t *s)
{
    for (int i = 1; i < s->size; ++i) {
        const uint8_t code = s->buffer[i];
        if (st_is_seq_token_triecode(code))
            return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////
// These are (currently) only used by the tester,
// so let's not compile them into the firmware.
#ifdef ST_TESTER

//////////////////////////////////////////////////////////////////////
int st_key_stack_cmp(const st_key_stack_t *s1,
                     const st_key_stack_t *s2,
                     bool same_dir)
{
    if (s1->size != s2->size) {
        return 1;
    }
    for (int i = 0; i < s1->size; ++i) {
        int j = same_dir ? i : s1->size - 1 - i;
        if (s1->buffer[i] != s2->buffer[j]) {
            return 1;
        }
    }
    return 0;
}
//////////////////////////////////////////////////////////////////////
int st_key_stack_cmp_buf(const st_key_stack_t *stack, const uint8_t *buf)
{
    for (int i = 0; i < stack->size; ++i) {
        if (stack->buffer[i] != st_get_metachar_example_triecode(buf[i])) {
            return 1;
        }
    }
    return 0;
}
//////////////////////////////////////////////////////////////////////
void st_key_stack_print(const st_key_stack_t *s)
{
    uprintf("output: |");
    for (int i = 0; i < s->size; ++i) {
        const uint8_t code = s->buffer[i];
        const char *token = st_get_seq_token_utf8(code);
        if (token) {
            uprintf("%s", token);
        } else {
            uprintf("%c", code);
        }
    }
    uprintf("| (%d)\n", s->size);
}
//////////////////////////////////////////////////////////////////////
void st_key_stack_to_utf8(const st_key_stack_t *s, char *str)
{
    for (int i = 0; i < s->size; ++i) {
        const uint8_t code = s->buffer[i];
        const char *token = st_get_trans_token_utf8(code);
        if (token) {
            while ((*str++ = *token++));
            str--;
        } else {
            *str++ = (char)code;
        }
    }
    *str = 0;
}

#endif // ST_TESTER
