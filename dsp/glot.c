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
    float Rd;

    float alpha;
    float E0;
    float epsilon;
    float shift;
    float Te;
    float omega;

    float phs;
};

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
    E0 = -1.0 / (s * exp(alpha*Te));

    glot->alpha = alpha;
    glot->E0 = E0;
    glot->epsilon = epsilon;
    glot->shift = shift;
    glot->Te = Te;
    glot->omega = omega;
}

float sk_glot_tick(sk_glot *glot, float t)
{
    float out;

    out = 0;

    if (glot->phs < 0 || t < glot->phs) {
        setup_waveform(glot);
    }

    if (t > glot->Te) {
        out =
            (-exp(-glot->epsilon * (t-glot->Te)) + glot->shift)
            / (1 - glot->shift);
    } else {
        out = glot->E0 * exp(glot->alpha * t) * sin(glot->omega * t);
    }

    glot->phs = t;

    return out;
}

void sk_glot_shape(sk_glot *glot, float shape)
{
    glot->Rd = 3 * (1 - shape);
}

void sk_glot_init(sk_glot *glot, float sr)
{
    glot->phs = -1;
    sk_glot_shape(glot, 0.5);

    setup_waveform(glot);
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
