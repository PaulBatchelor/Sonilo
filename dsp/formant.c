#include <stdint.h>
#include <math.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 8 words */
typedef struct dsp_formant {
    float y[2];
    float a, b, c;
    float bw;
    float frq;
    uint32_t mode;
} dsp_formant;

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    dsp_formant *fmt;
    int i;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        4, sizeof(dsp_formant) >> 2,
        &ugen);
    if (rc) return 2;

    /* ports */
    for (i = 2; i >= 0; i--) {
        rc = ugen_iport(mem, ctx, ugen, i);
        if (rc) return 3;
    }
    rc = ugen_oport(mem, ctx, ugen, 3);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    fmt = (dsp_formant *)ugen_state(mem, ugen);
    if (fmt == NULL) return 5;

    fmt->a = fmt->b = fmt->c = 0;
    fmt->mode = 0;
    fmt->y[0] = fmt->y[1] = 0;
    fmt->bw = fmt->frq = 0;

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    dsp_formant *fmt;
    uint32_t *ports;
    sonilo_port p_in, p_frq, p_bw, p_out;
    int n;

    fmt = (dsp_formant *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    p_in = sonilo_port_from_word(mem, ports[0]);
    p_frq = sonilo_port_from_word(mem, ports[1]);
    p_bw = sonilo_port_from_word(mem, ports[2]);
    p_out = sonilo_port_from_word(mem, ports[3]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float in, frq, bw, out;
        in = sonilo_port_read(&p_in, n);
        frq = sonilo_port_read(&p_frq, n);
        bw = sonilo_port_read(&p_bw, n);

        if (bw != fmt->bw || frq != fmt->frq) {
            uint32_t sr;
            float T;
            sr = sonilo_srate(mem);
            T = 1.0 / (float)sr;
            fmt->c = -exp(-2.0*M_PI*bw*T);
            fmt->b = 2*exp(-M_PI*bw*T)*cos(2.0*M_PI*frq*T);
            fmt->a = 1.0 - fmt->b - fmt->c;

            fmt->bw = bw;
            fmt->frq = frq;
        }

        out = fmt->a*in
            + fmt->b*fmt->y[0]
            + fmt->c*fmt->y[1];
        fmt->y[1] = fmt->y[0];
        fmt->y[0] = out;
        sonilo_port_write(&p_out, n, out);
    }

    return 0;
}

int ugen_formant(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("FMT");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
