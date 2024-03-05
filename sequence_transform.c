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
#  error "sequence_transform_data.h was generated with an incompatible version of the generator script"
#endif

#ifndef SEQUENCE_TRANSFORM_DISABLE_ENHANCED_BACKSPACE
static bool post_process_do_enhanced_backspace = false;
// Track backspace hold time
static uint32_t backspace_timer = 0;
#endif

#ifdef SEQUENCE_TRANSFORM_MISSED_RULES
static bool post_process_do_rule_search = false;
#endif

#define CDATA(L) pgm_read_byte(&trie->completions[L])
#define KEY_AT(i) st_key_buffer_get_keycode(&key_buffer, (i))

//////////////////////////////////////////////////////////////////
// Key history buffer
#ifdef SEQUENCE_TRANSFORM_EXTRA_BUFFER
#   if SEQUENCE_MAX_LENGTH + COMPLETION_MAX_LENGTH + SEQUENCE_TRANSFORM_EXTRA_BUFFER < 256
#       define KEY_BUFFER_CAPACITY SEQUENCE_MAX_LENGTH + COMPLETION_MAX_LENGTH + SEQUENCE_TRANSFORM_EXTRA_BUFFER
#   else
#       define KEY_BUFFER_CAPACITY 255
#   endif
#else
#   define KEY_BUFFER_CAPACITY SEQUENCE_MAX_LENGTH + COMPLETION_MAX_LENGTH
#endif
static st_key_action_t key_buffer_data[KEY_BUFFER_CAPACITY] = {{KC_SPC, ST_DEFAULT_KEY_ACTION}};
static st_key_buffer_t key_buffer = {
    key_buffer_data,
    KEY_BUFFER_CAPACITY,
    1
};

//////////////////////////////////////////////////////////////////////////////////////////
// Add KC_SPC on timeout
#if SEQUENCE_TRANSFORM_IDLE_TIMEOUT > 0
static uint32_t sequence_timer = 0;
void sequence_transform_task(void) {
    if (key_buffer.context_len > 1 &&
        timer_elapsed32(sequence_timer) > SEQUENCE_TRANSFORM_IDLE_TIMEOUT) {
        st_key_buffer_reset(&key_buffer);
        sequence_timer = timer_read32();
    }
}
#endif

//////////////////////////////////////////////////////////////////
// Trie key stack
static uint16_t trie_key_stack_data[SEQUENCE_MAX_LENGTH] = {0};
static st_key_stack_t trie_stack = {
    trie_key_stack_data,
    SEQUENCE_MAX_LENGTH,
    0
};

//////////////////////////////////////////////////////////////////
// Trie node and completion data
static st_trie_t trie = {
    DICTIONARY_SIZE,
    sequence_transform_data,
    COMPLETIONS_SIZE,
    sequence_transform_completions_data,
    COMPLETION_MAX_LENGTH,
    MAX_BACKSPACES,
    &trie_stack
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
    if (!record->event.pressed && QK_MODS_GET_BASIC_KEYCODE(*keycode) != KC_BSPC) {
        // We generally only process key presses, not releases, but we must make an
        // exception for Backspace, because enhanced backspace does its action on
        // the release of backspace.
        return false;
    }
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
            break;
        // Clear shift for alphas
        case LSFT(KC_A) ... LSFT(KC_Z):
        case RSFT(KC_A) ... RSFT(KC_Z):
            if (*keycode >= QK_LSFT && *keycode <= (QK_LSFT + 255)) {
                *mods |= MOD_LSFT;
            } else {
                *mods |= MOD_RSFT;
            }
            *keycode = QK_MODS_GET_BASIC_KEYCODE(*keycode); // Get the basic keycode.
            break;
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
///////////////////////////////////////////////////////////////////////////////
void log_rule(st_trie_t *trie, st_trie_search_result_t *res) {
#if defined(RECORD_RULE_USAGE) && defined(CONSOLE_ENABLE)
    // Main body
    char context_string[SEQUENCE_MAX_LENGTH + 1];
    st_key_buffer_to_str(&key_buffer, context_string, res->trie_match.seq_match_len);

    const int match_len = res->trie_match.seq_match_len - 1;
    char rule_trigger_char = context_string[match_len];
    context_string[match_len] = '\0';

    const bool is_repeat = KEY_AT(0) == SPECIAL_KEY_TRIECODE_0+1 && match_len == 0;

    if (is_repeat) {
        uint16_t last_key = KEY_AT(1);

        context_string[0] = st_keycode_to_char(last_key);
        context_string[1] = '\0';
    }

    uprintf("st_rule,%s,%d,%c,", context_string, res->trie_payload.num_backspaces, rule_trigger_char);

    // Completion string
    const uint16_t completion_end = res->trie_payload.completion_index + res->trie_payload.completion_len;

    for (uint16_t i = res->trie_payload.completion_index; i < completion_end; ++i) {
        char ascii_code = CDATA(i);
        uprintf("%c", ascii_code);
    }

    // Special function cases
    switch (res->trie_payload.func_code) {
        case 1:  // repeat
            if (is_repeat)
                uprintf("%s", context_string);
            break;

        case 2:  // set one-shot shift
            uprintf("%c", 'S');
            break;
    }

    // Terminator
    uprintf("\n");
#endif
}
//////////////////////////////////////////////////////////////////////
__attribute__((weak)) void sequence_transform_on_missed_rule_user(const st_trie_rule_t *rule)
{
#ifdef CONSOLE_ENABLE
    uprintf("Missed rule! %s -> %s\n", rule->sequence, rule->transform);
#endif
}
//////////////////////////////////////////////////////////////////////
void st_find_missed_rule(void)
{
#ifdef SEQUENCE_TRANSFORM_MISSED_RULES
    char sequence_str[SEQUENCE_MAX_LENGTH + 1] = {0};
    char transform_str[TRANSFORM_MAX_LEN + 1] = {0};
    static int search_len_from_space = 0;
    // find buffer index for last space
    int last_space_index = 0;
    while (last_space_index < key_buffer.context_len &&
           KEY_AT(last_space_index) != KC_SPACE) {
        ++last_space_index;
    }
    if (last_space_index == 0) {
        //uprintf("space at top, resetting search_len_from_space\n");
        search_len_from_space = 0;
        return;
    }
    //uprintf("last_space_index: %d, search_len_from_space: %d\n",
    //        last_space_index, search_len_from_space);
    const int len_to_last_space = key_buffer.context_len - last_space_index;
    const int search_len_start = st_clamp(len_to_last_space + search_len_from_space,
                                          1,
                                          key_buffer.context_len - 1);
    st_trie_rule_t result;
    result.sequence = sequence_str;
    result.transform = transform_str;
    const int new_len = st_trie_get_rule(&trie, &key_buffer, search_len_start, &result);    
    if (new_len != search_len_start) {
        sequence_transform_on_missed_rule_user(&result);
        // Next time, start searching from after completion
        search_len_from_space = new_len - len_to_last_space;
    }
#endif
}
//////////////////////////////////////////////////////////////////////////////////////////
void st_handle_result(st_trie_t *trie, st_trie_search_result_t *res) {
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("completion search res: index: %d, len: %d, bspaces: %d, func: %d\n",
            res->trie_payload.completion_index, res->trie_payload.completion_len, res->trie_payload.num_backspaces, res->trie_payload.func_code);
#endif

    log_rule(trie, res);

    // Most recent key in the buffer triggered a match action, record it in the buffer
    st_key_buffer_get(&key_buffer, 0)->action_taken = res->trie_match.trie_match_index;
    // Send backspaces
    st_multi_tap(KC_BSPC, res->trie_payload.num_backspaces);
    // Send completion string
    const uint16_t completion_end = res->trie_payload.completion_index + res->trie_payload.completion_len;
    bool ends_with_wordbreak = (res->trie_payload.completion_len > 0 && CDATA(completion_end - 1) == ' ');
    for (uint16_t i = res->trie_payload.completion_index; i < completion_end; ++i) {
        char ascii_code = CDATA(i);
        st_send_key(st_char_to_keycode(ascii_code));
    }
    switch (res->trie_payload.func_code) {
        case 1:  // repeat
            st_handle_repeat_key();
            break;
        case 2:  // set one-shot shift
            set_oneshot_mods(MOD_LSFT);
            break;
        case 3:  // disable auto-wordbreak
            ends_with_wordbreak = false;
    }
    if (ends_with_wordbreak && KEY_AT(0) != KC_SPC) {
        // If the last key in an action outputs a wordbreak character, we need to mark in the buffer
        // that we are at a word break. Currently, we must hack this by appending a fake KC_SPC to the buffer.
        // We mark the action as ST_IGNORE_KEY_ACTION to designate that this is a fake key press with
        // no associated output. We skip this fake space if the key that triggered the output ending with a
        // wordbreak was a space. All of this convoluted logic will be removed when proper tagging support is
        // added in a future feature
        st_key_buffer_push(&key_buffer, KC_SPC);
        st_key_buffer_get(&key_buffer, 0)->action_taken = ST_IGNORE_KEY_ACTION;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////
#ifndef SEQUENCE_TRANSFORM_DISABLE_ENHANCED_BACKSPACE
void resend_output(st_trie_t *trie, int buf_cur_pos, int key_count, int skip_count, int num_backspaces) {
    if (key_count <= 0) {
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
        uprintf("IMPOSSIBLE: resend_output called when no more keys need to be resent (%d, %d)",
                key_count, skip_count);
#endif
        return;
    }
    st_key_action_t *key_action = st_key_buffer_get(&key_buffer, buf_cur_pos);
    if (!key_action) {
        // End of buffer reached before fully restoring. Enlarge the extra buffer space
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
        uprintf("Unable to restore full changes. Expand the extra buffer space (%d, %d)",
                key_count, skip_count);
#endif
        return;
    }
    if (key_action->action_taken == ST_IGNORE_KEY_ACTION) {
        // This is a hacky fake key-press. Skip to next key in the buffer
        resend_output(trie, buf_cur_pos + 1, key_count, skip_count, num_backspaces);
        return;
    }
    if (key_action->action_taken == ST_DEFAULT_KEY_ACTION) {
        if (skip_count > 0) {
            resend_output(trie, buf_cur_pos + 1, key_count, skip_count - 1, num_backspaces);
            // skipping this keypress
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
            uprintf("Skipping single keypress: {%c}\n", st_keycode_to_char(key_action->keypressed));
#endif
            return;
        }
        resend_output(trie, buf_cur_pos + 1, key_count - 1, 0, num_backspaces);
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
        uprintf("Redoing single keypress: {%c}\n", st_keycode_to_char(key_action->keypressed));
#endif
        st_send_key(key_action->keypressed);
        return;
    }
    st_trie_payload_t action = {0, 0, 0, 0};
    st_get_payload_from_match_index(trie, &action, key_action->action_taken);
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("Key action: bs: %d, restore: %d\n",
            action.num_backspaces, action.completion_len);
#endif
    const uint16_t completion_end = action.completion_index + action.completion_len - skip_count;
    if (completion_end <= action.completion_index) {
        // skip everything and pass down any remaining skipt to next keyaction
        resend_output(trie, buf_cur_pos + 1, key_count, skip_count - action.completion_len, num_backspaces);
        return;
    }
    // Some characters from this action need to be resent
    uint16_t completion_start = completion_end - key_count;
    if (completion_start < action.completion_index) {
        resend_output(trie, buf_cur_pos + 1, key_count + skip_count - action.completion_len, 0, num_backspaces);
        completion_start = action.completion_index;
    } else {
        // Send backspaces now that we know we can do the full undo
        st_multi_tap(KC_BSPC, num_backspaces);
    }
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("Restore last %d key presses from key action with %d total output\n",
            key_count, action.completion_len);
#endif
    for (uint16_t i = completion_start; i < completion_end; ++i) {
        char ascii_code = CDATA(i);
        st_send_key(st_char_to_keycode(ascii_code));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////
void handle_backspace(st_trie_t *trie) {
    st_key_action_t *prev_key_action = st_key_buffer_get(&key_buffer, 0);
    if (!prev_key_action || prev_key_action->action_taken == ST_DEFAULT_KEY_ACTION) {
        // previous key-press didn't trigger a rule action. One total backspace required
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
        uprintf("Undoing backspace after non-matching keypress\n");
        st_key_buffer_print(&key_buffer);
#endif
        // backspace was already sent on keydown
        st_key_buffer_pop(&key_buffer, 1);
        return;
    }
    if (prev_key_action->action_taken == ST_IGNORE_KEY_ACTION) {
        // This is a hacky fake key-press. Pop it off the buffer and go again
        st_key_buffer_pop(&key_buffer, 1);
        handle_backspace(trie);
        return;
    }
    // Undo a rule action
    st_trie_payload_t prev_action = {0, 0, 0, 0};
    st_get_payload_from_match_index(trie, &prev_action, prev_key_action->action_taken);
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("Undoing previous key action (%d): bs: %d, restore: %d\n",
            prev_key_action->action_taken, prev_action.completion_len, prev_action.num_backspaces);
    st_key_buffer_print(&key_buffer);
#endif
    // If previous action used backspaces, restore the deleted output from earlier actions
    if (prev_action.num_backspaces > 0) {
        resend_output(trie, 1, prev_action.num_backspaces, 0, prev_action.completion_len - 1);
    } else {
        // Send backspaces now that we know we can do the full undo
        st_multi_tap(KC_BSPC, prev_action.completion_len - 1);
    }
    st_key_buffer_pop(&key_buffer, 1);
}
#endif

/**
 * @brief Performs sequence transform if a match is found in the trie
 *
 * @return true if sequence transform was performed
 */
bool st_perform() {
    // Get completion string from trie for our current key buffer.
    st_trie_search_result_t res = {{0, 0}, {0, 0, 0, 0}};
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
    uint8_t mods = get_mods();
#ifndef NO_ACTION_ONESHOT
    mods |= get_oneshot_mods();
#endif
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("pst keycode: 0x%04X, mods: 0x%02X, pressed: %d\n",
            keycode, mods, record->event.pressed);
#endif
    // If this is one of the special keycodes, convert to our internal trie code
    if (keycode >= special_key_start && keycode < special_key_start + SEQUENCE_TRANSFORM_COUNT) {
        keycode = keycode - special_key_start + SPECIAL_KEY_TRIECODE_0;
    }
    // keycode verification and extraction
    if (!st_process_check(&keycode, record, &mods))
        return true;

    if (keycode == KC_BSPC) {
#ifndef SEQUENCE_TRANSFORM_DISABLE_ENHANCED_BACKSPACE
        if (record->event.pressed) {
            backspace_timer = timer_read32();
            // set flag so that post_process_sequence_transform will perfom an undo
            post_process_do_enhanced_backspace = true;
            return true;
        }
        // This is a release
        if (timer_elapsed32(backspace_timer) > TAPPING_TERM) {
            // Clear the buffer if the backspace key was held past the tapping term
            st_key_buffer_reset(&key_buffer);
        }
        return true;
#else
        st_key_buffer_reset(&key_buffer);
        return true;
#endif
    }
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
#ifdef SEQUENCE_TRANSFORM_MISSED_RULES
        post_process_do_rule_search = true;
#endif
    }
    return true;
}

/**
 * @brief Performs sequence transform related actions that must occur after normal processing
 *
 * Should be called from the `post_process_record_user` function
 */
void post_process_sequence_transform()
{
#ifndef SEQUENCE_TRANSFORM_DISABLE_ENHANCED_BACKSPACE
    if (post_process_do_enhanced_backspace) {
        // remove last key from the buffer
        //   and undo the action of that key
        handle_backspace(&trie);
        post_process_do_enhanced_backspace = false;
    }
#endif
#ifdef SEQUENCE_TRANSFORM_MISSED_RULES
    if (post_process_do_rule_search) {
        st_find_missed_rule();
        post_process_do_rule_search = false;
    }
#endif
}
