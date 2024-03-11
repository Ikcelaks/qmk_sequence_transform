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
    for (int i = 0; i < count; i++) {
        const char simc = sim_output[i] == ' ' ? st_wordbreak_ascii : sim_output[i];
        const char virc = virtual_output[count - 1 - i];
        if (simc != virc) {
            return false;
        }
    }
    return true;
}
//////////////////////////////////////////////////////////////////////
void test_virtual_output(const st_test_rule_t *rule, st_test_result_t *res)
{
    static char message[512];
    res->message = message;
    // sim_st_perform was run in a previous test
    char *sim_output = sim_output_get(false);
    const int sim_len = sim_output_get_size();
    char virtual_output[256];
    const int virt_len = st_get_virtual_output(virtual_output, 255);
    if (virt_len != sim_len) {
        snprintf(message, sizeof(message), "virt len (%d) != sim len (%d)", virt_len, sim_len);
        return;
    }
    if (compare_output(virtual_output, sim_output, virt_len)) {
        res->code = TEST_OK;
    } else {
        snprintf(message, sizeof(message), "mismatch! virt: |%s| sim: |%s|", virtual_output, sim_output);
    }
}
