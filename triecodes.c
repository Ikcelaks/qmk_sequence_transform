// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "qmk_wrapper.h"
#include "triecodes.h"
#include "sequence_transform_data.h"
#include "st_assert.h"
#include "predicates.h"
#include <ctype.h>

// Note: we bit-pack in "reverse" order to optimize loading
#define PGM_LOADBIT(mem, pos) ((pgm_read_byte(&((mem)[(pos) / 8])) >> ((pos) % 8)) & 0x01)

static const char unshifted_keycode_to_ascii_lut[53] PROGMEM = {
//                                  KC_A    KC_B    KC_C    KC_D
                                    'a',    'b',    'c',    'd',
//  KC_E    KC_F    KC_G    KC_H    KC_I    KC_J    KC_K    KC_L
    'e',    'f',    'g',    'h',    'i',    'j',    'k',    'l',
//  KC_M    KC_N    KC_O    KC_P    KC_Q    KC_R    KC_S    KC_T
    'm',    'n',    'o',    'p',    'q',    'r',    's',    't',
//  KC_U    KC_V    KC_W    KC_X    KC_Y    KC_Z    KC_1    KC_2
    'u',    'v',    'w',    'x',    'y',    'z',    '1',    '2',
//  KC_3    KC_4    KC_5    KC_6    KC_7    KC_8    KC_9    KC_0
    '3',    '4',    '5',    '6',    '7',    '8',    '9',    '0',
//  KC_ENTR KC_ESC  KC_BSPC KC_TAB  KC_SPC  KC_MINS KC_EQL  KC_LBRC
    ' ',    ' ',    ' ',    ' ',    ' ',    '-',    '=',    '[',
//  KC_RBRC KC_BSLS KC_NUHS KC_SCLN KC_QUOT KC_GRV  KC_COMM KC_DOT
    ']',    '\\',   ' ',    ';',    '\'',   '`',    ',',    '.',
//  KC_SLSH
    '/'
};
static const char shifted_keycode_to_ascii_lut[53] PROGMEM = {
//                                  KC_A    KC_B    KC_C    KC_D
                                    'A',    'B',    'C',    'D',
//  KC_E    KC_F    KC_G    KC_H    KC_I    KC_J    KC_K    KC_L
    'E',    'F',    'G',    'H',    'I',    'J',    'K',    'L',
//  KC_M    KC_N    KC_O    KC_P    KC_Q    KC_R    KC_S    KC_T
    'M',    'N',    'O',    'P',    'Q',    'R',    'S',    'T',
//  KC_U    KC_V    KC_W    KC_X    KC_Y    KC_Z    KC_EXLM KC_AT
    'U',    'V',    'W',    'X',    'Y',    'Z',    '!',    '@',
//  KC_HASH KC_DLR  KC_PERC KC_CIRC KC_AMPR KC_ASTR KC_LPRN KC_RPRN
    '#',    '$',    '%',    '^',    '&',    '*',    '(',    ')',
//  KC_ENTR KC_ESC  KC_BSPC KC_TAB  KC_SPC  KC_UNDS KC_PLUS KC_LCBR
    ' ',    ' ',    ' ',    ' ',    ' ',    '_',    '+',    '{',
//  KC_RCBR KC_PIPE KC_NUHS KC_COLN KC_DQUO KC_GRV  KC_LABK KC_RABK
    '}',    '|',    ' ',    ':',    '"',    '~',    '<',    '>',
//  KC_QUES
    '?'
};

//////////////////////////////////////////////////////////////////////
bool st_is_seq_token_keycode(uint16_t keycode, uint16_t kc_seq_token_0)
{
    const uint16_t kc_first = kc_seq_token_0;
    const uint16_t kc_last = kc_first + SEQUENCE_TOKEN_COUNT;
    return (kc_first <= keycode && keycode < kc_last);
}
//////////////////////////////////////////////////////////////////////
bool st_is_seq_token_triecode(uint8_t triecode)
{
    const uint8_t tok_first = TRIECODE_SEQUENCE_TOKEN_0;
    const uint8_t tok_last = tok_first + SEQUENCE_TOKEN_COUNT;
    return (tok_first <= triecode && triecode < tok_last);
}
//////////////////////////////////////////////////////////////////////
bool st_is_seq_metachar_triecode(uint8_t triecode)
{
    const uint8_t pred_first = TRIECODE_SEQUENCE_METACHAR_0;
    const uint8_t pred_last = pred_first + SEQUENCE_METACHAR_COUNT;
    return (pred_first <= triecode && triecode < pred_last);
}
//////////////////////////////////////////////////////////////////////
bool st_is_trans_seq_ref_triecode(uint8_t triecode)
{
    const uint8_t tok_first = TRIECODE_SEQUENCE_REF_TOKEN_0;
    const uint8_t tok_last = tok_first + SEQUENCE_REF_TOKEN_COUNT;
    return (tok_first <= triecode && triecode < tok_last);
}
////////////////////////////////////////////////////////////////////////////////
// if triecode is a token that can be translated back to its user symbol,
// returns its ascii code, otherwise returns 0
char st_get_seq_token_ascii(uint8_t triecode)
{
    if (st_is_seq_token_triecode(triecode)) {
        return st_seq_token_ascii_chars[triecode - TRIECODE_SEQUENCE_TOKEN_0];
    }
    return 0;
}
////////////////////////////////////////////////////////////////////////////////
// converts a triecode to a printable ascii char
char st_triecode_to_ascii(uint8_t triecode)
{
    const char token = st_get_seq_token_ascii(triecode);
    if (token) {
        return token;
    }
    st_assert(triecode < 128, "Unprintable triecode: %d", triecode);
    return (char)triecode;
}
////////////////////////////////////////////////////////////////////////////////
uint8_t st_keycode_to_triecode(uint16_t keycode, uint16_t kc_seq_token_0)
{
    if (st_is_seq_token_keycode(keycode, kc_seq_token_0)) {
        return TRIECODE_SEQUENCE_TOKEN_0 + keycode - kc_seq_token_0;
    }
    return st_keycode_to_ascii(keycode);
}
////////////////////////////////////////////////////////////////////////////////
char st_keycode_to_ascii(uint16_t keycode)
{
    const bool shifted = keycode & QK_LSFT;
    keycode &= 0xFF;
    if (keycode >= KC_A && keycode <= KC_SLASH) {
        keycode -= KC_A;
        return shifted ? pgm_read_byte(&shifted_keycode_to_ascii_lut[keycode]) :
                         pgm_read_byte(&unshifted_keycode_to_ascii_lut[keycode]);
    }
    return 0;
}
////////////////////////////////////////////////////////////////////////////////
uint16_t st_ascii_to_keycode(uint8_t triecode)
{
    st_assert(triecode < 128, "char (%d) not valid ascii", triecode);
    if (triecode >= 128) {
        return KC_NO;
    }
    uint16_t k = pgm_read_byte(&ascii_to_keycode_lut[(uint8_t)triecode]);
    bool is_shifted = PGM_LOADBIT(ascii_to_shift_lut, (uint8_t)triecode);
    if (is_shifted)
        k = S(k);
    return k;
}
////////////////////////////////////////////////////////////////////////////////
bool st_match_triecode(uint8_t triecode, uint8_t key_triecode)
{
    if (triecode < TRIECODE_SEQUENCE_METACHAR_0) {
        // Not a MetaCharacter. Do an exact match
        return triecode == tolower(key_triecode);
    }
    const uint8_t pred_index = triecode - TRIECODE_SEQUENCE_METACHAR_0;
    return st_predicate_test_triecode(pred_index, key_triecode);
}
////////////////////////////////////////////////////////////////////////////////
int st_get_seq_ref_triecode_pos(uint8_t triecode)
{
    st_assert(st_is_trans_seq_ref_triecode(triecode), "triecode (%d) not a valid seq ref", triecode);
    const uint8_t seq_ref_index = triecode - TRIECODE_SEQUENCE_REF_TOKEN_0;
    return seq_ref_index;
}

//////////////////////////////////////////////////////////////////////
// These are (currently) only used by the tester,
// so let's not compile them into the firmware.
#ifdef ST_TESTER
////////////////////////////////////////////////////////////////////////////////
uint8_t st_get_metachar_example_triecode(uint8_t triecode)
{
    if (!st_is_seq_metachar_triecode(triecode)) {
        return triecode;
    }
    // TODO: get these from the generator
    switch (triecode) {
        case 0xA0:
            return 'A';
        case 0xA1:
            return 'a';
        case 0xA2:
            return '1';
        case 0xA3:
            return '.';
        case 0xA4:
            return ',';
        case 0xA5:
            return '!';
        case 0xA6:
            return ' ';
        case 0xA7:
            return '%';
    }
}
////////////////////////////////////////////////////////////////////////////////
uint16_t st_triecode_to_keycode(uint8_t triecode, uint16_t kc_seq_token_0)
{
    if (st_is_seq_token_triecode(triecode)) {
        return triecode - TRIECODE_SEQUENCE_TOKEN_0 + kc_seq_token_0;
    }
    return st_ascii_to_keycode(triecode);
}
////////////////////////////////////////////////////////////////////////////////
// if triecode is a token that can be translated back to its user symbol,
// returns pointer to its utf8 string, otherwise returns 0
const char *st_get_seq_token_utf8(uint8_t triecode)
{
    if (st_is_seq_token_triecode(triecode)) {
        return st_seq_tokens[triecode - TRIECODE_SEQUENCE_TOKEN_0];
    } if (st_is_seq_metachar_triecode(triecode)) {
        return st_seq_metachars[triecode - TRIECODE_SEQUENCE_METACHAR_0];
    } else if (triecode == ' ') {
        return st_space_token;
    }
    return 0;
}
////////////////////////////////////////////////////////////////////////////////
// if triecode is a token that can be translated back to its user symbol,
// returns pointer to its utf8 string, otherwise returns 0
const char *st_get_trans_token_utf8(uint8_t triecode)
{
    if (st_is_trans_seq_ref_triecode(triecode)) {
        return st_trans_seq_ref_tokens[triecode - TRIECODE_SEQUENCE_REF_TOKEN_0];
    } if (st_is_seq_metachar_triecode(triecode)) {
        return st_seq_metachars[triecode - TRIECODE_SEQUENCE_METACHAR_0];
    } else if (triecode == ' ') {
        return st_space_token;
    }
    return 0;
}
//////////////////////////////////////////////////////////////////
uint16_t st_test_ascii_to_keycode(char c)
{
    for (int i = 0; i < SEQUENCE_TOKEN_COUNT; ++i) {
        if (c == st_seq_token_ascii_chars[i]) {
            return TEST_KC_SEQ_TOKEN_0 + i;
        }
    }
    return st_ascii_to_keycode(c);
}

#endif // ST_TESTER
