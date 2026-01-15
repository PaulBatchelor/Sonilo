#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "mem.h"

#define BLOCKLIST_OFFSET 0x400

static volatile int running = 0;

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

    /* memory */
    uint32_t mem[0x10000];

    /* errors */
    uint32_t err;
} memwrite;

void memwrite_init(memwrite *mw)
{
    uint32_t i;
    mw->rw = 0;
    mw->prev = 0;
    for (i = 0; i < 0x10000; i++) mw->mem[i] = 0;
    mw->err = 0;
}

static uint32_t reverse_nibbles(uint32_t w)
{
    w = (w & 0x0F0F0F0F) << 4 | (w & 0xF0F0F0F0) >> 4;
    w = (w & 0x00FF00FF) << 8 | (w & 0xFF00FF00) >> 8;
    w = (w & 0x0000FFFF) << 16 | (w & 0xFFFF0000) >> 16;
    return w;
}

static uint16_t block_to_word(uint16_t b)
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
    if (iscmd(mw, c, "gb")) {
        /* multiply by 64 to get word address,
         * then add an offset to skip the block list
         * block list size: 2^16/64 = 1024. 1024 blocks = 1024 words
         */
        mw->cursor = block_to_word(mw->rw); 
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
            mw->err = 1;
            return;
        }
        p_top = stk[sp]; sp--;
        k = stk[sp]; sp--;
        stk[0] = sp;

        mw->rw = mem_alloc(mw->mem, p_top, k);
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
        fflush(stdout);
        mw->prev = 0;
        return;
    }

    /* ai: initialize array */
    if (iscmd(mw, c, "ai")) {
        int n;
        for (n = 0; n < 64; n++) mw->mem[mw->cursor + n] = 0;
        mw->prev = 0;
        return;
    }

    /* al: get array length */
    if (iscmd(mw, c, "al")) {
        /* first word in block stores length */
        mw->rw = mw->mem[mw->cursor];
        mw->prev = 0;
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
        int pos;
        mw->prev = 0;
        pos = mw->mem[mw->cursor];
        /* first word in block stores length */
        mw->mem[mw->cursor + pos + 1] = mw->rw;
        mw->mem[mw->cursor] = pos + 1;
        return;
    }

    /* ap: pop word from array */
    if (iscmd(mw, c, "ap")) {
        int pos;
        pos = mw->mem[mw->cursor];
        mw->prev = 0;
        if (pos == 0) {
            mw->err = 1;
            return;
        }
        /* first word in block stores length */
        mw->rw = mw->mem[mw->cursor + pos];
        mw->mem[mw->cursor] = pos - 1;
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
        uint16_t zp;
        uint16_t i;
        mw->prev = 0;
        /* get zero page address from rw register */
        zp = mw->rw;

        /* convert block to word */
        zp = block_to_word(zp);

        /* zero out block */
        for (i = 0; i < 64; i++) {
            mw->mem[zp + i] = 0;
        }

        /* set block stack (in cursor) to be slot 0 in zp */

        mw->mem[zp] = mw->cursor;

        /* return the zp memory address */
        mw->rw = zp;
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
        printf("\n");
        return;
    }

    /* bm: block to memory address */
    if (iscmd(mw, c, "bm")) {
        uint32_t blk;
        blk = mw->rw << 6;
        /* skip the blockstack address space */
        if (blk >= mw->cursor) {
            /* blockstack =
             * 10 bits/number * 1024 numbers /
             * (32 bits/word * 64 words/block) =
             * 5 blocks */
            blk += 5 << 6;
        }
        mw->rw = blk;
        mw->prev = 0;
        return;
    }

    /* mb: memory address to block */
    if (iscmd(mw, c, "mb")) {
        uint32_t m;
        mw->prev = 0;
        m = mw->rw;
        /* remove bias */
        if (m >= mw->cursor) {
            m -= (5 << 6);
        }
        m >>= 6;
        mw->rw = m;
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
