#include <stdint.h>
#define CONSTANT 0x10000

int push(uint32_t *stk, uint32_t x);
int pop(uint32_t *stk, uint32_t *x);

typedef struct sonilo {
    uint32_t *mem;
    /* data stack / stack pointer */
    int *ds;
    int *sp;
} sonilo;

struct dsp_phasor {
    uint32_t freq;
    uint32_t out;
    float phs;
    float onedsr;
};

int sonilo_blksz(uint32_t *mem)
{
    return 64;
}

uint32_t sonilo_sr(uint32_t *mem)
{
    return 44100;
}

void sonilo_port_writef(uint32_t *mem, uint32_t p, int i, float x)
{
    uint32_t ival;
    uint32_t type;
    uint32_t pos;

    pos = p & 0xFFFF;
    type = (p >> 16) & 0xFFFF;
    ival = *(uint32_t *)&x;

    if (type == CONSTANT) {
        mem[pos] = ival;
        return;
    }

    mem[pos + i] = ival;
}

float sonilo_port_readf(uint32_t *mem, uint32_t p, int i)
{
    uint32_t type;
    uint32_t pos;
    float f;

    pos = p & 0xFFFF;
    type = (p >> 16) & 0xFFFF;

    if (type == CONSTANT) {
        f = mem[pos];
        return f;
    }
    return mem[pos + i];
}


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

void ugen_phasor(uint32_t *mem, uint32_t pstk)
{
    uint32_t *stk;
    uint32_t pfreq;
    uint32_t p;
    struct dsp_phasor *ph;
    int rc;

    stk = &mem[pstk];
    pfreq = p = 0;

    rc = pop(stk, &p);

    if (rc) {
        /* TODO: error handling */
    }

    rc = pop(stk, &pfreq);

    if (rc) {
        /* TODO: error handling */
    }

    ph = (struct dsp_phasor *) &mem[p];

    ph->freq = pfreq;

    dsp_phasor_compute(mem, p);

    rc = push(stk, ph->out);

    if (rc) {
        /* TODO: error handling */
    }
}
