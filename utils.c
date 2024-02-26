#include "utils.h"
#include "send_string.h"
#include "quantum.h"

// Note: we bit-pack in "reverse" order to optimize loading
#define PGM_LOADBIT(mem, pos) ((pgm_read_byte(&((mem)[(pos) / 8])) >> ((pos) % 8)) & 0x01)

const char unshifted_keycode_to_ascii_lut[53] PROGMEM = {
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
const char shifted_keycode_to_ascii_lut[53] PROGMEM = {
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

////////////////////////////////////////////////////////////////////////////////
char keycode_to_char(uint16_t keycode)
{
    const bool shifted = keycode & QK_LSFT;
    keycode &= 0xFF;
    if (keycode >= KC_A && keycode <= KC_SLASH) {
        keycode -= KC_A;
        return shifted ? pgm_read_byte(&shifted_keycode_to_ascii_lut[keycode]) :
                         pgm_read_byte(&unshifted_keycode_to_ascii_lut[keycode]);
    }
    return ' ';
}

////////////////////////////////////////////////////////////////////////////////
uint16_t char_to_keycode(char c)
{
    uint16_t k = pgm_read_byte(&ascii_to_keycode_lut[(uint8_t)c]);
    bool is_shifted = PGM_LOADBIT(ascii_to_shift_lut, (uint8_t)c);
    if (is_shifted)
        k = S(k);
    return k;
}
////////////////////////////////////////////////////////////////////////////////
// removes mod or layer info from a keycode
uint16_t normalize_keycode(uint16_t keycode)
{
    if (IS_QK_MOD_TAP(keycode))
		return QK_MOD_TAP_GET_TAP_KEYCODE(keycode);
    if (IS_QK_LAYER_TAP(keycode))
		return QK_LAYER_TAP_GET_TAP_KEYCODE(keycode);
    return keycode;
}
////////////////////////////////////////////////////////////////////////////////
void multi_tap(uint16_t keycode, int count)
{
    for (int i = 0; i < count; ++i)
        tap_code16(keycode);
}
