#include <stdint.h>
#include "array.h"

int array_create(uint32_t *mem, uint16_t ctx, int wrdsz, uint16_t len)
{
    /* TODO */
    return 1;
}

/* set value of an array a[p] = x */
int array_read(uint32_t *mem, uint16_t a, uint16_t p, uint32_t x)
{
    /* TODO */
    return 1;
}

/* get value of an array x = a[p] */
int array_write(uint32_t *mem, uint16_t a, uint16_t p, uint32_t *x)
{
    /* TODO */
    return 1;
}

uint32_t array_value(uint32_t *mem, uint32_t ws)
{
    uint16_t addr;
    uint8_t start, end;
    uint32_t mask;

    addr = ws & 0xFFFF;
    ws >>= 16;

    start = ws & 0x1F;
    ws >>= 5;

    end = ws & 0x1F;
    ws >>= 5;

    if (start > end) {
        uint16_t tmp;
        /* TODO: xor trick */
        tmp = start;
        start = end;
        end = tmp;
    }

    mask = ((1 << (end - start + 1)) - 1) << start;

    return (mem[addr] & mask) >> start;
}
