// Copyright 2021 Google LLC
// Copyright 2021 @filterpaper
// Copyright 2023 Pablo Martinez (@elpekenin) <elpekenin@elpekenin.dev>
// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
// Original source/inspiration: https://getreuer.info/posts/keyboards/autocorrection
#include <stdint.h>
#include "quantum_keycodes.h"
#include "keybuffer.h"
#include "utils.h"
#include "print.h"


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
uint16_t st_key_buffer_get_keycode(st_key_buffer_t *buf, int index)
{
    const struct st_key_action_t* keyaction = st_key_buffer_get(buf, index);
    return (keyaction ? keyaction->keypressed : KC_NO);
}
//////////////////////////////////////////////////////////////////
struct st_key_action_t* st_key_buffer_get(st_key_buffer_t *buf, int index)
{
    if (index < 0) {
        index += buf->context_len;
    }
    if (index >= buf->context_len || index < 0) {
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
        uprintf("Accessing index (%d) outside valid range (-%d, %d)!\n", index, buf->context_len, buf->context_len);
#endif
        return NULL;
    }
    return &buf->data[
        buf->cur_pos >= index
            ? buf->cur_pos - index
            : buf->cur_pos - index + buf->size //wrap around
    ];
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_reset(st_key_buffer_t *buf)
{
    buf->context_len = 0;
    st_key_buffer_push(buf, KC_SPC);
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_push(st_key_buffer_t *buf, uint16_t keycode)
{
    // Store all alpha chars as lowercase
    const bool shifted = keycode & QK_LSFT;
    const uint8_t lowkey = keycode & 0xFF;
    if (shifted && IS_ALPHA_KEYCODE(lowkey))
        keycode = lowkey;
    if (buf->context_len < buf->size) {
        buf->context_len++;
    }
    if (++buf->cur_pos >= buf->size) {
        buf->cur_pos = 0;
    }
    buf->data[buf->cur_pos].keypressed = keycode;
    buf->data[buf->cur_pos].action_taken = ST_DEFAULT_KEY_ACTION;
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    st_key_buffer_print(buf);
#endif
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_pop(st_key_buffer_t *buf, uint8_t num)
{
    const int new_context_len = buf->context_len - num;
    buf->context_len = new_context_len < 0 ? 0 : new_context_len;

    const int new_pos = buf->cur_pos - num;
    buf->cur_pos = new_pos < 0 ? new_pos + buf->size : new_pos;
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_print(st_key_buffer_t *buf)
{
    uprintf("buffer: |");
    for (int i = 0; i < buf->context_len; ++i)
        uprintf("%c", st_keycode_to_char(st_key_buffer_get(buf, i)->keypressed));
    uprintf("| (%d)\n", buf->context_len);
}
