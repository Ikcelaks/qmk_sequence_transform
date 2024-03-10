#include "qmk_wrapper.h"
#include "keybuffer.h"
#include "sequence_transform.h"
#include "sequence_transform_data.h"
#include "sequence_transform_test.h"
#include "sim_output_buffer.h"
#include "tester.h"

//////////////////////////////////////////////////////////////////
// compare the virtual output buffer state to the sim output buffer
bool compare_output(char *virtual_output, char *sim_output, int count)
{
    // we don't use st_key_buffer_reset(buf) here because
    // we don't nec want a space at the start of the buffer
    for (int i = 0; i < count; i++) {
        if ((sim_output[i] == ' ' ? st_wordbreak_ascii : sim_output[i]) != virtual_output[count - 1 - i]) {
            return false;
        }
    }
    return true;
}
//////////////////////////////////////////////////////////////////////
void test_virtual_output(const st_test_rule_t *rule, st_test_result_t *res)
{
    static char message[300];
    res->message = message;
    // Test #2: test_virtual_output
    // sim_st_perform was run in o previous test
    // Ignore spaces at the start of output
    char *sim_output = sim_output_get(true);
    // Test virtual output
    const int expected_len = strlen(sim_output);
    char virtual_output[256];
    int length = st_get_virtual_output(virtual_output, 255);
    if (length != expected_len) {
        res->pass = false;
        snprintf(message, sizeof(message), "virtual_output incorrect length (%d); expected (%d)", length, expected_len);
    }
    res->pass = compare_output(virtual_output, sim_output, length);
    if (res->pass) {
        snprintf(message, sizeof(message), "OK!");
    } else {
        snprintf(message, sizeof(message), "virtual_output: %s", virtual_output);
    }
}
