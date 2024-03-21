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
#include <ctype.h>

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
    st_key_buffer_push(buf, ' ');
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_push(st_key_buffer_t *buf, uint8_t triecode)
{
    // Store all alpha chars as lowercase
    if (buf->size < buf->capacity) {
        buf->size++;
    }
    if (++buf->head >= buf->capacity) {  // increment cur_pos
        buf->head = 0;               // wrap to 0
    }
    buf->data[buf->head].triecode = tolower(triecode);
    buf->data[buf->head].action_taken = ST_DEFAULT_KEY_ACTION;
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_pop(st_key_buffer_t *buf, uint8_t num)
{
    if (buf->size <= num) {
        st_key_buffer_reset(buf);
    } else {
        buf->size -= num;
    }
    buf->head -= num;
    if (buf->head < 0) {
        buf->head += buf->capacity;
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
