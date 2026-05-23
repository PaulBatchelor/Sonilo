#include <stdint.h>
#include <string.h>
#include "sonilo.h"

/* size: get the size of the universe (in words) */
size_t universe_size(void)
{
    /* 2^16 blocks * 1024 bytes/block */
    return 67108864;
}

/* get: memory pointer to specific megablock */
/* note: block position in in words (256 words / block) */
int universe_get(uint32_t *u, uint16_t b, uint32_t **blk)
{
    if (blk == NULL) return 1;
    *blk = &u[b << 8];
    return 0;
}

/* pull: pull data from memory to sonilo memory */
int universe_pull(uint32_t *u,
    uint16_t from,
    uint16_t to,
    uint16_t sz)
{
    uint32_t *src, *dst;

    /* source: universe (block id) */
    src = &u[from << 8];

    /* destination: sonilo (memory address) */
    dst = &u[to];

    memmove(dst, src, sz << 2);

    return 0;
}

/* push: push data to universe from sonilo */
int universe_push(uint32_t *u,
    uint16_t to,
    uint16_t from,
    uint16_t sz)
{
    uint32_t *src, *dst;

    /* source: sonilo (memory address) */
    src = &u[from];

    /* destination: universe (block id) */
    dst = &u[to << 8];

    memmove(dst, src, sz << 2);
    return 0;
}
