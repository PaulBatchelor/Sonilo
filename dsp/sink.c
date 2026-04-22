#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk;
    uint16_t pstk;
    uint32_t cmd;
    uint16_t ugen;

    stk = CTX_STACK(mem, ctx);
    rc = array_pop(mem, stk, &cmd);
    if (rc) return 1;

    ugen = 0;
    rc = ugen_create(mem, ctx, (uint16_t) cmd, 2, 0, &ugen);
    if (rc) return 2;
    
    /* set up ports */
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 1);
    if (rc) return 4;
    
    /* TODO: param stack shouldn't be exposed here */
    pstk = CTX_PARAM_STACK(mem, ctx);
    pstack_sweep(mem, pstk);

    rc = array_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t p)
{
    /* TODO */
    return 1;
}

int ugen_sink(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("SNK");

    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
