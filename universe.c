#include <stdint.h>
#include "sonilo.h"

/* size: get the size of the universe (in words) */
size_t universe_size(void)
{
    /* 2^16 blocks * 1024 bytes/block */
    return 67108864;
}

/* get: memory pointer to specific megablock */
int universe_get(uint32_t *u, uint16_t b, uint32_t **blk)
{
    /* TODO: implement */
    return 1;
}

/* pull: pull data from memory to sonilo memory */
int universe_pull(uint32_t *u,
    uint16_t to,
    uint16_t from,
    uint16_t sz)
{
    /* TODO: implement */
    return 1;
}

/* push: push data to universe from sonilo */
int universe_push(uint32_t *u,
    uint16_t to,
    uint16_t from,
    uint16_t sz)
{
    /* TODO: implement */
    return 1;
}
