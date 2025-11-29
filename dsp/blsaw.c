#include <stdint.h>
#include <math.h>
#include "util.h"

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

int ugen_blsaw(uint32_t *mem, uint32_t pstk)
{
    uint32_t *stk;
    int rc;
    uint32_t pfreq;
    uint32_t pblep;
    struct dsp_blep *blep;

    stk = &mem[pstk];

    rc = stack_pop(stk, &pblep);
    if (rc) return 1;

    rc = stack_pop(stk, &pfreq);
    if (rc) return 1;

    blep = (struct dsp_blep *)&mem[pblep];

    blep->freq = pfreq;

    compute(mem, blep);

    rc = stack_push(stk, blep->out);
    if (rc) return 1;

    return 0;
}
