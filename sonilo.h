#ifndef SONILO_H
#define SONILO_H
#include <stdint.h>

typedef struct sonilo sonilo;
typedef struct sonilo_ctx {
    sonilo *s;
    uint32_t *zpage;
} sonilo_ctx;

typedef struct sonilo_ugen {
    sonilo_ctx *ctx;
    uint32_t top;
    uint32_t *ports;
    uint32_t *data;
    uint32_t dsp;
} sonilo_ugen;

void sonilo_init(sonilo *s);
void sonilo_ctx_init(sonilo_ctx *ctx, sonilo *s);
void sonilo_ctx_destroy(sonilo_ctx *ctx);
size_t sonilo_sizeof(void);
uint32_t* sonilo_mem(sonilo *s, uint32_t p);
uint32_t sonilo_alloc(sonilo_ctx *ctx, int sz);
void sonilo_free(sonilo_ctx *ctx, uint32_t p);
uint32_t sonilo_block_pop(sonilo *s);
void sonilo_block_push(sonilo *s, uint32_t p);
void sonilo_ugen_init(sonilo_ctx *ctx, sonilo_ugen *u, int nports, int sz);
int sonilo_iport(sonilo_ugen *u, int port);
int sonilo_oport(sonilo_ugen *u, int port);
int sonilo_constant(sonilo_context *ctx, float c);
int sonlio_clean(sonlo_context *ctx);

#endif
