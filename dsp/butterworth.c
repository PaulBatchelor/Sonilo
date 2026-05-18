#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

typedef struct {
    float freq, lfreq;
    float bw, lbw; /* for bandpass filter only */
    float a[7];
    float pidsr, tpidsr;
} butterworth;

#define ROOT2 1.4142135623730950488

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float filter(float in, float *a)
{
    float t, y;
    /* a5 = t(n - 1); a6 = t(n - 2) */
    t = in - a[3]*a[5] - a[4]*a[6];
    y = t*a[0] + a[1]*a[5] + a[2]*a[6];
    a[6] = a[5];
    a[5] = t;
    return y;
}

static void bw_init(butterworth *bw, int sr)
{
    int i;
    bw->freq = 1000.0;
    bw->bw = 1000.0;
    bw->lfreq = -1;
    bw->lbw = -1;
    bw->pidsr = M_PI / (float)sr;
    for (i = 0; i < 7; i++) bw->a[i] = 0.0;
}

static float butlp_tick(butterworth *bw, float in)
{
    if (bw->freq != bw->lfreq) {
        float *a, c;
        a = bw->a;
        bw->lfreq = bw->freq;
        /* derive C constant used in BLT */
        c = 1.0 / tan((float)(bw->pidsr * bw->lfreq));

        /* perform BLT, store components */
        a[0] = 1.0 / (1.0 + c*ROOT2 + c*c);
        a[1] = 2*a[0];
        a[2] = a[0];
        a[3] = 2.0 * (1.0 - c*c) * a[0];
        a[4] = (1.0 - c*ROOT2 + c*c) * a[0];
    }

    return filter(in, bw->a);
}

static float buthp_tick(butterworth *bw, float in)
{
    if (bw->freq != bw->lfreq) {
        float *a, c;
        a = bw->a;
        bw->lfreq = bw->freq;
        /* derive C constant used in BLT */
        c = tan((float)(bw->pidsr * bw->freq));

        /* perform BLT, store components */
        a[0] = 1.0 / (1.0 + c*ROOT2 + c*c);
        a[1] = -2*a[0];
        a[2] = a[0];
        a[3] = 2.0 * (c*c - 1.0) * a[0];
        a[4] = (1.0 - c*ROOT2 + c*c) * a[0];
    }

    return filter(in, bw->a);
}

static float butbp_tick(butterworth *bw, float in)
{
    if (bw->bw != bw->lbw || bw->freq != bw->lfreq) {
        float *a, c, d;
        a = bw->a;
        bw->lfreq = bw->freq;
        bw->lbw = bw->bw;

        /* Perform BLT and store components */
        c = 1.0 / tan((float)(bw->pidsr * bw->bw));
        d = 2.0 * cos((float)(2.0*bw->pidsr * bw->freq));
        a[0] = 1.0 / (1.0 + c);
        a[1] = 0.0;
        a[2] = -a[0];
        a[3] = - c * d * a[0];
        a[4] = (c - 1.0) * a[0];
    }

    return filter(in, bw->a);
}

static uint32_t lpf_init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk;
    uint32_t cmd;
    uint16_t ugen;
    butterworth *bw;
    uint32_t sr;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* initialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        3, sizeof(butterworth) >> 2,
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
    bw = (butterworth *)ugen_state(mem, ugen);
    if (bw == NULL) return 6;

    sr = sonilo_srate(mem);
    bw_init(bw, sr);

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 7;

    return 0;
}

static uint32_t lpf_render(uint32_t *mem, uint16_t ugen)
{
    butterworth *bw;
    uint32_t *ports;
    sonilo_port in, freq, out;
    int n;

    bw = (butterworth *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    in = sonilo_port_from_word(mem, ports[0]);
    freq = sonilo_port_from_word(mem, ports[1]);
    out = sonilo_port_from_word(mem, ports[2]);
    
    for (n = 0; n < UGEN_BLKSZ; n++) {
        float f, i, o;
        i = sonilo_port_read(&in, n);
        f = sonilo_port_read(&freq, n);
        bw->freq = f;
        o = butlp_tick(bw, i);
        sonilo_port_write(&out, n, o);
    }

    return 0;
}

int ugen_butterworth(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("LPF");

    rc = sonilo_command(s, key, lpf_init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), lpf_render);
    if (rc) return 2;

    return 0;
}
