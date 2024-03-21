// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
#pragma once

//////////////////////////////////////////////////////////////////
// Public API

#define ST_DEFAULT_KEY_ACTION 0xffff

typedef struct
{
    uint8_t triecode;  // bitwise OR of all matching predicates
    uint8_t pred_proxy;
} st_key_t;

typedef struct
{
    st_key_t key;
    uint16_t action;
} st_key_action_t;

typedef struct
{
    st_key_action_t * const data;       // array of keycodes
    const int               capacity;   // max buffer size
    int                     size;       // number of current keys in buffer
    int                     head;       // current head for circular access
} st_key_buffer_t;

st_key_action_t *st_key_buffer_get(const st_key_buffer_t *buf, int index);
uint8_t         st_key_buffer_get_triecode(const st_key_buffer_t *buf, int index);
uint8_t         st_key_buffer_get_pred_proxy(const st_key_buffer_t *buf, int index);
void            st_key_buffer_reset(st_key_buffer_t *buf);
void            st_key_buffer_push(st_key_buffer_t *buf, uint8_t triecode);
void            st_key_buffer_pop(st_key_buffer_t *buf, uint8_t num);
void            st_key_buffer_print(const st_key_buffer_t *buf);

#ifdef ST_TESTER
bool            st_key_buffer_has_unexpanded_seq(st_key_buffer_t *buf);
void            st_key_buffer_to_ascii_str(const st_key_buffer_t *buf, char *str);
#endif
