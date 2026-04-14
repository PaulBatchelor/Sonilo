#include "sonilo.h"
#include "ins.h"
#include "mem.h"

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

void sonilo_init(sonilo *s)
{
    uint32_t i;

    /* zero out memory */

    for (i = 0; i < 65536; i++) {
        s->mem[i] = 0;
    }

    s->rw = 0;

    s->a = s->b = 0;

    s->blocklist = 0;

    /* setup blocklist */

    blocklist_init(s->mem, s->blocklist);
}

void sonilo_ctx_init(sonilo_ctx *ctx, sonilo *s)
{
    ctx->context = context_init(s->mem, s->blocklist);
    ctx->s = s;
    /* TODO: error checking */
    context_allocator_setup(ctx->s->mem, ctx->context);
    ctx->allocator = context_allocator(ctx->s->mem, ctx->context);
}

void sonilo_ctx_destroy(sonilo_ctx *ctx)
{
    context_destroy(ctx->s->mem, ctx->context);
}

uint32_t* sonilo_mem(sonilo *s, uint16_t p)
{
    return &s->mem[p];
}

uint16_t sonilo_alloc(sonilo_ctx *ctx, int sz)
{
    /* TODO: get allocator via context_allocator() */
    /* TODO: call allocator_alloc using internal allocator */
    /* TODO: mem_alloc: takes args from allocator, returns local address */
    /* TODO: mem_alloc() doesn't use a stack? */
    /* TODO: add base address and local address */
    /* TODO: create add operation */
    /* TODO: pop values, return */

    return 0;
}

void sonilo_free(sonilo_ctx *ctx, uint16_t p)
{
    /* TODO: get allocator via context_allocator */
    /* TODO: allocator_free() */
    /* TODO: mem_free() */
}

size_t sonilo_sizeof(void)
{
    return sizeof(sonilo);
}

void sonilo_ugen_init(sonilo_ctx *ctx, sonilo_ugen *u, int nports, int sz)
{
    /* TODO: allocate top struct */
    /* TODO: allocate and initialize port array */
    /* TODO: set DSP callback to NOP */
}

int sonilo_iport(sonilo_ugen *u, int port)
{
    /* TODO: get param stack */
    /* TODO: pop param from stack */
    /* TODO: set param to port number */
    return 0;
}

int sonilo_oport(sonilo_ugen *u, int port)
{
    /* TODO: get a block (somewhere) */
    /* TODO: convert to param word data */
    /* TODO: push param word onto stack */
    /* TODO: save param word to port */
    return 0;
}

int sonilo_constant(sonilo_ctx *ctx, float c)
{
    /* TODO: convert constant to param */
    /* TODO: push param onto stack */
    return 0;
}

int sonlio_clean(sonilo_ctx *ctx)
{
    /* TODO: call rc_sweep */
    return 0;
}

void sonilo_ugen_compute(sonilo_ugen *u)
{
    /* TODO */
}
