#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

typedef struct sk_bigverb sk_bigverb;
typedef struct sk_bigverb_delay sk_bigverb_delay;
sk_bigverb * sk_bigverb_new(int sr);

static void sk_bigverb_size(sk_bigverb *bv, float size);
static void sk_bigverb_cutoff(sk_bigverb *bv, float cutoff);
static void sk_bigverb_tick(sk_bigverb *bv,
        float inL, float inR,
        float *outL, float *outR);

struct sk_bigverb_delay {
    float *buf;
    size_t sz;
    int wpos;
    int irpos;
    int frpos;
    int rng;
    int inc;
    int counter;
    int maxcount;
    float dels;
    float drift;
    float y;
};

struct sk_bigverb {
    int sr;
    float size;
    float cutoff;
    float pcutoff;
    float filt;
    float *buf;
    sk_bigverb_delay delay[8];
};

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
struct bigverb_paramset {
    int delay; /* in samples, 44.1 kHz */
    int drift; /* 1/10 milliseconds */
    int randfreq; /* Hertz * 1000 */
    int seed;
};

typedef struct dsp_bigverb {
    sk_bigverb *bv;
    float *buf;
} dsp_bigverb;

static const struct bigverb_paramset params[8] = {
    {0x09a9, 0x0a, 0xc1c, 0x07ae},
    {0x0acf, 0x0b, 0xdac, 0x7333},
    {0x0c91, 0x11, 0x456, 0x5999},
    {0x0de5, 0x06, 0xf85, 0x2666},
    {0x0f43, 0x0a, 0x925, 0x50a3},
    {0x101f, 0x0b, 0x769, 0x5999},
    {0x085f, 0x11, 0x37b, 0x7333},
    {0x078d, 0x06, 0xc95, 0x3851}
};
#define FRACSCALE 0x10000000
#define FRACMASK 0xFFFFFFF
#define FRACNBITS 28
static int get_delay_size(const struct bigverb_paramset *p, int sr);
static void delay_init(sk_bigverb_delay *d,
        const struct bigverb_paramset *p,
        float *buf,
        size_t sz,
        int sr);
static float delay_compute(sk_bigverb_delay *d,
        float in,
        float fdbk,
        float filt,
        int sr);
static void generate_next_line(sk_bigverb_delay *d, int sr);

sk_bigverb * sk_bigverb_new(int sr)
{
    sk_bigverb *bv;

    bv = calloc(1, sizeof(sk_bigverb));

    bv->sr = sr;
    sk_bigverb_size(bv, 0.93);
    sk_bigverb_cutoff(bv, 10000.0);
    bv->pcutoff = -1;
    bv->filt = 1.0;
    bv->buf = NULL;
    {
        unsigned long total_size;
        int i;
        float *buf;

        total_size = 0;
        buf = NULL;
        for (i = 0; i < 8; i++) {
            total_size += get_delay_size(&params[i], sr);
        }
        buf = calloc(1, sizeof(float) * total_size);
        bv->buf = buf;
        printf("total size: %lu samples\n", total_size);
        {
            unsigned long bufpos;
            bufpos = 0;
            for (i = 0; i < 8; i++) {
                unsigned int sz;
                sz = get_delay_size(&params[i], sr);

                delay_init(&bv->delay[i], &params[i],
                        &buf[bufpos], sz, sr);
                bufpos += sz;
            }
        }
    }

    return bv;
}

static void bigverb_init(sk_bigverb *bv, int sr, float *buf)
{
    if (sr > 44100) {
        /* nope */
        return;
    }
    bv->sr = sr;
    sk_bigverb_size(bv, 0.93);
    sk_bigverb_cutoff(bv, 10000.0);
    bv->pcutoff = -1;
    bv->filt = 1.0;
    bv->buf = buf;
    {
        int i;
        {
            unsigned long bufpos;
            bufpos = 0;
            for (i = 0; i < 8; i++) {
                unsigned int sz;
                sz = get_delay_size(&params[i], sr);

                delay_init(&bv->delay[i], &params[i],
                        &buf[bufpos], sz, sr);
                bufpos += sz;
            }
        }
    }
}

static void sk_bigverb_size(sk_bigverb *bv, float size)
{
    bv->size = size;
}

static void sk_bigverb_cutoff(sk_bigverb *bv, float cutoff)
{
    bv->cutoff = cutoff;
}

static void sk_bigverb_tick(sk_bigverb *bv,
        float inL, float inR,
        float *outL, float *outR)
{
    float lsum, rsum;

    lsum = 0;
    rsum = 0;

    if (bv->pcutoff != bv->cutoff) {
        bv->pcutoff = bv->cutoff;
        bv->filt = 2.0 - cos(bv->pcutoff * 2 * M_PI / bv->sr);
        bv->filt = bv->filt - sqrt(bv->filt * bv->filt - 1.0);
    }
    {
        int i;
        float jp;

        jp = 0;

        for (i = 0; i < 8; i++) {
            jp += bv->delay[i].y;
        }

        jp *= 0.25;

        inL = jp + inL;
        inR = jp + inR;
    }
    {
        int i;
        for (i = 0; i < 8; i++) {
            if (i & 1) {
                rsum += delay_compute(&bv->delay[i],
                        inR,
                        bv->size,
                        bv->filt,
                        bv->sr);
            } else {
                lsum += delay_compute(&bv->delay[i],
                        inL,
                        bv->size,
                        bv->filt,
                        bv->sr);
            }
        }
    }
    rsum *= 0.35f;
    lsum *= 0.35f;

    *outL = lsum;
    *outR = rsum;
}

static int get_delay_size(const struct bigverb_paramset *p, int sr)
{
    float sz;
    sz = (float)p->delay/44100 + (p->drift * 0.0001) * 1.125;
    return floor(16 + sz*sr);
}

static void delay_init(sk_bigverb_delay *d,
        const struct bigverb_paramset *p,
        float *buf,
        size_t sz,
        int sr)
{
    float readpos;
    d->buf = buf;
    d->sz = sz;
    d->wpos = 0;
    d->rng = p->seed;
    readpos = ((float)p->delay / 44100);
    readpos += d->rng * (p->drift * 0.0001) / 32768.0;
    readpos = sz - (readpos * sr);
    d->irpos = floor(readpos);
    d->frpos = floor((readpos - d->irpos) * FRACSCALE);
    d->inc = 0;
    d->counter = 0;
    d->maxcount = floor((sr / ((float)p->randfreq * 0.001)));
    d->dels = p->delay / 44100.0;
    d->drift = p->drift;
    generate_next_line(d, sr);
    d->y = 0.0;
}

static float delay_compute(sk_bigverb_delay *del,
        float in,
        float fdbk,
        float filt,
        int sr)
{
    float out;
    float frac_norm;
    float a, b, c, d;
    float s[4];
    out = 0;
    del->buf[del->wpos] = in - del->y;
    del->wpos++;
    if (del->wpos >= del->sz) del->wpos -= del->sz;
    if (del->frpos >= FRACSCALE) {
        del->irpos += del->frpos >> FRACNBITS;
        del->frpos &= FRACMASK;
    }
    if (del->irpos >= del->sz) del->irpos -= del->sz;
    frac_norm = del->frpos / (float)FRACSCALE;
    {
        float tmp[2];
        d = ((frac_norm * frac_norm) - 1) / 6.0;
        tmp[0] = ((frac_norm + 1.0) * 0.5);
        tmp[1] = 3.0 * d;
        a = tmp[0] - 1.0 - d;
        c = tmp[0] - tmp[1];
        b = tmp[1] - frac_norm;
    }
    {
        int n;
        float *x;
        n = del->irpos;
        x = del->buf;

        if (n > 0 && n < (del->sz - 2)) {
            s[0] = x[n - 1];
            s[1] = x[n];
            s[2] = x[n + 1];
            s[3] = x[n + 2];
        } else {
            int k;
            n--;
            if (n < 0) n += del->sz;
            s[0] = x[n];
            for (k = 0; k < 3; k++) {
                n++;
                if (n >= del->sz) n -= del->sz;
                s[k + 1] = x[n];
            }
        }
    }
    out = (a*s[0] + b*s[1] + c*s[2] + d*s[3]) * frac_norm + s[1];
    del->frpos += del->inc;
    out *= fdbk;
    out += (del->y - out) * filt;
    del->y = out;
    del->counter--;
    if (del->counter <= 0) {
        generate_next_line(del, sr);
    }
    return out;
}

static void generate_next_line(sk_bigverb_delay *d, int sr)
{
    float curdel;
    float nxtdel;
    float inc;
    if (d->rng < 0) d->rng += 0x10000;
    /* 5^6 = 15625 */
    d->rng = (1 + d->rng * 0x3d09);
    d->rng &= 0xFFFF;
    if (d->rng >= 0x8000) d->rng -= 0x10000;
    d->counter = d->maxcount;
    curdel = d->wpos -
        (d->irpos + (d->frpos/(float)FRACSCALE));
    while (curdel < 0) curdel += d->sz;
    curdel /= sr;
    nxtdel = (d->rng * (d->drift * 0.0001) / 32768.0) + d->dels;
    inc = ((curdel - nxtdel) / (float)d->counter)*sr;
    inc += 1;
    d->inc = floor(inc * FRACSCALE);
}

size_t sk_bigverb_sizeof(void)
{
    return sizeof(sk_bigverb);
}

static uint16_t allot(uint32_t *mem, uint16_t ctx, uint16_t sz)
{
    uint16_t f;

    f = mem[ctx + SLOT_UNIVERSE_FREE] & 0xFFFF;
    mem[ctx + SLOT_UNIVERSE_FREE] += sz;

    return f;
}

static uint32_t *resolve(uint32_t *mem, uint16_t mb)
{
    /* 1 mb = 256 (2^8) words */
    return &mem[(mb - 1) << 8];
}

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    dsp_bigverb *bv;
    uint16_t bvsz;
    uint32_t *tmp;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* TODO: make sure this size is word aligned */
    bvsz = (uint16_t)sizeof(dsp_bigverb);
    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        6, bvsz >> 2,
        &ugen);
    if (rc) return 2;

    /* ports */

    /* size */
    rc = ugen_iport(mem, ctx, ugen, 3);
    if (rc) return 3;

    /* cutoff */
    rc = ugen_iport(mem, ctx, ugen, 2);
    if (rc) return 3;

    /* inR */
    rc = ugen_iport(mem, ctx, ugen, 1);
    if (rc) return 3;

    /* inL */
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;

    /* outL/outR */
    rc = ugen_oport(mem, ctx, ugen, 4);
    if (rc) return 4;
    rc = ugen_oport(mem, ctx, ugen, 5);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    bv = (dsp_bigverb *)ugen_state(mem, ugen);
    if (bv == NULL) return 5;

    /* 24684 samples @ 44.1kHz = ~97 megablocks */
    tmp = resolve(mem, allot(mem, ctx, 97));
    bv->buf = (float *)tmp;
    /* sk_bigverb needs about 120 words, so one block will cover it */
    tmp = resolve(mem, allot(mem, ctx, 1));
    bv->bv = (sk_bigverb *)tmp;
    bigverb_init(bv->bv, sonilo_srate(mem), bv->buf);

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    dsp_bigverb *bv;
    uint32_t *ports;
    sonilo_port in[2], out[2], psize, pcutoff;
    int n;

    bv = (dsp_bigverb *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    in[0] = sonilo_port_from_word(mem, ports[0]);
    in[1] = sonilo_port_from_word(mem, ports[1]);
    psize = sonilo_port_from_word(mem, ports[2]);
    pcutoff = sonilo_port_from_word(mem, ports[3]);

    out[0] = sonilo_port_from_word(mem, ports[4]);
    out[1] = sonilo_port_from_word(mem, ports[5]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float i[2], size, cutoff, o[2];
        i[0] = sonilo_port_read(&in[0], n);
        i[1] = sonilo_port_read(&in[1], n);
        size = sonilo_port_read(&psize, n);
        cutoff = sonilo_port_read(&pcutoff, n);
        sk_bigverb_size(bv->bv, size);
        sk_bigverb_cutoff(bv->bv, cutoff);
        sk_bigverb_tick(bv->bv, i[0], i[1], &o[0], &o[1]);
        sonilo_port_write(&out[0], n, o[0]);
        sonilo_port_write(&out[1], n, o[1]);
    }
    return 0;
}

int ugen_bigverb(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("BVR");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;
    return 0;
}
