#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

typedef struct dsp_phasor {
    float phs;
    float onedsr;
} dsp_phasor;

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    dsp_phasor *ph;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        2, sizeof(dsp_phasor) >> 2,
        &ugen);
    if (rc) return 2;

    /* ports */
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 1);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    ph = (dsp_phasor *)ugen_state(mem, ugen);
    if (ph == NULL) return 5;

    ph->phs = 0;
    ph->onedsr = 1.0 / sonilo_srate(mem);

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    dsp_phasor *ph;
    uint32_t *ports;
    sonilo_port p_frq, p_out;
    float phs;
    int n;

    ph = (dsp_phasor *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    p_frq = sonilo_port_from_word(mem, ports[0]);
    p_out = sonilo_port_from_word(mem, ports[1]);

    phs = ph->phs;

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float frq, o, incr;

        frq = sonilo_port_read(&p_frq, n);
        incr = frq * ph->onedsr;

        o = phs;

        phs += incr;

        if (phs >= 1.0) {
            phs -= 1.0;
        } else if (phs < 0.0) {
            phs += 1.0;
        }

        sonilo_port_write(&p_out, n, o);
    }

    ph->phs = phs;

    return 0;
}

int ugen_phasor(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("PHS");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
