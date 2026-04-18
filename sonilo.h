#ifndef SONILO_H
#define SONILO_H
#include <stddef.h>
#include <stdint.h>

typedef uint32_t (*instr_func)(uint32_t *, uint16_t);

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
    uint32_t *state;
    uint16_t dsp;
} sonilo_ugen;

enum {
    PORT_NONE = 0,
    PORT_BLOCK,
    PORT_CONSTANT
};

typedef struct ugen_block {
    uint16_t addr;
    float *block;
} ugen_block;

typedef struct sonilo_port {
    int type;
    union {
        float constant;
        ugen_block block;
    } data;
} sonilo_port;

/* initialize sonilo VM */
void sonilo_init(sonilo *s);
size_t sonilo_sizeof(void);

/* sonilo context setup/teardown */
void sonilo_ctx_init(sonilo_ctx *ctx, sonilo *s);
void sonilo_ctx_destroy(sonilo_ctx *ctx);

/* ugen operations */
int sonilo_ugen_init(sonilo_ctx *ctx, sonilo_ugen *u, uint16_t ukey, int nports, int sz);

/* iport: input port. pops value from pstack, sets it to port */
int sonilo_iport(sonilo_ugen *u, int port);

/* oport: output port. pushs block (signal) to pstack, stores in port */
int sonilo_oport(sonilo_ugen *u, int port);

/* port: set up a port C wrapper from port word */
int sonilo_ugen_port(uint32_t *mem, uint32_t w, sonilo_port *p);
sonilo_port sonilo_port_constant(float c);
sonilo_port sonilo_port_from_word(uint32_t *mem, uint32_t w);
uint32_t sonilo_port_to_word(uint32_t *mem, sonilo_port *p);
float sonilo_port_read(sonilo_port *p, int n);
void sonilo_port_write(sonilo_port *p, int n, float s);

/* constant: push a constant value to pstack */
int sonilo_constant(sonilo_ctx *ctx, float c);

/* clean: perform RC sweep (call after ugen sets up ports) */
int sonilo_clean(sonilo_ctx *ctx);

/* compute: compute a block of audio from a ugen */
void sonilo_ugen_compute(sonilo_ugen *u);

/* block: gets block at port. errors if port is not a block */
int sonilo_ugen_block(sonilo_ugen *u, int port, float **block);

/* get a ugen from a sonilo memory location */
int sonilo_ugen_get(uint32_t *mem, sonilo_ugen *u, uint16_t p);

/* create key from 3-letter alphabetic (A-Z) combo */
uint16_t sonilo_key(const char *key);
uint16_t sonilo_command(sonilo *s, uint16_t key, instr_func func);

/* get sonilo samplerate (possibly from memory) */
uint32_t sonilo_srate(uint32_t *mem);

#endif
