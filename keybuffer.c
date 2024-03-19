// Copyright 2021 Google LLC
// Copyright 2021 @filterpaper
// Copyright 2023 Pablo Martinez (@elpekenin) <elpekenin@elpekenin.dev>
// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
// Original source/inspiration: https://getreuer.info/posts/keyboards/autocorrection

#include "st_defaults.h"
#include "qmk_wrapper.h"
#include "st_debug.h"
#include "keybuffer.h"
#include "utils.h"
#include <ctype.h>

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
    if (st_debug_check(ST_DBG_GENERAL)) {
        st_key_buffer_print(buf);
    }
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
    uprintf("buffer: |");
    for (int i = -1; i >= -buf->size; --i)
        uprintf("%c", st_triecode_to_char(st_key_buffer_get(buf, i)->triecode));
    uprintf("| (%d)\n", buf->size);
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_to_str(const st_key_buffer_t *buf, char* output_string, uint8_t len)
{
    int i = 0;

    for (; i < len; i += 1) {
        uint16_t current_triecode = st_key_buffer_get_triecode(buf, len - i - 1);
        output_string[i] = st_triecode_to_char(current_triecode);
    }

    output_string[i] = '\0';
}
