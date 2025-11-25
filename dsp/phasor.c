#include <stdint.h>
#include "util.h"

struct dsp_phasor {
    uint32_t freq;
    uint32_t out;
    float phs;
    float onedsr;
};

void dsp_phasor_init(uint32_t *mem, uint32_t p, uint32_t out)
{
    uint32_t sr;
    struct dsp_phasor *ph;

    ph = (struct dsp_phasor *)&mem[p];
    sr = sonilo_sr(mem);
    ph->phs = 0;
    ph->onedsr = 1.0 / sr;
    ph->out = out;
}

void dsp_phasor_compute(uint32_t *mem, uint32_t p)
{
    int n;
    int blksz;
    struct dsp_phasor *ph;

    blksz = sonilo_blksz(mem);

    ph = (struct dsp_phasor *)&mem[p];

    for (n = 0; n < blksz; n++) {
        float out;
        float phs;
        float incr;
        float freq;

        phs = ph->phs;
        freq = sonilo_port_readf(mem, ph->freq, n);
        incr = freq * ph->onedsr;

        out = phs;

        phs += incr;

        if (phs >= 1.0) {
            phs -= 1.0;
        } else if (phs < 0.0) {
            phs += 1.0;
        }

        ph->phs = phs;

        sonilo_port_writef(mem, ph->out, n, out);
    }
}

int ugen_phasor_init(uint32_t *mem, uint32_t pstk)
{
    uint32_t *stk;
    uint32_t out;
    uint32_t ph;
    int rc;

    stk = &mem[pstk];

    rc = stack_pop(stk, &ph);

    if (rc) {
        return 1;
    }

    rc = stack_pop(stk, &out);

    if (rc) {
        return 1;
    }

    dsp_phasor_init(mem, ph, out);
    return 0;
}

int ugen_phasor(uint32_t *mem, uint32_t pstk)
{
    uint32_t *stk;
    uint32_t pfreq;
    uint32_t p;
    struct dsp_phasor *ph;
    int rc;

    stk = &mem[pstk];
    pfreq = p = 0;

    rc = stack_pop(stk, &p);

    if (rc) {
        return 1;
    }

    rc = stack_pop(stk, &pfreq);

    if (rc) {
        return 1;
    }

    ph = (struct dsp_phasor *) &mem[p];

    ph->freq = pfreq;

    dsp_phasor_compute(mem, p);

    rc = stack_push(stk, ph->out);

    if (rc) {
        return 1;
    }

    return 0;
}
