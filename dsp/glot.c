#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

typedef struct sk_glot sk_glot;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define LCG_MAX 2147483648

static void setup_waveform(sk_glot *glot);

#define ROOT2 1.4142135623730950488

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct sk_glot_butfilt {
    float freq, lfreq;
    float a[7];
    float pidsr, tpidsr;
};

struct sk_glot {
    float freq;
    float Rd;
    float waveform_length;
    float time_in_waveform;

    float alpha;
    float E0;
    float epsilon;
    float shift;
    float delta;
    float Te;
    float omega;

    float T;
    float phs;

    /* 3 words padding for 16 words */
    uint32_t padding[3];
#if 0
    unsigned long rng;
    /* pulsed noise */
    float hanning[SK_GLOT_ENV_SIZE];

    /* Lu suggests that scale can be fixed between
     * 40-80% of glottal wave (pg 93) */
    float env_size; /* A_n */

    /* lag is recommend to be between 0 and 15% of period */
    float lag; /* L */
    float t_env_start;
    float env_pos;
    float env_delta;
    float noise_floor; /* B_n */
    float aspiration;

    struct sk_glot_butfilt asp_hpfilt;
    struct sk_glot_butfilt asp_lpfilt;
#endif
};

/* NOTE: aspiration */
#if 0
static void butfilt_init(struct sk_glot_butfilt *but, int sr)
{
    int i;
    but->freq = 1000.0;
    but->lfreq = -1;
    but->pidsr = M_PI / (float)sr;
    for (i = 0; i < 7; i++) but->a[i] = 0.0;
}

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

static float butlp_tick(struct sk_glot_butfilt *but, float in)
{
    if (but->freq != but->lfreq) {
        float *a, c;
        a = but->a;
        but->lfreq = but->freq;
        /* derive C constant used in BLT */
        c = 1.0 / tan((float)(but->pidsr * but->lfreq));

        /* perform BLT, store components */
        a[0] = 1.0 / (1.0 + c*ROOT2 + c*c);
        a[1] = 2*a[0];
        a[2] = a[0];
        a[3] = 2.0 * (1.0 - c*c) * a[0];
        a[4] = (1.0 - c*ROOT2 + c*c) * a[0];
    }

    return filter(in, but->a);
}
#endif

/* NOTE: aspiration */
#if 0
static float buthp_tick(struct sk_glot_butfilt *but, float in)
{
    if (but->freq != but->lfreq) {
        float *a, c;
        a = but->a;
        but->lfreq = but->freq;
        /* derive C constant used in BLT */
        c = tan((float)(but->pidsr * but->freq));

        /* perform BLT, store components */
        a[0] = 1.0 / (1.0 + c*ROOT2 + c*c);
        a[1] = -2*a[0];
        a[2] = a[0];
        a[3] = 2.0 * (c*c - 1.0) * a[0];
        a[4] = (1.0 - c*ROOT2 + c*c) * a[0];
    }

    return filter(in, but->a);
}
#endif

#if 0
static void hanning_table(float *tab, int m)
{
    int n;
    float om;

    /* omega m */
    om = (2.0 * M_PI / m);

    for (n = 0; n < m; n++) {
        float out;

        out = sin((om*0.5)*n);
        out *= out;

        tab[n] = out;
    }
}
#endif

void sk_glot_freq(sk_glot *glot, float freq)
{
    glot->freq = freq;
}

static void setup_waveform(sk_glot *glot)
{
    float Rd;
    float Ra;
    float Rk;
    float Rg;

    float Ta;
    float Tp;
    float Te;

    float epsilon;
    float shift;
    float delta;
    float rhs_integral;

    float lower_integral;
    float upper_integral;

    float omega;
    float s;
    float y;
    float z;

    float alpha;
    float E0;

    glot->waveform_length = 1.0 / glot->freq;
    Rd = glot->Rd;
    if (Rd < 0.5) Rd = 0.5;
    if (Rd > 2.7) Rd = 2.7;
    Ra = -0.01 + 0.048*Rd;
    Rk = 0.224 + 0.118*Rd;
    Rg = (Rk/4)*(0.5 + 1.2*Rk)/(0.11*Rd-Ra*(0.5+1.2*Rk));
    Ta = Ra;
    Tp = (float)1.0 / (2*Rg);
    Te = Tp + Tp*Rk;
    epsilon = (float)1.0 / Ta;
    shift = exp(-epsilon * (1 - Te));
    delta = 1 - shift;
    rhs_integral = (float)(1.0/epsilon) * (shift-1) + (1-Te)*shift;
    rhs_integral = rhs_integral / delta;
    lower_integral = - (Te - Tp) / 2 + rhs_integral;
    upper_integral = -lower_integral;

    omega = M_PI / Tp;
    s = sin(omega * Te);

    y = -M_PI * s * upper_integral / (Tp*2);
    z = log(y);
    alpha = z / (Tp/2 - Te);
    E0 = -1 / (s * exp(alpha*Te));

    glot->alpha = alpha;
    glot->E0 = E0;
    glot->epsilon = epsilon;
    glot->shift = shift;
    glot->delta = delta;
    glot->Te = Te;
    glot->omega = omega;
#if 0
    /* calculate envelope start from lag
     * and glottal closure (Te) (note that Te is normalized)
     * make sure to factor in delay to make it centered
     */

    glot->t_env_start =
        (glot->Te + glot->lag) - 0.5*glot->env_size;


    /* reset envelope position */
    glot->env_pos = 0;

    /* how much to increment the envelope by */
    /* 1/sz is a ramp sz samples long, scaled by env_size */
    glot->env_delta = 1.0 / (SK_GLOT_ENV_SIZE * glot->env_size);
#endif
}

#if 0
void sk_glot_srand(sk_glot *glot, unsigned long s)
{
    glot->rng = s;
}
#endif

/* NOTE: aspiration */
#if 0
static unsigned long glot_lcg(sk_glot *glot)
{
    glot->rng = (1103515245 * glot->rng + 12345) % LCG_MAX;
    return glot->rng;
}
#endif

float sk_glot_tick(sk_glot *glot, float t)
{
    float out;

    out = 0;

    if (glot->phs < 0 || t < glot->phs) {
        setup_waveform(glot);
    }

    if (t > glot->Te) {
        out = (-exp(-glot->epsilon * (t-glot->Te)) + glot->shift) / glot->delta;
    } else {
        out = glot->E0 * exp(glot->alpha * t) * sin(glot->omega * t);
    }

    glot->phs = t;

#if 0
    /* TODO: break out aspiration noise into separate component */

    /* generate gaussian noise, essentially white noise.
     */

    noise = (glot_lcg(glot) / (float)LCG_MAX);

    /* shave off some high end */
    noise = butlp_tick(&glot->asp_lpfilt, noise);

    /* noise filtering... Lu says 4kHz highpass cutoff */
    /* noise = sk_buthp_tick(&glot->asp_hpfilt, noise); */
    noise = buthp_tick(&glot->asp_hpfilt, noise);


    /* amplitude modulation
     * This is a scaled pitch-synchronous Hanning window,
     * centered on the glottal closure instants and desired lag.
     *
     * Per Lu's thesis, only one pulse per period is considered
     * as a good first approximation. The timing position
     * for the glottal closure instance is Te.
     *
     * Lag is specified as percentage relative to glottal
     * period length.
     *
     * The envelope "sits on top of the noise floor". That
     * is to say, it doesn't close all the way, letting
     * some noise out at the lower level. This is also
     * a paraglter.
     *
     */

    env = 0;

    /* check and see if it is time to use envelope */

    if (t > glot->t_env_start && glot->env_pos <= 1.0) {
        /* table-lookup with linear interpolation */
        float fpos;
        int ipos;
        fpos = glot->env_pos * (SK_GLOT_ENV_SIZE - 2);
        ipos = (int)fpos;
        fpos -= ipos;
        env =
            (1 - fpos) * glot->hanning[ipos] +
            fpos * glot->hanning[ipos + 1];

        glot->env_pos += glot->env_delta;
    }

    /* noise floor / pulsed noise, this is just crossfading */

    nf = glot->noise_floor;
    env = (nf + (1 - nf)*env) * noise;

    /* attenuate by aspiration level */

    env *= glot->aspiration;

    out += env;
#endif
    return out;
}

void sk_glot_shape(sk_glot *glot, float shape)
{
    glot->Rd = 3 * (1 - shape);
}

/* NOTE: aspiration */
#if 0
void sk_glot_aspiration(sk_glot *glot, float aspiration)
{
    glot->aspiration = aspiration;
}
#endif

/* NOTE: aspiration */
#if 0
void sk_glot_noise_floor(sk_glot *glot, float nf)
{
    glot->noise_floor = nf;
}
#endif

void sk_glot_init(sk_glot *glot, float sr)
{
    glot->freq = 140; /* 140Hz frequency by default */
    glot->T = 1.0/sr; /* big T */
    glot->time_in_waveform = 0;
    glot->phs = -1;
    sk_glot_shape(glot, 0.5);

    setup_waveform(glot);
#if 0
    glot->lag = 0.07; /* 7% of period (max 15) */
    glot->noise_floor = 0.005;
    glot->env_size = 0.6; /* 40-80 percent */
    glot->aspiration = 0.3;
    sk_glot_srand(glot, 0);
    hanning_table(glot->hanning, SK_GLOT_ENV_SIZE);
    butfilt_init(&glot->asp_hpfilt, sr);
    glot->asp_hpfilt.freq = 4500;

    /* I decided to add this to make noise less grating */
    butfilt_init(&glot->asp_lpfilt, sr);
    glot->asp_lpfilt.freq = 6000;
#endif
}

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    sk_glot *glt;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        3, sizeof(sk_glot) >> 2,
        &ugen);
    if (rc) return 2;

    /* ports */
    rc = ugen_iport(mem, ctx, ugen, 1);
    if (rc) return 3;
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 2);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    glt = (sk_glot *)ugen_state(mem, ugen);
    if (glt == NULL) return 5;
    sk_glot_init(glt, sonilo_srate(mem));

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    sk_glot *glt;
    uint32_t *ports;
    sonilo_port p_frq, p_shp, p_out;
    int n;

    glt = (sk_glot *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    p_frq = sonilo_port_from_word(mem, ports[0]);
    p_shp = sonilo_port_from_word(mem, ports[1]);
    p_out = sonilo_port_from_word(mem, ports[2]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float phs, shp, o;
        phs = sonilo_port_read(&p_frq, n);
        shp = sonilo_port_read(&p_shp, n);
        sk_glot_shape(glt, shp);
        o = sk_glot_tick(glt, phs);
        sonilo_port_write(&p_out, n, o);
    }

    return 0;
}

int ugen_glot(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("GLT");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
