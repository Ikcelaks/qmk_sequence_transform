#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "sim_output_buffer.h"
#include "utils.h"

//////////////////////////////////////////////////////////////////
#define SIM_OUTPUT_BUFFER_CAPACITY 256
static char sim_output_buffer[SIM_OUTPUT_BUFFER_CAPACITY];
static int  sim_output_buffer_size = 0;

//////////////////////////////////////////////////////////////////
void sim_output_reset(void)
{
    sim_output_buffer_size = 0;
    sim_output_buffer[0] = 0;
}
//////////////////////////////////////////////////////////////////
void sim_output_push(char c)
{
    if (sim_output_buffer_size < SIM_OUTPUT_BUFFER_CAPACITY - 1) {
        sim_output_buffer[sim_output_buffer_size++] = c;
        sim_output_buffer[sim_output_buffer_size] = 0;
    }       
}
//////////////////////////////////////////////////////////////////
void sim_output_pop(int n)
{
    sim_output_buffer_size = st_max(0, sim_output_buffer_size - n);
    sim_output_buffer[sim_output_buffer_size] = 0;
}
//////////////////////////////////////////////////////////////////
char *sim_output_get(bool trim_spaces)
{
    if (!trim_spaces) {
        return sim_output_buffer;
    }
    char *output = sim_output_buffer;
    while (*output++ == ' ');
    return --output;
}
//////////////////////////////////////////////////////////////////
int sim_output_get_size()
{
    return sim_output_buffer_size;
}
//////////////////////////////////////////////////////////////////
void sim_output_print()
{
    printf("output: |");
    for (int i = 0; i < sim_output_buffer_size; ++i) {
        printf("%c", sim_output_buffer[i]);
    }
    printf("| (%d)\n", sim_output_buffer_size);
}
