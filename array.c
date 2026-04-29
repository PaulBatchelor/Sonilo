#include <stdint.h>
#include <stddef.h>
#include "array.h"
#include "mem.h"

int array_create(uint32_t *mem, uint16_t ctx, int wrdsz, uint16_t len, uint16_t *pa)
{
    uint16_t nbits;
    uint16_t nwords;
    int rc;
    uint16_t a;
    uint16_t i;

    if (len == 0) return 1;

    if (wrdsz > 6) return 2;

    /* compute the number of words needed hold the bits */
    nbits = wrdsz * len;

    nwords = 0;
    while ((nwords << 3) < nbits) nwords++;

    /* add extra word for header */
    nwords++;

    if (nwords > 64) return 3;

    /* attempt to allocate words from context */
    a = 0;
    rc = sonilo_alloc(mem, ctx, nwords, &a);
    if (rc) return 4;

    /* initialize array */

    /* header: word size (2^k) | length */
    mem[a] = (wrdsz << 16) | len;

    /* zero out words */

    if (pa == NULL) return 5;

    for (i = 1; i < nwords; i++) {
        mem[a + i] = 0;
    }

    *pa = a;

    return 0;
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
