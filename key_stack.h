// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#pragma once

typedef struct
{
    uint16_t    *buffer;
    int         capacity;
    int         size;	
} st_key_stack_t;

void st_key_stack_push(st_key_stack_t *s, uint16_t key);
void st_key_stack_pop(st_key_stack_t *s);
void st_key_stack_to_str(const st_key_stack_t *s, char *str);
