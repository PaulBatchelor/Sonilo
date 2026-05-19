#include <math.h>
#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

typedef struct dsp_smoother {
    float smooth;
    float a1, b0, y0, psmooth;
    float onedsr;
    uint32_t init;
} dsp_smoother;

static void smoother_init(dsp_smoother *s, int sr);
static void reset(dsp_smoother *s, float in);
static float tick(dsp_smoother *s, float in);

static void smoother_init(dsp_smoother *s, int sr)
{
    s->smooth = 0.01;
    s->psmooth = -1;
    s->a1 = 0;
    s->b0 = 0;
    s->y0 = 0;
    s->onedsr = 1.0 / sr;
    s->init = 1;
}

static void reset(dsp_smoother *s, float in)
{
    s->y0 = in;
}

static float tick(dsp_smoother *s, float in)
{
    float out;

    if (s->psmooth != s->smooth) {
        s->a1 = pow(0.5, s->onedsr/s->smooth);
        s->b0 = 1.0 - s->a1;
        s->psmooth = s->smooth;
    }

    s->y0 = s->b0 * in + s->a1 * s->y0;
    out = s->y0;

    return out;
}

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    uint16_t stk, ugen;
    int rc;
    uint32_t cmd;
    dsp_smoother *smooth;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* create ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        3, sizeof(dsp_smoother) >> 2,
        &ugen);

    if (rc) return 2;

    /* ports */
    rc = ugen_iport(mem, ctx, ugen, 1);
    if (rc) return 4;
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 2);
    if (rc) return 5;

    context_pstack_sweep(mem, ctx);

    /* state */
    smooth = (dsp_smoother *)ugen_state(mem, ugen);
    if (smooth == NULL) return 6;

    smoother_init(smooth, sonilo_srate(mem));

    /* push ugen */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    int n;
    dsp_smoother *smooth;
    uint32_t *ports;
    sonilo_port in, time, out;

    smooth = (dsp_smoother *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    in = sonilo_port_from_word(mem, ports[0]);
    time = sonilo_port_from_word(mem, ports[1]);
    out = sonilo_port_from_word(mem, ports[2]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float i, t, o;
        i = sonilo_port_read(&in, n);
        t = sonilo_port_read(&time, n);
        smooth->smooth = t;
        if (smooth->init) {
            smooth->init = 0;
            reset(smooth, i);
        }
        o = tick(smooth, i);
        sonilo_port_write(&out, n, o);
    }
    return 0;
}

int ugen_smoother(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("SMO");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
