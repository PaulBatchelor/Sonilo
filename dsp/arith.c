#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

static uint32_t add_init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk;
    uint32_t cmd;
    uint16_t ugen;

    stk = CTX_STACK(mem, ctx);
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    ugen = 0;
    rc = ugen_create(mem, ctx, (uint16_t) cmd, 3, 0, &ugen);
    if (rc) return 2;
    
    /* set up ports */
    rc = ugen_iport(mem, ctx, ugen, 1);
    if (rc) return 3;
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 4;
    rc = ugen_oport(mem, ctx, ugen, 2);
    if (rc) return 5;

    context_pstack_sweep(mem, ctx);

    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t add_render(uint32_t *mem, uint16_t ugen)
{
    uint32_t *ports;
    sonilo_port ins[2], out;
    int n;

    /* get ports */
    ports = ugen_ports(mem, ugen);
    ins[0] = sonilo_port_from_word(mem, ports[0]);
    ins[1] = sonilo_port_from_word(mem, ports[1]);
    out = sonilo_port_from_word(mem, ports[2]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float x, y;
        x = sonilo_port_read(&ins[0], n);
        y = sonilo_port_read(&ins[1], n);
        sonilo_port_write(&out, n, x + y);
    }

    return 0;
}

static uint32_t mul_init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk;
    uint32_t cmd;
    uint16_t ugen;

    stk = CTX_STACK(mem, ctx);
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    ugen = 0;
    rc = ugen_create(mem, ctx, (uint16_t) cmd, 3, 0, &ugen);
    if (rc) return 2;
    
    /* set up ports */
    rc = ugen_iport(mem, ctx, ugen, 1);
    if (rc) return 3;
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 4;
    rc = ugen_oport(mem, ctx, ugen, 2);
    if (rc) return 5;

    context_pstack_sweep(mem, ctx);

    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t mul_render(uint32_t *mem, uint16_t ugen)
{
    uint32_t *ports;
    sonilo_port ins[2], out;
    int n;

    /* get ports */
    ports = ugen_ports(mem, ugen);
    ins[0] = sonilo_port_from_word(mem, ports[0]);
    ins[1] = sonilo_port_from_word(mem, ports[1]);
    out = sonilo_port_from_word(mem, ports[2]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float x, y;
        x = sonilo_port_read(&ins[0], n);
        y = sonilo_port_read(&ins[1], n);
        sonilo_port_write(&out, n, x * y);
    }

    return 0;
}

int ugen_arith(sonilo *s)
{
    int rc;

    rc = sonilo_ugen_register(s, "ADD", add_init, add_render);
    if (rc) return rc;
    rc = sonilo_ugen_register(s, "MUL", mul_init, mul_render);
    if (rc) return rc;

    return 0;
}
