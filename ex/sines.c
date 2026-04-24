#include <stdlib.h>
#include <stdio.h>
#include "sonilo.h"

int render(sonilo_ctx *ctx, uint16_t *lst, uint16_t sink)
{
    int rc;
    sonilo *s;
    float *out;
    int i;
    FILE *fp;
    uint32_t *mem;
    uint16_t sz;

    fp = fopen("sines.raw", "wb");

    s = ctx->s;
    sz = lst[0];
    mem = sonilo_mem(s);
    rc = sonilo_ugen_block(mem, sink, 0, &out);
    if (rc) return 1;

    for (i = 0; i < 3445*2; i++) {
        int k;
        for (k = 1; k <= sz; k++) {
            int rc;
            uint32_t rw;
            rc = 0;
            rw = 1;
            /* set up function args: ugen address | callback */
            rw = (lst[k] << 16) | (mem[lst[k] + 1] & 0xFFFF);
            rc = sonilo_set(s, rw);
            if (rc) break;
            rc = sonilo_call_direct(s);
            if (rc) break;

            /* ugens should return 0 */
            rc = sonilo_get(s, &rw);
            if (rc || rw) break;
        }
        if (rc) break;
        fwrite(out, sizeof(float), 64, fp);
    }

    fclose(fp);

    return 0;
}

int ugen(sonilo_ctx *ctx, const char *sym, uint16_t *lst)
{
    int rc;
    uint32_t p;

    rc = sonilo_symbol(ctx, sym);
    if (rc) return 1;

    rc = sonilo_ugen_create(ctx);
    if (rc) return 1;

    p = 0;
    rc = sonilo_pop(ctx, &p);
    if (rc) return 2;

    lst[0]++;
    lst[lst[0]] = p & 0xFFFF;

    return 0;
}

/* map [-1, 1] -> [a, b] */
int biscale(sonilo_ctx *ctx, uint16_t *lst, float a, float b)
{
    int rc;

    /* scale LFO from [-1, 1] to [0, 1] */
    rc = sonilo_constant(ctx, 1);
    if (rc) return 1;
    rc = ugen(ctx, "ADD", lst);
    if (rc) return 2;

    /* add multiplier (b - a) */
    rc = sonilo_constant(ctx, b - a);
    if (rc) return 3;
    rc = ugen(ctx, "MUL", lst);
    if (rc) return 4;

    /* add bias to start at a */
    rc = sonilo_constant(ctx, a);
    if (rc) return 5;
    rc = ugen(ctx, "ADD", lst);
    if (rc) return 6;

    return 0;
}

int main(int argc, char *argv[])
{
    sonilo *s;
    sonilo_ctx ctx;
    int rc;
    uint16_t ugen_list[64];
    int i;
    uint16_t sink;
    uint32_t lfo;

    for (i = 0; i < 64; i++) ugen_list[i] = 0;

    s = malloc(sonilo_sizeof());
    rc = sonilo_init(s);
    if (rc) goto clean;
    rc = sonilo_ctx_init(&ctx, s);
    if (rc) goto clean;

    /* create LFO oscillator */
    rc = sonilo_constant(&ctx, 0.2);
    if (rc) goto clean;
    rc = ugen(&ctx, "SIN", ugen_list);
    if (rc) goto clean;

    /* hold */
    rc = sonilo_hold(&ctx, &lfo);
    if (rc) goto clean;

    /* 440hz sine oscillator */
    rc = sonilo_constant(&ctx, 440);
    if (rc) goto clean;

    /* add LFO bias */
    rc = sonilo_ppush(&ctx, lfo);
    if (rc) goto clean;
    rc = biscale(&ctx, ugen_list, 20, 200);
    if (rc) goto clean;
    rc = ugen(&ctx, "ADD", ugen_list);
    if (rc) goto clean;
    rc = ugen(&ctx, "SIN", ugen_list);
    if (rc) goto clean;

    /* 330hz sine oscillator */
    rc = sonilo_constant(&ctx, 350);
    if (rc) goto clean;

    /* modulate with vibrato, whose rate is modulated with LFO */
    rc = sonilo_ppush(&ctx, lfo);
    if (rc) goto clean;
    rc = biscale(&ctx, ugen_list, 6, 10);
    if (rc) goto clean;


    if (rc) goto clean;
    rc = ugen(&ctx, "SIN", ugen_list);
    if (rc) goto clean;

    /* modulate the vibrato frequen */
    rc = sonilo_constant(&ctx, 50);
    rc = ugen(&ctx, "MUL", ugen_list);
    if (rc) goto clean;
    rc = ugen(&ctx, "ADD", ugen_list);
    if (rc) goto clean;

    /* unhold LFO signal */
    rc = sonilo_unhold(&ctx, lfo);
    if (rc) goto clean;

    rc = ugen(&ctx, "SIN", ugen_list);
    if (rc) goto clean;

    /* ADD */
    rc = ugen(&ctx, "ADD", ugen_list);
    if (rc) goto clean;

    /* Gain reduction (MUL) */
    rc = sonilo_constant(&ctx, 0.3);
    if (rc) goto clean;
    rc = ugen(&ctx, "MUL", ugen_list);
    if (rc) goto clean;

    /* sink source: the output signal that will be written to disk */
    rc = ugen(&ctx, "SNK", ugen_list);
    if (rc) goto clean;
    sink = ugen_list[ugen_list[0]];

    rc = render(&ctx, ugen_list, sink);

    if (rc) goto clean;

    clean:
    sonilo_ctx_destroy(&ctx);
    free(s);
    return 0;
}
