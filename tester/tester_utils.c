#include "qmk_wrapper.h"
#include "tester_utils.h"
#include "sequence_transform_data.h"
#include "sequence_transform_test.h"
#include "utils.h"

////////////////////////////////////////////////////////////////////////////////
// if keycode is a token that can be translated back to its user symbol,
// returns pointer to it, otherwise returns 0
const char *st_get_seq_token_symbol(uint16_t keycode)
{
    if (SPECIAL_KEY_TRIECODE_0 <= keycode && keycode < SPECIAL_KEY_TRIECODE_0 + SEQUENCE_TRANSFORM_COUNT) {
        return st_seq_tokens[keycode - SPECIAL_KEY_TRIECODE_0];        
    } else if (keycode == KC_SPACE) {
        return st_wordbreak_token;
    }
    return 0;
}
//////////////////////////////////////////////////////////////////
void keycodes_to_utf8_str(const uint16_t *keycodes, char *str)
{
    for (uint16_t key = *keycodes; key; key = *++keycodes) {
        const char *token = st_get_seq_token_symbol(key);
        if (token) {
            while ((*str++ = *token++));
            str--;
        } else {
            *str++ = st_keycode_to_char(key);
        }
    }
    *str = 0;
}
//////////////////////////////////////////////////////////////////
void keycodes_to_ascii_str(const uint16_t *keycodes, char *str)
{
    for (uint16_t key = *keycodes; key; key = *++keycodes) {
        *str++ = st_keycode_to_char(key);
    }
    *str = 0;
}
//////////////////////////////////////////////////////////////////
uint16_t ascii_to_keycode(char c)
{
    for (int i = 0; i < sizeof(st_seq_tokens_ascii); ++i) {
        if (c == st_seq_tokens_ascii[i]) {
            return SPECIAL_KEY_TRIECODE_0 + i;
        }
    }
    if (c == st_wordbreak_ascii) {
        return KC_SPACE;
    }
    return st_char_to_keycode(c);
}
