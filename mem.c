#include <stdint.h>
#include <stdio.h>
#include "mem.h"
#include "context.h"
#define AVAIL_OFFSET 64
#define LOC_AVAIL(K) (K + AVAIL_OFFSET)
#define TAGS(MEM, TOP) (MEM[TOP + 1])
#define AVAIL(MEM, TOP) (MEM[TOP] & 0xFFFF)
/* TODO: what is block again? */
#define BLOCK(MEM, TOP) ((MEM[TOP] >> 16) & 0xFFFF)
#define RC_ENTRY_SIZE 22 /* bits */
#define RC_NENTRY 48
#define RC_ADDR(E) (E & 0xFFFF)
#define NSLOTS(MEM, A) MEM[A + 1]
#define SLOT(MEM, A, S) MEM[A + 2 + S]
#define BUDSLOT_SIZE 12
#define BUDBLK_HEADER_SIZE 4

static uint32_t linkf_set(uint32_t w, int a)
{
    w &= ~(127);
    w |= (a & 127);
    return w;
}

static int linkf_get(uint32_t w)
{
    return (w) & 127;
}

static uint32_t linkb_set(uint32_t w, int a)
{
    w &= ~(127 << 7);
    w |= (a & 127) << 7;
    return w;
}

static int linkb_get(uint32_t w)
{
    return (w >> 7) & 127;
}

static uint32_t *get_avail(uint32_t *mem, uint16_t p_top)
{
    uint16_t p_avail;
    p_avail = AVAIL(mem, p_top);
    return &mem[p_avail];
}

static void availf_set_v2(uint32_t *avail, int k, int p)
{
    avail[k] = linkf_set(avail[k], p);
}

static int availf_get_v2(uint32_t *avail, int k) 
{
    return linkf_get(avail[k]);
}

static void availb_set_v2(uint32_t *avail, int k, int p)
{
    avail[k] = linkb_set(avail[k], p);
}

static int availb_get_v2(uint32_t *avail, int k) 
{
    return linkb_get(avail[k]);
}

static void availf_set(uint32_t *avail, int k, int p)
{
    availf_set_v2(avail, k, p);
}

static int availf_get(uint32_t *avail, int k) 
{
    return availf_get_v2(avail, k);
}

static void availb_set(uint32_t *avail, int k, int p)
{
    availb_set_v2(avail, k, p);
}

static int availb_get(uint32_t *avail, int k)
{
    return availb_get_v2(avail, k);
}

static void kval_set(uint32_t *mem, uint16_t p_top, int p, int k)
{
    uint32_t *block;
    uint16_t p_block;

    p_block = BLOCK(mem, p_top);
    block = &mem[p_block];
    /* TODO: what are the other 14 bits used for ? */
    block[p] &= ~(7 << 14);
    block[p] |= (k & 7) << 14;
}

static int kval_get(uint32_t *mem, uint16_t p_top, int p)
{
    uint32_t *block;
    uint16_t p_block;
    p_block = BLOCK(mem, p_top);
    block = &mem[p_block];
    return (block[p] >> 14) & 7;
}

static void tag_set(uint32_t *tags, int p, int tag)
{
    int w;
    w = 0;
    if (p >= 32) {
       w = 1;
       p -= 32;
    }

    tags[w] &= ~(1 << p);
    tags[w] |= tag << p;
}

static int tag_get(uint32_t *tags, int p)
{
    int w;
    w = 0;
    if (p >= 32) {
       w = 1;
       p -= 32;
    }
    return (tags[w] >> p) & 1;
}

static int availf_nonempty(uint32_t *avail, int k)
{
    return availf_get(avail, k) != LOC_AVAIL(k);
}

static uint32_t* get_word(uint32_t *mem, uint16_t p_top, int p)
{
    uint32_t *avail;
    uint32_t *block;
    uint32_t p_avail;
    uint32_t p_block;

    p_avail = AVAIL(mem, p_top); 
    p_block = BLOCK(mem, p_top);

    avail = &mem[p_avail];
    block = &mem[p_block];
    if (p < 64) return &block[p];
    else return &avail[p - 64];
}

/* 10-bit array routines */
static void a10set(uint32_t *mem, int a, int i, int x)
{
    bits_set(mem, (a << 5) + 10*i, 10, x);
}

static int a10get(uint32_t *mem, int a, int i)
{
    return bits_get(mem, (a << 5) + 10*i, 10);
}

void blocklist_init(uint32_t *mem, uint16_t list)
{
    int i;
    int head;
    head = 0;

    for (i = 1023; i >= 1; i--) {
        a10set(mem, list, i, head);
        head = i;
    }

    a10set(mem, list, 0, head);
}

uint16_t blocklist_pop(uint32_t *mem, uint16_t list)
{
    int head;
    int block;

    head = a10get(mem, list, 0);
    block = 0;

    if (head == 0) return 0;

    block = head;
    head = a10get(mem, list, head);
    a10set(mem, list, 0, head);
    /* mark the block as popped by pointing to itself */
    a10set(mem, list, block, block);

    return block;
}

int blocklist_push(uint32_t *mem, uint16_t list, uint16_t block)
{
    int head;

    /* bounds checking */
    if (block <= 0 || block >= 1024) return 1;

    head = a10get(mem, list, 0);

    /* make sure block has been marked as popped */
    if (a10get(mem, list, block) != block) return 2;

    /* block.next = head */
    a10set(mem, list, block, head);

    /* update head */
    a10set(mem, list, 0, block);

    return 0;
}

/* bitset: 1024 bitmap used for cache */
void bitset_init(uint32_t *mem, uint16_t bm)
{
    uint16_t i;

    /* 1024 bits requires 32 words (half a block) */
    for (i = 0; i < 32; i++) {
        mem[bm + i] = 0;
    }
}

/* add value to the set */
void bitset_add(uint32_t *mem, uint16_t bm, uint16_t val)
{
    uint8_t wpos;
    uint8_t bit;

    /* ignore out of range values */
    if (val >= 1024) return;

    /* calculate word and bit positions */
    wpos = val / 32;
    bit = val % 32;

    mem[bm + wpos] |= 1 << bit;
}

/* remove value from set */
void bitset_remove(uint32_t *mem, uint16_t bm, uint16_t val)
{
    uint8_t wpos;
    uint8_t bit;

    /* similar to add, but with AND logic */
    if (val >= 1024) return;

    wpos = val / 32;
    bit = val % 32;

    mem[bm + wpos] &= ~(1 << bit);
}

/* check to see if value exists in set */
int bitset_exists(uint32_t *mem, uint16_t bm, uint16_t val)
{
    uint8_t wpos;
    uint8_t bit;

    /* word/bit calculations */
    wpos = val / 32;
    bit = val % 32;
    return (mem[bm + wpos] & (1 << bit)) > 0;
}

int bitset_len(uint32_t *mem, uint16_t bm)
{
    uint16_t i, k;
    int len;

    len = 0;

    for (i = 0; i < 32; i++) {
        for (k = 0; k < 32; k++) {
            len += (mem[bm + i] & (1 << k)) > 0;
        }
    }

    return len;
}

void mem_init(uint32_t *mem,
              uint16_t p_top,
              uint16_t p_block,
              uint16_t p_avail,
              uint16_t p_tags)
{
    int i;
    int m;
    uint32_t *tags;
    uint32_t *avail;
    uint32_t *w;
    uint32_t *block;

    /* write virtual pointer addresses to memory */
    mem[p_top] = p_avail  | (p_block << 16);
    mem[p_top + 1] = p_tags;

    /* convert virtual pointers to memory addresses */
    block = &mem[p_block];
    avail = &mem[p_avail];
    tags = &mem[p_tags];

    m = 6;
    for (i = 0; i < 64; i++) block[i] = 0;
    for (i = 0; i < 7; i++) avail[i] = 0;

    tags[0] = tags[1] = 0;


    /* AVAILF[m] = AVAILB[m] = 0 */
    availf_set(avail, m, 0);
    availb_set(avail, m, 0);

    /* TAG(0) = 1 */
    tag_set(tags, 0, 1);
    /* KVAL(0) = m */
    kval_set(mem, p_top, 0, m);
    /* LINKF(0) = LINKB(0) = LOC(AVAIL[m]) */
    w = &block[0];
    *w = linkf_set(*w, LOC_AVAIL(m));
    *w = linkb_set(*w, LOC_AVAIL(m));

    /* AVAILF[k] = AVAILB[k] = LOC(AVAIL[k])*/
    for (i = 0; i < m; i++) {
        availf_set(avail, i, LOC_AVAIL(i));
        availb_set(avail, i, LOC_AVAIL(i));
    }
}

/* Reserve Block. TAOCP 2.5 "Buddy System" */
int mem_alloc(uint32_t *mem, uint16_t p_top, uint16_t k)
{
    int j, i;
    int L, P;
    uint32_t *tags;
    uint32_t *w;
    uint32_t *block;
    uint16_t p_tags;
    uint32_t *avail;

    p_tags = TAGS(mem, p_top);

    avail = get_avail(mem, p_top);
    tags = &mem[p_tags];
    block = &mem[BLOCK(mem, p_top)];
    /* R1: find block */

    j = -1;
    for (i = k; i <= 6; i++) {
        /* AVAILF != LOC(AVAIL(j) */
        if (availf_nonempty(avail, i)) {
            j = i;
            break;
        }
    }

    if (j < 0) {
        return -1;
    }

    /* R2: Remove from list */
    /* L <- AVAILB[j] */
    L = availb_get(avail, j);
    /* P <- LINKB(L) */
    P = linkb_get(block[L]);
    /* AVAILB[j] <- P */
    availb_set(avail, j, P);
    /* LINKF(P) <- LOC(AVAIL[j]) */
    w = get_word(mem, p_top, P);
    *w = linkf_set(*w, LOC_AVAIL(j));
    /* TAG(L) <- 0 */
    tag_set(tags, L, 0);
    kval_set(mem, p_top, L, k);

    /* R3: Split required? */
    while (j > k) {
        /* R4: Split */
        /* Decrease j by 1 */
        j--;
        /* P = L + 2^j */
        P = L + (1 << j);

        /* TAG(P) <- 1 */
        tag_set(tags, P, 1);
        /* KVAL(P) <- j */
        kval_set(mem, p_top, P, j);

        /* LINKF(P) <- LINKB(P) <- LOC(AVAIL[j]) */
        w = get_word(mem, p_top, P);
        *w = linkf_set(*w, LOC_AVAIL(j));
        *w = linkb_set(*w, LOC_AVAIL(j));

        /* AVAILF[j] <- AVAILB[j] <- P */
        availf_set(avail, j, P);
        availb_set(avail, j, P);
    }

    return L;
}

int mem_klen(uint32_t *mem, uint16_t p_top, int k)
{
    int L;
    int navail;
    uint32_t *block;
    uint32_t *avail;

    block = &mem[BLOCK(mem, p_top)];
    avail = get_avail(mem, p_top);

    navail = 0;
    L = availf_get(avail, k);

    while (L != LOC_AVAIL(k)) {
        navail++;
        L = linkf_get(block[L]);
    }

    return navail;
}

int mem_kavail(uint32_t *mem, uint16_t p_top, int k)
{
    int i;

    if (k < 0 || k > 6) return -1;

    for (i = k; i < 6; i++) {
        if (mem_klen(mem, p_top, i)) return 1;
    }

    return 0;
}

uint32_t mem_cksum(uint32_t *mem, uint16_t p_top)
{
    int k;
    int navail;
    int nwords;
    uint32_t cksum;
    int b;

    nwords = 0;
    cksum = 0;

    k = 6;
    b = 0;

    for (k = 0; k <= 6; k++) {
        navail = mem_klen(mem, p_top, k);

        nwords += navail * (1 << k);
        cksum &= ~(7 << b);
        cksum |= (navail & 7) << b;
        b += 3;
    }

    cksum &= ~(127<< b);
    cksum |= (nwords & 127) << b;

    return cksum;
}

static int find_buddy(int x, int k)
{
    int xmod;
    xmod = x % (1 << (k + 1));
    if (xmod == 0) return x + (1 << k);
    return x - (1 << k);
}

/* adapted from "liberate" function */
void mem_free(uint32_t *mem, uint16_t p_top, int L, int k)
{
    int P;
    int m;
    uint32_t *tags;
    uint32_t *w, *q;
    uint16_t p_tags;
    uint32_t *avail;

    p_tags = TAGS(mem, p_top);
    m = 6;
    tags = &mem[p_tags];
    avail = get_avail(mem, p_top);

    /* S1: is buddy available? */
    while (1) {
        /* set P <- buddy_k(L) */
        P = find_buddy(L, k);
        /* if k = m or if TAG(P) = 0, or if TAG(P) = 1 and KVAL(P) != k), goto S3 */
        if (k == m) break;
        if (tag_get(tags, P) == 0) break;
        if (tag_get(tags, P) == 1 && kval_get(mem, p_top, P) != k) break;
        /* S2: combine with buddy */
        /* remove P from the AVAIL[k] list */
        /* LINKF(LINKB(P)) <- LINKF(P) */
        q = get_word(mem, p_top, P);
        w = get_word(mem, p_top, linkb_get(*q));
        *w = linkf_set(*w, linkf_get(*q));
        /* LINKB(LINKF(P)) <- LINKB(P) */
        w = get_word(mem, p_top, linkf_get(*q));
        *w = linkb_set(*w, linkb_get(*q));

        /* k <- k + 1 */
        k = k + 1;

        /* if P < L, set L <- P. return to S1 */
        if (P < L) {
            L = P;
        }
    }

    /* S3: put on list */
    /* TAG(L) <- 1 */
    tag_set(tags, L, 1);
    /* P <- AVAILF[k] */
    P = availf_get(avail, k);
    /* LINKF(L) <- P */
    w = get_word(mem, p_top, L);
    *w = linkf_set(*w, P);
    /* LINKB(P) <- L */
    w = get_word(mem, p_top, P);
    *w = linkb_set(*w, L);
    /* KVAL(L) <- k */
    kval_set(mem, p_top, L, k);
    /* LINKB(L) <- LOC(AVAIL[k]) */
    w = get_word(mem, p_top, L);
    *w = linkb_set(*w, LOC_AVAIL(k));
    /* AVAILF[k] <- L */
    availf_set(avail, k, L);
}

void bits_set(uint32_t *mem, uint32_t off, uint32_t sz, uint32_t w)
{
    /* start/end bounds, in units of bits */
    uint32_t end;
    /* p: word pointer addresses */
    uint32_t p_a, p_b;
    /* w: temp variables for words */
    uint32_t w_a, w_b;
    /* i: bit offsets to write to */
    uint32_t i_a;
    /* n: number of bits to write for each word */
    uint32_t n_a, n_b;
    uint32_t mask;

    /* compute end addr */
    end = off + sz;

    /* word size is max */
    if (sz > 32) sz = 32;


    /* determine word bounds */
    /* TODO: okay if p_a/p_b same? */
    p_a = off / 32; /* start >>= 5 ? */
    p_b = end / 32;

    w_a = mem[p_a];
    /* write n_a bits to W_a, starting at offset i_a */
    i_a = off - p_a*32;
    /* calculate end position in relative bits */
    n_a = i_a + sz;
    /* truncate end position to word size (32) if needed */
    n_a = n_a < 32 ? n_a : 32;
    /* compute difference from start and end bounds */
    n_a = n_a - i_a;

    mask = (1 << n_a) - 1;
    w_a &= ~(mask << i_a);
    w_a |= (w & mask) << i_a;
    mem[p_a] = w_a;

    /* always read after writing previous word in case
     * they are the same address */
    w_b = mem[p_b];

    /* write n_b bits to W_b */
    n_b = sz - n_a;
    mask = (1 << n_b) - 1;

    /* always in lower "spillover" bits */
    w_b &= ~mask;
    w_b |= (w >> n_a) & mask;

    mem[p_b] = w_b;
}

uint32_t bits_get(uint32_t *mem, uint32_t off, uint32_t sz)
{
    uint32_t out;
    /* start/end bounds, in units of bits */
    uint32_t end;
    /* p: word pointer addresses */
    uint32_t p_a, p_b;
    /* w: temp variables for words */
    uint32_t w_a, w_b;
    /* i: bit offsets to write to */
    uint32_t i_a;
    /* n: number of bits to write for each word */
    uint32_t n_a, n_b;
    uint32_t mask;

    out = 0;

    /* compute end addr */
    end = off + sz;

    /* determine word bounds */
    p_a = off / 32;
    p_b = end / 32;
    i_a = off - p_a*32;

    w_a = mem[p_a];
    w_b = mem[p_b];

    n_a = i_a + sz;
    n_a = (n_a < 32) ? n_a : 32;
    n_a = n_a - i_a;

    /* append n_b bits from W_b region to output */
    n_b = sz - n_a;
    mask = (1 << n_b) - 1;
    out = w_b & mask;

    /* append n_a bits from W_a[i_a : i_a + n_a] to output */
    mask = (1 << n_a) - 1;
    out <<= n_a;
    out |= (w_a >> i_a) & mask;

    return out;
}

static void rc_entry_set(uint32_t *mem, uint16_t r, int i, uint32_t e)
{
    bits_set(mem, (r << 3) + RC_ENTRY_SIZE*i, RC_ENTRY_SIZE, e);
}

void rc_init(uint32_t *mem, uint16_t r)
{
    int i;

    for (i = 0; i < RC_NENTRY; i++) {
        rc_entry_set(mem, r, i, 0);
    }
}

static uint32_t rc_addr_set(uint32_t e, uint16_t m)
{
    e &= ~0xFFFF;
    e |= m;
    return e;
}

static uint32_t rc_entry_get(uint32_t *mem, uint16_t r, int i)
{
    return bits_get(mem, (r << 3) + RC_ENTRY_SIZE*i, RC_ENTRY_SIZE);
}

static void rc_entry_clear(uint32_t *mem, uint16_t r, int i)
{
    bits_set(mem, (r << 3) + RC_ENTRY_SIZE*i, RC_ENTRY_SIZE, 0);
}

static int rc_count_get(uint32_t e)
{
    return (e >> 16) & 0xF;
}

static uint32_t rc_count_set(uint32_t e, int c)
{
    e &= ~(0xF << 16);
    e |= c << 16;
    return e;
}

static int rc_used_get(uint32_t e)
{
    return (e >> 20) & 1;
}

static uint32_t rc_used_set(uint32_t e, int u)
{
    e &= ~(1 << 20);
    e |= (u & 1) << 20;
    return e;
}

static int rc_hold_get(uint32_t e)
{
    return (e >> 21) & 1;
}

static uint32_t rc_hold_set(uint32_t e, int u)
{
    e &= ~(1 << 21);
    e |= (u & 1) << 21;
    return e;
}

int rc_add(uint32_t *mem, uint16_t r, uint16_t m)
{
    int avail;
    uint32_t e;
    int i;
    uint32_t start;

    /* invalid address */
    if (m == 0) return 2;

    start = r << 3;

    avail = -1;
    for (i = 0; i < RC_NENTRY; i++) {
        uint32_t e = bits_get(mem,
            start + RC_ENTRY_SIZE*i,
            RC_ENTRY_SIZE);
        if (RC_ADDR(e) == 0) {
            avail = i;
            break;
        }
    }

    if (avail < 0) {
        /* no free slots found */
        return 1;
    }

    e = rc_entry_get(mem, r, avail);
   
    e = rc_addr_set(e, m);
    e = rc_count_set(e, 1);
    e = rc_used_set(e, 1);

    rc_entry_set(mem, r, avail, e);

    return 0;
}

int rc_del(uint32_t *mem, uint16_t r, uint16_t m)
{
    int i;

    i = rc_find(mem, r, m);

    /* address not found */
    if (r == 0xFF) return 1;

    rc_entry_clear(mem, r, i);

    return 0;
}

int rc_find(uint32_t *mem, uint16_t r, uint16_t m)
{
    int i;
    uint32_t start;
    int slot;

    start = r << 3;
    slot = 0xFF;

    for (i = 0; i < RC_NENTRY; i++) {
        uint32_t e = bits_get(mem,
            start + RC_ENTRY_SIZE*i,
            RC_ENTRY_SIZE);
        if (RC_ADDR(e) == m) {
            slot = i;
            break;
        }
    }

    return slot;
}

int rc_sweep(uint32_t *mem, uint16_t r)
{
    int i;
    int count;

    count = 0;

    for (i = 0; i < RC_NENTRY; i++) {
        uint32_t e = rc_entry_get(mem, r, i);
        if (rc_used_get(e) && !rc_hold_get(e) && rc_count_get(e) == 0) {
            e = rc_used_set(e, 0);
            rc_entry_set(mem, r, i, e);
            count++;
        }
    }

    return count;
}

uint16_t rc_get(uint32_t *mem, uint16_t r)
{
    int i;
    uint16_t avail;

    avail = 0;

    for (i = 0; i < RC_NENTRY; i++) {
        uint32_t e = rc_entry_get(mem, r, i);
        if (RC_ADDR(e) != 0 && !rc_used_get(e)) {
            avail = RC_ADDR(e);
            e = rc_used_set(e, 1);
            e = rc_count_set(e, 1);
            rc_entry_set(mem, r, i, e);
            break;
        }
    }

    return avail;
}

int rc_hold(uint32_t *mem, uint16_t r, uint16_t m)
{
    int i;
    uint32_t e;
    int h;
    i = rc_find(mem, r, m);

    if (i == 0xFF) return 1;

    e = rc_entry_get(mem, r, i);
    h = rc_hold_get(e);

    if (h) {
        /* already held */
        return 2;
    }

    e = rc_hold_set(e, 1);

    rc_entry_set(mem, r, i, e);

    return 0;
}

int rc_unhold(uint32_t *mem, uint16_t r, uint16_t m)
{
    int i;
    uint32_t e;
    int h;
    i = rc_find(mem, r, m);

    if (i == 0xFF) return 1;

    e = rc_entry_get(mem, r, i);
    h = rc_hold_get(e);

    if (!h) {
        /* already unheld */
        return 2;
    }

    e = rc_hold_set(e, 0);
    e = rc_count_set(e, 0);

    rc_entry_set(mem, r, i, e);

    return 0;
}

int rc_length(uint32_t *mem, uint16_t r)
{
    int i;
    uint32_t start;
    int count;

    start = r << 3;
    count = 0;

    for (i = 0; i < RC_NENTRY; i++) {
        uint32_t e = bits_get(mem,
            start + RC_ENTRY_SIZE*i,
            RC_ENTRY_SIZE);
        if (RC_ADDR(e) != 0) {
            count++;
        }
    }

    return count;
}

int rc_incr(uint32_t *mem, uint16_t r, uint16_t m)
{
    int i;
    uint32_t e;
    int c;
    i = rc_find(mem, r, m);

    if (i == 0xFF) return 1;

    e = rc_entry_get(mem, r, i);
    c = rc_count_get(e);
    e = rc_count_set(e, c + 1);
    rc_entry_set(mem, r, i, e);

    return 0;
}

int rc_decr(uint32_t *mem, uint16_t r, uint16_t m)
{
    int i;
    uint32_t e;
    int c;
    i = rc_find(mem, r, m);

    if (i == 0xFF) return 1;

    e = rc_entry_get(mem, r, i);
    c = rc_count_get(e);

    /* nothing to decrement */
    if (c <= 0) return 2;

    e = rc_count_set(e, c - 1);
    rc_entry_set(mem, r, i, e);

    return 0;
}

int rc_get_count(uint32_t *mem, uint16_t r, uint16_t m)
{
    int i;
    uint32_t e;
    i = rc_find(mem, r, m);
    if (i == 0xFF) return i;
    e = rc_entry_get(mem, r, i);
    return rc_count_get(e);
}

int rc_get_hold(uint32_t *mem, uint16_t r, uint16_t m)
{
    int i;
    uint32_t e;
    i = rc_find(mem, r, m);
    if (i == 0xFF) return i;
    e = rc_entry_get(mem, r, i);
    return rc_hold_get(e);
}

int rc_get_active(uint32_t *mem, uint16_t r)
{
    int i;
    int count;

    count = 0;

    for (i = 0; i < RC_NENTRY; i++) {
        uint32_t e = rc_entry_get(mem, r, i);
        if (rc_used_get(e)) {
            count++;
        }
    }

    return count;
}

uint16_t block_to_word(uint16_t blist, uint16_t blk)
{
    blk <<= 6;
    /* skip the blockstack address space */
    if (blk >= blist) {
        /* blockstack =
         * 10 bits/number * 1024 numbers /
         * (32 bits/word * 64 words/block) =
         * 5 blocks */
        blk += 5 << 6;
    }
    return blk;
}

uint16_t word_to_block(uint16_t blist, uint16_t wrd)
{
    /* remove bias */
    if (wrd >= blist) {
        wrd -= (5 << 6);
    }
    wrd >>= 6;
    return wrd;
}

uint16_t zero_page_init(uint32_t *mem, uint16_t blist, uint16_t zp)
{
    uint16_t i;

    /* zero out block */
    for (i = 0; i < 64; i++) {
        mem[zp + i] = 0;
    }

    /* set block stack (in cursor) to be slot 0 in zp */
    mem[zp] = blist;

    return zp;
}

void array_init(uint32_t *mem, uint16_t a)
{
    int n;
    for (n = 0; n < 64; n++) mem[a + n] = 0;
}

uint32_t array_length(uint32_t *mem, uint16_t a)
{
    /* first word in block stores length */
    return mem[a];
}

int array_append(uint32_t *mem, uint16_t a, uint32_t x)
{
    int pos;
    pos = mem[a];

    /* no room left in array */
    if (pos >= 64) return 1;

    /* first word in block stores length */
    mem[a + pos + 1] = x;
    mem[a] = pos + 1;

    return 0;
}

int array_pop(uint32_t *mem, uint16_t a, uint32_t *x)
{
    int pos;
    pos = mem[a];
    if (pos == 0) {
        return 1;
    }
    if (x != NULL) *x = mem[a + pos];
    mem[a] = pos - 1;
    return 0;
}

/* see if word address A is being used in a buddy instance */
uint16_t mem_find(uint32_t *mem, uint16_t p_top, uint16_t a)
{
    const uint32_t err = 0xF000;
    int p;
    int k;
    uint32_t *tags;

    /* check word address boundaries */
    if (a < p_top || a >= (p_top + 64)) {
        return err | 0;
    }

    /* convert address A to relative address p */
    p = a - p_top;
    k = -1;

    /* check and see if it is being used or not */
    tags = &mem[TAGS(mem, p_top)];

    /* 1 (avail) means it is not being used */
    if (tag_get(tags, p)) {
        return err | 2;
    }

    /* retrive k value */
    k = kval_get(mem, p_top, p); 

    /* TODO: confirm if address is start of block (needed?) */

    return (p & 63) | ((k & 7) << 6);

    return err | 1;
}

uint32_t mem_aux(uint32_t *mem, uint16_t pstk)
{
    uint32_t *stk;
    uint32_t mode;
    uint32_t top;
    uint32_t sp;
    const uint32_t err = 0xF0000000;

    sp = mem[pstk];
    stk = &mem[pstk + 1];

    if (sp < 2) {
        return err | 1;
    }


    mode = stk[sp - 2];
    top = stk[sp - 1];

    /* mode 0: get k value for memory address */
    if (mode == 0) {
        uint32_t L;

        if (sp < 3) {
            return err | 2;
        }

        L = stk[sp - 3];

        mem[pstk] -= 3;
        return kval_get(mem, top, L);
    }

    /* mode 1: get AVAIL tag for memory address */
    if (mode == 1) {
        uint32_t L;
        uint32_t *tags;

        if (sp < 3) {
            return err | 2;
        }

        L = stk[sp - 3];
        tags = &mem[TAGS(mem, top)];

        mem[pstk] -= 3;
        return tag_get(tags, L);
    }

    /* mode 2: get length of list of k-sized blocks */
    if (mode == 2) {
        int k;
        if (sp < 3) return err | 2;
        k = stk[sp - 3];

        if (k < 0 || k > 6) return err | 4;

        mem[pstk] -= 3;
        return mem_klen(mem, top, k);
    }

    /* mode 3: check to see if k-size is available */
    if (mode == 3) {
        int k;
        if (sp < 3) return err | 2;
        k = stk[sp - 3];

        if (k < 0 || k > 6) return err | 4;

        mem[pstk] -= 3;
        return mem_kavail(mem, top, k);
    }

    mem[pstk] -= 2;

    return err | 3;
}

/* returns all blocks in set to the block list, and clears the set */
void bitset_free(uint32_t *mem, uint16_t bm, uint16_t blist)
{
    /* TODO: iterate through each word */
    /* Find the MSBs, compute block address, and turn bits off */
}

static void zeroblock(uint32_t *m, uint16_t b)
{
    int i;
    for (i = 0; i < 64; i++) m[b + i] = 0;
}



/* initialize buddy-slot allocator */
int allocator_init(uint32_t *mem, uint16_t ctx, uint16_t *out)
{
    uint16_t a;
    uint16_t bd;
    int rc;
    a = 0;
    rc = context_mktemp(mem, ctx, &a);
    if (rc) return rc;
    zeroblock(mem, a);

    /* store context address */
    mem[a] = ctx;

    /* create initial "buddy data" block and store it
     * these will contiguously store the top-level struct, along with
     * AVAIL, and TAGS
     */
    rc = context_mktemp(mem, ctx, &bd);
    if (rc) return rc;
    zeroblock(mem, bd);
    mem[a] |= bd << 16;

    if (out != NULL) *out = a;

    return 0;
}

int allocator_alloc(uint32_t *mem, uint16_t a, uint8_t sz)
{
    int k;
    int slot, nslots, s;
    uint32_t slt;
    uint16_t stk;
    uint16_t ctx;
    uint16_t bd;

    /* ensure requested size is between 1 and 63 */
    if (sz < 1 || sz > 63) return 1;

    k = 0;
    while ((1 << k) < sz) k++;

    slot = -1;
    nslots = NSLOTS(mem, a);

    /* search for next buddy slot with space */
    for (s = 0; s < nslots; s++) {
        uint16_t sa;
        sa = SLOT(mem, a, s) & 0xFFFF;
        if (mem_kavail(mem, sa, k)) {
            slot = s;
        }
    }

    ctx = mem[a] & 0xFFFF;
    bd = (mem[a] >> 16) & 0xFFFF;

    /* instantiate new buddy allocator if nothing available */
    if (slot < 0) {
        uint16_t memblk;
        uint16_t budblk;
        int budcnt;
        int rc;
        uint16_t budtop;

        /* make sure there's a slot available */
        if (nslots >= 64) return 1;

        /* allocate block to main (this holds the interesting things) */
        memblk = 0;
        rc = context_mkblock(mem, ctx, &memblk);
        if (rc) return rc;

        /* allocate buddy "bookkeeping" block */
        budblk = 0;
        rc = context_mktemp(mem, ctx, &budblk);
        if (rc) return rc;
        /* allocate buddy data segment */
        /* examine to see if there's a free slot in current block,
         * otherwise make another block */
        budcnt = mem[bd];
        if (budcnt >= 5) {
            uint16_t tmp;
            tmp = 0;
            rc = context_mktemp(mem, ctx, &tmp);
            if (rc) return rc;
            zeroblock(mem, tmp);
            /* TODO: link to old block somehow */
            bd = tmp;
            budcnt = 0;
        }
        /* set up top struct */
        /* compute local word offset */
        budtop = budcnt * BUDSLOT_SIZE + BUDBLK_HEADER_SIZE;
        /* add global offset (start of allocator) */
        budtop += a;
        mem_init(mem,
                budtop, budblk,
                /* AVAIL */
                budtop + 4,
                /* TAGS */
                budtop + 10);

        /* store top and memory block address as pair
         * in next slot position */
        slot = nslots;
        SLOT(mem, a, slot) = (((memblk & 0xFFFF) << 16)) | (budtop & 0xFFFF);
        NSLOTS(mem, a)++;
    }

    /* push args onto stack (base, k, buddy) */
    stk = mem[ctx + 1] & 0xFFFF;
    slt = SLOT(mem, a, slot);

    /* TODO: add error checking */
    /* base: base address to apply offset to */
    array_append(mem, stk, (slt >> 16) & 0xFFFF);

    /* k value arg */
    array_append(mem, stk, k);

    /* address of buddy instance */
    array_append(mem, stk, slt & 0xFFFF);

    /* update buddy data address (it could have been changed) */
    mem[a] |= bd << 16;

    return 0;
}

uint16_t allocator_free(uint32_t *mem, uint16_t a, uint16_t m)
{
    int s;
    int nslots;
    int slot;
    uint16_t p;
    uint16_t bud;
    uint32_t w;
    int k;
    uint16_t stk;
    uint16_t ctx;

    /* find slot that could be associated with memory address */
    slot = -1;
    nslots = NSLOTS(mem, a);

    for (s = 0; s < nslots; s++) {
        uint16_t sa;
        sa = SLOT(mem, a, s) & 0xFFFF;
        if (m >= sa && m < (sa + 64)) {
            slot = s;
        }
    }

    /* memory out of bounds */
    if (slot < 0) return 1;

    /* compute local address */
    w = SLOT(mem, a, slot);
    p = m - (w & 0xFFFF);
    bud = w >> 16;

    /* TODO: I may need to do more to determine if the address
     * provided is actually the start of a memory segment or not
     */

    /* check TAGS list to determine if it can be freed */
    if (tag_get(&mem[TAGS(mem, bud)], p)) {
        /* TAGS returned 1, meaning it was already free */
        return 1;
    }

    /* find associated k value */
    k = kval_get(mem, bud, p);

    /* push results on to stack */
    ctx = mem[a] & 0xFFFF;
    stk = mem[ctx + 1] & 0xFFFF;

    /* TODO: add error checking */
    array_append(mem, stk, p);
    array_append(mem, stk, k);
    array_append(mem, stk, bud);

    return 0;
}
