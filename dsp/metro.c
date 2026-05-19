#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

typedef struct dsp_metro {
    float phs;
} dsp_metro;

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    dsp_metro *met;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        2, sizeof(dsp_metro) >> 2,
        &ugen);
    if (rc) return 2;

    /* ports */
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 1);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    met = (dsp_metro *)ugen_state(mem, ugen);
    if (met == NULL) return 5;

    met->phs = -1;

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    dsp_metro *met;
    uint32_t *ports;
    sonilo_port in, out;
    float phs;
    int n;

    met = (dsp_metro *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    in = sonilo_port_from_word(mem, ports[0]);
    out = sonilo_port_from_word(mem, ports[1]);

    phs = met->phs;

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float i,  o;
        i = sonilo_port_read(&in, n);
        o = i < phs || phs < 0;
        phs = i;
        sonilo_port_write(&out, n, o);
    }

    met->phs = phs;

    return 0;
}

int ugen_metro(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("MET");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
