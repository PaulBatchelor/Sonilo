#ifndef SONILO_H
#define SONILO_H
#include <stddef.h>
#include <stdint.h>

typedef uint32_t (*instr_func)(uint32_t *, uint16_t);
typedef struct sonilo_vm sonilo_vm;
typedef struct sonilo_host sonilo_host;
typedef struct sonilo sonilo;
typedef struct sonilo_ctx {
    sonilo *s;
    uint16_t context;
    uint16_t allocator;
    uint16_t pstack;
} sonilo_ctx;

typedef struct sonilo_ugen_data {
    uint16_t top;
    uint32_t *ports;
    uint32_t *state;
    uint16_t cmd;
} sonilo_ugen_data;

typedef struct sonilo_ugen {
    sonilo_ctx *ctx;
    sonilo_ugen_data data;
} sonilo_ugen;

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
int sonilo_vm_init(sonilo_vm *vm);
int sonilo_host_init(sonilo_host *host, sonilo_vm *vm);
size_t sonilo_vm_sizeof(void);
size_t sonilo_host_sizeof(void);
int sonilo_create(sonilo **ps);
void sonilo_destroy(sonilo *s);
int sonilo_init(sonilo *s);
size_t sonilo_sizeof(void);
sonilo_vm* sonilo_get_vm(sonilo *s);
sonilo_host* sonilo_get_host(sonilo *s);

/* mem: return word memory */
uint32_t *sonilo_mem(sonilo *s);

/* set: set r/w register */
int sonilo_set(sonilo *s, uint32_t w);
int sonilo_get(sonilo *s, uint32_t *w);

/* go: set cursor to location in rw */
int sonilo_go(sonilo *s);

/* read: read value in cursor to rw */
int sonilo_read(sonilo *s);

/* call: use index in rw to call stored subroutine */
int sonilo_call(sonilo *s);
int sonilo_call_direct(sonilo *s);

/* sonilo context setup/teardown */
int sonilo_ctx_init(sonilo_ctx *ctx, sonilo *s);
int sonilo_ctx_destroy(sonilo_ctx *ctx);

/* ugen operations */
int sonilo_ugen_init(sonilo_ctx *ctx, sonilo_ugen *u, uint16_t ukey, int nports, int sz);
int sonilo_ugen_new(uint32_t *mem, uint16_t ukey, int nports, int sz);

/* iport: input port. pops value from pstack, sets it to port */
int sonilo_iport(sonilo_ugen *u, int port);

/* oport: output port. pushs block (signal) to pstack, stores in port */
int sonilo_oport(sonilo_ugen *u, int port);

/* hold/unhold: used to indefinitely keep a signal a live */
int sonilo_hold(sonilo_ctx *ctx, uint32_t *w);
int sonilo_unhold(sonilo_ctx *ctx, uint32_t w);

/* ports */
sonilo_port sonilo_port_constant(float c);
sonilo_port sonilo_port_block(uint32_t *mem, uint32_t p);
sonilo_port sonilo_port_from_word(uint32_t *mem, uint32_t w);
uint32_t sonilo_port_to_word(uint32_t *mem, sonilo_port *p);
float sonilo_port_read(sonilo_port *p, int n);
void sonilo_port_write(sonilo_port *p, int n, float s);

/* constant: push a constant value to pstack */
int sonilo_constant(sonilo_ctx *ctx, float c);

/* symbol: push a 'symbol' (3-letter identifier) to stack */
int sonilo_symbol(sonilo_ctx *ctx, const char *sym);

/* clean: perform RC sweep (call after ugen sets up ports) */
int sonilo_flush(sonilo_ctx *ctx);

/* pop: pop value from system stack */
int sonilo_pop(sonilo_ctx *ctx, uint32_t *w);

/* push: push to system stack */
int sonilo_push(sonilo_ctx *ctx, uint32_t w);

/* ppush: push to parameter stack */
int sonilo_ppush(sonilo_ctx *ctx, uint32_t w);

/* peak: retrieve value from system stack without popping */
int sonilo_peak(sonilo_ctx *ctx, uint32_t *w);

/* compute: compute a block of audio from a ugen */
void sonilo_ugen_compute(sonilo_ugen *u);

/* block: gets block at port. errors if port is not a block */
int sonilo_ugen_block(uint32_t *mem,
        uint16_t ugen,
        int portnum,
        float **block);

/* get a ugen from a sonilo memory location */
int sonilo_ugen_get(uint32_t *mem, sonilo_ugen_data *u, uint16_t p);

/* create a instance of a predefined ugen */
int sonilo_ugen_create(sonilo_ctx *ctx);

/* create key from 3-letter alphabetic (A-Z) combo */
uint16_t sonilo_key(const char *key);
uint16_t sonilo_command(sonilo *s, uint16_t key, instr_func func);

/* generate alternate lookup key from key */
uint16_t sonilo_alt(uint16_t key);

/* get sonilo samplerate (possibly from memory) */
uint32_t sonilo_srate(uint32_t *mem);

/* register ugen to system */

int sonilo_ugen_register(sonilo *s,
        const char *sym,
        instr_func init,
        instr_func render);

/* process: compute ugen block contained in context */
int sonilo_process_old(sonilo_ctx *ctx);
/* same thing, just without context struct */
int sonilo_process(sonilo *s, uint16_t ctx);

/* mkugen: lookup and create ugen, and append to ugen block */
int sonilo_mkugen(sonilo_ctx *ctx, const char *ugen);

/* last ugen: retrieve last ugen in block */
int sonilo_last_ugen(sonilo_ctx *ctx, uint16_t *last);

/* universe */

/* size: get the size of the universe (in words) */
size_t universe_size(void);

/* get: memory pointer to specific megablock */
int universe_get(uint32_t *u, uint16_t b, uint32_t **blk);

/* pull: pull data from universe (src) to sonilo (dst) */
int universe_pull(uint32_t *u,
    uint16_t dst,
    uint16_t src,
    uint16_t sz);

/* push: push data to universe (dst) from sonilo (src) */
int universe_push(uint32_t *u,
    uint16_t dst,
    uint16_t src,
    uint16_t sz);

sonilo_vm* universe_sonilo(uint32_t *u);

/* upush: sonilo wrapper for universe push.
 * uses context stack. Args: to from sz */
int sonilo_upush(sonilo *s);

/* upull: sonilo wrapper for universe pull.
 * Args: from to sz */
int sonilo_upull(sonilo *s);

/* VM */

/* read/write register */
uint32_t sonilo_vm_rw_get(sonilo_vm *vm);
void sonilo_vm_rw_set(sonilo_vm *vm, uint32_t rw);
uint32_t* sonilo_vm_rw_ptr(sonilo_vm *vm);

/* error flag */
uint32_t sonilo_vm_err_get(sonilo_vm *vm);
void sonilo_vm_err_set(sonilo_vm *vm, uint32_t err);
uint32_t* sonilo_vm_err_ptr(sonilo_vm *vm);

/* memory */
uint32_t* sonilo_vm_mem(sonilo_vm *vm);

/* cursor */
uint32_t sonilo_vm_cursor_get(sonilo_vm *vm);
void sonilo_vm_cursor_set(sonilo_vm *vm, uint16_t cur);
void sonilo_vm_cursor_swap(sonilo_vm *vm);
void sonilo_vm_cursor_select(sonilo_vm *vm, int which);
uint16_t* sonilo_vm_cursor_ptr(sonilo_vm *vm);

/* read/write words to memory based on cursor location */
void sonilo_vm_read(sonilo_vm *vm);
void sonilo_vm_write(sonilo_vm *vm);

/* addchar: append a character to RW using 5-bit encoding */
int sonilo_vm_char(sonilo_vm *vm, char c);

int sonilo_host_cfunc(sonilo_host *host, uint16_t key, instr_func func);
int sonilo_host_ex(sonilo_host *host, uint32_t *mem, uint32_t i, uint32_t *rw);
int sonilo_host_block(sonilo_host *host, uint32_t *mem, uint16_t p, uint32_t *rw);
int sonilo_host_index(sonilo_host *host, uint16_t key);

uint32_t sonilo_vm_blocklist(sonilo_vm *vm);

/* process opcodes inside the sonilo VM */
/* form A uses a 7-bit data type */
int sonilo_op_a(sonilo *s, char type, uint8_t data);
/* form B uses a 32-bit data type */
int sonilo_op_b(sonilo *s, char type, uint32_t data);

/* send: send byte to word machine */
int sonilo_send(sonilo *s, unsigned char c);

/* sendw: send word to word machine */
int sonilo_sendw(sonilo *s, uint32_t w);

/* begin: begin new message */
int sonilo_begin(sonilo *s);

/* end: end message and evaluate */
int sonilo_end(sonilo *s);

/* wm_init: (re-)initialize word machine */
void sonilo_wm_init(sonilo *s);

float sonilo_qtof(uint32_t w);
uint32_t sonilo_ftoq(float x);

#endif
