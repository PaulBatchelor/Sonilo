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

int universe_pull(uint32_t *u,
    uint16_t dst,
    uint16_t src,
    uint16_t sz)
{
    memmove(&u[dst], &u[src<<8], sz << 2);

    return 0;
}

int universe_push(uint32_t *u,
    uint16_t dst,
    uint16_t src,
    uint16_t sz)
{
    memmove(&u[dst<<8], &u[src], sz << 2);
    return 0;
}
