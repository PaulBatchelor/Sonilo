#include <stdlib.h>
#include <stdio.h>
#include "sonilo.h"

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

int render(sonilo_ctx *ctx, uint16_t *lst, uint16_t sink)
{
    int rc;
    sonilo *s;
    float *out;
    int i;
    FILE *fp;
    uint32_t *mem;
    uint16_t sz;

    fp = fopen("seq.raw", "wb");

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

int main(int argc, char *argv[])
{
    sonilo *s;
    sonilo_ctx ctx;
    int rc;
    uint16_t ugen_list[64];
    int i;
    uint16_t sink;

    for (i = 0; i < 64; i++) ugen_list[i] = 0;

    s = malloc(sonilo_sizeof());
    rc = sonilo_init(s);
    if (rc) goto clean;
    rc = sonilo_ctx_init(&ctx, s);
    if (rc) goto clean;

    /* TODO: clock */
    /* TODO: sequencer driven by clock */
    /* TODO: add base pitch */
    /* TODO: smoother on pitch signal */
    /* TODO: midi to frequency */
    /* TODO: saw, controlled via freq signal */
    /* TEMP: control saw with constant freq */
    rc = sonilo_constant(&ctx, 200);
    if (rc) goto clean;
    rc = ugen(&ctx, "SAW", ugen_list);
    if (rc) goto clean;

    /* TODO: filter saw with LPF */
    /* output */
    rc = ugen(&ctx, "SNK", ugen_list);
    if (rc) goto clean;
    sink = ugen_list[ugen_list[0]];
    /* render  */
    rc = render(&ctx, ugen_list, sink);

    if (rc) goto clean;

    clean:
    if (rc) {
        fprintf(stderr, "sonilo error: %d\n", rc);
    }
    sonilo_ctx_destroy(&ctx);
    free(s);

    return 0;
}
