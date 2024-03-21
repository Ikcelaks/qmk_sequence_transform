// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
#pragma once

typedef struct
{
    uint8_t    *buffer;
    int         capacity;
    int         size;
} st_key_stack_t;

void    st_key_stack_reset(st_key_stack_t *s);
void    st_key_stack_push(st_key_stack_t *s, uint8_t key);
void    st_key_stack_pop(st_key_stack_t *s);
void    st_key_stack_to_str(const st_key_stack_t *s, char *str);
bool    st_stack_has_unexpanded_seq(const st_key_stack_t *s);

#ifdef ST_TESTER
int     st_key_stack_cmp_buf(const st_key_stack_t *stack, const uint8_t *buf);
int     st_key_stack_cmp(const st_key_stack_t *s1, const st_key_stack_t *s2, bool same_dir);
void    st_key_stack_print(const st_key_stack_t *s);
void    st_key_stack_to_utf8(const st_key_stack_t *s, char *str);
#endif
