#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mem.h"
#include "util.h"

#define MEMSIZE (1 << 16)
#define NUMBLOCKS 1024

int ugen_phasor_init(uint32_t *mem, uint32_t pstk);
int ugen_phasor(uint32_t *mem, uint32_t pstk);

int ugen_blsaw_init(uint32_t *mem, uint32_t pstk);
int ugen_blsaw(uint32_t *mem, uint32_t pstk);

static void write_block(FILE *fp, uint32_t *mem, uint32_t *stk)
{
    int rc;
    uint32_t out;
    float *blk;

    out = 0;
    rc = stack_pop(stk, &out);

    if (rc) return;

    blk = (float *)&mem[out];

    fwrite(blk, sizeof(float), 64, fp);

    return;
}

int main(int argc, char *argv[])
{
    FILE *fp;
    uint32_t *mem;
    uint32_t i;
    uint32_t pstk;
    uint32_t *stk;
    uint32_t ph;
    uint32_t freq;
    uint32_t out;

    mem = malloc(MEMSIZE * sizeof(uint32_t));

    for (i = 0; i < MEMSIZE; i++) {
        mem[i] = 0;
    }

    pstk = 0;

    stk = &mem[pstk];

    fp = fopen("out.raw", "wb");

    /* phasor begins at first block */
    ph = 64;
    /* output begins at second block */

    /* memory address for constant is right after phasor */
    /* freq = ph + 5; */
    freq = ph + 17;

    /* output: block 2 */
    out = 128;

    mem[freq] = ftoi(321.123);

    stack_push(stk, sonilo_block(out));
    stack_push(stk, ph);
    /* ugen_phasor_init(mem, pstk); */
    ugen_blsaw_init(mem, pstk);

    for (i = 0; i < NUMBLOCKS; i++) {
        stack_push(stk, sonilo_constant(freq));
        stack_push(stk, ph);
        /* ugen_phasor(mem, pstk); */
        ugen_blsaw(mem, pstk);
        write_block(fp, mem, stk);
    }

    fclose(fp);

    return 0;
}
