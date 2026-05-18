#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

typedef struct dsp_clock {
    float phs;
    float onedsr;
} dsp_clock;

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd, sr;
    dsp_clock *clk;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        2, sizeof(dsp_clock) >> 2,
        &ugen);
    if (rc) return 2;

    /* ports */
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 1);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    clk = (dsp_clock *)ugen_state(mem, ugen);
    if (clk == NULL) return 5;

    sr = sonilo_srate(mem);
    clk->phs = 0;
    clk->onedsr = 1.0 / sr;

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    dsp_clock *clk;
    uint32_t *ports;
    sonilo_port bpm, out;
    float onedsr, phs;
    int n;

    clk = (dsp_clock *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    bpm = sonilo_port_from_word(mem, ports[0]);
    out = sonilo_port_from_word(mem, ports[1]);

    phs = clk->phs;
    onedsr = clk->onedsr;

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float f,  o;
        f = sonilo_port_read(&bpm, n);
        /* convert BPM to CPS (Hz) */
        f *= 1.0 / 60.0;

        o = phs;

        phs += f * onedsr;

        if (phs > 1.0) {
            phs = 0.0;
        }

        sonilo_port_write(&out, n, o);
    }

    clk->phs = phs;

    return 0;
}

int ugen_clock(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("CLK");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
