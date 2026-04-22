#include <math.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

#ifndef M_PI
#define M_PI 3.1415926535
#endif

struct state {
    float phs;
};

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk;
    uint16_t pstk;
    uint32_t cmd;
    uint16_t ugen;
    struct state *st;

    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = array_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* set up initial ugen */
    ugen = 0;
    rc = ugen_create(mem, ctx, (uint16_t) cmd, 2, 1, &ugen);
    if (rc) return 2;

    /* set up ports */
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 1);
    if (rc) return 4;

    /* TODO: param stack shouldn't be exposed here */
    pstk = CTX_PARAM_STACK(mem, ctx);
    pstack_sweep(mem, pstk);

    /* initialize ugen state */
    st = (struct state *)ugen_state(mem, ugen);
    if (st == NULL) return 5;

    st->phs = 0.0;

    /* push ugen address to stack */
    
    rc = array_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    uint32_t *ports;
    struct state *st;
    sonilo_port freq, out;
    int n;
    uint32_t sr;
    float phs;

    /* internal state */
    st = (struct state *)ugen_state(mem, ugen);

    /* get ports */
    ports = ugen_ports(mem, ugen);
    freq = sonilo_port_from_word(mem, ports[0]);
    out = sonilo_port_from_word(mem, ports[1]);

    /* global samplerate */
    sr = sonilo_srate(mem);

    phs = st->phs;
    for (n = 0; n < UGEN_BLKSZ; n++) {
        float f, o;
        f = sonilo_port_read(&freq, n);
        o = sin(2.0 * M_PI * phs);
        phs += f * (1.0 / sr);
        sonilo_port_write(&out, n, o);
    }
    st->phs = phs;

    return 0;
}

int ugen_sine(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("SIN");

    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
