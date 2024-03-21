// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
#pragma once

//////////////////////////////////////////////////////////////////
// Public API

#define ST_DEFAULT_KEY_ACTION 0xffff

#define ST_PBIT_NONALPHA 0x01
#define ST_PBIT_DIGIT 0x02
#define ST_PBIT_ALPHA 0x04
#define ST_PBIT_UPPERALPHA 0x08
#define ST_PBIT_PUNCT_TERMINILNAL 0x10
#define ST_PBIT_PUNCT_CONNECTING 0x20

typedef struct
{
    uint8_t triecode;
    uint8_t preds;  // bitwise OR of all matching predicates
} st_key_info_t;

typedef struct
{
    st_key_info_t key;
    uint16_t action_taken;
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
bool            st_key_buffer_get_key(const st_key_buffer_t *buf, int index, st_key_info_t *key);
bool            st_key_buffer_matches_predicate(const st_key_buffer_t *buf, int index, uint8_t pred);
void            st_key_buffer_reset(st_key_buffer_t *buf);
void            st_key_buffer_push(st_key_buffer_t *buf, uint8_t triecode);
void            st_key_buffer_pop(st_key_buffer_t *buf, uint8_t num);
void            st_key_buffer_print(const st_key_buffer_t *buf);

#ifdef ST_TESTER
bool            st_key_buffer_has_unexpanded_seq(st_key_buffer_t *buf);
void            st_key_buffer_to_ascii_str(const st_key_buffer_t *buf, char *str);
#endif
