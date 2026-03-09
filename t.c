#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "mem.h"
#include "ins.h"
#include "ugen.h"

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
    /* word register */
    uint32_t rw; 
    /* previous character */
    char prev;
    /* cursor */
    uint16_t cursor;
    /* alt cursor */
    uint16_t alt;

    /* memory */
    uint32_t mem[0x10000];

    /* errors */
    uint32_t err;

    /* instruction command lookup */
    instr_map instr;

    /* 5-bit encoding mode */
    int encode;
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

void memwrite_init(memwrite *mw)
{
    uint32_t i;
    mw->rw = 0;
    mw->prev = 0;
    for (i = 0; i < 0x10000; i++) mw->mem[i] = 0;
    mw->err = 0;
    mw->alt = 0;
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
        w = mw->rw;
        x = (uint8_t)(c - '0');
        w <<= 4;
        w |= x;
        mw->rw = w;
        return;
    } else if (c >= 'A' && c <= 'F') {
        uint8_t x;
        uint32_t w;
        w = mw->rw;
        x = (uint8_t)(c - 'A');
        x += 10;
        w <<= 4;
        w |= x;
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
        mw->rw = reverse_nibbles(mw->rw); 
        mw->prev = 0;
        return;
    }

    /* cl or '#': clear the word register */
    if (c == '#' || (mw->prev == 'c' && c == 'l')) {
        mw->rw = 0;
        mw->prev = 0;
        return;
    }

    /* go: set cursor location */
    if (iscmd(mw, c, "go")) {
        mw->prev = 0;
        mw->cursor = mw->rw & 0xFFFF;
        return;
    }

    /* wr: write word to memory */
    if (iscmd(mw, c, "wr")) {
        mw->mem[mw->cursor] = mw->rw;
        mw->prev = 0;
        return;
    }

    /* read word from memory to word register */
    if (iscmd(mw, c, "rd")) {
        mw->rw = mw->mem[mw->cursor];
        mw->prev = 0;
        return;
    }

    /* bi: init blocklist */
    if (iscmd(mw, c, "bi")) {
        blocklist_init(mw->mem, mw->cursor);
        mw->prev = 0;
        return;
    }

    /* ba: allocate block, write block id to word register */
    if (iscmd(mw, c, "ba")) {
        mw->rw = blocklist_pop(mw->mem, mw->cursor);
        if (mw->rw == 0) mw->err = 1;
        mw->prev = 0;
        return;
    }

    /* bf: free block, read block id from register word */
    if (iscmd(mw, c, "bf")) {
        mw->err = blocklist_push(mw->mem, mw->cursor, mw->rw & 0x3ff);
        mw->prev = 0;
        return;
    }

    /* si: initialize bitset */
    if (iscmd(mw, c, "si")) {
        bitset_init(mw->mem, mw->cursor);
        mw->prev = 0;
        return;
    }

    /* sa: add item to bitset */
    if (iscmd(mw, c, "sa")) {
        bitset_add(mw->mem, mw->cursor, mw->rw);
        mw->prev = 0;
        return;
    }

    /* se: check to see if item exists in set */
    if (iscmd(mw, c, "se")) {
        mw->rw = bitset_exists(mw->mem, mw->cursor, mw->rw);
        mw->prev = 0;
        return;
    }

    /* sr: remove item from bitset */
    if (iscmd(mw, c, "sr")) {
        bitset_remove(mw->mem, mw->cursor, mw->rw);
        mw->prev = 0;
        return;
    }

    /* rb: read word from block */
    if (iscmd(mw, c, "rb")) {
        int offset;
        mw->prev = 0;
        /* 6-bit address 0 - 63 */
        offset = mw->rw & 0x3f;
        mw->rw = mw->mem[mw->cursor + offset];
        return;
    }

    /* mi: initialize memory */
    if (iscmd(mw, c, "mi")) {
        uint32_t *stk;
        uint16_t p_top, p_block, p_avail, p_tags;
        uint8_t sp;
        mw->prev = 0;

        stk = &mw->mem[mw->cursor];
        sp = stk[0];

        if (sp < 4) {
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

        stk = &mw->mem[mw->cursor];
        sp = stk[0];

        if (sp < 2) {
            mw->err = 2;
            return;
        }
        p_top = stk[sp]; sp--;
        k = stk[sp]; sp--;
        stk[0] = sp;

        mw->rw = mem_alloc(mw->mem, p_top, k);
        /* check for out of bounds results */
        mw->err = mw->rw > 63;
        return;
    }

    /* mf: free a block */
    if (iscmd(mw, c, "mf")) {
        uint8_t sp;
        uint32_t *stk;
        uint16_t p_top, L, k;
        mw->prev = 0;

        stk = &mw->mem[mw->cursor];
        sp = stk[0];

        if (sp < 3) {
            mw->err = 1;
            return;
        }
        p_top = stk[sp]; sp--;
        k = stk[sp]; sp--;
        L = stk[sp]; sp--;
        stk[0] = sp;

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
        array_init(mw->mem, mw->cursor);
        return;
    }

    /* al: get array length */
    if (iscmd(mw, c, "al")) {
        mw->prev = 0;
        mw->rw = array_length(mw->mem, mw->cursor);
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
        mw->err = array_append(mw->mem, mw->cursor, mw->rw);
        return;
    }

    /* ap: pop word from array */
    if (iscmd(mw, c, "ap")) {
        mw->err = array_pop(mw->mem, mw->cursor, &mw->rw);
        mw->prev = 0;
        return;
    }

    /* pe: print error flag */
    if (iscmd(mw, c, "pe")) {
        printf("%x", mw->err);
        fflush(stdout);
        mw->prev = 0;
        return;
    }

    /* ce: clear error flag */
    if (iscmd(mw, c, "ce")) {
        mw->prev = 0;
        mw->err = 0;
        return;
    }

    /* bw: convert block.offset #BBBOO notation into
     * a word address */
    if (iscmd(mw, c, "bw")) {
        uint16_t addr;
        mw->prev = 0;

        addr = mw->rw;
        mw->rw = (addr >> 8) * 64 + (addr & 63);

        /* see: 'gb' */
        mw->rw += BLOCKLIST_OFFSET;

        return;
    }

    /* rc: read cursor into rw */
    if (iscmd(mw, c, "rc")) {
        mw->rw = mw->cursor;
        mw->prev = 0;
        return;
    }

    /* mc: generate 32-bit checksum of memory allocator */
    if (iscmd(mw, c, "mc")) {
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
        mw->rw &= 63; 
        return;
    }

    /* ad: add two numbers from a stack, store in word register */
    if (iscmd(mw, c, "ad")) {
        uint8_t sp;
        uint32_t *stk;
        uint16_t x, y;
        mw->prev = 0;

        stk = &mw->mem[mw->cursor];
        sp = stk[0];

        if (sp < 2) {
            mw->err = 1;
            return;
        }

        y = stk[sp]; sp--;
        x = stk[sp]; sp--;

        mw->rw = x + y;

        stk[0] = sp;
        return;
    }

    /* zp: initialize zero page */
    if (iscmd(mw, c, "zp")) {
        mw->prev = 0;
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

        rw = mw->rw;
        off = val = sz = 0;

        val = rw & 0xFFFF;
        rw >>= 16;
        sz = rw & 0xF;
        rw >>= 4;
        off = rw & 0xFFF;

        bits_set(mw->mem, (mw->cursor << 4) + off, sz, val);

        return;
    }

    if (iscmd(mw, c, "xr")) {
        uint32_t rw;
        uint16_t off;
        uint8_t sz;
        mw->prev = 0;

        rw = mw->rw;
        off = sz = 0;

        sz = rw & 0xF;
        rw >>= 4;
        off = rw & 0xFFF;

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
        mw->rw = block_to_word(mw->cursor, mw->rw);
        return;
    }

    /* mb: memory address to block */
    if (iscmd(mw, c, "mb")) {
        mw->prev = 0;
        mw->rw = word_to_block(mw->cursor, mw->rw);
        return;
    }

    /* jf: jump forward */
    if (iscmd(mw, c, "jf")) {
        int jump;
        mw->prev = 0;
        jump = mw->rw & 0xFF;
        mw->rw >>= 8;
        mw->cursor += jump;
        return;
    }

    /* jb: jump backward */
    if (iscmd(mw, c, "jb")) {
        int jump;
        mw->prev = 0;
        jump = mw->rw & 0xFF;
        mw->rw >>= 8;
        mw->cursor -= jump;
        return;
    }

    /* sc: swap cursor */
    if (iscmd(mw, c, "sc")) {
        uint16_t tmp;
        mw->prev = 0;
        tmp = mw->cursor;
        mw->cursor = mw->alt;
        mw->alt = tmp;
        return;
    }

    /* pa: pack addresses */
    if (iscmd(mw, c, "pa")) {
        uint16_t lsb, msb;

        mw->prev = 0;
        lsb = mw->cursor;
        msb = mw->rw & 0xFFFF;

        mw->rw = (msb << 16) | lsb;

        return;
    }
    
    /* sn: number of elements in bitset */
    if (iscmd(mw, c, "sn")) {
        mw->prev = 0;

        mw->rw = bitset_len(mw->mem, mw->cursor);
        return;
    }

    /* bb: allocate and mark */
    if (iscmd(mw, c, "bb")) {
        uint16_t lsb, msb;
        int blk;

        mw->prev = 0;
        lsb = mw->rw & 0xFFFF;
        msb = mw->rw >> 16;

        blk = blocklist_pop(mw->mem, lsb);

        if (blk <= 0) {
            mw->err = 1;
            return;
        }

        bitset_add(mw->mem, msb, blk);

        mw->rw = blk;

        return;
    }

    /* ex: execute command */
    if (iscmd(mw, c, "ex")) {
        mw->err = instr_ex(mw->mem,
                &mw->instr,
                mw->rw,
                &mw->rw);

        return;
    }
   
    /* bx: execute block */
    if (iscmd(mw, c, "bx")) {
        mw->prev = 0;
        mw->err = instr_block(mw->mem, &mw->instr, mw->cursor, &mw->rw);
        return;
    }

    /* ri: initialize rfcnt */
    if (iscmd(mw, c, "ri")) {
        mw->prev = 0;
        rc_init(mw->mem, mw->cursor);
        return;
    }

    /* ra: refcnt add */
    if (iscmd(mw, c, "ra")) {
        mw->prev = 0;
        rc_add(mw->mem, mw->cursor, mw->rw);
        return;
    }

    /* rm: refcnt remove */
    if (iscmd(mw, c, "rm")) {
        mw->prev = 0;
        rc_del(mw->mem, mw->cursor, mw->rw);
        return;
    }

    /* rx: refcnt aux */
    if (iscmd(mw, c, "rx")) {
        int mode;

        mw->prev = 0;

        mode = mw->rw & 0xF;
        mw->rw >>= 4;

        if (mode == 0) {
            mw->rw = rc_length(mw->mem, mw->cursor);
            return;
        } else if (mode == 1) {
            mw->rw = rc_get_count(mw->mem, mw->cursor, mw->rw);
            return;
        } else if (mode == 2) {
            mw->rw = rc_get_hold(mw->mem, mw->cursor, mw->rw);
            return;
        } else if (mode == 3) {
            mw->rw = rc_get_active(mw->mem, mw->cursor);
            return;
        }
        return;
    }

    /* rf: refcount find */
    if (iscmd(mw, c, "rf")) {
        mw->prev = 0;
        mw->rw = rc_find(mw->mem, mw->cursor, mw->rw);
        return;
    }
    
    /* ru: refcount update */
    if (iscmd(mw, c, "ru")) {
        int mode;

        mw->prev = 0;
        mode = mw->rw & 0xF;
        mw->rw >>= 4;

        if (mode) {
            /* decrement */
            mw->rw = rc_decr(mw->mem, mw->cursor, mw->rw);
        } else {
            /* increment */
            mw->rw = rc_incr(mw->mem, mw->cursor, mw->rw);
        }
        return;
    }

    /* rh: refcount hold/unhold */
    if (iscmd(mw, c, "rh")) {
        int mode;

        mw->prev = 0;
        mode = mw->rw & 0xF;
        mw->rw >>= 4;

        if (mode) {
            /* unhold */
            rc_unhold(mw->mem, mw->cursor, mw->rw);
        } else {
            /* hold */
            rc_hold(mw->mem, mw->cursor, mw->rw);
        }
        return;
    }

    /* rs: refcount sweep */
    if (iscmd(mw, c, "rs")) {
        mw->prev = 0;
        rc_sweep(mw->mem, mw->cursor);
        return;
    }
    
    /* rg: refcount get */
    if (iscmd(mw, c, "rg")) {
        mw->prev = 0;
        mw->rw = rc_get(mw->mem, mw->cursor);
        return;
    }

    /* pi: pstack init */
    if (iscmd(mw, c, "pi")) {
        mw->prev = 0;
        pstack_init(mw->mem, mw->cursor);
        return;
    }

    /* pu: pstack push */
    if (iscmd(mw, c, "pu")) {
        int type;
        int data;
        mw->prev = 0;

        type = mw->rw & 0xF;
        mw->rw >>= 4;
        data = mw->rw;
        mw->err =
            pstack_push(mw->mem,
                mw->cursor,
                pstack_param(type, data));

        return;
    }

    /* po: pstack pop */
    if (iscmd(mw, c, "po")) {
        mw->prev = 0;
        mw->err = pstack_pop(mw->mem, mw->cursor, &mw->rw);
        return;
    }

    /* pd: pstack dup */
    if (iscmd(mw, c, "pd")) {
        mw->prev = 0;
        mw->err = pstack_dup(mw->mem, mw->cursor);
        return;
    }

    /* pR: pstack rot */
    if (iscmd(mw, c, "pR")) {
        mw->prev = 0;
        mw->err = pstack_rot(mw->mem, mw->cursor);
        return;
    }

    /* ph: pstack hold */
    if (iscmd(mw, c, "ph")) {
        int which;
        mw->prev = 0;
        which = mw->rw & 0xF;
        mw->rw >>= 4;
        if (which == 0) {
            mw->err = pstack_hold(mw->mem, mw->cursor);
        } else if (which == 1) {
            mw->err = pstack_unhold(mw->mem, mw->cursor);
        }
        return;
    }

    /* ps: pstack swap */
    if (iscmd(mw, c, "ps")) {
        mw->prev = 0;
        mw->err = pstack_swap(mw->mem, mw->cursor);
        return;
    }

    /* px: pstack aux */
    if (iscmd(mw, c, "px")) {
        int type;

        mw->prev = 0;
        type = mw->rw & 0xF;
        mw->rw >>= 4;

        if (type == 0) {
            /* extract data component from param word */
            mw->rw >>= 2;
            return;
        }

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
    free(mw);
    return 0;
}
