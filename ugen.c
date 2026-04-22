#include <stdint.h>
#include <stddef.h>
#include "mem.h"
#include "ugen.h"
#include "context.h"

/* location of pstack inside block, RC is in beginning of block */
#define PSTACK_OFFSET 2048
#define STACK_SIZE 19

static uint32_t * get_stack(uint32_t *mem, uint16_t p)
{
    return &mem[p + PSTACK_OFFSET];
}

void pstack_init(uint32_t *mem, uint16_t p)
{
    uint32_t *stk;
    int i;

    stk = get_stack(mem, p);

    for (i = 0; i <= STACK_SIZE; i++) {
        stk[i] = 0;
    }
}

uint32_t pstack_param(int type, uint32_t data)
{
    return (data << 2) | (type & 0x3);
}

int pstack_push(uint32_t *mem, uint16_t p, uint32_t w)
{
    uint32_t *stk;
    int sp;

    stk = get_stack(mem, p);
    sp = stk[0];

    /* stack overflow */
    if (sp >= STACK_SIZE) return 1;

    sp++;
    stk[sp] = w;
    stk[0] = sp;

    /* update RC if param is a block */
    if ((w & 3) == PARAM_BLOCK) {
        int rc;
        rc = rc_incr(mem, p, w >> 2);
        /* address not found */
        if (rc) return 2;
    }

    return 0;
}

int pstack_pop(uint32_t *mem, uint16_t p, uint32_t *wp)
{
    uint32_t *stk;
    int sp;
    uint32_t w;

    stk = get_stack(mem, p);
    sp = stk[0];

    /* stack underflow */
    if (sp == 0) return 1;

    w = stk[sp];
    sp--;
    stk[0] = sp;

    if (wp != NULL) *wp = w;

    if ((w & 3) == PARAM_BLOCK) {
        return rc_decr(mem, p, w >> 2);
    }

    return 0;
}

int pstack_hold(uint32_t *mem, uint16_t p)
{
    uint32_t *stk;
    int sp;

    stk = get_stack(mem, p);
    sp = stk[0];

    if (sp == 0) return 1;

    /* hold only works on blocks */
    if ((stk[sp] & 3) != PARAM_BLOCK) return 2;

    return rc_hold(mem, p, stk[sp] >> 2);
}

int pstack_unhold(uint32_t *mem, uint16_t p)
{
    uint32_t *stk;
    int sp;

    stk = get_stack(mem, p);
    sp = stk[0];

    if (sp == 0) return 1;

    /* hold only works on blocks */
    if ((stk[sp] & 3) != PARAM_BLOCK) return 2;

    return rc_unhold(mem, p, stk[sp] >> 2);
}

int pstack_rot(uint32_t *mem, uint16_t p)
{
    uint32_t *stk;
    int sp;
    int a, b, c;

    stk = get_stack(mem, p);

    sp = stk[0];

    if (sp < 3) return 1;

    a = stk[sp - 2];
    b = stk[sp - 1];
    c = stk[sp];

    stk[sp - 2] = c;
    stk[sp - 1] = a;
    stk[sp] = b;

    return 0;
}

int pstack_dup(uint32_t *mem, uint16_t p)
{
    uint32_t *stk;
    int sp;

    stk = get_stack(mem, p);

    sp = stk[0];

    if (sp == 0) return 1;

    return pstack_push(mem, p, stk[sp]);
}

int pstack_swap(uint32_t *mem, uint16_t p)
{
    uint32_t *stk;
    int sp;
    uint32_t tmp;

    stk = get_stack(mem, p);

    sp = stk[0];
    if (sp < 2) return 1;

    tmp = stk[sp];
    stk[sp] = stk[sp - 1];
    stk[sp - 1] = tmp;

    return 0;
}

int pstack_sweep(uint32_t *mem, uint16_t p)
{
    return rc_sweep(mem, p);
}

int ugen_create(uint32_t *mem,
        uint16_t ctx,
        uint16_t cmd,
        int nports,
        int sz,
        uint16_t *ugen)
{
    uint16_t top, ports, state;
    int rc;

    top = ports = state = 0;
    /* top-level ugen struct allocation (2 words) */
    rc = sonilo_alloc(mem, ctx, 2, &top);
    if (rc) return 1;

    /* allocate and initialize port array (N words) */
    if (nports > MAX_PORTS || nports <= 0) return 2;
    rc = sonilo_alloc(mem, ctx, nports, &ports);
    if (rc) return 3;
   
    state = 0;
    /* allocate user data (SZ words) */
    if (sz > 0) {
        rc = sonilo_alloc(mem, ctx, sz, &state);
        if (rc) return 4;
    }

    /* store ugen addresses in memory */
    mem[top] = (state << 16) | ports;
    mem[top + 1] = cmd;

    if (ugen == NULL) return 5;

    *ugen = top;

    return 0;
}

static uint32_t *get_ports(uint32_t *mem, uint16_t ugen)
{
    return &mem[mem[ugen] & 0xFFFF];
}

uint32_t* ugen_ports(uint32_t *mem, uint16_t ugen)
{
    return get_ports(mem, ugen);
}

int ugen_iport(uint32_t *mem, uint16_t ctx, uint16_t ugen, int port)
{
    uint32_t w;
    int rc;
    uint16_t pstk;
    uint32_t *ports;

    pstk = CTX_PARAM_STACK(mem, ctx);
    /* pop param from stack */
    w = 0;
    rc = pstack_pop(mem, pstk, &w);
    if (rc) return 1;

    /* set param to port number */
    ports = get_ports(mem, ugen);
    ports[port] = w;
    return 0;
}

static int new_block_port(uint32_t *mem, uint16_t ctx, uint32_t *w)
{
    int rc;
    uint16_t b;

    if (w == NULL) return 2;

    /* allocate block to main */
    rc = context_mkblock(mem, ctx, &b);
    if (rc) return 1;

    /* create parameter word from block */
    *w = (b << 2) | PORT_BLOCK;

    return 0;
}

int ugen_oport(uint32_t *mem, uint16_t ctx, uint16_t ugen, int port)
{
    uint32_t w;
    int rc;
    uint32_t *ports;
    uint16_t pstk;

    /* allocate a block inside of a port */
    rc = new_block_port(mem, ctx, &w);
    if (rc) return 1;

    pstk = CTX_PARAM_STACK(mem, ctx);

    /* push param word onto stack */
    rc = pstack_push(mem, pstk, w);
    if (rc) return 2;

    /* save param word to port */
    ports = get_ports(mem, ugen);
    ports[port] = w;
    return 0;
}

void * ugen_state(uint32_t *mem, uint16_t ugen)
{
    uint16_t p;
    p = mem[ugen] >> 16;
    if (p == 0) return NULL;

    return &mem[p];
}
