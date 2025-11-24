#include <stdint.h>

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
    /* TODO: type checking */
    mem[p + i] = x;
}

float sonilo_port_readf(uint32_t *mem, uint32_t p, int i)
{
    /* TODO: type checking */
    return mem[p + i];
}


void dsp_phasor_init(uint32_t *mem, uint32_t p)
{
    uint32_t sr;
    struct dsp_phasor *ph;

    ph = (struct dsp_phasor *)&mem[p];
    sr = sonilo_sr(mem);
    ph->phs = 0;
    ph->onedsr = 1.0 / sr;
}

void dsp_phasor_compute(uint32_t *mem, uint32_t p)
{
    int n;
    int blksz;
    struct dsp_phasor *ph;

    ph = (struct dsp_phasor *)&mem[p];

    blksz = sonilo_blksz(mem);

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
