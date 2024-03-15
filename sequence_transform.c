// Copyright 2021 Google LLC
// Copyright 2021 @filterpaper
// Copyright 2023 Pablo Martinez (@elpekenin) <elpekenin@elpekenin.dev>
// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
// Original source/inspiration: https://getreuer.info/posts/keyboards/autocorrection

#include "qmk_wrapper.h"
#include "sequence_transform.h"
#include "sequence_transform_data.h"
#include "utils.h"

#ifndef SEQUENCE_TRANSFORM_GENERATOR_VERSION_2_0
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

#define KEY_AT(i) st_key_buffer_get_keycode(&key_buffer, (i))

//////////////////////////////////////////////////////////////////
// Key history buffer
#ifdef SEQUENCE_TRANSFORM_EXTRA_BUFFER
#   if TRANSFORM_TRIE_MAX_LEN + SEQUENCE_TRANSFORM_EXTRA_BUFFER < 256
#       define KEY_BUFFER_CAPACITY SEQUENCE_MAX_LENGTH + COMPLETION_MAX_LENGTH + SEQUENCE_TRANSFORM_EXTRA_BUFFER
#   else
#       define KEY_BUFFER_CAPACITY 255
#   endif
#else
#   define KEY_BUFFER_CAPACITY TRANSFORM_TRIE_MAX_LEN
#endif
static st_key_action_t key_buffer_data[KEY_BUFFER_CAPACITY] = {{KC_SPC, ST_DEFAULT_KEY_ACTION}};
static st_key_buffer_t key_buffer = {
    key_buffer_data,
    KEY_BUFFER_CAPACITY,
    1
};

//////////////////////////////////////////////////////////////////////////////////////////
uint16_t sequence_transform_past_keycode(int index) {
    return st_key_buffer_get_keycode(&key_buffer, index);
}

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
#define ST_STACK_SIZE MAX(SEQUENCE_MAX_LENGTH, MAX_BACKSPACES)
static uint16_t trie_key_stack_data[ST_STACK_SIZE] = {0};
static st_key_stack_t trie_stack = {
    trie_key_stack_data,
    ST_STACK_SIZE,
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

//////////////////////////////////////////////////////////////////
// Transform trie node
static st_transform_trie_t transform_trie = {
    TRANSFORM_TRIE_SIZE,
    sequence_transform_miss_data
};

//////////////////////////////////////////////////////////////////
// Trie cursor
static st_cursor_t trie_cursor = {
    &key_buffer,
    &trie,
    {0, 255,0, false},
    {0},
    false,
};

//////////////////////////////////////////////////////////////////
#ifdef ST_TESTER
st_trie_t       *st_get_trie(void) { return &trie; }
st_key_buffer_t *st_get_key_buffer(void) { return &key_buffer; }
st_cursor_t *st_get_cursor(void) { return &trie_cursor; }
#endif

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
uint16_t search_for_regular_keypress(void)
{
    uint16_t keycode = KC_NO;
    for (int i = 1; i < key_buffer.context_len; ++i) {
        keycode = st_key_buffer_get_keycode(&key_buffer, i);
        if (!keycode || !(keycode & SPECIAL_KEY_TRIECODE_0)) {
            break;
        }
    }
    return keycode ? keycode : KC_NO;
}
//////////////////////////////////////////////////////////////////////////////////////////
void st_handle_repeat_key(void)
{
    const uint16_t last_regular_keypress = search_for_regular_keypress();
    if (last_regular_keypress) {
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
        uprintf("repeat keycode: 0x%04X\n", last_regular_keypress);
#endif
        st_key_buffer_get(&key_buffer, 0)->keypressed = last_regular_keypress;
        st_key_buffer_get(&key_buffer, 0)->action_taken = ST_DEFAULT_KEY_ACTION;
        st_send_key(last_regular_keypress);
    }
}
///////////////////////////////////////////////////////////////////////////////
void log_rule(st_trie_search_result_t *res, char *completion_str) {
#if defined(RECORD_RULE_USAGE) && defined(CONSOLE_ENABLE)
    st_cursor_init(&trie_cursor, 0, false);
    const uint16_t rule_trigger_keycode = st_cursor_get_keycode(&trie_cursor);
    const st_trie_payload_t *rule_action = st_cursor_get_action(&trie_cursor);
    const bool is_repeat = rule_action->func_code == 1;
    const int prev_seq_len = res->trie_match.seq_match_pos.segment_len - 1;
    // The cursor can't be empty here even if it is as output, because we know it matched a rule
    st_cursor_init(&trie_cursor, 1, res->trie_match.seq_match_pos.as_output);
    st_cursor_push_to_stack(&trie_cursor, prev_seq_len);
    char seq_str[SEQUENCE_MAX_LENGTH + 1];
    st_key_stack_to_str(trie.key_stack, seq_str);

    uprintf("st_rule,%s,%d,%c,", seq_str, res->trie_payload.num_backspaces, st_keycode_to_char(rule_trigger_keycode));

    // Completion string
    uprintf(completion_str);

    // Special function cases
    switch (res->trie_payload.func_code) {
        case 1:  // repeat
            if (is_repeat)
                uprintf("%c", st_keycode_to_char(search_for_regular_keypress()));
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
    // find buffer index for the space before the last word,
    // first skipping past trailing spaces
    // (in case a rule has spaces at the end of its completion)
    int word_start_idx = 0;
    while (word_start_idx < key_buffer.context_len &&
           KEY_AT(word_start_idx) == KC_SPACE) {
        ++word_start_idx;
    }
    // if we reached the end of the buffer here,
    // it means it's filled wish spaces, so bail.
    if (word_start_idx == key_buffer.context_len) {        
        return;
    }
    // we've skipped trailing spaces, so now find the next space
    while (word_start_idx < key_buffer.context_len &&
           KEY_AT(word_start_idx) != KC_SPACE) {
        ++word_start_idx;
    }
    //uprintf("word_start_idx: %d\n", word_start_idx);
    st_trie_rule_t result;
    result.sequence = sequence_str;
    result.transform = transform_str;
    if (st_trie_do_rule_searches(&trie, &key_buffer, word_start_idx, &result)) {
        sequence_transform_on_missed_rule_user(&result);
    }
#endif
}
//////////////////////////////////////////////////////////////////////////////////////////
void st_handle_result(st_trie_t *trie, st_trie_search_result_t *res) {
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("completion search res: index: %d, len: %d, bspaces: %d, func: %d\n",
            res->trie_payload.completion_index, res->trie_payload.completion_len, res->trie_payload.num_backspaces, res->trie_payload.func_code);
#endif
    // Most recent key in the buffer triggered a match action, record it in the buffer
    st_key_buffer_get(&key_buffer, 0)->action_taken = res->trie_match.trie_match_index;
    // fill completion buffer
    char completion_str[COMPLETION_MAX_LENGTH + 1] = {0};
    st_completion_to_str(trie, &res->trie_payload, completion_str);
    // Log newly added rule match
    log_rule(res, completion_str);
    // Send backspaces
    st_multi_tap(KC_BSPC, res->trie_payload.num_backspaces);
    // Send completion string
    for (char *c = completion_str; *c; ++c) {
        st_send_key(st_char_to_keycode(*c));
    }
    switch (res->trie_payload.func_code) {
        case 1:  // repeat
            st_handle_repeat_key();
            break;
        case 2:  // set one-shot shift
            set_oneshot_mods(MOD_LSFT);
            break;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////
#ifndef SEQUENCE_TRANSFORM_DISABLE_ENHANCED_BACKSPACE
void st_handle_backspace() {
    // initialize cursor as input cursor, so that `st_cursor_get_action` is stable
    st_cursor_init(&trie_cursor, 0, false);
    const st_trie_payload_t *action = st_cursor_get_action(&trie_cursor);
    if (action->completion_index == ST_DEFAULT_KEY_ACTION) {
        // previous key-press didn't trigger a rule action. One total backspace required
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
        uprintf("Undoing backspace after non-matching keypress\n");
        st_key_buffer_print(&key_buffer);
#endif
        // backspace was already sent on keydown
        st_key_buffer_pop(&key_buffer, 1);
        return;
    }
    // Undo a rule action
    int backspaces_needed_count = action->completion_len - 1;
    int resend_count = action->num_backspaces;
    if (backspaces_needed_count < 0) {
        // The natural backspace is unwanted. We need to resend that extra keypress
        resend_count -= backspaces_needed_count;
        backspaces_needed_count = 0;
    }
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("Undoing previous key action: bs: %d, restore: %d\n",
            backspaces_needed_count, resend_count);
    st_key_buffer_print(&key_buffer);
#endif
    // If previous action used backspaces, restore the deleted output from earlier actions
    if (resend_count > 0) {
        // reinitialize cursor as output cursor one keystroke before the previous action
        if (st_cursor_init(&trie_cursor, 1, true) && st_cursor_push_to_stack(&trie_cursor, resend_count)) {
            // Send backspaces now that we know we can do the full undo
            st_multi_tap(KC_BSPC, backspaces_needed_count);
            // Send saved keys in original order
            for (int i = trie.key_stack->size - 1; i >= 0; --i) {
                st_send_key(trie.key_stack->buffer[i]);
            }
        }
    } else {
        // Send backspaces since no resend is needed to complete the undo
        st_multi_tap(KC_BSPC, backspaces_needed_count);
    }
    st_key_buffer_pop(&key_buffer, 1);
}
#endif

/**
 * @brief Fills the provided buffer with up to `count` characters from the virtual output
 *
 * @param str char* to write the virtual output to. Will be null terminated.
 * @param history int 0 for the current output. N for the output N keypresses ago.
 * @param count int representing the number of characters requested. str must be hold `count + 1` chars
 * @return the number of characters written to the str not including the null terminator
 */
uint8_t st_get_virtual_output(char *str, uint8_t count)
{
    if (!st_cursor_init(&trie_cursor, 0, true)) {
        str[0] = '\0';
        return 0;
    }
    int i = 0;
    for (; i < count; ++i, st_cursor_next(&trie_cursor)) {
        const uint16_t keycode = st_cursor_get_keycode(&trie_cursor);
        if (!keycode) {
            break;
        }
        str[i] = st_keycode_to_char(keycode);
    }
    str[i] = '\0';
    return i;
}

/**
 * @brief Performs sequence transform if a match is found in the trie
 *
 * @return true if sequence transform was performed
 */
bool st_perform() {
    // Get completion string from trie for our current key buffer.
    st_trie_search_result_t res = {{0, {0,0,0}}, {0, 0, 0, 0}};
    if (st_trie_get_completion(&transform_trie, &trie_cursor, &res)) {
        st_handle_result(&trie, &res);
        st_trie_check_for_unused_rule(&transform_trie, &trie_cursor);
        return true;
    }
    st_trie_check_for_unused_rule(&transform_trie, &trie_cursor);
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
        st_log_time(st_handle_backspace());
        post_process_do_enhanced_backspace = false;
    }
#endif
#ifdef SEQUENCE_TRANSFORM_MISSED_RULES
    if (post_process_do_rule_search) {
        st_log_time(st_find_missed_rule());
        post_process_do_rule_search = false;
    }
#endif
}
