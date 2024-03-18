#include "st_defaults.h"
#include "qmk_wrapper.h"
#include "utils.h"
#include "sequence_transform.h"
#include "sim_output_buffer.h"
#include "tester_utils.h"
#include "tester.h"

#define KEY_AT(i) st_key_buffer_get_keycode(buf, (i))

//////////////////////////////////////////////////////////////////
// Converts keys from buffer to null terminated ascii string
// KC_SPACE -> ' ' (instead of st_wordbreak_ascii)
void key_buffer_to_space_str(const st_key_buffer_t *buf, char *str)
{
    const int len = buf->context_len;
    for (int i = 0; i < len; ++i) {
        const uint16_t key = KEY_AT(len - i - 1);
        *str++ = key == KC_SPACE ? ' ' : st_keycode_to_char(key);
    }
    *str = 0;
}
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
// Setup input buffer from rule transform so that
// it's ready for the st_find_missed_rule call.
// Fills chained_transform with an ascii string of the actual transform
// that will be tested.
// returns false if rule is untestable, true otherwise
bool setup_input_from_transform(const st_test_rule_t *rule, char *chained_transform)
{    
    st_key_buffer_t *buf = st_get_key_buffer();
    // send input rule seq so we can get output transform to test
    sim_st_perform(rule->seq_keycodes);

    // Check if sequence contains an unexpanded token
    if (!buf_needs_expanding(buf)) {
        // 'normal' rule, so we can use the transform as is
        chained_transform[0] = 0;
        strncat(chained_transform, rule->transform_str, 255);        
        // send the output into input buffer
        // to simulate user typing it directly
        buf->context_len = 0;
        char *output = sim_output_get(false);
        for (char c = *output; c; c = *++output) {
            const uint16_t key = ascii_to_keycode(c);
            st_key_buffer_push(buf, key);
        }        
        return true;
    }
    // Rule contains a sequence token before the end so
    // rule_search won't be able to find this rule
    // if we use the expanded transform.
    // We must instead convert it to a form that it could find.
    // Example: ^d@r -> developer
    // seq_prefix: ^d@
    // trans_prefix: develop
    // chained_transform: ^d@er

    // find seq_prefix/trans_prefix by backspacing non seq tokens
    uint16_t key = 0;
    do {
        tap_code16(KC_BSPC);
        st_handle_backspace();
        key = KEY_AT(0);
    } while (key && !st_is_seq_token_keycode(key));
    if (!key) {
        //printf("empty seq_prefix!\n");
        return false;
    }
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
    // input should now contain ^d@er and is ready for searching
    key_buffer_to_space_str(buf, chained_transform);
    //printf("chained_transform: %s\n", chained_transform);
    if (!strcmp(ltrim_str(chained_transform), rule->transform_str)) {
        // transform already a chained expression,
        // so nothing to test!
        //printf("transform already a chained expression!\n");
        return false;
    }
    return true;
}
//////////////////////////////////////////////////////////////////////
void test_find_rule(const st_test_rule_t *rule, st_test_result_t *res)
{
    char seq_ascii[256] = {0};
    char chained_transform[256] = {0};
    // rule search starts looking from the last space in the buffer
    // so if there is a space in the rule transform,
    // this rule is untestable (except if space is at the end)
    char *space = strchr(rule->transform_str, ' ');
    int transform_len = strlen(rule->transform_str);
    if (space && space - rule->transform_str < transform_len - 1) {
        RES_WARN("untestable rule (space in transform)!");
        return;
    }
    // setup input buffer from rule transform so that
    // it's ready for the st_find_missed_rule call
    if (!setup_input_from_transform(rule, chained_transform)) {
        RES_WARN("untestable rule!");
        return;
    }
    // currently the rule transform string doesn't have leading spaces
    // so we must remove them from our computed chained_transform
    // before comparisons.
    char *transform = ltrim_str(chained_transform);
    // from this new input buffer, find missed rule
    missed_rule_seq[0] = 0;
    missed_rule_transform[0] = 0;
    st_find_missed_rule();    
    // Check if found rule matches ours
    keycodes_to_ascii_str(rule->seq_keycodes, seq_ascii);
    const int missed_rule_seq_len = strlen(missed_rule_seq);
    const int seq_ascii_len = strlen(seq_ascii);
    if (!missed_rule_seq_len) {
        RES_FAIL("found nothing!");
        return;
    }
    const int seq_dif = strcmp(missed_rule_seq, seq_ascii);
    const int trans_dif = strcmp(missed_rule_transform, transform);
    if (!trans_dif && missed_rule_seq_len < seq_ascii_len) {
        RES_WARN("found shorter sequence rule: %s ⇒ %s",
                 missed_rule_seq, missed_rule_transform);
        return;
    }
    if (!trans_dif && seq_dif) {
        RES_WARN("found diff sequence for same transform: %s ⇒ %s",
                 missed_rule_seq, missed_rule_transform);
        return;
    }
    if (seq_dif || trans_dif) {
        //printf("(%s -> %s)\n", seq_ascii, transform);
        RES_FAIL("found: %s ⇒ %s",
                 missed_rule_seq, missed_rule_transform);
        return;
    }
}
