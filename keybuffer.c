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
// buffer indexing from start or end (with negative index)
// returns 0 if index is out of bounds
//
uint16_t st_key_buffer_get(st_key_buffer_t *buf, int index)
{
    if (index > 0) {
        return index < buf->context_len ? buf->data[index] : 0;
    } else {
        return -index <= buf->context_len ? buf->data[buf->context_len + index] : 0;
    }
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_reset(st_key_buffer_t *buf)
{
    buf->data[0] = KC_SPC;
    buf->context_len = 1;
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_push(st_key_buffer_t *buf, uint16_t keycode)
{
    // Store all alpha chars as lowercase
    const bool shifted = keycode & QK_LSFT;
    const uint8_t lowkey = keycode & 0xFF;
    if (shifted && IS_ALPHA_KEYCODE(lowkey))
        keycode = lowkey;
    // Rotate oldest character if buffer is full.
    if (buf->context_len >= buf->size) {
        memmove(buf->data, buf->data + 1, sizeof(uint16_t) * (buf->size - 1));
        buf->context_len = buf->size - 1;
    }
    buf->data[buf->context_len++] = keycode;
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    st_key_buffer_print(buf);
#endif
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_pop(st_key_buffer_t *buf, uint8_t num)
{
    buf->context_len -= MIN(num, buf->context_len);
    if (!buf->context_len)
        st_key_buffer_reset(buf);
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_print(st_key_buffer_t *buf)
{
    uprintf("buffer: |");
    for (int i = 0; i < buf->context_len; ++i)
        uprintf("%c", st_keycode_to_char(buf->data[i]));
    uprintf("| (%d)\n", buf->context_len);
}
