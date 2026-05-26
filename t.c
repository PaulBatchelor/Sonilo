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

typedef struct memwrite {
    /* cursor */
    uint16_t cursor;
    /* alt cursor */
    uint16_t alt;
    /* swap bit */
    uint8_t swap;

    /* errors */
    uint32_t err;

    /* instruction command lookup */
    instr_map instr;

    /* previous character */
    int8_t prev;
    /* 5-bit encoding mode */
    int encode;
    sonilo_vm *vm;
} memwrite;

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

/* TODO: build sonilo_vm_curswap */
static void swap_cursors(memwrite *mw)
{
    uint16_t tmp;
    tmp = mw->cursor;
    mw->cursor = mw->alt;
    mw->alt = tmp;
    mw->swap ^= 1;
}

void memwrite_init(memwrite *mw)
{
    mw->prev = 0;
    mw->err = 0;
    mw->alt = 0;
    mw->swap = 0;

    /* TODO: rework to use updated instr interface */
    instr_map_init(&mw->instr);
    instr_map_set(&mw->instr, instr_key("INC"), incr);
    instr_map_set(&mw->instr, instr_key("SAY"), say);
    instr_map_set(&mw->instr, instr_key("MVU"), move_up);
    instr_map_set(&mw->instr, instr_key("MVD"), move_down);
    instr_map_set(&mw->instr, instr_key("MVL"), move_left);
    instr_map_set(&mw->instr, instr_key("MVR"), move_right);
    instr_map_set(&mw->instr, instr_key("PRA"), invert);
    instr_map_set(&mw->instr, instr_key("PRB"), reverse);
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

void parse_memwrite(memwrite *mw, char c)
{
    sonilo_vm *vm;
#ifdef DEBUG
    fputc(c, stdout);
#endif
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
        /* DONE: GET rw */
        w = sonilo_vm_rw_get(vm);
        x = (uint8_t)(c - '0');
        w <<= 4;
        w |= x;
        /* DONE: SET rw */
        sonilo_vm_rw_set(vm, w);
        return;
    } else if (c >= 'A' && c <= 'F') {
        uint8_t x;
        uint32_t w;
        /* DONE: GET rw */
        w = sonilo_vm_rw_get(vm);
        x = (uint8_t)(c - 'A');
        x += 10;
        w <<= 4;
        w |= x;
        /* DONE: SET rw */
        sonilo_vm_rw_set(vm, w);
        return;
    }

    if (c == ' ' || c == '\n') return;

    /* '.' used to visually group nibbles */
    if (c == '.') return;

    if (iscmd(mw, c, "pr")) {
        uint32_t rw;
        rw = sonilo_vm_rw_get(vm);
        printf("%x\n", rw);
        mw->prev = 0;
        return;
    }

    if (iscmd(mw, c, "rv")) {
        /* DONE: GET/SET rw */
        uint32_t rw;
        rw = sonilo_vm_rw_get(vm);
        rw = reverse_nibbles(rw);
        sonilo_vm_rw_set(vm, rw);
        mw->prev = 0;
        return;
    }

    /* cl or '#': clear the word register */
    if (c == '#' || (mw->prev == 'c' && c == 'l')) {
        /* DONE: SET rw */
        sonilo_vm_rw_set(vm, 0);
        mw->prev = 0;
        return;
    }

    /* go: set cursor location */
    if (iscmd(mw, c, "go")) {
        uint32_t rw;
        mw->prev = 0;
        rw = 0;
        /* DONE: GET rw
         * TODO: SET cursor
         */
        rw = sonilo_vm_rw_get(vm);
        mw->cursor = rw & 0xFFFF;
        return;
    }

    /* wr: write word to memory */
    if (iscmd(mw, c, "wr")) {
        /* TODO: WRITE word operation */
        uint32_t rw;
        uint32_t *mem;
        mem = sonilo_vm_mem(vm);
        rw = sonilo_vm_rw_get(vm);
        mem[mw->cursor] = rw;
        mw->prev = 0;
        return;
    }

    /* read word from memory to word register */
    if (iscmd(mw, c, "rd")) {
        uint32_t *mem;
        /* TODO: READ word operation */
        mem = sonilo_vm_mem(vm);
        sonilo_vm_rw_set(vm, mem[mw->cursor]);
        mw->prev = 0;
        return;
    }

    /* bi: init blocklist */
    if (iscmd(mw, c, "bi")) {
        /* DONE: GET mem
         * TODO: GET cursor */
        blocklist_init(sonilo_vm_mem(vm), mw->cursor);
        mw->prev = 0;
        return;
    }

    /* ba: allocate block, write block id to word register */
    if (iscmd(mw, c, "ba")) {
        /* DONE: SET RW
         * TODO: GET cursor
         * DONE: GET mem
         */
        uint32_t rw;
        rw = blocklist_pop(sonilo_vm_mem(vm), mw->cursor);
        sonilo_vm_rw_set(vm, rw);
        /* TODO SET err */
        if (rw == 0) mw->err = 1;
        mw->prev = 0;
        return;
    }

    /* bf: free block, read block id from register word */
    if (iscmd(mw, c, "bf")) {
        /* TODO: SET err
         * DONE: GET mem
         * TODO: GET cursor
         * DONE: GET rw
         */
        uint32_t rw;
        rw = sonilo_vm_rw_get(vm);
        mw->err = blocklist_push(sonilo_vm_mem(vm), mw->cursor, rw & 0x3ff);
        mw->prev = 0;
        return;
    }

    /* si: initialize bitset */
    if (iscmd(mw, c, "si")) {
        /* DONE: GET mem
         * TODO: GET cursor
         */
        bitset_init(sonilo_vm_mem(vm), mw->cursor);
        mw->prev = 0;
        return;
    }

    /* sa: add item to bitset */
    if (iscmd(mw, c, "sa")) {
        /* DONE: GET mem
         * TODO: GET cursor
         * DONE: GET rw
         */
        uint32_t rw;
        rw = sonilo_vm_rw_get(vm);
        bitset_add(sonilo_vm_mem(vm), mw->cursor, rw);
        mw->prev = 0;
        return;
    }

    /* se: check to see if item exists in set */
    if (iscmd(mw, c, "se")) {
        /* DONE: GET mem
         * TODO: GET cursor
         * DONE: GET/GET rw
         */
        uint32_t rw;
        rw = sonilo_vm_rw_get(vm);
        rw = bitset_exists(sonilo_vm_mem(vm), mw->cursor, rw);
        sonilo_vm_rw_set(vm, rw);
        mw->prev = 0;
        return;
    }

    /* sr: remove item from bitset */
    if (iscmd(mw, c, "sr")) {
        /* DONE: GET mem
         * TODO: GET cursor
         * DONE: GET rw
         */
        uint32_t rw;
        rw = sonilo_vm_rw_get(vm);
        bitset_remove(sonilo_vm_mem(vm), mw->cursor, rw);
        mw->prev = 0;
        return;
    }

    /* rb: read word from block */
    if (iscmd(mw, c, "rb")) {
        int offset;
        uint32_t rw;
        uint32_t *mem;

        mem = sonilo_vm_mem(vm);
        mw->prev = 0;
        /* 6-bit address 0 - 63 */
        /* DONE: GET rw */
        rw = sonilo_vm_rw_get(vm);
        offset = rw & 0x3f;
        /* DONE: SET rw
         * TODO: GET cursor
         */
        rw = mem[mw->cursor + offset];
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* mi: initialize memory */
    if (iscmd(mw, c, "mi")) {
        uint32_t *stk;
        uint16_t p_top, p_block, p_avail, p_tags;
        uint8_t sp;
        uint32_t *mem;
        mw->prev = 0;

        /* DONE: GET mem
         * TODO: GET cursor
         */
        mem = sonilo_vm_mem(vm);
        stk = &mem[mw->cursor];
        sp = stk[0];

        if (sp < 4) {
            /* TODO: SET err */
            mw->err = 1;
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
        /* TODO: SET cursor
         * TODO: GET rw
         */
        rw = sonilo_vm_rw_get(vm);
        mw->cursor = old_block_to_word(rw); 
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

        mw->prev = 0;

        /* TODO: GET cursor
         * DONE: GET mem
         */
        mem = sonilo_vm_mem(vm);
        stk = &mem[mw->cursor];
        sp = stk[0];

        if (sp < 2) {
            mw->err = 2;
            return;
        }
        p_top = stk[sp]; sp--;
        k = stk[sp]; sp--;
        stk[0] = sp;

        /* DONE: SET rw
         * DONE: GET mem */
        rw = mem_alloc(mem, p_top, k);
        /* check for out of bounds results */
        /* DONE: GET rw
         * TODO: SET err
         */
        mw->err = rw > 63;
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* mf: free a block */
    if (iscmd(mw, c, "mf")) {
        uint8_t sp;
        uint32_t *stk, *mem;
        uint16_t p_top, L, k;

        mw->prev = 0;

        mem = sonilo_vm_mem(vm);

        /* DONE: GET mem
         * TODO: GET cursor */
        stk = &mem[mw->cursor];
        sp = stk[0];

        if (sp < 3) {
            /* TODO: SET err */
            mw->err = 1;
            return;
        }
        p_top = stk[sp]; sp--;
        k = stk[sp]; sp--;
        L = stk[sp]; sp--;
        stk[0] = sp;

        /* DONE: GET mem */
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
        mw->prev = 0;
        /* DONE: GET mem
         * TODO: GET cursor
         */
        barray_init(sonilo_vm_mem(vm), mw->cursor);
        return;
    }

    /* al: get array length */
    if (iscmd(mw, c, "al")) {
        uint32_t rw;
        mw->prev = 0;
        /* DONE: GET mem
         * TODO: GET cursor
         */
        rw = barray_length(sonilo_vm_mem(vm), mw->cursor);
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
        mw->prev = 0;
        /* TODO: SET err
         * DONE: GET mem
         * TODO: GET cursor
         * DONE: GET rw
         */
        rw = sonilo_vm_rw_get(vm);
        mw->err = barray_append(sonilo_vm_mem(vm), mw->cursor, rw);
        return;
    }

    /* ap: pop word from array */
    if (iscmd(mw, c, "ap")) {
        /* DONE: GET mem
         * TODO: GET cursor
         * DONE: GET rw ptr
         * TODO: SET err */
        uint32_t *rw;
        rw = sonilo_vm_rw_ptr(vm);
        mw->err = barray_pop(sonilo_vm_mem(vm), mw->cursor, rw);
        mw->prev = 0;
        return;
    }

    /* pe: print error flag */
    if (iscmd(mw, c, "pe")) {
        /* TODO: GET err */
        printf("%x", mw->err);
        fflush(stdout);
        mw->prev = 0;
        return;
    }

    /* ce: clear error flag */
    if (iscmd(mw, c, "ce")) {
        /* TODO: SET err */
        mw->prev = 0;
        mw->err = 0;
        return;
    }

    /* bw: convert block.offset #BBBOO notation into
     * a word address */
    if (iscmd(mw, c, "bw")) {
        uint16_t addr;
        mw->prev = 0;

        /* DONE: GET rw */
        addr = sonilo_vm_rw_get(vm);
        /* DONE: SET rw */
        addr = (addr >> 8) * 64 + (addr & 63);
        addr += BLOCKLIST_OFFSET;

        /* see: 'gb' */
        /* DONE: SET/GET rw */
        sonilo_vm_rw_set(vm, addr);

        return;
    }

    /* rc: read cursor into rw */
    if (iscmd(mw, c, "rc")) {
        /* DONE: SET rw
         * TODO: GET cursor */
        sonilo_vm_rw_set(vm, mw->cursor);
        mw->prev = 0;
        return;
    }

    /* mc: generate 32-bit checksum of memory allocator */
    if (iscmd(mw, c, "mc")) {
        /* DONE: GET rw
         * DONE: GET mem
         * DONE: SET rw */
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
        /* DONE: SET rw, GET rw */
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
        mw->prev = 0;

        /* DONE: GET mem
         * TODO: GET cursor */
        mem = sonilo_vm_mem(vm);
        stk = &mem[mw->cursor];
        sp = stk[0];

        if (sp < 2) {
            mw->err = 1;
            return;
        }

        y = stk[sp]; sp--;
        x = stk[sp]; sp--;

        /* DONE: set rw */
        sonilo_vm_rw_set(vm, x + y);

        stk[0] = sp;
        return;
    }

    /* zp: initialize zero page */
    if (iscmd(mw, c, "zp")) {
        uint32_t rw;
        mw->prev = 0;
        /* DONE: GET mem,
         * TODO: GET cursor
         * DONE: GET rw
         * DONE: SET rw */
        rw = sonilo_vm_rw_get(vm);
        rw = zero_page_init(sonilo_vm_mem(vm), mw->cursor, rw);
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
        mw->prev = 0;

        /* DONE: GET rw */
        rw = sonilo_vm_rw_get(vm);
        off = val = sz = 0;

        val = rw & 0xFFFF;
        rw >>= 16;
        sz = rw & 0xF;
        rw >>= 4;
        off = rw & 0xFFF;

        /* DONE: GET mem
         * TODO: GET cursor */
        bits_set(sonilo_vm_mem(vm), (mw->cursor << 4) + off, sz, val);

        return;
    }

    /* xr: */

    if (iscmd(mw, c, "xr")) {
        uint32_t rw;
        uint16_t off;
        uint8_t sz;
        mw->prev = 0;

        /* DONE: GET rw */
        rw = sonilo_vm_rw_get(vm);
        off = sz = 0;

        sz = rw & 0xF;
        rw >>= 4;
        off = rw & 0xFFF;

        /* DONE: GET mem
         * TODO: GET cursor
         * DONE: SET rw */
        rw = bits_get(sonilo_vm_mem(vm), (mw->cursor << 4) + off, sz);
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
        mw->prev = 0;
        /* DONE: SET rw
         * TODO: GET cursor
         * DONE: GET rw */
        rw = sonilo_vm_rw_get(vm);
        rw = block_to_word(mw->cursor, rw);
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* mb: memory address to block */
    if (iscmd(mw, c, "mb")) {
        uint32_t rw;
        mw->prev = 0;
        /* TODO: GET cursor
         * DONE: GET rw
         * DONE: SET rw */
        rw = sonilo_vm_rw_get(vm);
        rw = word_to_block(mw->cursor, rw);
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* jf: jump forward */
    if (iscmd(mw, c, "jf")) {
        int jump;
        uint32_t rw;
        mw->prev = 0;
        /* DONE: GET rw */
        rw = sonilo_vm_rw_get(vm);
        jump = rw & 0xFF;
        /* DONE: SET rw */
        rw >>= 8;
        sonilo_vm_rw_set(vm, rw);
        /* TODO: GET cursor
         * TODO: SET cursor */
        mw->cursor += jump;
        return;
    }

    /* jb: jump backward */
    if (iscmd(mw, c, "jb")) {
        int jump;
        uint32_t rw;

        mw->prev = 0;
        /* TODO: GET cursor */
        /* DONE: GET rw */
        rw = sonilo_vm_rw_get(vm);
        jump = rw & 0xFF;
        /* DONE : SET rw */
        rw >>= 8;
        /* TODO: SET cursor */
        mw->cursor -= jump;
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* sc: swap cursor */
    if (iscmd(mw, c, "sc")) {
        mw->prev = 0;
        swap_cursors(mw);
        return;
    }

    /* pa: pack addresses */
    if (iscmd(mw, c, "pa")) {
        uint16_t lsb, msb;
        uint32_t rw;

        /* DONE : GET rw */
        rw = sonilo_vm_rw_get(vm);
        mw->prev = 0;
        /* TODO: GET cursor */
        lsb = mw->cursor;
        msb = rw & 0xFFFF;

        /* DONE: SET rw */
        rw = (msb << 16) | lsb;
        sonilo_vm_rw_set(vm, rw);

        return;
    }
    
    /* sn: number of elements in bitset */
    if (iscmd(mw, c, "sn")) {
        mw->prev = 0;

        /* DONE: SET rw
         * TODO: GET cursor
         * DONE: GET mem */
        sonilo_vm_rw_set(vm, bitset_len(sonilo_vm_mem(vm), mw->cursor));
        return;
    }

    /* bb: allocate and mark */
    if (iscmd(mw, c, "bb")) {
        uint16_t lsb, msb;
        int blk;
        uint32_t rw;

        mw->prev = 0;
        rw = sonilo_vm_rw_get(vm);
        /* DONE: GET rw */
        lsb = rw & 0xFFFF;
        /* DONE: GET rw */
        msb = rw >> 16;

        /* DONE: GET mem */
        blk = blocklist_pop(sonilo_vm_mem(vm), lsb);

        if (blk <= 0) {
            /* TODO: SET err */
            mw->err = 1;
            return;
        }

        /* DONE: GET mem */
        bitset_add(sonilo_vm_mem(vm), msb, blk);

        /* DONE: SET rw */
        rw = blk;
        sonilo_vm_rw_set(vm, rw);

        return;
    }

    /* ex: execute command */
    if (iscmd(mw, c, "ex")) {
        uint32_t *rw;
        /* DONE: GET rw (pointer)
         * DONE: GET mem
         * TODO: GET instr (pointer) */
        rw = sonilo_vm_rw_ptr(vm);
        mw->err = instr_ex(sonilo_vm_mem(vm),
                &mw->instr,
                *rw,
                rw);

        return;
    }
   
    /* bx: execute block */
    if (iscmd(mw, c, "bx")) {
        uint32_t *rw;
        mw->prev = 0;
        rw = sonilo_vm_rw_ptr(vm);
        /* DONE: GET mem
         * TODO: GET instr (pointer)
         * TODO: GET cursor
         * TODO: SET err
         * DONE: GET rw (ptr) */
        mw->err = instr_block(sonilo_vm_mem(vm),
            &mw->instr, mw->cursor, rw);
        return;
    }

    /* ri: initialize rfcnt */
    if (iscmd(mw, c, "ri")) {
        mw->prev = 0;
        /* DONE: GET mem
         * TODO: GET cursor */
        rc_init(sonilo_vm_mem(vm), mw->cursor);
        return;
    }

    /* ra: refcnt add */
    if (iscmd(mw, c, "ra")) {
        uint32_t rw;
        mw->prev = 0;
        rw = sonilo_vm_rw_get(vm);
        /* DONE: GET mem
         * TODO: GET cursor
         * DONE: GET rw */
        rc_add(sonilo_vm_mem(vm), mw->cursor, rw);
        return;
    }

    /* rm: refcnt remove */
    if (iscmd(mw, c, "rm")) {
        uint32_t rw;

        rw = sonilo_vm_rw_get(vm);
        mw->prev = 0;
        /* DONE: GET mem
         * TODO: GET cursor
         * DONE: GET rw */
        rc_del(sonilo_vm_mem(vm), mw->cursor, rw);
        return;
    }

    /* rx: refcnt aux */
    if (iscmd(mw, c, "rx")) {
        int mode;
        uint32_t rw;

        mw->prev = 0;

        rw = sonilo_vm_rw_get(vm);
        /* DONE: GET rw */
        mode = rw & 0xF;
        /* DONE: SET rw */
        rw >>= 4;

        if (mode == 0) {
            /* DONE: GET rw
             * DONE: GET mem
             * TODO: GET cursor */
            rw = rc_length(sonilo_vm_mem(vm), mw->cursor);
            sonilo_vm_rw_set(vm, rw);
            return;
        } else if (mode == 1) {
            /* DONE: GET mem
             * DONE: GET rw
             * TODO: GET cursor
             * DONE: SET rw */
            rw = rc_get_count(sonilo_vm_mem(vm), mw->cursor, rw);
            sonilo_vm_rw_set(vm, rw);
            return;
        } else if (mode == 2) {
            /* DONE: GET mem
             * TODO: GET cursor
             * DONE: GET rw */
            rw = rc_get_hold(sonilo_vm_mem(vm), mw->cursor, rw);
            sonilo_vm_rw_set(vm, rw);
            return;
        } else if (mode == 3) {
            /* DONE: GET mem
             * TODO: GET cursor */
            rw = rc_get_active(sonilo_vm_mem(vm), mw->cursor);
            sonilo_vm_rw_set(vm, rw);
            return;
        }
        return;
    }

    /* rf: refcount find */
    if (iscmd(mw, c, "rf")) {
        uint32_t rw;
        mw->prev = 0;
        rw = sonilo_vm_rw_get(vm);
        /* DONE: GET mem
         * TODO: GET cursor
         * DONE: GET rw
         * DONE: SET rw */
        rw = sonilo_vm_rw_get(vm);
        rw = rc_find(sonilo_vm_mem(vm), mw->cursor, rw);
        sonilo_vm_rw_set(vm, rw);
        return;
    }
    
    /* ru: refcount update */
    if (iscmd(mw, c, "ru")) {
        int mode;
        uint32_t rw;

        mw->prev = 0;
        /* DONE: GET rw */
        rw = sonilo_vm_rw_get(vm);
        mode = rw & 0xF;
        /* DONE: set RW */
        rw >>= 4;

        if (mode) {
            /* decrement */
            /* DONE: GET rw
             * DONE: GET mem
             * TODO: GET cursor
             * DONE: SET rw */
            rw = rc_decr(sonilo_vm_mem(vm), mw->cursor, rw);
        } else {
            /* increment */
            /* DONE: SET rw
             * DONE: GET mem
             * TODO: GET cursor
             * DONE: GET rw */
            rw = rc_incr(sonilo_vm_mem(vm), mw->cursor, rw);
        }
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* rh: refcount hold/unhold */
    if (iscmd(mw, c, "rh")) {
        int mode;
        uint32_t rw;

        rw = sonilo_vm_rw_get(vm);
        mw->prev = 0;
        /* DONE: GET RW */
        mode = rw & 0xF;
        /* DONE: SET RW */
        rw >>= 4;

        if (mode) {
            /* unhold */
            /* DONE: GET mem
             * TODO: GET cursor
             * DONE: GET rw */
            rc_unhold(sonilo_vm_mem(vm), mw->cursor, rw);
        } else {
            /* hold */
            /* DONE: GET mem
             * TODO: GET cursor
             * DONE: GET rw */
            rc_hold(sonilo_vm_mem(vm), mw->cursor, rw);
        }
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* rs: refcount sweep */
    if (iscmd(mw, c, "rs")) {
        mw->prev = 0;
        /* DONE: GET mem
         * TODO: GET cursor */
        rc_sweep(sonilo_vm_mem(vm), mw->cursor);
        return;
    }
    
    /* rg: refcount get */
    if (iscmd(mw, c, "rg")) {
        mw->prev = 0;
        /* DONE: SET rw
         * DONE: GET mem
         * TODO: GET cursor */
        sonilo_vm_rw_set(vm, rc_get(sonilo_vm_mem(vm), mw->cursor));
        return;
    }

    /* pi: pstack init */
    if (iscmd(mw, c, "pi")) {
        mw->prev = 0;
        /* DONE: GET mem
         * TODO: GET cursor */
        pstack_init(sonilo_vm_mem(vm), mw->cursor);
        return;
    }

    /* pu: pstack push */
    if (iscmd(mw, c, "pu")) {
        int type;
        int data;
        uint32_t rw;

        mw->prev = 0;

        /* DONE: GET rw */
        rw = sonilo_vm_rw_get(vm);
        type = rw & 0xF;
        /* DONE: SET rw */
        rw >>= 4;
        /* DONE: GET rw */
        data = rw;
        /* TODO: SET err
         * DONE: GET mem
         * TODO: GET cursor */
        mw->err =
            pstack_push(sonilo_vm_mem(vm),
                mw->cursor,
                pstack_param(type, data));

        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* po: pstack pop */
    if (iscmd(mw, c, "po")) {
        uint32_t *rw;
        mw->prev = 0;
        rw = sonilo_vm_rw_ptr(vm);
        /* TODO: SET err
         * DONE: GET mem
         * TODO: GET cursor
         * DONE: GET rw (ptr) */
        mw->err = pstack_pop(sonilo_vm_mem(vm), mw->cursor, rw);
        return;
    }

    /* pd: pstack dup */
    if (iscmd(mw, c, "pd")) {
        mw->prev = 0;
        /* TODO: GET err
         * DONE: GET mem
         * TODO: GET cursor */
        mw->err = pstack_dup(sonilo_vm_mem(vm), mw->cursor);
        return;
    }

    /* pR: pstack rot */
    if (iscmd(mw, c, "pR")) {
        mw->prev = 0;
        /* TODO: SET err
         * DONE: GET mem
         * TODO: GET cursor */
        mw->err = pstack_rot(sonilo_vm_mem(vm), mw->cursor);
        return;
    }

    /* ph: pstack hold */
    if (iscmd(mw, c, "ph")) {
        int which;
        uint32_t rw;

        mw->prev = 0;
        rw = sonilo_vm_rw_get(vm);
        /* DONE: GET rw */
        which = rw & 0xF;
        /* DONE: SET rw */
        rw >>= 4;
        if (which == 0) {
            /* TODO: SET err
             * DONE: GET mem
             * TODO: GET cursor */
            mw->err = pstack_hold(sonilo_vm_mem(vm), mw->cursor);
        } else if (which == 1) {
            /* DONE: GET mem
             * TODO: GET cursor
             * TODO: SET err */
            mw->err = pstack_unhold(sonilo_vm_mem(vm), mw->cursor);
        }
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* ps: pstack swap */
    if (iscmd(mw, c, "ps")) {
        mw->prev = 0;
        /* TODO: SET err
         * DONE: GET mem
         * TODO: GET cursor */
        mw->err = pstack_swap(sonilo_vm_mem(vm), mw->cursor);
        return;
    }

    /* px: pstack aux */
    if (iscmd(mw, c, "px")) {
        int type;
        uint32_t rw;

        rw = sonilo_vm_rw_get(vm);
        mw->prev = 0;
        /* DONE: GET rw */
        type = rw & 0xF;
        /* DONE: SET rw */
        rw >>= 4;

        if (type == 0) {
            /* extract data component from param word */
            /* DONE: SET rw */
            rw >>= 2;
        }

        sonilo_vm_rw_set(vm, rw);

        return;
    }

    /* mx: memory aux */
    if (iscmd(mw, c, "mx")) {
        mw->prev = 0;
        /* DONE: SET rw
         * DONE: GET mem
         * TODO: GET cursor */
        sonilo_vm_rw_set(vm, mem_aux(sonilo_vm_mem(vm), mw->cursor));

        return;
    }

    /* ws: wordslice to value */
    if (iscmd(mw, c, "ws")) {
        uint32_t ws, rw;
        uint16_t addr, start, end;
        mw->prev = 0;
        ws = 0;

        /* extract arguments from RW register */
        /* DONE: GET rw */
        rw = sonilo_vm_rw_get(vm);
        end = rw & 0xFF;
        /* DONE: SET rw */
        rw >>= 8;
        /* DONE: GET rw */
        start = rw & 0xFF;
        /* DONE: SET rw */
        rw >>= 8;
        /* DONE: GET rw */
        addr = rw & 0xFFFF;

        /* build up word slice */
        ws = addr |
            ((start & 0x1f) << 16) |
            ((end & 0x1f) << 21);

        /* DONE: SET rw
         * DONE: GET mem */
        sonilo_vm_rw_set(vm, array_value(sonilo_vm_mem(vm), ws));
    }

    /* ci: context init */
    if (iscmd(mw, c, "ci")) {
        uint16_t ctx;
        int rc;
        mw->prev = 0;
        /* DONE: GET mem
         * TODO: GET cursor */
        ctx = context_init(sonilo_vm_mem(vm), mw->cursor);
    
        /* extra context goodies */

        /* DONE: GET mem */
        rc = context_allocator_setup(sonilo_vm_mem(vm), ctx);
        if (rc) {
            /* TODO: SET err */
            mw->err = 1;
            return;
        }

        /* TODO: GET mem */
        rc = context_pstack_setup(sonilo_vm_mem(vm), ctx);

        if (rc) {
            /* TODO: SET err */
            mw->err = 2;
            return;
        }

        /* TODO: SET err */
        mw->err = 0;

        /* DONE: SET rw */
        sonilo_vm_rw_set(vm, ctx);

        return;
    }

    /* sw: swap */
    if (iscmd(mw, c, "sw")) {
        mw->prev = 0;
        /* DONE: GET mem
         * TODO: GET cursor
         * TODO: SET err */
        mw->err = barray_swap(sonilo_vm_mem(vm), mw->cursor);
        return;
    }

    /* ac: array create */
    if (iscmd(mw, c, "ac")) {
        mw->prev = 0;
        /* TODO: SET err
         * DONE: GET mem
         * TODO: GET cursor */
        mw->err = array_create(sonilo_vm_mem(vm), mw->cursor);
        return;
    }

    /* ar: array read */
    if (iscmd(mw, c, "ar")) {
        mw->prev = 0;
        /* TODO: SET err
         * DONE: GET mem
         * TODO: GET cursor */
        mw->err = array_read(sonilo_vm_mem(vm), mw->cursor);
        return;
    }

    /* av: array value */
    if (iscmd(mw, c, "av")) {
        uint32_t rw;
        mw->prev = 0;
        rw = sonilo_vm_rw_get(vm);
        /* TODO: SET rw
         * DONE: GET mem
         * TODO: GET rw */
        rw = array_value(sonilo_vm_mem(vm), rw);
        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* aw: array write */
    if (iscmd(mw, c, "aw")) {
        mw->prev = 0;
        /* TODO: SET err
         * DONE: GET mem
         * TODO: GET cursor */
        mw->err = array_write(sonilo_vm_mem(vm), mw->cursor);
        return;
    }

    /* cu: select cursor */
    /* TODO: select cursor? */
    if (iscmd(mw, c, "cu")) {
        int cur;
        uint32_t rw;
        
        rw = sonilo_vm_rw_get(vm);
        mw->prev = 0;
        /* DONE: GET rw */
        cur = rw & 1;
        /* DONE: SET rw */
        rw >>= 4;

        if (cur) {
            /* want: cursor 1 */
            /* swap if unswapped to make cursor 1 active */
            if (!mw->swap) swap_cursors(mw);
        } else {
            /* want: cursor 0 */
            /* if swapped, swap to get cursor 0 active */
            if (mw->swap) swap_cursors(mw);
        }

        sonilo_vm_rw_set(vm, rw);
        return;
    }

    /* dr: stack drop */
    if (iscmd(mw, c, "dr")) {
        mw->prev = 0;
        /* DONE: GET mem
         * TODO: GET cursor
         * TODO: SET err */
        mw->err = barray_drop(sonilo_vm_mem(vm), mw->cursor);
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

        /* DONE: GET mem */
        mem = sonilo_vm_mem(vm);

        /* get stack */
        /* TODO: GET cursor */
        stk = mw->cursor;

        /* stack args: context, array */

        rc = barray_pop(mem, stk, &x);
        if (rc) {
            /* TODO: SET err */
            mw->err = 2;
            return;
        }
        ctx = x;

        x = 0;
        rc = barray_pop(mem, stk, &x);
        if (rc) {
            /* TODO: SET err */
            mw->err = 1;
            return;
        }
        arr = x;

        /* iter_alloc */

        iter = 0;
        rc = iter_alloc(mem, ctx, &iter);
        if (rc) {
            /* TODO: SET err */
            mw->err = 6;
            return;
        }


        /* iter_init */

        rc = iter_init(mem, iter);
        if (rc) {
            /* TODO: SET err */
            mw->err = 3;
            return;
        }

        /* iter_array */
        rc = iter_array(mem, iter, arr);

        if (rc) {
            /* TODO: SET err */
            mw->err = 4;
            return;
        }

        /* push iter to stack */
        rc = barray_append(mem, stk, iter);
        if (rc) {
            /* TODO: SET err */
            mw->err = 5;
            return;
        }

        /* TODO: SET err */
        mw->err = 0;
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

        mw->err = 0;

        mem = sonilo_vm_mem(vm);

        /* get stack */
        /* TODO: GET cursor */
        stk = mw->cursor;

        /* stack args: iterator */
        rc = barray_pop(mem, stk, &val);
        if (rc) {
            /* TODO: SET err */
            mw->err = 1;
            return;
        }

        iter = val;

        /* iter_next */
        slice = iter_next(mem, iter);

        rc = barray_append(mem, stk, iter);
        if (rc) {
            /* TODO: SET err */
            mw->err = 2;
            return;
        }

        /* push slice */
        rc = barray_append(mem, stk, slice);
        if (rc) {
            /* TODO: SET err */
            mw->err = 4;
            return;
        }

        mw->err = 0;
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
    mw->vm = malloc(sonilo_vm_sizeof());
    memwrite_init(mw);
    sonilo_vm_init(mw->vm);
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
    free(mw->vm);
    free(mw);
    return 0;
}
