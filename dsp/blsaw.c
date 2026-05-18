#include <stdint.h>
#include <math.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

struct dsp_blep {
    uint32_t freq;
    uint32_t out;
    float pfreq;
    float onedsr;
    float inc;
    float phs;
    float A;
    float prev;
    float R, x, y;
};

static float polyblep(float dt, float t)
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0;
    } else if(t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }

    return 0.0;
}

#if 0
static void init(struct dsp_blep *blep, uint32_t sr)
{
    blep->pfreq = -1;
    blep->onedsr = 1.0 / sr;
    blep->inc = 0;
    blep->phs = 0;
    blep->A = exp(-1.0/(0.1 * sr));
    blep->prev = 0;
    blep->R = exp(-1.0/(0.0025 * sr));
    blep->x = 0;
    blep->y = 0;
}
#endif

static float tick(struct dsp_blep *blep,
                  float (*wave)(struct dsp_blep *, float))
{
    float out;

    out = 0.0;

    out = wave(blep, blep->phs);
    blep->phs += blep->inc;

    if (blep->phs > 1.0) {
        blep->phs -= 1.0;
    }

    return out;
}

static float blep_saw(struct dsp_blep *blep, float t)
{
    float value;

    value = (2.0 * t) - 1.0;
    value -= polyblep(blep->inc, t);

    return value;
}
#if 0
static void compute(uint32_t *mem, struct dsp_blep *blep)
{

    int n;
    int blksz;

    blksz = sonilo_blksz(mem);

    for (n = 0; n < blksz; n++) {
        float out, freq;
        freq = sonilo_port_readf(mem, blep->freq, n);
        if (freq != blep->pfreq) {
            blep->pfreq = freq;
            blep->inc = freq * blep->onedsr;
        }
        out = tick(blep, blep_saw);
        sonilo_port_writef(mem, blep->out, n, out);
    }
}
#endif

#if 0
int ugen_blsaw_init(uint32_t *mem, uint32_t pstk)
{
    int rc;
    uint32_t *stk;
    uint32_t out;
    struct dsp_blep *blep;
    uint32_t pblep;
    uint32_t sr;

    stk = &mem[pstk];

    out = pblep = 0;

    rc = stack_pop(stk, &pblep);
    if (rc) return 1;

    rc = stack_pop(stk, &out);
    if (rc) return 1;

    blep = (struct dsp_blep *) &mem[pblep];

    sr = sonilo_sr(mem);
    init(blep, sr);
    blep->out = out;
    
    return 0;
}
#endif

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk;
    uint32_t cmd;
    uint16_t ugen;
    struct dsp_blep *blep;
    uint32_t sr;

    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);

    ugen = 0;
    rc = ugen_create(mem, ctx, (uint16_t) cmd, 2, 11, &ugen);
    if (rc) return 2;

    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 1);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);
    blep = (struct dsp_blep *)ugen_state(mem, ugen);
    if (blep == NULL) return 5;
    sr = sonilo_srate(mem);

    blep->pfreq = -1;
    blep->onedsr = 1.0 / sr;
    blep->inc = 0;
    blep->phs = 0;
    blep->A = exp(-1.0/(0.1 * sr));
    blep->prev = 0;
    blep->R = exp(-1.0/(0.0025 * sr));
    blep->x = 0;
    blep->y = 0;

    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    int n;
    struct dsp_blep *blep;
    uint32_t *ports;
    sonilo_port freq, out;

    blep = (struct dsp_blep *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    freq = sonilo_port_from_word(mem, ports[0]);
    out = sonilo_port_from_word(mem, ports[1]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float f, o;
        f = sonilo_port_read(&freq, n);
        if (f != blep->pfreq) {
            blep->pfreq = f;
            blep->inc = f * blep->onedsr;
        }
        o = tick(blep, blep_saw);
        sonilo_port_write(&out, n, o);
    }

    return 0;
}

int ugen_blsaw(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("SAW");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
