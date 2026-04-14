#ifndef SONILO_H
#define SONILO_H
#include <stddef.h>
#include <stdint.h>

typedef struct sonilo sonilo;
typedef struct sonilo_ctx {
    sonilo *s;
    uint16_t context;
    uint16_t allocator;
} sonilo_ctx;

typedef struct sonilo_ugen {
    sonilo_ctx *ctx;
    uint16_t top;
    uint32_t *ports;
    uint32_t *data;
    uint16_t dsp;
} sonilo_ugen;

void sonilo_init(sonilo *s);
void sonilo_ctx_init(sonilo_ctx *ctx, sonilo *s);
void sonilo_ctx_destroy(sonilo_ctx *ctx);
size_t sonilo_sizeof(void);
uint32_t* sonilo_mem(sonilo *s, uint16_t p);
uint16_t sonilo_alloc(sonilo_ctx *ctx, int sz);
void sonilo_free(sonilo_ctx *ctx, uint16_t p);
uint32_t sonilo_block_pop(sonilo *s);
void sonilo_block_push(sonilo *s, uint32_t p);
void sonilo_ugen_init(sonilo_ctx *ctx, sonilo_ugen *u, int nports, int sz);
int sonilo_iport(sonilo_ugen *u, int port);
int sonilo_oport(sonilo_ugen *u, int port);
int sonilo_constant(sonilo_ctx *ctx, float c);
int sonlio_clean(sonilo_ctx *ctx);
void sonilo_ugen_compute(sonilo_ugen *u);

#endif
