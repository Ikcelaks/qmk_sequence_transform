// Copyright 2021 Google LLC
// Copyright 2021 @filterpaper
// Copyright 2023 Pablo Martinez (@elpekenin) <elpekenin@elpekenin.dev>
// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
// Original source/inspiration: https://getreuer.info/posts/keyboards/autocorrection

#include <string.h>
#include "quantum.h"
#include "sequence_transform.h"
#include "sequence_transform_data.h"
#include "utils.h"

#ifndef SEQUENCE_TRANSFORM_GENERATOR_VERSION_0_1_0
    #error "sequence_transform_data.h was generated with an incompatible version of the generator script"
#endif

#define CDATA(L) pgm_read_byte(&trie->completions[L])

//////////////////////////////////////////////////////////////////
// Key history buffer
static st_key_action_t key_buffer_data[SEQUENCE_MAX_LENGTH] = {{KC_SPC, ST_DEFAULT_KEY_ACTION}};
static st_key_buffer_t key_buffer = {
    key_buffer_data,
    SEQUENCE_MAX_LENGTH,
    1
};

//////////////////////////////////////////////////////////////////////////////////////////
// Add KC_SPC on timeout
#if SEQUENCE_TRANSFORM_IDLE_TIMEOUT > 0
static uint32_t sequence_timer = 0;
void sequence_transform_task(void) {
    if (timer_elapsed32(sequence_timer) > SEQUENCE_TRANSFORM_IDLE_TIMEOUT) {
        st_key_buffer_reset(&key_buffer);
        sequence_timer = timer_read32();
    }
}
#endif

//////////////////////////////////////////////////////////////////
// Trie node and completion data
static st_trie_t trie = {
    DICTIONARY_SIZE,
    sequence_transform_data,
    COMPLETIONS_SIZE,
    sequence_transform_completions_data,
    COMPLETION_MAX_LENGTH,
    MAX_BACKSPACES
};

/**
 * @brief determine if context_magic should process this keypress,
 *        and remove any mods from keycode.
 *
 * @param keycode Keycode registered by matrix press, per keymap
 * @param record keyrecord_t structure
 * @param mods allow processing of mod status
 * @return true Allow context_magic
 * @return false Stop processing and escape from context_magic.
 */
bool st_process_check(uint16_t *keycode, keyrecord_t *record, uint8_t *mods) {
    // See quantum_keycodes.h for reference on these matched ranges.
    switch (*keycode) {
        // Exclude these keycodes from processing.
        case KC_LSFT:
        case KC_RSFT:
        case KC_CAPS:
        case QK_TO ... QK_TO_MAX:
        case QK_MOMENTARY ... QK_MOMENTARY_MAX:
        case QK_DEF_LAYER ... QK_DEF_LAYER_MAX:
        case QK_TOGGLE_LAYER ... QK_TOGGLE_LAYER_MAX:
        case QK_ONE_SHOT_LAYER ... QK_ONE_SHOT_LAYER_MAX:
        case QK_LAYER_TAP_TOGGLE ... QK_LAYER_TAP_TOGGLE_MAX:
        case QK_LAYER_MOD ... QK_LAYER_MOD_MAX:
        case QK_ONE_SHOT_MOD ... QK_ONE_SHOT_MOD_MAX:
            return false;

        // bake shift mod into keycode symbols
        case KC_1 ... KC_SLASH:
            if (*mods & MOD_MASK_SHIFT) {
                *keycode |= QK_LSFT;
            }
            return true;
        // Clear shift for alphas
        case LSFT(KC_A) ... LSFT(KC_Z):
        case RSFT(KC_A) ... RSFT(KC_Z):
            if (*keycode >= QK_LSFT && *keycode <= (QK_LSFT + 255)) {
                *mods |= MOD_LSFT;
            } else {
                *mods |= MOD_RSFT;
            }
            *keycode = QK_MODS_GET_BASIC_KEYCODE(*keycode); // Get the basic keycode.
            return true;
#ifndef NO_ACTION_TAPPING
        // Exclude tap-hold keys when they are held down
        // and mask for base keycode when they are tapped.
        case QK_LAYER_TAP ... QK_LAYER_TAP_MAX:
#    ifdef NO_ACTION_LAYER
            // Exclude Layer Tap, if layers are disabled
            // but action tapping is still enabled.
            return false;
#    else
            // Exclude hold keycode
            if (!record->tap.count) {
                return false;
            }
            *keycode = QK_LAYER_TAP_GET_TAP_KEYCODE(*keycode);
            break;
#    endif
        case QK_MOD_TAP ... QK_MOD_TAP_MAX:
            // Exclude hold keycode
            if (!record->tap.count) {
                return false;
            }
            *keycode = QK_MOD_TAP_GET_TAP_KEYCODE(*keycode);
            if (*mods & MOD_MASK_SHIFT) {
                *keycode |= QK_LSFT;
            }
            break;
#else
        case QK_MOD_TAP ... QK_MOD_TAP_MAX:
        case QK_LAYER_TAP ... QK_LAYER_TAP_MAX:
            // Exclude if disabled
            return false;
#endif
        // Exclude swap hands keys when they are held down
        // and mask for base keycode when they are tapped.
        case QK_SWAP_HANDS ... QK_SWAP_HANDS_MAX:
#ifdef SWAP_HANDS_ENABLE
            // Note: IS_SWAP_HANDS_KEYCODE() actually tests for the special action keycodes like SH_TOGG, SH_TT, ...,
            // which currently overlap the SH_T(kc) range.
            if (IS_SWAP_HANDS_KEYCODE(*keycode)
#    ifndef NO_ACTION_TAPPING
                || !record->tap.count
#    endif // NO_ACTION_TAPPING
            ) {
                return false;
            }
            *keycode = QK_SWAP_HANDS_GET_TAP_KEYCODE(*keycode);
            break;
#else
            // Exclude if disabled
            return false;
#endif
    }
    // Disable autocorrect while a mod other than shift is active.
    if ((*mods & ~MOD_MASK_SHIFT) != 0) {
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
        uprintf("clearing buffer (mods: 0x%04X)\n", *mods);
#endif
        st_key_buffer_reset(&key_buffer);
        return false;
    }

    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////
void st_record_send_key(uint16_t keycode) {
    st_key_buffer_push(&key_buffer, keycode);
    st_send_key(keycode);
}
//////////////////////////////////////////////////////////////////////////////////////////
void st_handle_repeat_key()
{
    st_key_action_t *keyaction = NULL;
    for (int i = 1; i < key_buffer.context_len; ++i) {
        keyaction = st_key_buffer_get(&key_buffer, i);
        if (!keyaction || keyaction->action_taken == ST_DEFAULT_KEY_ACTION) {
            break;
        }
    }

#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("repeat keycode: 0x%04X\n", keyaction->keypressed);
#endif
    if (keyaction) {
        *st_key_buffer_get(&key_buffer, 0) = *keyaction;
        st_send_key(keyaction->keypressed);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////
void st_handle_result(st_trie_t *trie, st_trie_payload_t *res) {
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("completion search res: index: %d, len: %d, bspaces: %d, func: %d\n",
            res->completion_index, res->completion_len, res->num_backspaces, res->func_code);
#endif

#if defined(RECORD_RULE_USAGE) && defined(CONSOLE_ENABLE)
    char context_string[SEQUENCE_MAX_LENGTH];
    st_key_buffer_get_context_string(&key_buffer, context_string, res->context_len);

    char rule_trigger_char = context_string[res->context_len - 1]; 
    context_string[res->context_len - 1] = '\0';

    bool is_repeat = rule_trigger_char == 'R' && strlen(context_string) == 0;

    if (is_repeat) {
        context_string[0] = st_keycode_to_char(st_key_buffer_get_keycode(&key_buffer, -2));
        context_string[1] = '\0';
    }

    uprintf("st_rule,%s,%d,%c,", context_string, res->num_backspaces, rule_trigger_char);
#endif

    // Send backspaces
    st_multi_tap(KC_BSPC, res->num_backspaces);

    // Send completion string
    const uint16_t completion_end = res->completion_index + res->completion_len;
    bool ends_with_wordbreak = (res->completion_len > 0 && CDATA(completion_end - 1) == ' ');

    for (uint16_t i = res->completion_index; i < completion_end; ++i) {
        char ascii_code = CDATA(i);
#if defined(RECORD_RULE_USAGE) && defined(CONSOLE_ENABLE)
        uprintf("%c", ascii_code);
#endif
        st_send_key(st_char_to_keycode(ascii_code));
    }

    switch (res->func_code) {
        case 1:  // repeat
            st_handle_repeat_key();
#if defined(RECORD_RULE_USAGE) && defined(CONSOLE_ENABLE)
            if (is_repeat)
                uprintf("%s", context_string);
#endif
            break;
        case 2:  // set one-shot shift
            set_oneshot_mods(MOD_LSFT);
#if defined(RECORD_RULE_USAGE) && defined(CONSOLE_ENABLE)
            uprintf("%c", 'S');
#endif
            break;
        case 3:  // disable auto-wordbreak
            ends_with_wordbreak = false;
    }


#if defined(RECORD_RULE_USAGE) && defined(CONSOLE_ENABLE)
        uprintf("\n");
#endif

    if (ends_with_wordbreak) {
        st_key_buffer_push(&key_buffer, KC_SPC);
    }
}

/**
 * @brief Performs sequence transform if a match is found in the trie
 *
 * @return true if sequence transform was performed
 */
bool st_perform() {
    // Get completion string from trie for our current key buffer.
    st_trie_payload_t res  = {0, 0, 0, 0};

    if (st_trie_get_completion(&trie, &key_buffer, &res)) {
        st_handle_result(&trie, &res);
        return true;
    }

    return false;
}

/**
 * @brief Process handler for sequence_transform feature.
 *
 * @param keycode Keycode registered by matrix press, per keymap
 * @param record keyrecord_t structure
 * @param special_key_start starting keycode index for special keys used in rules
 * @return true Continue processing keycodes, and send to host
 * @return false Stop processing keycodes, and don't send to host
 */
bool process_sequence_transform(uint16_t keycode, keyrecord_t *record, uint16_t special_key_start) {
#if SEQUENCE_TRANSFORM_IDLE_TIMEOUT > 0
    sequence_timer = timer_read32();
#endif
    if (!record->event.pressed)
        return true;
    uint8_t mods = get_mods();
#ifndef NO_ACTION_ONESHOT
    mods |= get_oneshot_mods();
#endif
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("pst keycode: 0x%04X\n", keycode);
#endif
    // If this is one of the special keycodes, convert to our internal trie code
    if (keycode >= special_key_start && keycode < special_key_start + SEQUENCE_TRANSFORM_COUNT) {
        keycode = keycode - special_key_start + SPECIAL_KEY_TRIECODE_0;
    }
    // keycode verification and extraction
    if (!st_process_check(&keycode, record, &mods))
        return true;

    // keycode buffer check
    switch (keycode) {
        case SPECIAL_KEY_TRIECODE_0 ... SPECIAL_KEY_TRIECODE_0 + SEQUENCE_TRANSFORM_COUNT:
        case KC_A ... KC_0:
        case S(KC_1)... S(KC_0):
        case KC_MINUS ... KC_SLASH:
            // process normally
            break;
        case S(KC_MINUS)... S(KC_SLASH):
            // treat " (shifted ') as a word boundary
            if (keycode == S(KC_QUOTE)) keycode = KC_SPC;
            break;
        case KC_BSPC:
            // remove last character from the buffer
            // FIXME: proper logic here needs to be determined
            st_key_buffer_pop(&key_buffer, 1);
            //st_key_buffer_reset(&key_buffer);
            return true;
        default:
            // set word boundary if some other non-alpha key is pressed
            keycode = KC_SPC;
    }
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("  translated keycode: 0x%04X (%c)\n", keycode, st_keycode_to_char(keycode));
#endif
    st_key_buffer_push(&key_buffer, keycode);
    if (st_perform()) {
        // tell QMK to not process this key
        return false;
    } else {
        // TODO: search for rules
    }
    return true;
}
