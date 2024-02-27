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
// Private helper functions
int get_real_index(key_buffer_t *buf, int index)
{
    if (index < 0) {
        index += buf->context_len;
    }
    if (index > buf->context_len || index < 0) {
        uprintf("Accessing index (%d) outside valid range (-%d, %d)!", index, buf->context_len, buf->context_len);
        return -1;
    }
    index = buf->cur_pos - index;
    if (index < 0) {
        index += buf->size;
    }
    return index;
}

void resize_context(key_buffer_t *buf, int delta)
{
    if (delta < -buf->size || delta > buf->size) {
        uprintf("ABS of buffer resize request (%d) exceeds total available space index (%d)!", delta, buf->size);
        // do nothing; should never happen;
        return;
    }
    const int new_context_len = buf->context_len + delta;
    if (new_context_len < 0) {
        buf->context_len = 0;
    } else if (new_context_len > buf->size) {
        buf->context_len = buf->size;
    } else {
        buf->context_len = new_context_len;
    }
    const int new_pos = buf->cur_pos + delta;
    if (new_pos < 0) {
        buf->cur_pos = new_pos + buf->size;
    } else if (new_pos > buf->size) {
        buf->cur_pos = new_pos - buf->size;
    } else {
        buf->cur_pos = new_pos;
    }
    // We do not initialize the key_action_t item at the new cur_pos,
    // because the calling function will immediately fill it with data.
}

//////////////////////////////////////////////////////////////////
// Public
//////////////////////////////////////////////////////////////////
// buffer indexing from start or end (with negative index)
// returns 0 if index is out of bounds
//
struct key_action_t* st_key_buffer_get(st_key_buffer_t *buf, int index)
{
    int real_index = get_real_index(buf, index);
    if (real_index < 0) { // index was out of bounds
        return NULL;
    }
    return &buf->data[index];
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_reset(st_key_buffer_t *buf)
{
    buf->context_len = 0;
    key_buffer_push(buf, KC_SPC);
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
    resize_context(buf, 1);
    buf->data[buf->cur_pos].keypressed = keycode;
    buf->data[buf->cur_pos].match_offset = 0xffff;
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    st_key_buffer_print(buf);
#endif
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_pop(st_key_buffer_t *buf, uint8_t num)
{
    resize_context(buf, -num);
}
//////////////////////////////////////////////////////////////////
void st_key_buffer_print(st_key_buffer_t *buf)
{
    uprintf("buffer: |");
    for (int i = 0; i < buf->context_len; ++i)
        uprintf("%c", st_keycode_to_char(buf->data[i]));
    uprintf("| (%d)\n", buf->context_len);
}
