// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "qmk_wrapper.h"
#include "tester_utils.h"
#include "triecodes.h"

//////////////////////////////////////////////////////////////////
// expects triecodes to be null terminated
void st_triecodes_to_utf8_str(const uint8_t *triecodes, char *str)
{
    for (uint8_t code = *triecodes; code; code = *++triecodes) {
        const char *token = st_get_seq_token_utf8(code);
        if (token) {
            while ((*str++ = *token++));
            str--;
        } else {
            *str++ = (char)code;
        }
    }
    *str = 0;
}
//////////////////////////////////////////////////////////////////
// expects triecodes to be null terminated
void st_triecodes_transform_to_utf8_str(const uint8_t *triecodes, char *str)
{
    for (uint8_t code = *triecodes; code; code = *++triecodes) {
        const char *token = st_get_trans_token_utf8(code);
        if (token) {
            while ((*str++ = *token++));
            str--;
        } else {
            *str++ = (char)code;
        }
    }
    *str = 0;
}
//////////////////////////////////////////////////////////////////
// expects triecodes to be null terminated
void st_triecodes_to_ascii_str(const uint8_t *triecodes, char *str)
{
    for (uint8_t code = *triecodes; code; code = *++triecodes) {
        *str++ = st_triecode_to_ascii(code);
    }
    *str = 0;
}
