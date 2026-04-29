#include <stdint.h>
#include <stddef.h>
#include "array.h"
#include "mem.h"

/* int array_create(uint32_t *mem, uint16_t ctx, int wrdsz, uint16_t len, uint16_t *pa) */
int array_create(uint32_t *mem, uint16_t stk)
{
    uint16_t ctx;
    uint8_t wrdsz;
    uint16_t len;
    uint16_t nbits;
    uint16_t nwords;
    int rc;
    uint16_t a;
    uint16_t i;
    uint32_t val;

    /* pop args: len.wrdsz context */

    val = 0;
    rc = array_pop(mem, stk, &val);
    if (rc) return 6;
    ctx = val & 0xFF;

    rc = array_pop(mem, stk, &val);
    if (rc) return 7;
    wrdsz = val & 0x7;
    val >>= 4;
    len = val & 0xFFFF;

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

    for (i = 1; i < nwords; i++) {
        mem[a + i] = 0;
    }

    /* push array address onto stack */

    rc = array_append(mem, stk, a);

    if (rc) return 8;

    return 0;
}

/* write value of an array a[p] = x */
int array_write(uint32_t *mem, uint16_t stk)
{
    uint16_t a, p;
    uint32_t x;
    int rc;
    uint32_t v;
    uint16_t len, wsz;
    uint16_t ow, ob;

    if (mem[stk] < 3) return 1;

    /* args: x p a */
    v = 0;
    rc = array_pop(mem, stk, &v);
    if (rc) return 1;
    a = v & 0xFFFF;

    rc = array_pop(mem, stk, &v);
    if (rc) return 2;
    p = v & 0xFFFF;

    rc = array_pop(mem, stk, &v);
    if (rc) return 3;
    x = v;

    /* extract length and word size from header */

    len = mem[a] & 0xFFFF;
    wsz = (mem[a] >> 16) & 7;

    /* bounds checking */

    if (p >= len) return 4;

    if (wsz > 5) return 5;

    /* calculate word and bit offsets */
    ow = p >> (5 - wsz);
    ob = p * (1 << wz) - (ow << 5);

    /* TODO: write bits to word */

    return -1;
}

/* read value of an array x = a[p] */
int array_read(uint32_t *mem, uint16_t stk)
{
    /* TODO: pop args: a, p */
    /* TODO: extract length and word size from header */
    /* TODO: bounds checking */
    /* TODO: calculate word and bit offsets */
    /* TODO: generate word slice */
    return -1;
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
