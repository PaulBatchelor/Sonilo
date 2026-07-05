#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "sonilo.h"
#include "mem.h"
#include "ins.h"
#include "ugen.h"
#include "array.h"
#include "context.h"
#include "iter.h"

#define BLOCKLIST_OFFSET 0x400

void sonilo_mem_aux(sonilo_vm *vm, int mode);
uint32_t cksum_range(uint32_t *mem, uint16_t w, uint16_t sz);

static volatile int running = 0;

static uint32_t reverse_nibbles(uint32_t w);

enum {
    PRINT,
    COMMENT,
    MEMWRITE,
    UPDATE,
    LABEL,
    QUIT
};

typedef struct memwrite {
    /* previous character */
    int8_t prev;
    /* 5-bit encoding mode */
    int encode;
    sonilo *s;
    sonilo_vm *vm;
    sonilo_host *host;
} memwrite;

int update_mode(char c) {
    switch (c) {
        case 'p':
            return PRINT;
        case 'c':
            return COMMENT;
        case 'm':
            return MEMWRITE;
        case 'l':
            return LABEL;
        case 'q':
            running = 0;
            return QUIT;
    }

    return PRINT;
}

static uint32_t incr(uint32_t *mem, uint16_t dat)
{
    return dat + 1;
}

static uint32_t say(uint32_t *mem, uint16_t dat)
{
    uint32_t *stk;
    uint32_t p;
    stk = &mem[dat];
    p = stk[0];

    if (p == 0) return 1;
    printf("%x ", stk[p]);
    fflush(stdout);

    return 0;
}

static uint32_t move_up(uint32_t *mem, uint16_t dat)
{
    uint32_t *stk;
    uint32_t p;
    stk = &mem[dat];
    p = stk[0];

    if (p == 0) return 1;
    /* update in place instead of push, add, push */
    stk[p] += 1;

    return 0;
}

static uint32_t move_down(uint32_t *mem, uint16_t dat)
{
    uint32_t *stk;
    uint32_t p;
    stk = &mem[dat];
    p = stk[0];

    if (p == 0) return 1;
    /* update in place instead of push, sub, push */
    stk[p] -= 1;

    return 0;
}

static uint32_t move_left(uint32_t *mem, uint16_t dat)
{
    uint32_t *stk;
    uint32_t p;
    stk = &mem[dat];
    p = stk[0];

    if (p == 0) return 1;
    stk[p] <<= 1;

    return 0;
}

static uint32_t move_right(uint32_t *mem, uint16_t dat)
{
    uint32_t *stk;
    uint32_t p;
    stk = &mem[dat];
    p = stk[0];

    if (p == 0) return 1;
    stk[p] >>= 1;

    return 0;
}

static uint32_t invert(uint32_t *mem, uint16_t dat)
{
    uint32_t *stk;
    uint32_t p;
    stk = &mem[dat];
    p = stk[0];

    if (p == 0) return 1;
    stk[p] = ~stk[p];

    return 0;
}

static uint32_t reverse(uint32_t *mem, uint16_t dat)
{
    uint32_t *stk;
    uint32_t p;
    stk = &mem[dat];
    p = stk[0];

    if (p == 0) return 1;
    stk[p] = reverse_nibbles(stk[p]);

    return 0;
}

void memwrite_init(memwrite *mw)
{
    mw->prev = 0;
    sonilo_host_cfunc(mw->host, instr_key("INC"), incr);
    sonilo_host_cfunc(mw->host, instr_key("SAY"), say);
    sonilo_host_cfunc(mw->host, instr_key("MVU"), move_up);
    sonilo_host_cfunc(mw->host, instr_key("MVD"), move_down);
    sonilo_host_cfunc(mw->host, instr_key("MVL"), move_left);
    sonilo_host_cfunc(mw->host, instr_key("MVR"), move_right);
    sonilo_host_cfunc(mw->host, instr_key("PRA"), invert);
    sonilo_host_cfunc(mw->host, instr_key("PRB"), reverse);
    mw->encode = 0;
}

static uint32_t reverse_nibbles(uint32_t w)
{
    w = (w & 0x0F0F0F0F) << 4 | (w & 0xF0F0F0F0) >> 4;
    w = (w & 0x00FF00FF) << 8 | (w & 0xFF00FF00) >> 8;
    w = (w & 0x0000FFFF) << 16 | (w & 0xFFFF0000) >> 16;
    return w;
}

static uint16_t old_block_to_word(uint16_t b)
{
    /* multiply by 64 to get word address,
     * then add an offset to skip the block list
     * block list size: 2^16/64 = 1024. 1024 blocks = 1024 words
     * 1024 is 0x400 in hex.
     * NOTE: this assumes blocklist address is 0.
     */
    return (b << 6) + BLOCKLIST_OFFSET;
}

static int iscmd(memwrite *mw, char c, const char *cmd)
{
    return mw->prev == cmd[0] && c == cmd[1];
}

static void context_aux(sonilo_vm *vm, int mode)
{
    int rc;
    uint16_t ctx;

    rc = 1;
    ctx = sonilo_vm_cursor_get(vm);
    if (mode == 0) { /* get stack address, set to RW */
        uint16_t stk;
        stk = CTX_STACK(sonilo_vm_mem(vm), ctx);
        sonilo_vm_rw_set(vm, stk);
        rc = 0;
    }

    sonilo_vm_err_set(vm, rc);
}

void parse_memwrite(memwrite *mw, char c)
{
    sonilo_vm *vm;
    vm = mw->vm;
    /* handle 5-bit encoding mode */
    if (mw->encode) {
        mw->encode = sonilo_vm_char(vm, c);
        return;
    }

    if (c == '\'') {
        mw->encode = 1;
        return;
    }

    /* process nibbles */
    if (c >= '0' && c <= '9') {
        uint8_t x;
        uint32_t w;
        w = sonilo_vm_rw_get(vm);
        x = (uint8_t)(c - '0');
        w <<= 4;
        w |= x;
        sonilo_vm_rw_set(vm, w);
        return;
    } else if (c >= 'A' && c <= 'F') {
        uint8_t x;
        uint32_t w;
        w = sonilo_vm_rw_get(vm);
        x = (uint8_t)(c - 'A');
        x += 10;
        w <<= 4;
        w |= x;
        sonilo_vm_rw_set(vm, w);
        return;
    }

    if (c == ' ' || c == '\n') return;

    /* '.' used to visually group nibbles */
    if (c == '.') return;

    /* pr: print word */
    if (iscmd(mw, c, "pr")) {
        uint32_t rw;
        rw = sonilo_vm_rw_get(vm);
        printf("%x\n", rw);
        fflush(stdout);
        mw->prev = 0;
        return;
    }

    if (iscmd(mw, c, "rv")) {
        uint32_t rw;
        rw = sonilo_vm_rw_get(vm);
        rw = reverse_nibbles(rw);
        sonilo_vm_rw_set(vm, rw);
        mw->prev = 0;
        return;
    }

    /* cl or '#': clear the word register */
    if (c == '#' || (mw->prev == 'c' && c == 'l')) {
        sonilo_vm_rw_set(vm, 0);
        mw->prev = 0;
        return;
    }

    /* go: set cursor location */
    if (iscmd(mw, c, "go")) {
        uint32_t rw;
        mw->prev = 0;
        rw = 0;
        rw = sonilo_vm_rw_get(vm);
        sonilo_vm_cursor_set(vm, rw & 0xFFFF);
        return;
    }

    /* wr: write word to memory */
    if (iscmd(mw, c, "wr")) {
        sonilo_vm_write(vm);
        mw->prev = 0;
        return;
    }

    /* read word from memory to word register */
    if (iscmd(mw, c, "rd")) {
        sonilo_vm_read(vm);
        mw->prev = 0;
        return;
    }

    /* bi: init blocklist */
    if (iscmd(mw, c, "bi")) {
        uint16_t cur;
        cur = sonilo_vm_cursor_get(vm);
        blocklist_init(sonilo_vm_mem(vm), cur);
        mw->prev = 0;
        return;
    }

    /* ba: allocate block, write block id to word register */
    if (iscmd(mw, c, "ba")) {
        uint32_t rw;
        uint16_t cur;
        cur = sonilo_vm_cursor_get(vm);
        rw = blocklist_pop(sonilo_vm_mem(vm), cur);
        sonilo_vm_rw_set(vm, rw);
        if (rw == 0) sonilo_vm_err_set(vm, 1);
        mw->prev = 0;
        return;
    }

    /* bf: free block, read block id from register word */
    if (iscmd(mw, c, "bf")) {
        uint32_t rw;
        uint32_t cur;
        uint32_t err;
        cur = sonilo_vm_cursor_get(vm);
        rw = sonilo_vm_rw_get(vm);
        err = blocklist_push(sonilo_vm_mem(vm), cur, rw & 0x3ff);
        sonilo_vm_err_set(vm, err);
        mw->prev = 0;
        return;
    }

    /* si: initialize bitset */
    if (iscmd(mw, c, "si")) {
        uint16_t cur;
        cur = sonilo_vm_cursor_get(vm);
        bitset_init(sonilo_vm_mem(vm), cur);
        mw->prev = 0;
        return;
    }

    /* sa: add item to bitset */
    if (iscmd(mw, c, "sa")) {
        uint32_t rw;
        uint16_t cur;
        cur = sonilo_vm_cursor_get(vm);
        rw = sonilo_vm_rw_get(vm);
        bitset_add(sonilo_vm_mem(vm), cur, rw);
        mw->prev = 0;
        return;
    }

    /* se: check to see if item exists in set */
    if (iscmd(mw, c, "se")) {
        uint32_t rw;
        uint16_t cur;
        cur = sonilo_vm_cursor_get(vm);
        rw = sonilo_vm_rw_get(vm);
        rw = bitset_exists(sonilo_vm_mem(vm), cur, rw);
        sonilo_vm_rw_set(vm, rw);
        mw->prev = 0;
        return;
    }

    /* sr: remove item from bitset */
    if (iscmd(mw, c, "sr")) {
        uint32_t rw;
        uint16_t cur;
        cur = sonilo_vm_cursor_get(vm);
        rw = sonilo_vm_rw_get(vm);
        bitset_remove(sonilo_vm_mem(vm), cur, rw);
        mw->prev = 0;
        return;
    }

    /* rb: read word from block */
    if (iscmd(mw, c, "rb")) {
        int offset;
        uint32_t rw;
        uint32_t *mem;
        uint16_t cur;

        mem = sonilo_vm_mem(vm);
        mw->prev = 0;
        /* 6-bit address 0 - 63 */
        rw = sonilo_vm_rw_get(vm);
        offset = rw & 0x3f;
        cur = sonilo_vm_cursor_get(vm);
        rw = mem[cur + offset];
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* mi: initialize memory */
    if (iscmd(mw, c, "mi")) {
        uint32_t *stk;
        uint16_t p_top, p_block, p_avail, p_tags;
        uint8_t sp;
        uint32_t *mem;
        uint16_t cur;
        mw->prev = 0;

        cur = sonilo_vm_cursor_get(vm);
        mem = sonilo_vm_mem(vm);
        stk = &mem[cur];
        sp = stk[0];

        if (sp < 4) {
            sonilo_vm_err_set(vm, 1);
            return;
        }

        /* pop off stack */
        p_tags = stk[sp]; sp--;
        p_avail = stk[sp]; sp--;
        p_block = stk[sp]; sp--;
        p_top = stk[sp]; sp--;

        stk[0] = sp;

        mem_init(mem, p_top, p_block, p_avail, p_tags);
        return;
    }

    /* gb: goto block */
    /* TODO: replace with bm go */
    if (iscmd(mw, c, "gb")) {
        uint32_t rw;
        /* multiply by 64 to get word address,
         * then add an offset to skip the block list
         * block list size: 2^16/64 = 1024. 1024 blocks = 1024 words
         */
        rw = sonilo_vm_rw_get(vm);
        sonilo_vm_cursor_set(vm, old_block_to_word(rw)); 
        mw->prev = 0;
        return;
    }

    /* ma: allocate a block */
    if (iscmd(mw, c, "ma")) {
        uint8_t sp;
        uint32_t *stk;
        uint16_t p_top, k;
        uint32_t rw;
        uint32_t *mem;
        uint16_t cur;

        mw->prev = 0;

        cur = sonilo_vm_cursor_get(vm);
        mem = sonilo_vm_mem(vm);
        stk = &mem[cur];
        sp = stk[0];

        if (sp < 2) {
            sonilo_vm_err_set(vm, 2);
            return;
        }
        p_top = stk[sp]; sp--;
        k = stk[sp]; sp--;
        stk[0] = sp;

        rw = mem_alloc(mem, p_top, k);
        /* check for out of bounds results */
        sonilo_vm_err_set(vm, rw > 63);
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* mf: free a block */
    if (iscmd(mw, c, "mf")) {
        uint8_t sp;
        uint32_t *stk, *mem;
        uint16_t p_top, L, k;
        uint16_t cur;

        mw->prev = 0;

        mem = sonilo_vm_mem(vm);

        cur = sonilo_vm_cursor_get(vm);
        stk = &mem[cur];
        sp = stk[0];

        if (sp < 3) {
            sonilo_vm_err_set(vm, 1);
            return;
        }
        p_top = stk[sp]; sp--;
        k = stk[sp]; sp--;
        L = stk[sp]; sp--;
        stk[0] = sp;

        mem_free(mem, p_top, L, k);
        return;
    }

    /* pw: print word */
    if (iscmd(mw, c, "pw")) {
        uint32_t rw;
        rw = sonilo_vm_rw_get(vm);
        printf("%x", rw);
        mw->prev = 0;
        return;
    }

    /* ai: initialize array */
    if (iscmd(mw, c, "ai")) {
        uint16_t cur;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        barray_init(sonilo_vm_mem(vm), cur);
        return;
    }

    /* al: get array length */
    if (iscmd(mw, c, "al")) {
        uint32_t rw;
        uint16_t cur;

        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        rw = barray_length(sonilo_vm_mem(vm), cur);
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* sp: display space character */
    if (iscmd(mw, c, "sp")) {
        putchar(' ');
        fflush(stdout);
        mw->prev = 0;
        return;
    }

    /* aa: append value to array */
    if (iscmd(mw, c, "aa")) {
        uint32_t rw;
        uint16_t cur;
        uint32_t err;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        rw = sonilo_vm_rw_get(vm);
        err = barray_append(sonilo_vm_mem(vm), cur, rw);
        sonilo_vm_err_set(vm, err);
        return;
    }

    /* ap: pop word from array */
    if (iscmd(mw, c, "ap")) {
        uint32_t *rw;
        uint16_t cur;
        uint32_t err;
        cur = sonilo_vm_cursor_get(vm);
        rw = sonilo_vm_rw_ptr(vm);
        err = barray_pop(sonilo_vm_mem(vm), cur, rw);
        sonilo_vm_err_set(vm, err);
        mw->prev = 0;
        return;
    }

    /* pe: print error flag */
    if (iscmd(mw, c, "pe")) {
        printf("%x", sonilo_vm_err_get(vm));
        fflush(stdout);
        mw->prev = 0;
        return;
    }

    /* ce: clear error flag */
    if (iscmd(mw, c, "ce")) {
        mw->prev = 0;
        sonilo_vm_err_set(vm, 0);
        return;
    }

    /* bw: convert block.offset #BBBOO notation into
     * a word address */
    if (iscmd(mw, c, "bw")) {
        uint16_t addr;
        mw->prev = 0;

        addr = sonilo_vm_rw_get(vm);
        addr = (addr >> 8) * 64 + (addr & 63);
        addr += BLOCKLIST_OFFSET;

        /* see: 'gb' */
        sonilo_vm_rw_set(vm, addr);

        return;
    }

    /* rc: read cursor into rw */
    if (iscmd(mw, c, "rc")) {
        uint16_t cur;
        cur = sonilo_vm_cursor_get(vm);
        sonilo_vm_rw_set(vm, cur);
        mw->prev = 0;
        return;
    }

    /* mc: generate 32-bit checksum of memory allocator */
    if (iscmd(mw, c, "mc")) {
        uint32_t rw;
        rw = sonilo_vm_rw_get(vm);
        rw = mem_cksum(sonilo_vm_mem(vm), rw);
        sonilo_vm_rw_set(vm, rw);
        mw->prev = 0;
        return;
    }

    /* bo: calculate block offset from word address */
    if (iscmd(mw, c, "bo")) {
        uint32_t rw;
        mw->prev = 0;
        /* assuming blocks are aligned, last 6 bits
         * should be the offset
         */
        rw = sonilo_vm_rw_get(vm);
        rw &= 63;
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* ad: add two numbers from a stack, store in word register */
    if (iscmd(mw, c, "ad")) {
        uint8_t sp;
        uint32_t *stk;
        uint16_t x, y;
        uint32_t *mem;
        uint16_t cur;

        mw->prev = 0;

        cur = sonilo_vm_cursor_get(vm);
        mem = sonilo_vm_mem(vm);
        stk = &mem[cur];
        sp = stk[0];

        if (sp < 2) {
            sonilo_vm_err_set(vm, 1);
            return;
        }

        y = stk[sp]; sp--;
        x = stk[sp]; sp--;

        sonilo_vm_rw_set(vm, x + y);

        stk[0] = sp;
        return;
    }

    /* zp: initialize zero page */
    if (iscmd(mw, c, "zp")) {
        uint32_t rw;
        uint16_t cur;

        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        rw = sonilo_vm_rw_get(vm);
        rw = zero_page_init(sonilo_vm_mem(vm), cur, rw);
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* TODO su: subtract two numbers from a stack, store in word register */
    /* TODO di : divide two numbers from a stack, store in word register */
    /* TODO mu : multiply two numbers from a stack, store in word register */

    /* xw: write bits */
    if (iscmd(mw, c, "xw")) {
        uint32_t rw;
        uint16_t off;
        uint16_t val;
        uint8_t sz;
        uint16_t cur;

        mw->prev = 0;

        rw = sonilo_vm_rw_get(vm);
        off = val = sz = 0;

        val = rw & 0xFFFF;
        rw >>= 16;
        sz = rw & 0xF;
        rw >>= 4;
        off = rw & 0xFFF;

        cur = sonilo_vm_cursor_get(vm);
        bits_set(sonilo_vm_mem(vm), (cur << 4) + off, sz, val);

        return;
    }

    /* xr: */

    if (iscmd(mw, c, "xr")) {
        uint32_t rw;
        uint16_t off;
        uint8_t sz;
        uint16_t cur;

        mw->prev = 0;

        rw = sonilo_vm_rw_get(vm);
        off = sz = 0;

        sz = rw & 0xF;
        rw >>= 4;
        off = rw & 0xFFF;

        cur = sonilo_vm_cursor_get(vm);
        rw = bits_get(sonilo_vm_mem(vm), (cur << 4) + off, sz);
        sonilo_vm_rw_set(vm, rw);

        return;
    }

    /* nl: print newline */
    if (iscmd(mw, c, "nl")) {
        mw->prev = 0;
        printf("\n");
        return;
    }

    /* bm: block to memory address */
    if (iscmd(mw, c, "bm")) {
        uint32_t rw;
        uint16_t cur;

        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        rw = sonilo_vm_rw_get(vm);
        rw = block_to_word(cur, rw);
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* mb: memory address to block */
    if (iscmd(mw, c, "mb")) {
        uint32_t rw;
        uint16_t cur;

        mw->prev = 0;
        rw = sonilo_vm_rw_get(vm);
        cur = sonilo_vm_cursor_get(vm);
        rw = word_to_block(cur, rw);
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* jf: jump forward */
    if (iscmd(mw, c, "jf")) {
        int jump;
        uint32_t rw;
        uint16_t cur;

        mw->prev = 0;
        rw = sonilo_vm_rw_get(vm);
        jump = rw & 0xFF;
        rw >>= 8;
        sonilo_vm_rw_set(vm, rw);
        cur = sonilo_vm_cursor_get(vm);
        cur += jump;
        sonilo_vm_cursor_set(vm, cur);
        return;
    }

    /* jb: jump backward */
    if (iscmd(mw, c, "jb")) {
        int jump;
        uint32_t rw;
        uint16_t cur;

        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        rw = sonilo_vm_rw_get(vm);
        jump = rw & 0xFF;
        rw >>= 8;
        cur -= jump;
        sonilo_vm_cursor_set(vm, cur);
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* sc: swap cursor */
    if (iscmd(mw, c, "sc")) {
        mw->prev = 0;
        sonilo_vm_cursor_swap(vm);
        return;
    }

    /* pa: pack addresses */
    if (iscmd(mw, c, "pa")) {
        uint16_t lsb, msb;
        uint32_t rw;

        rw = sonilo_vm_rw_get(vm);
        mw->prev = 0;
        lsb = sonilo_vm_cursor_get(vm);
        msb = rw & 0xFFFF;

        rw = (msb << 16) | lsb;
        sonilo_vm_rw_set(vm, rw);

        return;
    }
    
    /* sn: number of elements in bitset */
    if (iscmd(mw, c, "sn")) {
        uint16_t cur;
        mw->prev = 0;

        cur = sonilo_vm_cursor_get(vm);
        sonilo_vm_rw_set(vm, bitset_len(sonilo_vm_mem(vm), cur));
        return;
    }

    /* bb: allocate and mark */
    if (iscmd(mw, c, "bb")) {
        uint16_t lsb, msb;
        int blk;
        uint32_t rw;

        mw->prev = 0;
        rw = sonilo_vm_rw_get(vm);
        lsb = rw & 0xFFFF;
        msb = rw >> 16;

        blk = blocklist_pop(sonilo_vm_mem(vm), lsb);

        if (blk <= 0) {
            sonilo_vm_err_set(vm, 1);
            return;
        }

        bitset_add(sonilo_vm_mem(vm), msb, blk);

        rw = blk;
        sonilo_vm_rw_set(vm, rw);

        return;
    }

    /* ex: execute command */
    if (iscmd(mw, c, "ex")) {
        uint32_t *rw, err;
        rw = sonilo_vm_rw_ptr(vm);
        err = sonilo_host_ex(mw->host,
                sonilo_vm_mem(vm),
                *rw,
                rw);
        sonilo_vm_err_set(vm, err);

        return;
    }
   
    /* bx: execute block */
    if (iscmd(mw, c, "bx")) {
        uint32_t *rw;
        uint16_t cur;
        uint32_t err;
        mw->prev = 0;
        rw = sonilo_vm_rw_ptr(vm);
        cur = sonilo_vm_cursor_get(vm);
        err = sonilo_host_block(mw->host, sonilo_vm_mem(vm), cur, rw);
        sonilo_vm_err_set(vm, err);
        return;
    }

    /* ri: initialize rfcnt */
    if (iscmd(mw, c, "ri")) {
        uint16_t cur;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        rc_init(sonilo_vm_mem(vm), cur);
        return;
    }

    /* ra: refcnt add */
    if (iscmd(mw, c, "ra")) {
        uint32_t rw;
        uint16_t cur;

        mw->prev = 0;
        rw = sonilo_vm_rw_get(vm);
        cur = sonilo_vm_cursor_get(vm);
        rc_add(sonilo_vm_mem(vm), cur, rw);
        return;
    }

    /* rm: refcnt remove */
    if (iscmd(mw, c, "rm")) {
        uint32_t rw;
        uint16_t cur;

        rw = sonilo_vm_rw_get(vm);
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        rc_del(sonilo_vm_mem(vm), cur, rw);
        return;
    }

    /* rx: refcnt aux */
    if (iscmd(mw, c, "rx")) {
        int mode;
        uint32_t rw;
        uint16_t cur;

        mw->prev = 0;

        rw = sonilo_vm_rw_get(vm);
        mode = rw & 0xF;
        rw >>= 4;

        cur = sonilo_vm_cursor_get(vm);
        if (mode == 0) {
            rw = rc_length(sonilo_vm_mem(vm), cur);
            sonilo_vm_rw_set(vm, rw);
            return;
        } else if (mode == 1) {
            rw = rc_get_count(sonilo_vm_mem(vm), cur, rw);
            sonilo_vm_rw_set(vm, rw);
            return;
        } else if (mode == 2) {
            rw = rc_get_hold(sonilo_vm_mem(vm), cur, rw);
            sonilo_vm_rw_set(vm, rw);
            return;
        } else if (mode == 3) {
            rw = rc_get_active(sonilo_vm_mem(vm), cur);
            sonilo_vm_rw_set(vm, rw);
            return;
        }
        return;
    }

    /* rf: refcount find */
    if (iscmd(mw, c, "rf")) {
        uint32_t rw;
        uint16_t cur;

        mw->prev = 0;
        rw = sonilo_vm_rw_get(vm);
        cur = sonilo_vm_cursor_get(vm);
        rw = sonilo_vm_rw_get(vm);
        rw = rc_find(sonilo_vm_mem(vm), cur, rw);
        sonilo_vm_rw_set(vm, rw);
        return;
    }
    
    /* ru: refcount update */
    if (iscmd(mw, c, "ru")) {
        int mode;
        uint32_t rw;
        uint16_t cur;

        mw->prev = 0;
        rw = sonilo_vm_rw_get(vm);
        mode = rw & 0xF;
        rw >>= 4;

        cur = sonilo_vm_cursor_get(vm);
        if (mode) {
            /* decrement */
            rw = rc_decr(sonilo_vm_mem(vm), cur, rw);
        } else {
            /* increment */
            rw = rc_incr(sonilo_vm_mem(vm), cur, rw);
        }
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* rh: refcount hold/unhold */
    if (iscmd(mw, c, "rh")) {
        int mode;
        uint32_t rw;
        uint16_t cur;

        rw = sonilo_vm_rw_get(vm);
        mw->prev = 0;
        mode = rw & 0xF;
        rw >>= 4;

        cur = sonilo_vm_cursor_get(vm);
        if (mode) {
            /* unhold */
            rc_unhold(sonilo_vm_mem(vm), cur, rw);
        } else {
            /* hold */
            rc_hold(sonilo_vm_mem(vm), cur, rw);
        }
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* rs: refcount sweep */
    if (iscmd(mw, c, "rs")) {
        uint16_t cur;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        rc_sweep(sonilo_vm_mem(vm), cur);
        return;
    }
    
    /* rg: refcount get */
    if (iscmd(mw, c, "rg")) {
        uint16_t cur;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        sonilo_vm_rw_set(vm, rc_get(sonilo_vm_mem(vm), cur));
        return;
    }

    /* pi: pstack init */
    if (iscmd(mw, c, "pi")) {
        uint16_t cur;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        pstack_init(sonilo_vm_mem(vm), cur);
        return;
    }

    /* pu: pstack push */
    if (iscmd(mw, c, "pu")) {
        int type;
        int data;
        uint32_t rw, err;
        uint16_t cur;

        mw->prev = 0;

        rw = sonilo_vm_rw_get(vm);
        type = rw & 0xF;
        rw >>= 4;
        data = rw;
        cur = sonilo_vm_cursor_get(vm);
        err =
            pstack_push(sonilo_vm_mem(vm),
                cur,
                pstack_param(type, data));

        sonilo_vm_err_set(vm, err);
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* po: pstack pop */
    if (iscmd(mw, c, "po")) {
        uint32_t *rw, err;
        uint16_t cur;
        mw->prev = 0;
        rw = sonilo_vm_rw_ptr(vm);
        cur = sonilo_vm_cursor_get(vm);
        err = pstack_pop(sonilo_vm_mem(vm), cur, rw);
        sonilo_vm_err_set(vm, err);
        return;
    }

    /* pd: pstack dup */
    if (iscmd(mw, c, "pd")) {
        uint16_t cur;
        uint32_t err;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        err = pstack_dup(sonilo_vm_mem(vm), cur);
        sonilo_vm_err_set(vm, err);
        return;
    }

    /* pR: pstack rot */
    if (iscmd(mw, c, "pR")) {
        uint16_t cur;
        uint32_t err;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        err = pstack_rot(sonilo_vm_mem(vm), cur);
        sonilo_vm_err_set(vm, err);
        return;
    }

    /* ph: pstack hold */
    if (iscmd(mw, c, "ph")) {
        int which;
        uint32_t rw, err;
        uint16_t cur;

        mw->prev = 0;
        rw = sonilo_vm_rw_get(vm);
        which = rw & 0xF;
        rw >>= 4;
        cur = sonilo_vm_cursor_get(vm);
        if (which == 0) {
            err = pstack_hold(sonilo_vm_mem(vm), cur);
            sonilo_vm_err_set(vm, err);
        } else if (which == 1) {
            err = pstack_unhold(sonilo_vm_mem(vm), cur);
            sonilo_vm_err_set(vm, err);
        }
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* ps: pstack swap */
    if (iscmd(mw, c, "ps")) {
        uint32_t err;
        uint16_t cur;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        err = pstack_swap(sonilo_vm_mem(vm), cur);
        sonilo_vm_err_set(vm, err);
        return;
    }

    /* px: pstack aux */
    if (iscmd(mw, c, "px")) {
        int type;
        uint32_t rw;

        rw = sonilo_vm_rw_get(vm);
        mw->prev = 0;
        type = rw & 0xF;
        rw >>= 4;

        if (type == 0) {
            /* extract data component from param word */
            rw >>= 2;
        }

        sonilo_vm_rw_set(vm, rw);

        return;
    }

    /* mx: memory aux */
    if (iscmd(mw, c, "mx")) {
        uint16_t cur;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        sonilo_vm_rw_set(vm, mem_aux(sonilo_vm_mem(vm), cur));

        return;
    }

    /* ws: wordslice to value */
    if (iscmd(mw, c, "ws")) {
        uint32_t ws, rw;
        uint16_t addr, start, end;
        mw->prev = 0;
        ws = 0;

        /* extract arguments from RW register */
        rw = sonilo_vm_rw_get(vm);
        end = rw & 0xFF;
        rw >>= 8;
        start = rw & 0xFF;
        rw >>= 8;
        addr = rw & 0xFFFF;

        /* build up word slice */
        ws = addr |
            ((start & 0x1f) << 16) |
            ((end & 0x1f) << 21);

        sonilo_vm_rw_set(vm, array_value(sonilo_vm_mem(vm), ws));
    }

    /* ci: context init */
    if (iscmd(mw, c, "ci")) {
        uint16_t ctx;
        int rc;
        uint16_t cur;

        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        ctx = context_init(sonilo_vm_mem(vm), cur);
    
        /* extra context goodies */

        rc = context_allocator_setup(sonilo_vm_mem(vm), ctx);
        if (rc) {
            sonilo_vm_err_set(vm, 1);
            return;
        }

        rc = context_pstack_setup(sonilo_vm_mem(vm), ctx);

        if (rc) {
            sonilo_vm_err_set(vm, 2);
            return;
        }

        sonilo_vm_err_set(vm, 0);

        sonilo_vm_rw_set(vm, ctx);

        return;
    }

    /* sw: swap */
    if (iscmd(mw, c, "sw")) {
        uint16_t cur;
        uint32_t err;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        err = barray_swap(sonilo_vm_mem(vm), cur);
        sonilo_vm_err_set(vm, err);
        return;
    }

    /* ac: array create */
    if (iscmd(mw, c, "ac")) {
        uint16_t cur;
        uint32_t err;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        err = array_create_old(sonilo_vm_mem(vm), cur);
        sonilo_vm_err_set(vm, err);
        return;
    }

    /* ar: array read */
    if (iscmd(mw, c, "ar")) {
        uint16_t cur;
        uint32_t err;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        err = array_read(sonilo_vm_mem(vm), cur);
        sonilo_vm_err_set(vm, err);
        return;
    }

    /* av: array value */
    if (iscmd(mw, c, "av")) {
        uint32_t rw;
        mw->prev = 0;
        rw = sonilo_vm_rw_get(vm);
        rw = array_value(sonilo_vm_mem(vm), rw);
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* aw: array write */
    if (iscmd(mw, c, "aw")) {
        uint16_t cur;
        uint32_t err;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        err = array_write(sonilo_vm_mem(vm), cur);
        sonilo_vm_err_set(vm, err);
        return;
    }

    /* cu: select cursor */
    if (iscmd(mw, c, "cu")) {
        int cur;
        uint32_t rw;
        
        rw = sonilo_vm_rw_get(vm);
        mw->prev = 0;
        cur = rw & 1;
        rw >>= 4;
        sonilo_vm_cursor_select(vm, cur);

        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* dr: stack drop */
    if (iscmd(mw, c, "dr")) {
        uint16_t cur;
        uint32_t err;
        mw->prev = 0;
        cur = sonilo_vm_cursor_get(vm);
        err = barray_drop(sonilo_vm_mem(vm), cur);
        sonilo_vm_err_set(vm, err);
        return;
    }

    /* ia: create array iterator */
    if (iscmd(mw, c, "ia")) {
        int rc;
        uint16_t stk;
        uint32_t x;
        uint16_t ctx, arr;
        uint16_t iter;
        uint32_t *mem;

        mw->prev = 0;

        mem = sonilo_vm_mem(vm);

        /* get stack */
        /* TODO: GET cursor */
        stk = sonilo_vm_cursor_get(vm);

        /* stack args: context, array */

        rc = barray_pop(mem, stk, &x);
        if (rc) {
            sonilo_vm_err_set(vm, 2);
            return;
        }
        ctx = x;

        x = 0;
        rc = barray_pop(mem, stk, &x);
        if (rc) {
            sonilo_vm_err_set(vm, 1);
            return;
        }
        arr = x;

        /* iter_alloc */

        iter = 0;
        rc = iter_alloc(mem, ctx, &iter);
        if (rc) {
            sonilo_vm_err_set(vm, 6);
            return;
        }


        /* iter_init */

        rc = iter_init(mem, iter);
        if (rc) {
            sonilo_vm_err_set(vm, 3);
            return;
        }

        /* iter_array */
        rc = iter_array(mem, iter, arr);

        if (rc) {
            sonilo_vm_err_set(vm, 4);
            return;
        }

        /* push iter to stack */
        rc = barray_append(mem, stk, iter);
        if (rc) {
            sonilo_vm_err_set(vm, 5);
            return;
        }

        sonilo_vm_err_set(vm, 0);
        return;
    }

    /* in: call array next */
    if (iscmd(mw, c, "in")) {
        uint16_t stk;
        uint32_t val;
        uint16_t iter;
        uint32_t slice;
        int rc;
        uint32_t *mem;

        mw->prev = 0;

        sonilo_vm_err_set(vm, 0);
        mem = sonilo_vm_mem(vm);

        /* get stack */
        stk = sonilo_vm_cursor_get(vm);

        /* stack args: iterator */
        rc = barray_pop(mem, stk, &val);
        if (rc) {
            sonilo_vm_err_set(vm, 1);
            return;
        }

        iter = val;

        /* iter_next */
        slice = iter_next(mem, iter);

        rc = barray_append(mem, stk, iter);
        if (rc) {
            sonilo_vm_err_set(vm, 2);
            return;
        }

        /* push slice */
        rc = barray_append(mem, stk, slice);
        if (rc) {
            sonilo_vm_err_set(vm, 4);
            return;
        }

        sonilo_vm_err_set(vm, 0);
        return;
    }

    /* un: universe utility */
    if (iscmd(mw, c, "un")) {
        uint32_t rw;
        uint8_t nib;
        uint32_t err;

        mw->prev = 0;
        rw = sonilo_vm_rw_get(vm);

        nib = rw & 0xF;
        rw >>= 4;

        sonilo_vm_rw_set(vm, rw);

        err = 0;

        /* nib 0: push, nib 1: pull */

        if (nib == 0) {
            err = sonilo_upush(mw->s);
        } else if (nib == 1) {
            err = sonilo_upull(mw->s);
        }

        sonilo_vm_err_set(vm, err);

        return;
    }

    /* ww: send word to word machine */
    if (iscmd(mw, c, "ww")) {
        int err;
        uint32_t rw;
        mw->prev = 0;
        rw = sonilo_vm_rw_get(mw->vm);
        err = sonilo_sendw(mw->s, rw);
        sonilo_vm_err_set(mw->vm, err);
        return;
    }

    /* wx: word machine utilities */
    if (iscmd(mw, c, "wx")) {
        uint32_t rw;
        uint8_t nib;
        mw->prev = 0;
        rw = sonilo_vm_rw_get(mw->vm);
        nib = rw & 0xF;
        rw >>= 4;
        sonilo_vm_rw_set(mw->vm, rw);

        if (nib == 0) {
            /* re-initialize word machine */
            sonilo_wm_init(mw->s);
            return;
        }
        return;
    }

    /* ax: main allocator aux utilities */
    if (iscmd(mw, c, "ax")) {
        int mode;
        uint32_t rw;
        mw->prev = 0;

        rw = sonilo_vm_rw_get(mw->vm);
        mode = rw & 0xF;
        rw >>= 4;
        sonilo_vm_rw_set(mw->vm, rw);
        sonilo_mem_aux(mw->vm, mode);
    }

    /* cx: context aux functions */
    if (iscmd(mw, c, "cx")) {
        int mode;
        uint32_t rw;
        mw->prev = 0;

        rw = sonilo_vm_rw_get(mw->vm);
        mode = rw & 0xF;
        rw >>= 4;
        sonilo_vm_rw_set(mw->vm, rw);
        context_aux(mw->vm, mode);
    }

    /* cs: perform block checksum */
    if (iscmd(mw, c, "cs")) {
        mw->prev = 0;
        sonilo_cksum(mw->vm);
        return;
    }

    mw->prev = c;
}

int main (int argc, char *argv[])
{
    int mode;
    memwrite *mw;

    mw = malloc(sizeof(memwrite));
    mode = PRINT;
    sonilo_create(&mw->s);
    mw->vm = sonilo_get_vm(mw->s);
    mw->host = sonilo_get_host(mw->s);
    memwrite_init(mw);
    running = 1;

    while (!feof(stdin) && running) {
        char c;
        c = fgetc(stdin);

        /* handle FEOF character */
        if (c == -1) continue;

        if (mode == UPDATE) {
            mode = update_mode(c);
            continue;
        }

        if (c == '@') {
            mode = UPDATE;
            continue;
        }

        if (mode == COMMENT) {
            /* ignore comment for now */
            continue;
        }

        if (mode == MEMWRITE) {
            parse_memwrite(mw, c);
            continue;
        }

        if (mode == LABEL) {
            if (c != ' ' && c != '\n') fputc(c, stdout);
            continue;
        }

        /* print mode */
        fputc(c, stdout);
    }

    sonilo_destroy(mw->s);
    free(mw);
    return 0;
}
