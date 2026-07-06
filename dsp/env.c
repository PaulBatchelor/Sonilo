#include <stdint.h>
#include <math.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

#define EPS 5e-8

typedef struct sk_env sk_env;

struct sk_env {
    uint32_t sr;
    float timer;
    float inc;
    float atk_env;
    float rel_env;
    uint32_t mode;
    float prev;
    float atk;
    float patk;
    float rel;
    float prel;
    float hold;
    float phold;
};


enum {
    MODE_ZERO,
    MODE_ATTACK,
    MODE_HOLD,
    MODE_RELEASE
};

static void sk_env_attack(sk_env *env, float atk)
{
    env->atk = atk;
}

static void sk_env_release(sk_env *env, float rel)
{
    env->rel = rel;
}

static void sk_env_hold(sk_env *env, float hold)
{
  env->hold = hold;
}


static void sk_env_init(sk_env *env, int sr)
{
    env->sr = sr;
    env->timer = 0;
    env->inc = 0;
    env->atk_env = 0;
    env->rel_env = 0;
    env->mode = MODE_ZERO;
    env->prev = 0;
    sk_env_attack(env, 0.1);
    env->patk = -1;
    sk_env_release(env, 0.1);
    env->prel = -1;
    sk_env_hold(env, 0.1);
    env->phold = -1;
}

static float sk_env_tick(sk_env *env, float trig)
{
    float out;
    out = 0;

    if (trig != 0) {
        env->mode = MODE_ATTACK;

        if (env->patk != env->atk) {
            env->patk = env->atk;
            env->atk_env = exp (-1.0 / (env->atk * env->sr));
        }
    }

    switch (env->mode) {
        case MODE_ZERO:
            break;
        case MODE_ATTACK:
            out = env->atk_env * env->prev + (1.0 - env->atk_env);

            if ((out - env->prev) <= EPS) {
                env->mode = MODE_HOLD;
                env->timer = 0;

                if (env->phold != env->hold) {
                    if (env->hold <= 0) {
                        env->inc = 1.0;
                    } else {
                        env->phold = env->hold;
                        env->inc = 1.0 / (env->hold * env->sr);
                    }
                }
            }

            env->prev = out;
            break;
        case MODE_HOLD:
            out = env->prev;
            env->timer += env->inc;

            if (env->timer >= 1.0)
            {
                env->mode = MODE_RELEASE;

                if (env->prel != env->rel) {
                    env->prel = env->rel;
                    env->rel_env = exp (-1 / (env->rel * env->sr));
                }
            }
            break;
        case MODE_RELEASE:
            out = env->rel_env * env->prev;
            env->prev = out;

            if (out <= EPS) {
                env->mode = MODE_ZERO;
            }
            break;
        default:
            break;
    }
    return out;
}
static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    sk_env *env;
    int i;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        5, sizeof(sk_env) >> 2,
        &ugen);
    if (rc) return 2;

    /* ports */
    for (i = 3; i >= 0; i--) {
        rc = ugen_iport(mem, ctx, ugen, i);
        if (rc) return 3;
    }
    rc = ugen_oport(mem, ctx, ugen, 4);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    env = (sk_env *)ugen_state(mem, ugen);
    if (env == NULL) return 5;

    sk_env_init(env, sonilo_srate(mem));

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;
    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    sk_env *env;
    uint32_t *ports;
    sonilo_port p_trig, p_out, p_atk, p_hold, p_rel;
    int n;

    env = (sk_env *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    p_trig = sonilo_port_from_word(mem, ports[0]);
    p_atk = sonilo_port_from_word(mem, ports[1]);
    p_hold = sonilo_port_from_word(mem, ports[2]);
    p_rel = sonilo_port_from_word(mem, ports[3]);
    p_out = sonilo_port_from_word(mem, ports[4]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float trig, atk, hold, rel, out;
        trig = sonilo_port_read(&p_trig, n);
        atk = sonilo_port_read(&p_atk, n);
        hold = sonilo_port_read(&p_hold, n);
        rel = sonilo_port_read(&p_rel, n);
        sk_env_attack(env, atk);
        sk_env_hold(env, hold);
        sk_env_release(env, rel);
        out = sk_env_tick(env, trig);
        sonilo_port_write(&p_out, n, out);
    }

    return 0;
}

int ugen_env(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("ENV");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
