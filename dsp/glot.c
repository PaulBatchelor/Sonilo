#include <math.h>
#include <stdlib.h>

typedef struct sk_glot sk_glot;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define LCG_MAX 2147483648

static void setup_waveform(sk_glot *glot);
static unsigned long glot_lcg(sk_glot *glot);

#define ROOT2 1.4142135623730950488

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SK_GLOT_ENV_SIZE 512
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
    /* sk_butterworth asp_lpfilt; */
};

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
}

void sk_glot_srand(sk_glot *glot, unsigned long s)
{
    glot->rng = s;
}

static unsigned long glot_lcg(sk_glot *glot)
{
    glot->rng = (1103515245 * glot->rng + 12345) % LCG_MAX;
    return glot->rng;
}

float sk_glot_tick(sk_glot *glot)
{
    float out;
    float noise;
    float t;
    float env;
    float nf;

    out = 0;

    /* TODO: refactor and replace with phasor */
    glot->time_in_waveform += glot->T;
    if (glot->time_in_waveform > glot->waveform_length) {
        glot->time_in_waveform -= glot->waveform_length;
        setup_waveform(glot);
    }

    t = (glot->time_in_waveform / glot->waveform_length);

    if (t > glot->Te) {
        out = (-exp(-glot->epsilon * (t-glot->Te)) + glot->shift) / glot->delta;
    } else {
        out = glot->E0 * exp(glot->alpha * t) * sin(glot->omega * t);
    }

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
     * a parameter.
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
    return out;
}

void sk_glot_shape(sk_glot *glot, float shape)
{
    glot->Rd = 3 * (1 - shape);
}

void sk_glot_aspiration(sk_glot *glot, float aspiration)
{
    glot->aspiration = aspiration;
}

void sk_glot_noise_floor(sk_glot *glot, float nf)
{
    glot->noise_floor = nf;
}

void sk_glot_init(sk_glot *glot, float sr)
{
    glot->freq = 140; /* 140Hz frequency by default */
    glot->T = 1.0/sr; /* big T */
    glot->time_in_waveform = 0;
    glot->lag = 0.07; /* 7% of period (max 15) */
    glot->noise_floor = 0.005;
    glot->env_size = 0.6; /* 40-80 percent */
    glot->aspiration = 0.3;
    sk_glot_shape(glot, 0.5);
    setup_waveform(glot);
    sk_glot_srand(glot, 0);
    hanning_table(glot->hanning, SK_GLOT_ENV_SIZE);
    /*
    sk_butterworth_init(&glot->asp_hpfilt, sr);
    sk_butterworth_freq(&glot->asp_hpfilt, 4500);
    */
    butfilt_init(&glot->asp_hpfilt, sr);
    glot->asp_hpfilt.freq = 4500;

    /* I decided to add this to make noise less grating */
    butfilt_init(&glot->asp_lpfilt, sr);
    glot->asp_lpfilt.freq = 6000;
}
