#include <stdint.h>
#include "util.h"
#define CONSTANT 0x10000

int stack_pop(uint32_t *stk, uint32_t *x)
{
    uint32_t sp;

    sp = stk[0];
    if (sp == 0) return 1;
    *x = stk[sp];
    sp--;
    stk[0] = sp;

    return 0;
}

int stack_push(uint32_t *stk, uint32_t x)
{
    uint32_t sp;

    sp = stk[0];
    if (sp >= 64) return 1;
    sp++;
    stk[sp] = x;
    stk[0] = sp;


    return 0;
}

uint32_t ftoi(float f)
{
    return *(uint32_t *)(&f);
}

float itof(uint32_t i)
{
    return *(float *)&i;
}

int sonilo_blksz(uint32_t *mem)
{
    return 64;
}

uint32_t sonilo_sr(uint32_t *mem)
{
    return 44100;
}

void sonilo_port_writef(uint32_t *mem, uint32_t p, int i, float x)
{
    uint32_t ival;
    uint32_t type;
    uint32_t pos;

    pos = p & 0xFFFF;
    type = (p >> 16) & 0xFFFF;
    ival = ftoi(x);

    if (type == CONSTANT) {
        mem[pos] = ival;
        return;
    }

    mem[pos + i] = ival;
}

float sonilo_port_readf(uint32_t *mem, uint32_t p, int i)
{
    uint32_t type;
    uint32_t pos;
    float f;

    pos = p & 0xFFFF;
    type = (p >> 16) & 0xFFFF;

    if (type == CONSTANT) {
        f = itof(mem[pos]);
        return f;
    }
    return itof(mem[pos + i]);
}

uint32_t sonilo_constant(uint16_t a)
{
    return a | CONSTANT;
}

uint32_t sonilo_block(uint16_t a)
{
    /* pass-thru: no upper bits set */
    return a;
}
