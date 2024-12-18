// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "st_defaults.h"
#include "qmk_wrapper.h"
#include "st_debug.h"
#include "triecodes.h"
#include "keybuffer.h"
#include "utils.h"

#define TRIECODE_AT(i) st_key_buffer_get_triecode(buf, (i))

//////////////////////////////////////////////////////////////////
// Public
//////////////////////////////////////////////////////////////////
// Most recent keypress data is at index 0.
// Positive indexes move back towards older keypresses
// Negative indexes start at the oldest keypress still in the
//   buffer and move towards more recent presses
// If you type "abc" then:
//   st_key_buffer_get(buf, 0) -> c
//   st_key_buffer_get(buf, 1) -> b
//   st_key_buffer_get(buf, 2) -> a
//   st_key_buffer_get(buf, -1) -> a
//   st_key_buffer_get(buf, -2) -> b
//   st_key_buffer_get(buf, -3) -> c
// returns KC_NO if index is out of bounds
uint8_t st_key_buffer_get_triecode(const st_key_buffer_t *buf, int index)
{
    const st_key_action_t *keyaction = st_key_buffer_get(buf, index);
    return (keyaction ? keyaction->triecode : '\0');
}
/**
 * @brief Gets an st_key_action_t from the `index` position in the key_buffer
 * @param buf st_key_buffer* receives the st_key_action
 * @param index int index starting with 0 as the most recent keypress and increasing for older keypresses
 *
 * @return true if the index points to a valid key_action in the buffer
 * @return false if index is out-of-bounds
 */
st_key_action_t *st_key_buffer_get(const st_key_buffer_t *buf, int index)
{
    if (index < 0) {
        index += buf->size;
    }
    if (index >= buf->size || index < 0) {
        return NULL;
    }
    int buf_index = buf->head - index;
    if (buf_index < 0) {
        buf_index += buf->capacity;
    }
    return &buf->data[buf_index];
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_reset(st_key_buffer_t *buf)
{
    buf->size = 0;
    buf->seq_ref_size = 0;
    st_key_buffer_push(buf, ' ', 0);
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_push(st_key_buffer_t *buf, uint8_t triecode, uint8_t key_flags)
{
    // Store all alpha chars as lowercase
    if (buf->size < buf->capacity) {
        buf->size++;
    }
    if (++buf->head >= buf->capacity) {  // increment cur_pos
        buf->head = 0;               // wrap to 0
    }
    buf->data[buf->head].triecode = triecode;
    buf->data[buf->head].key_flags = key_flags;
    buf->data[buf->head].action_taken = ST_DEFAULT_KEY_ACTION;
    st_key_buffer_push_seq_ref(buf, '\0');
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_pop(st_key_buffer_t *buf)
{
    if (buf->size <= 1) {
        st_key_buffer_reset(buf);
        return;
    } else {
        buf->size--;
    }
    buf->head--;
    if (buf->head < 0) {
        buf->head += buf->capacity;
    }
    while (buf->seq_ref_cache[buf->seq_ref_head]) {
        buf->seq_ref_size--;
        buf->seq_ref_head--;
        if (buf->seq_ref_size < 1) {
            return;
        }
        if (buf->seq_ref_head < 0) {
            buf->seq_ref_head += buf->seq_ref_capacity;
        }
    }
    if (buf->seq_ref_size > 0) {
        buf->seq_ref_size--;
        buf->seq_ref_head--;
    }
    if (buf->seq_ref_head < 0) {
        buf->seq_ref_head += buf->seq_ref_capacity;
    }
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_print(const st_key_buffer_t *buf)
{
#ifndef NO_PRINT
    uprintf("buffer: |");
    for (int i = -1; i >= -buf->size; --i) {
        const uint8_t code = TRIECODE_AT(i);
#ifdef ST_TESTER
        const char *token = st_get_seq_token_utf8(code);
        if (token) {
            uprintf("%s", token);
        } else {
            uprintf("%c", code);
        }
#else
        const char c = st_triecode_to_ascii(code);
        uprintf("%c", c);
#endif
    }
    uprintf("| (%d)\n", buf->size);
#endif
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_push_seq_ref(st_key_buffer_t *buf, uint8_t triecode)
{
    // Store all alpha chars as lowercase
    if (buf->seq_ref_head < buf->seq_ref_capacity) {
        buf->seq_ref_size++;
    }
    if (++buf->seq_ref_head >= buf->seq_ref_capacity) {  // increment cur_pos
        buf->seq_ref_head = 0;               // wrap to 0
    }
    buf->seq_ref_cache[buf->seq_ref_head] = triecode;
}
//////////////////////////////////////////////////////////////////
uint8_t st_key_buffer_get_seq_ref(const st_key_buffer_t * const buf, int index)
{
    if (index >= buf->seq_ref_size) {
        return '\0';
    }
    int i = buf->seq_ref_head - index;
    if (i < 0) {
        i += buf->seq_ref_capacity;
    }
    return buf->seq_ref_cache[i];
}
//////////////////////////////////////////////////////////////////
bool st_key_buffer_advance_seq_ref_index(const st_key_buffer_t * const buf, int *index)
{
    while (st_key_buffer_get_seq_ref(buf, *index) != '\0') {
        ++*index;
    }
    ++*index;
    return *index < buf->seq_ref_size;
}

//////////////////////////////////////////////////////////////////////
// These are (currently) only used by the tester,
// so let's not compile them into the firmware.
#ifdef ST_TESTER

//////////////////////////////////////////////////////////////////////
// returns true if there are any sequence tokens in the buffer
// before the most recent key
bool st_key_buffer_has_unexpanded_seq(st_key_buffer_t *buf)
{
    for (int i = 1; i < buf->size; ++i) {
        const uint8_t code = TRIECODE_AT(i);
        if (st_is_seq_token_triecode(code)) {
            return true;
        }
    }
    return false;
}
//////////////////////////////////////////////////////////////////
// Converts keys from buffer to null terminated ascii string
void st_key_buffer_to_ascii_str(const st_key_buffer_t *buf, char *str)
{
    for (int i = -1; i >= -buf->size; --i) {
        const uint8_t code = TRIECODE_AT(i);
        *str++ = st_triecode_to_ascii(code);
    }
    *str = 0;
}

#endif // ST_TESTER
