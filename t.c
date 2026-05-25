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
    /* memory */
    uint32_t mem[0x10000];

    /* word register */
    uint32_t rw; 
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
    uint32_t i;
    /* TODO: move to sonilo_vm_init */
    mw->rw = 0;
    mw->prev = 0;
    for (i = 0; i < 0x10000; i++) mw->mem[i] = 0;
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
#ifdef DEBUG
        fputc(c, stdout);
#endif
    /* handle 5-bit encoding mode */
    if (mw->encode) {
        int b;
        /* TODO: vm_append_char? */
        if (c == '\'') {
            mw->encode = 0;
            /* shift 1-bit. assuming 3-characters, will make
             * it align to 16 bits */
            mw->rw <<= 1;
            return;
        }
        b = instr_char_sym(c);
        if (b < 0) return;
        mw->rw <<= 5;
        mw->rw |= b;

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
        /* TODO: GET rw */
        w = mw->rw;
        x = (uint8_t)(c - '0');
        w <<= 4;
        w |= x;
        /* TODO: SET rw */
        mw->rw = w;
        return;
    } else if (c >= 'A' && c <= 'F') {
        uint8_t x;
        uint32_t w;
        /* TODO: GET rw */
        w = mw->rw;
        x = (uint8_t)(c - 'A');
        x += 10;
        w <<= 4;
        w |= x;
        /* TODO: SET rw */
        mw->rw = w;
        return;
    }

    if (c == ' ' || c == '\n') return;

    /* '.' used to visually group nibbles */
    if (c == '.') return;

    if (iscmd(mw, c, "pr")) {
        printf("%x\n", mw->rw);
        mw->prev = 0;
        return;
    }

    if (iscmd(mw, c, "rv")) {
        /* TODO: GET/SET rw */
        mw->rw = reverse_nibbles(mw->rw); 
        mw->prev = 0;
        return;
    }

    /* cl or '#': clear the word register */
    if (c == '#' || (mw->prev == 'c' && c == 'l')) {
        /* TODO: SET rw */
        mw->rw = 0;
        mw->prev = 0;
        return;
    }

    /* go: set cursor location */
    if (iscmd(mw, c, "go")) {
        mw->prev = 0;
        /* TODO: GET rw, SET cursor */
        mw->cursor = mw->rw & 0xFFFF;
        return;
    }

    /* wr: write word to memory */
    if (iscmd(mw, c, "wr")) {
        /* TODO: WRITE word operation */
        mw->mem[mw->cursor] = mw->rw;
        mw->prev = 0;
        return;
    }

    /* read word from memory to word register */
    if (iscmd(mw, c, "rd")) {
        /* TODO: READ word operation */
        mw->rw = mw->mem[mw->cursor];
        mw->prev = 0;
        return;
    }

    /* bi: init blocklist */
    if (iscmd(mw, c, "bi")) {
        /* TODO: GET mem, GET cursor */
        blocklist_init(mw->mem, mw->cursor);
        mw->prev = 0;
        return;
    }

    /* ba: allocate block, write block id to word register */
    if (iscmd(mw, c, "ba")) {
        /* TODO SET RW, GET cursor, GET mem */
        mw->rw = blocklist_pop(mw->mem, mw->cursor);
        /* TODO SET err */
        if (mw->rw == 0) mw->err = 1;
        mw->prev = 0;
        return;
    }

    /* bf: free block, read block id from register word */
    if (iscmd(mw, c, "bf")) {
        /* TODO: SET err, GET mem, GET cursor, GET rw */
        mw->err = blocklist_push(mw->mem, mw->cursor, mw->rw & 0x3ff);
        mw->prev = 0;
        return;
    }

    /* si: initialize bitset */
    if (iscmd(mw, c, "si")) {
        /* TODO: GET mem, GET cursor */
        bitset_init(mw->mem, mw->cursor);
        mw->prev = 0;
        return;
    }

    /* sa: add item to bitset */
    if (iscmd(mw, c, "sa")) {
        /* TODO: GET mem, GET cursor, GET rw */
        bitset_add(mw->mem, mw->cursor, mw->rw);
        mw->prev = 0;
        return;
    }

    /* se: check to see if item exists in set */
    if (iscmd(mw, c, "se")) {
        /* TODO: GET mem, GET cursor, GET rw */
        mw->rw = bitset_exists(mw->mem, mw->cursor, mw->rw);
        mw->prev = 0;
        return;
    }

    /* sr: remove item from bitset */
    if (iscmd(mw, c, "sr")) {
        /* TODO: GET mem, GET cursor, GET rw */
        bitset_remove(mw->mem, mw->cursor, mw->rw);
        mw->prev = 0;
        return;
    }

    /* rb: read word from block */
    if (iscmd(mw, c, "rb")) {
        int offset;
        mw->prev = 0;
        /* 6-bit address 0 - 63 */
        /* TODO: GET rw */
        offset = mw->rw & 0x3f;
        /* TODO: READ word, GET cursor */
        mw->rw = mw->mem[mw->cursor + offset];
        return;
    }

    /* mi: initialize memory */
    if (iscmd(mw, c, "mi")) {
        uint32_t *stk;
        uint16_t p_top, p_block, p_avail, p_tags;
        uint8_t sp;
        mw->prev = 0;

        /* TODO: GET mem, GET cursor */
        stk = &mw->mem[mw->cursor];
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

        mem_init(mw->mem, p_top, p_block, p_avail, p_tags);
        return;
    }

    /* gb: goto block */
    /* TODO: replace with bm go */
    if (iscmd(mw, c, "gb")) {
        /* multiply by 64 to get word address,
         * then add an offset to skip the block list
         * block list size: 2^16/64 = 1024. 1024 blocks = 1024 words
         */
        /* TODO: SET cursor, GET rw */
        mw->cursor = old_block_to_word(mw->rw); 
        mw->prev = 0;
        return;
    }

    /* ma: allocate a block */
    if (iscmd(mw, c, "ma")) {
        uint8_t sp;
        uint32_t *stk;
        uint16_t p_top, k;
        mw->prev = 0;

        /* TODO: GET cursor, GET mem */
        stk = &mw->mem[mw->cursor];
        sp = stk[0];

        if (sp < 2) {
            mw->err = 2;
            return;
        }
        p_top = stk[sp]; sp--;
        k = stk[sp]; sp--;
        stk[0] = sp;

        /* TODO: SET rw, GET mem */
        mw->rw = mem_alloc(mw->mem, p_top, k);
        /* check for out of bounds results */
        /* TODO: GET rw, SET err */
        mw->err = mw->rw > 63;
        return;
    }

    /* mf: free a block */
    if (iscmd(mw, c, "mf")) {
        uint8_t sp;
        uint32_t *stk;
        uint16_t p_top, L, k;
        mw->prev = 0;

        /* TODO: GET mem, GET cursor */
        stk = &mw->mem[mw->cursor];
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

        /* TODO: GET mem */
        mem_free(mw->mem, p_top, L, k);
        return;
    }

    /* pw: print word */
    if (iscmd(mw, c, "pw")) {
        printf("%x", mw->rw);
        mw->prev = 0;
        return;
    }

    /* ai: initialize array */
    if (iscmd(mw, c, "ai")) {
        mw->prev = 0;
        /* TODO: GET mem, GET cursor */
        barray_init(mw->mem, mw->cursor);
        return;
    }

    /* al: get array length */
    if (iscmd(mw, c, "al")) {
        mw->prev = 0;
        /* TODO: GET mem, GET cursor */
        mw->rw = barray_length(mw->mem, mw->cursor);
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
        mw->prev = 0;
        /* TODO: SET err, GET mem, GET cursor, GET rw */
        mw->err = barray_append(mw->mem, mw->cursor, mw->rw);
        return;
    }

    /* ap: pop word from array */
    if (iscmd(mw, c, "ap")) {
        /* TODO: GET mem, GET cursor, GET rw, SET err */
        mw->err = barray_pop(mw->mem, mw->cursor, &mw->rw);
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

        /* TODO: GET rw */
        addr = mw->rw;
        /* TODO: SET rw */
        mw->rw = (addr >> 8) * 64 + (addr & 63);

        /* see: 'gb' */
        /* TODO: SET/GET rw */
        mw->rw += BLOCKLIST_OFFSET;

        return;
    }

    /* rc: read cursor into rw */
    if (iscmd(mw, c, "rc")) {
        /* TODO: SET rw, GET cursor */
        mw->rw = mw->cursor;
        mw->prev = 0;
        return;
    }

    /* mc: generate 32-bit checksum of memory allocator */
    if (iscmd(mw, c, "mc")) {
        /* TODO: GET rw, GET mem, SET rw */
        mw->rw = mem_cksum(mw->mem, mw->rw);
        mw->prev = 0;
        return;
    }

    /* bo: calculate block offset from word address */
    if (iscmd(mw, c, "bo")) {
        mw->prev = 0;
        /* assuming blocks are aligned, last 6 bits
         * should be the offset
         */
        /* TODO: SET rw, GET rw */
        mw->rw &= 63; 
        return;
    }

    /* ad: add two numbers from a stack, store in word register */
    if (iscmd(mw, c, "ad")) {
        uint8_t sp;
        uint32_t *stk;
        uint16_t x, y;
        mw->prev = 0;

        /* TODO: GET mem (at cursor) */
        stk = &mw->mem[mw->cursor];
        sp = stk[0];

        if (sp < 2) {
            mw->err = 1;
            return;
        }

        y = stk[sp]; sp--;
        x = stk[sp]; sp--;

        /* TODO: set rw */
        mw->rw = x + y;

        stk[0] = sp;
        return;
    }

    /* zp: initialize zero page */
    if (iscmd(mw, c, "zp")) {
        mw->prev = 0;
        /* TODO: GET mem, GET cursor, GET rw, SET rw */
        mw->rw = zero_page_init(mw->mem, mw->cursor, mw->rw);
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

        /* GET rw */
        rw = mw->rw;
        off = val = sz = 0;

        val = rw & 0xFFFF;
        rw >>= 16;
        sz = rw & 0xF;
        rw >>= 4;
        off = rw & 0xFFF;

        /* GET mem, GET cursor */
        bits_set(mw->mem, (mw->cursor << 4) + off, sz, val);

        return;
    }

    /* xr: */

    if (iscmd(mw, c, "xr")) {
        uint32_t rw;
        uint16_t off;
        uint8_t sz;
        mw->prev = 0;

        /* TODO: GET rw */
        rw = mw->rw;
        off = sz = 0;

        sz = rw & 0xF;
        rw >>= 4;
        off = rw & 0xFFF;

        /* TODO: GET mem, GET cursor */
        mw->rw = bits_get(mw->mem, (mw->cursor << 4) + off, sz);

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
        mw->prev = 0;
        /* TODO: SET rw, GET cursor, GET rw */
        mw->rw = block_to_word(mw->cursor, mw->rw);
        return;
    }

    /* mb: memory address to block */
    if (iscmd(mw, c, "mb")) {
        mw->prev = 0;
        /* TODO: GET cursor, GET rw, SET rw */
        mw->rw = word_to_block(mw->cursor, mw->rw);
        return;
    }

    /* jf: jump forward */
    if (iscmd(mw, c, "jf")) {
        int jump;
        mw->prev = 0;
        /* TODO: GET rw */
        jump = mw->rw & 0xFF;
        /* TODO: SET rw */
        mw->rw >>= 8;
        /* TODO GET cursor, SET CURSOR */
        mw->cursor += jump;
        return;
    }

    /* jb: jump backward */
    if (iscmd(mw, c, "jb")) {
        int jump;
        mw->prev = 0;
        /* TODO: GET cursor */
        jump = mw->rw & 0xFF;
        /* TODO: SET rw */
        mw->rw >>= 8;
        /* TODO: SET cursor */
        mw->cursor -= jump;
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

        mw->prev = 0;
        /* TODO: GET cursor */
        lsb = mw->cursor;
        /* TODO: GET rw */
        msb = mw->rw & 0xFFFF;

        /* TODO: SET rw */
        mw->rw = (msb << 16) | lsb;

        return;
    }
    
    /* sn: number of elements in bitset */
    if (iscmd(mw, c, "sn")) {
        mw->prev = 0;

        /* TODO: SET rw */
        mw->rw = bitset_len(mw->mem, mw->cursor);
        return;
    }

    /* bb: allocate and mark */
    if (iscmd(mw, c, "bb")) {
        uint16_t lsb, msb;
        int blk;

        mw->prev = 0;
        /* TODO: GET rw */
        lsb = mw->rw & 0xFFFF;
        /* TODO: GET rw */
        msb = mw->rw >> 16;

        /* TODO: GET mem */
        blk = blocklist_pop(mw->mem, lsb);

        if (blk <= 0) {
            /* TODO: SET err */
            mw->err = 1;
            return;
        }

        bitset_add(mw->mem, msb, blk);

        mw->rw = blk;

        return;
    }

    /* ex: execute command */
    if (iscmd(mw, c, "ex")) {
        /* TODO: GET rw, GET rw (pointer), GET mem, GET instr (pointer) */
        mw->err = instr_ex(mw->mem,
                &mw->instr,
                mw->rw,
                &mw->rw);

        return;
    }
   
    /* bx: execute block */
    if (iscmd(mw, c, "bx")) {
        mw->prev = 0;
        /* TODO: GET mem, GET instr (pointer), GET cursor, GET rw (ptr) */
        mw->err = instr_block(mw->mem, &mw->instr, mw->cursor, &mw->rw);
        return;
    }

    /* ri: initialize rfcnt */
    if (iscmd(mw, c, "ri")) {
        mw->prev = 0;
        /* TODO: GET mem, GET cursor */
        rc_init(mw->mem, mw->cursor);
        return;
    }

    /* ra: refcnt add */
    if (iscmd(mw, c, "ra")) {
        mw->prev = 0;
        /* TODO: GET mem, GET cursor, GET rw */
        rc_add(mw->mem, mw->cursor, mw->rw);
        return;
    }

    /* rm: refcnt remove */
    if (iscmd(mw, c, "rm")) {
        mw->prev = 0;
        /* TODO: GET mem, GET cursor, GET rw */
        rc_del(mw->mem, mw->cursor, mw->rw);
        return;
    }

    /* rx: refcnt aux */
    if (iscmd(mw, c, "rx")) {
        int mode;

        mw->prev = 0;

        /* TODO: GET rw */
        mode = mw->rw & 0xF;
        /* TODO: SET rw */
        mw->rw >>= 4;

        if (mode == 0) {
            /* TODO: GET rw, GET mem, GET cursor */
            mw->rw = rc_length(mw->mem, mw->cursor);
            return;
        } else if (mode == 1) {
            /* TODO: GET mem, GET rw, GET cursor, SET rw */
            mw->rw = rc_get_count(mw->mem, mw->cursor, mw->rw);
            return;
        } else if (mode == 2) {
            /* TODO: GET mem, GET cursor, GET rw */
            mw->rw = rc_get_hold(mw->mem, mw->cursor, mw->rw);
            return;
        } else if (mode == 3) {
            /* TODO: GET mem, GET cursor */
            mw->rw = rc_get_active(mw->mem, mw->cursor);
            return;
        }
        return;
    }

    /* rf: refcount find */
    if (iscmd(mw, c, "rf")) {
        mw->prev = 0;
        /* TODO: GET mem, GET cursor, GET rw, SET rw */
        mw->rw = rc_find(mw->mem, mw->cursor, mw->rw);
        return;
    }
    
    /* ru: refcount update */
    if (iscmd(mw, c, "ru")) {
        int mode;

        mw->prev = 0;
        /* TODO: GET rw */
        mode = mw->rw & 0xF;
        /* TODO: set RW */
        mw->rw >>= 4;

        if (mode) {
            /* decrement */
            /* TODO: GET rw, GET mem, GET cursor, SET rw */
            mw->rw = rc_decr(mw->mem, mw->cursor, mw->rw);
        } else {
            /* increment */
            /* TODO: SET rw, GET mem, GET cursor, GET rw */
            mw->rw = rc_incr(mw->mem, mw->cursor, mw->rw);
        }
        return;
    }

    /* rh: refcount hold/unhold */
    if (iscmd(mw, c, "rh")) {
        int mode;

        mw->prev = 0;
        /* TODO: GET RW */
        mode = mw->rw & 0xF;
        /* TODO: SET RW */
        mw->rw >>= 4;

        if (mode) {
            /* unhold */
            /* TODO: GET mem, GET cursor, GET rw */
            rc_unhold(mw->mem, mw->cursor, mw->rw);
        } else {
            /* hold */
            /* TODO: GET mem, GET cursor, GET rw */
            rc_hold(mw->mem, mw->cursor, mw->rw);
        }
        return;
    }

    /* rs: refcount sweep */
    if (iscmd(mw, c, "rs")) {
        mw->prev = 0;
        /* TODO: GET mem, GET cursor */
        rc_sweep(mw->mem, mw->cursor);
        return;
    }
    
    /* rg: refcount get */
    if (iscmd(mw, c, "rg")) {
        mw->prev = 0;
        /* TODO: GET rw, GET mem, GET cursor */
        mw->rw = rc_get(mw->mem, mw->cursor);
        return;
    }

    /* pi: pstack init */
    if (iscmd(mw, c, "pi")) {
        mw->prev = 0;
        /* TODO: GET mem, GET cursor */
        pstack_init(mw->mem, mw->cursor);
        return;
    }

    /* pu: pstack push */
    if (iscmd(mw, c, "pu")) {
        int type;
        int data;
        mw->prev = 0;

        /* TODO: GET rw */
        type = mw->rw & 0xF;
        /* TODO: SET rw */
        mw->rw >>= 4;
        /* TODO: GET rw */
        data = mw->rw;
        /* TODO: SET err, GET mem, GET cursor */
        mw->err =
            pstack_push(mw->mem,
                mw->cursor,
                pstack_param(type, data));

        return;
    }

    /* po: pstack pop */
    if (iscmd(mw, c, "po")) {
        mw->prev = 0;
        /* TODO: SET err, GET mem, GET cursor, GET rw (ptr) */
        mw->err = pstack_pop(mw->mem, mw->cursor, &mw->rw);
        return;
    }

    /* pd: pstack dup */
    if (iscmd(mw, c, "pd")) {
        mw->prev = 0;
        /* TODO: GET err, GET mem, GET cursor */
        mw->err = pstack_dup(mw->mem, mw->cursor);
        return;
    }

    /* pR: pstack rot */
    if (iscmd(mw, c, "pR")) {
        mw->prev = 0;
        /* SET err, GET mem, GET cursor */
        mw->err = pstack_rot(mw->mem, mw->cursor);
        return;
    }

    /* ph: pstack hold */
    if (iscmd(mw, c, "ph")) {
        int which;
        mw->prev = 0;
        /* TODO: GET rw */
        which = mw->rw & 0xF;
        /* TODO: SET rw */
        mw->rw >>= 4;
        if (which == 0) {
            /* TODO: SET err, GET mem, GET cursor */
            mw->err = pstack_hold(mw->mem, mw->cursor);
        } else if (which == 1) {
            /* TODO: GET mem, GET cursor, SET err */
            mw->err = pstack_unhold(mw->mem, mw->cursor);
        }
        return;
    }

    /* ps: pstack swap */
    if (iscmd(mw, c, "ps")) {
        mw->prev = 0;
        /* TODO: SET err, GET mem, GET cursor */
        mw->err = pstack_swap(mw->mem, mw->cursor);
        return;
    }

    /* px: pstack aux */
    if (iscmd(mw, c, "px")) {
        int type;

        mw->prev = 0;
        /* TODO: GET rw */
        type = mw->rw & 0xF;
        /* TODO: SET rw */
        mw->rw >>= 4;

        if (type == 0) {
            /* extract data component from param word */
            /* TODO: SET rw */
            mw->rw >>= 2;
            return;
        }

        return;
    }

    /* mx: memory aux */
    if (iscmd(mw, c, "mx")) {
        mw->prev = 0;
        /* TODO: set rw, GET mem, GET cursor */
        mw->rw = mem_aux(mw->mem, mw->cursor);

        return;
    }

    /* ws: wordslice to value */
    if (iscmd(mw, c, "ws")) {
        uint32_t ws;
        uint16_t addr, start, end;
        mw->prev = 0;
        ws = 0;

        /* extract arguments from RW register */
        /* TODO: GET rw */
        end = mw->rw & 0xFF;
        /* TODO: SET rw */
        mw->rw >>= 8;
        /* TODO: GET rw */
        start = mw->rw & 0xFF;
        /* TODO: SET rw */
        mw->rw >>= 8;
        /* TODO: GET rw */
        addr = mw->rw & 0xFFFF;

        /* build up word slice */
        ws = addr |
            ((start & 0x1f) << 16) |
            ((end & 0x1f) << 21);

        /* TODO: SET rw, GET mem */
        mw->rw = array_value(mw->mem, ws);
    }

    /* ci: context init */
    if (iscmd(mw, c, "ci")) {
        uint16_t ctx;
        int rc;
        mw->prev = 0;
        /* TODO: GET mem, GET cursor */
        ctx = context_init(mw->mem, mw->cursor);
    
        /* extra context goodies */

        /* TODO: GET mem */
        rc = context_allocator_setup(mw->mem, ctx);
        if (rc) {
            /* TODO: SET err */
            mw->err = 1;
            return;
        }

        /* TODO: GET mem */
        rc = context_pstack_setup(mw->mem, ctx);

        if (rc) {
            /* TODO: SET err */
            mw->err = 2;
            return;
        }

        /* TODO: SET err */
        mw->err = 0;

        /* TODO: SET rw */
        mw->rw = ctx;

        return;
    }

    /* sw: swap */
    if (iscmd(mw, c, "sw")) {
        mw->prev = 0;
        /* TODO: GET mem, GET cursor, SET err */
        mw->err = barray_swap(mw->mem, mw->cursor);
        return;
    }

    /* ac: array create */
    if (iscmd(mw, c, "ac")) {
        mw->prev = 0;
        /* TODO: SET err, GET mem, GET cursor */
        mw->err = array_create(mw->mem, mw->cursor);
        return;
    }

    /* ar: array read */
    if (iscmd(mw, c, "ar")) {
        mw->prev = 0;
        /* TODO: SET err, GET mem, GET cursor */
        mw->err = array_read(mw->mem, mw->cursor);
        return;
    }

    /* av: array value */
    if (iscmd(mw, c, "av")) {
        mw->prev = 0;
        /* TODO: SET rw, GET mem, GET rw */
        mw->rw = array_value(mw->mem, mw->rw);
        return;
    }

    /* aw: array write */
    if (iscmd(mw, c, "aw")) {
        mw->prev = 0;
        /* TODO: SET err, GET mem, GET cursor */
        mw->err = array_write(mw->mem, mw->cursor);
        return;
    }

    /* cu: select cursor */
    /* TODO: select cursor? */
    if (iscmd(mw, c, "cu")) {
        int cur;
        mw->prev = 0;
        /* TODO: GET rw */
        cur = mw->rw & 1;
        /* TODO: SET rw */
        mw->rw >>= 4;

        if (cur) {
            /* want: cursor 1 */
            /* swap if unswapped to make cursor 1 active */
            if (!mw->swap) swap_cursors(mw);
        } else {
            /* want: cursor 0 */
            /* if swapped, swap to get cursor 0 active */
            if (mw->swap) swap_cursors(mw);
        }

        return;
    }

    /* dr: stack drop */
    if (iscmd(mw, c, "dr")) {
        mw->prev = 0;
        /* TODO: GET mem, GET cursor, SET err */
        mw->err = barray_drop(mw->mem, mw->cursor);
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

        /* TODO: GET mem */
        mem = mw->mem;

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

        mem = mw->mem;

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
    memwrite_init(mw);
    mw->vm = malloc(sonilo_vm_sizeof());
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
