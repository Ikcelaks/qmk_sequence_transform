#include "qmk_wrapper.h"
#include "keybuffer.h"
#include "key_stack.h"
#include "trie.h"
#include "utils.h"
#include "sequence_transform.h"
#include "sequence_transform_data.h"
#include "sequence_transform_test.h"
#include "sim_output_buffer.h"
#include "tester_utils.h"
#include "tester.h"

#define KEY_AT(i) st_key_buffer_get_keycode(buf, (i))

//////////////////////////////////////////////////////////////////////
bool buf_needs_expanding(st_key_buffer_t *buf)
{
    for (int i = 1; i < buf->context_len; ++i) {
        const uint16_t key = KEY_AT(i);
        if (st_is_seq_token_keycode(key)) {
            return true;
        }
    }
    return false;
}
//////////////////////////////////////////////////////////////////////
// (partial) simulation of process_sequence_transform logic
// to test st_find_missed_rule
// returns false if rule is untestable
bool sim_st_find_missed_rule(const st_test_rule_t *rule)
{
    missed_rule_seq[0] = 0;
    missed_rule_transform[0] = 0;
    st_key_buffer_t *buf = st_get_key_buffer();
    // send input rule seq so we can get output transform to test
    sim_st_perform(rule->seq_keycodes);
    if (buf_needs_expanding(buf)) {
        // find seq_prefix/trans_prefix by backspacing non seq tokens
        do {
            tap_code16(KC_BSPC);
            st_handle_backspace();
        } while (!st_is_seq_token_keycode(KEY_AT(0)));
        char *trans_prefix = sim_output_get(true);
        int   out_len = strlen(trans_prefix);
        int   rule_trans_len = strlen(rule->transform_str);
        //printf("trans_prefix: %s\n", trans_prefix);
        if (!strstr(rule->transform_str, trans_prefix)) {
            // If trans_prefix is not in transform_str,
            // this is an untestable rule. 
            return false;
        }
        // output now contains trans_prefix (develop)
        // input now contains seq_prefix (^d@)
        // add removed chars (er) to new end of sequence in input buffer
        for (int i = out_len; i < rule_trans_len; ++i) {
            const char     c   = rule->transform_str[i];
            const uint16_t key = ascii_to_keycode(c);
            st_key_buffer_push(buf, key);
        }
        // input should now contain ^d@er
        char chained_transform[TRANSFORM_MAX_LEN + 1];
        st_key_buffer_to_str(buf, chained_transform, buf->context_len);
        //printf("chained_transform: %s\n", chained_transform);
        if (!strcmp(chained_transform, rule->transform_str)) {
            // transform already a chained expression,
            // so nothing to test!
            return false;
        }
    } else {
        buf->context_len = 0;
        // send the output into input buffer
        // to simulate user typing it directly
        char *output = sim_output_get(false);
        for (char c = *output; c; c = *++output) {
            const uint16_t key = ascii_to_keycode(c);
            st_key_buffer_push(buf, key);
        }
    }
    // from this new input buffer, find missed rule
    st_find_missed_rule();
    return true;
}
//////////////////////////////////////////////////////////////////////
void test_find_rule(const st_test_rule_t *rule, st_test_result_t *res)
{
    static char message[256];
    static char seq_ascii[256];
    res->message = message;
    // rule search starts looking from the last space in the buffer
    // so if there is a space in the rule transform,
    // this rule is untestable (except if space is at the end)
    char *space = strchr(rule->transform_str, ' ');
    int transform_len = strlen(rule->transform_str);
    if (space && space - rule->transform_str < transform_len - 1) {
        res->code = TEST_WARN;
        snprintf(message, sizeof(message), "untestable rule (space in transform)!");
        return;
    }
    if (!sim_st_find_missed_rule(rule)) {
        res->code = TEST_WARN;
        snprintf(message, sizeof(message), "untestable rule!");
        return;
    }
    keycodes_to_ascii_str(rule->seq_keycodes, seq_ascii);
    if (!strcmp(missed_rule_seq, seq_ascii)) {
        res->code = TEST_OK;
        return;
    }
    const int missed_rule_seq_len = strlen(missed_rule_seq);
    if (!missed_rule_seq_len) {
        snprintf(message, sizeof(message), "found nothing!");
        return;
    }
    const int seq_ascii_len = strlen(seq_ascii);
    if (missed_rule_seq_len < seq_ascii_len) {
        res->code = TEST_WARN;
        snprintf(message, sizeof(message), "found shorter sequence rule: %s ⇒ %s",
                 missed_rule_seq, missed_rule_transform);
        return;
    }
    if (!strcmp(rule->transform_str, missed_rule_transform)) {
        res->code = TEST_WARN;
        snprintf(message, sizeof(message), "found diff sequence for same transform: %s ⇒ %s",
             missed_rule_seq, missed_rule_transform);
        return;
    }
    snprintf(message, sizeof(message), "found: %s ⇒ %s",
             missed_rule_seq, missed_rule_transform);
}
