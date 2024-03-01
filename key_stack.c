// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include "key_stack.h"
#include "utils.h"

//////////////////////////////////////////////////////////////////////
void st_key_stack_push(st_key_stack_t *s, uint16_t key)
{
    if (s->size < s->capacity)
        s->buffer[s->size++] = key;
}
//////////////////////////////////////////////////////////////////////
void st_key_stack_pop(st_key_stack_t *s)
{
    if (s->size > 0)
        s->size--;
}
//////////////////////////////////////////////////////////////////////
void st_key_stack_to_str(const st_key_stack_t *s, char *str)
{
    char *dst = str;
	for (int i = s->size - 1; i >= 0; --i)
		*dst++ = st_keycode_to_char(s->buffer[i]);
	str[s->size] = 0;
}
