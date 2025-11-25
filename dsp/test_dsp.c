#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mem.h"

#define MEMSIZE (1 << 16)
#define NUMBLOCKS 1024
#define CONSTANT 0x10000

void dsp_phasor_init(uint32_t *mem, uint32_t p, uint32_t out);

void ugen_phasor(uint32_t *mem, uint32_t pstk);


uint32_t ftoi(float f)
{
    return *(uint32_t *)(&f);
}

float itof(uint32_t i)
{
    return *(float *)&i;
}

int pop(uint32_t *stk, uint32_t *x)
{
    uint32_t sp;

    sp = stk[0];
    if (sp == 0) return 1;
    *x = stk[sp];
    sp--;
    stk[0] = sp;

    return 0;
}

int push(uint32_t *stk, uint32_t x)
{
    uint32_t sp;

    sp = stk[0];
    if (sp >= 64) return 1;
    sp++;
    stk[sp] = x;
    stk[0] = sp;


    return 0;
}

static void write_block(FILE *fp, uint32_t *mem, uint32_t *stk)
{
    int rc;
    uint32_t out;
    float *blk;

    out = 0;
    rc = pop(stk, &out);

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
    freq = ph + 5;

    /* output: block 2 */

    out = 128;

    mem[freq] = ftoi(321.123);

    dsp_phasor_init(mem, ph, out);

    for (i = 0; i < NUMBLOCKS; i++) {
        push(stk, freq | CONSTANT);
        push(stk, ph);
        ugen_phasor(mem, pstk);
        write_block(fp, mem, stk);
    }

    fclose(fp);

    return 0;
}
