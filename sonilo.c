#include "sonilo.h"
#include "ins.h"

struct sonilo {
    /* linear memory */
    uint32_t mem[65536];
    /* read/write register */

    uint32_t rw;

    /* a/b cursors */
    uint16_t a, b;

    /* pointer to blocklist */
    uint32_t blocklist;

    /* instruction map lookup */
    instr_map instr;
};

struct sonilo_ctx {
    sonilo *s;
    int32_t *zpage;
};

void sonilo_init(sonilo *s)
{
    /* TODO */
}

void sonilo_ctx_init(sonilo_ctx *ctx, sonilo *s)
{
    /* TODO */
}

void sonilo_ctx_destroy(sonilo_ctx *ctx)
{
    /* TODO */
}

uint32_t* sonilo_mem(sonilo *s, uint32_t p)
{
    /* TODO */
    return 0;
}

uint32_t sonilo_alloc(sonilo_ctx *ctx, int sz)
{
    /* TODO */
    return 0;
}

void sonilo_free(sonilo_ctx *ctx, uint32_t p)
{
    /* TODO */
}

uint32_t sonilo_block_pop(sonilo *s)
{
    /* TODO */
    return 0;
}

void sonilo_block_push(sonilo *s, uint32_t p)
{
    /* TODO */
}

size_t sonilo_sizeof(void)
{
    return sizeof(sonilo);
}
