#include <stdlib.h>
#include <stdio.h>
#include "sonilo.h"
#include "array.h"
#include "context.h"
#include "iter.h"

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
const int sequence[] = {
    0, 5, 7, 10, 12, 10, 7, 5,
    0, 5, 7, 10, 12, 10, 7, 5,
    -2, 3, 5, 8, 10, 8, 5, 3,
    -2, 3, 5, 8, 10, 8, 5, 3,
};

int mkseq(sonilo_ctx *ctx, uint16_t *lst)
{
    uint32_t args;
    uint32_t val;
    uint16_t arr, iter, stk;
    int rc;
    int i;
    uint32_t *mem;

    mem = sonilo_mem(ctx->s);

    stk = CTX_STACK(mem, ctx->context);
    /* create an array of 16 8-bit (2^3) values */
    /* array args are packed in a word: len.wrdsz */
    args = 3 | (32 << 4);
    rc = sonilo_push(ctx, args);
    if (rc) return 1;

    /* push context address (needed for array) */
    rc = sonilo_push(ctx, ctx->context);
    if (rc) return 2;

    rc = array_create(mem, stk);
    if (rc) return 3;

    /* get the address from the stack */
    val = 0;
    rc = sonilo_pop(ctx, &val);
    if (rc) return 4;
    arr = val;

    /* set up sequence values */
    for (i = 0; i < 32; i++) {
        rc = sonilo_push(ctx, sequence[i] + 60);
        if (rc) return 5;
        rc = sonilo_push(ctx, i);
        if (rc) return 6;
        rc = sonilo_push(ctx, arr);
        if (rc) return 7;
        rc = array_write(mem, stk);
        if (rc) return 8;
    }

    /* create an array iterator */
    iter = 0;
    rc = iter_alloc(mem, ctx->context, &iter);
    if (rc) return 9;
    rc = iter_init(mem, iter);
    if (rc) return 10;
    rc = iter_array(mem, iter, arr);
    if (rc) return 11;

    /* push iterator onto stack */
    rc = sonilo_push(ctx, iter);
    if (rc) return 12;

    /* create sequencer ugen */
    rc = ugen(ctx, "SEQ", lst);
    if (rc) return 13;
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

    /* clock */
    rc = sonilo_constant(&ctx, 125 * 4);
    if (rc) goto clean;
    rc = ugen(&ctx, "CLK", ugen_list);
    if (rc) goto clean;
    rc = ugen(&ctx, "MET", ugen_list);
    if (rc) goto clean;

    rc = mkseq(&ctx, ugen_list);
    if (rc) goto clean;
    /* TODO: sequencer driven by clock */
    /* TODO: smoother on pitch signal */
    /* midi to frequency */
    rc = ugen(&ctx, "MTF", ugen_list);
    if (rc) goto clean;
    /* saw, controlled via freq signal */
    rc = ugen(&ctx, "SAW", ugen_list);
    if (rc) goto clean;

    rc = sonilo_constant(&ctx, 200);
    if (rc) goto clean;
    /* filter saw with LPF */
    rc = ugen(&ctx, "LPF", ugen_list);
    if (rc) goto clean;

    rc = sonilo_constant(&ctx, 0.7);
    if (rc) goto clean;
    rc = ugen(&ctx, "MUL", ugen_list);
    if (rc) goto clean;

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
