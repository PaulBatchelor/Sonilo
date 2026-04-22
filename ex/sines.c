#include <stdlib.h>
#include <stdio.h>
#include "sonilo.h"

int render(sonilo_ctx *ctx, uint16_t *lst, int sz, uint16_t sink)
{
    int rc;
    sonilo *s;
    float *out;
    int i;
    FILE *fp;
    uint32_t *mem;

    fp = fopen("sines.raw", "wb");

    s = ctx->s;
    mem = sonilo_mem(s);
    rc = sonilo_ugen_block(mem, sink, 0, &out);
    if (rc) return 1;

    for (i = 0; i < 3445; i++) {
        int k;
        for (k = 0; k < sz; k++) {
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

int ugen(sonilo_ctx *ctx, const char *sym, uint16_t *lst, int *n)
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

    lst[*n] = p & 0xFFFF;
    *n = (*n) + 1;

    return 0;
}

int main(int argc, char *argv[])
{
    sonilo *s;
    sonilo_ctx ctx;
    int rc;
    uint16_t ugen_list[4];
    int nugens;
    int i;
    uint16_t sink;

    for (i = 0; i < 4; i++) ugen_list[i] = 0;
    nugens = 0;

    s = malloc(sonilo_sizeof());
    rc = sonilo_init(s);
    if (rc) goto clean;
    rc = sonilo_ctx_init(&ctx, s);
    if (rc) goto clean;

    /* sine oscillator */
    rc = sonilo_constant(&ctx, 440);
    if (rc) goto clean;
    rc = ugen(&ctx, "SIN", ugen_list, &nugens);
    if (rc) goto clean;

    /* sink source: the output signal that will be written to disk */
    rc = ugen(&ctx, "SNK", ugen_list, &nugens);
    if (rc) goto clean;
    sink = ugen_list[nugens - 1];

    rc = render(&ctx, ugen_list, nugens, sink);

    if (rc) goto clean;

    clean:
    sonilo_ctx_destroy(&ctx);
    free(s);
    return 0;
}
