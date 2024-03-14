#pragma once

const char  *st_get_seq_token_symbol(uint16_t keycode);
void        keycodes_to_utf8_str(const uint16_t *keycodes, char *str);
void        keycodes_to_ascii_str(const uint16_t *keycodes, char *str);
uint16_t    ascii_to_keycode(char c);
char        *ltrim_str(char *str);
