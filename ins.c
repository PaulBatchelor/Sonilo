#include <stdlib.h>
#include <stdint.h>
#include "sonilo.h"
#include "ins.h"

void instr_map_init(instr_map *map)
{
    int i;
    for (i = 0; i < MAX_INSTR; i++) {
        map->ent[i].key = 0;
        map->ent[i].func = NULL;
    }
}

static uint16_t hash(uint16_t key) {
    /* TODO: make this hash better */
    return key % MAX_INSTR;
}

void instr_map_set(instr_map *map, uint16_t key, instr_func func)
{
    uint16_t h, i;

    h = hash(key);
    for (i = 0; i < MAX_INSTR; i++) {
        instr_entry *ent;
        ent = &map->ent[h];
        if (ent->func == NULL) {
            ent->key = key;
            ent->func = func;
            break;
        }
        h += 1;
        h %= MAX_INSTR;
    }
}

void instr_map_NEW_set(instr_map_NEW *map, uint16_t key, instr_func func)
{
    uint16_t h, i;

    h = hash(key);
    for (i = 0; i < MAX_INSTR; i++) {
        if (map->func[h] == NULL) {
            map->key[h] = key;
            map->func[h] = *func;
            break;
        }
        h += 1;
        h %= MAX_INSTR;
    }
}

instr_func instr_map_get(instr_map *map, uint16_t key)
{
    uint16_t h, i;
    h = hash(key);

    for (i = 0; i < MAX_INSTR; i++) {
        instr_entry *ent;
        ent = &map->ent[h];
        if (ent->func != NULL && ent->key == key) {
            return ent->func;
        }
        h += 1;
        h %= MAX_INSTR;
    }

    return NULL;
}

instr_func instr_map_NEW_get(instr_map_NEW *map, uint16_t key)
{
    uint16_t h, i;
    h = hash(key);

    for (i = 0; i < MAX_INSTR; i++) {
        if (map->func[h] != NULL && map->key[h] == key) {
            return map->func[h];
        }
        h += 1;
        h %= MAX_INSTR;
    }

    return NULL;
}

/* convert ASCII character into 5-bit encoding scheme */
int instr_char_sym(char c)
{
    /* case insensitive a-zA-Z mapping */
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a';
    return -1;
}

/* convert symbol into ascii character */
static const char *lookup = "ABCDEFGHIJKLMNOPQRSTUVWXYZ!*+-/=";
int instr_sym_char(int sym)
{
    if (sym < 0 || sym >= 32) return -1;
    return lookup[sym];
}

uint16_t instr_key(const char *str)
{
    int i;
    uint16_t k;

    k = 0;
    for (i = 0; i < 3; i++) {
        int b;
        b = instr_char_sym(str[i]);
        if (b < 0) continue;
        k <<= 5;
        k |= b;
    }

    /* shift the last bit to make it fit into 16 bits */
    return k << 1;
}

int instr_ex(uint32_t *mem, instr_map *map, uint32_t i, uint32_t *rw)
{
    uint16_t cmd, dat;
    instr_func f;
    uint32_t w;

    cmd = i & 0xFFFF;
    dat = i >> 16;
    f = instr_map_get(map, cmd);
    if (f == NULL) return 1;
    w = f(mem, dat);
    if (rw == NULL) return 2;
    *rw = w;
    return 0;
}

int instr_ex_NEW(uint32_t *mem, instr_map_NEW *map, uint32_t i, uint32_t *rw)
{
    uint16_t cmd, dat;
    instr_func f;
    uint32_t w;

    cmd = i & 0xFFFF;
    dat = i >> 16;
    f = instr_map_NEW_get(map, cmd);
    if (f == NULL) return 1;
    w = f(mem, dat);
    if (rw == NULL) return 2;
    *rw = w;
    return 0;
}

int instr_block(uint32_t *mem, instr_map *map, uint16_t p, uint32_t *rw)
{
    uint32_t i;
    uint32_t *blk;
    uint32_t sz;

    blk = &mem[p];
    sz = blk[0];

    for (i = 1; i <= sz; i++) {
        int rc;
        rc = instr_ex(mem, map, blk[i], rw);
        if (rc) return rc;
    }

    return 0;
}

int instr_block_NEW(uint32_t *mem, instr_map_NEW *map, uint16_t p, uint32_t *rw)
{
    uint32_t i;
    uint32_t *blk;
    uint32_t sz;

    blk = &mem[p];
    sz = blk[0];

    for (i = 1; i <= sz; i++) {
        int rc;
        rc = instr_ex_NEW(mem, map, blk[i], rw);
        if (rc) return rc;
    }

    return 0;
}

int instr_map_index(instr_map *map, uint16_t key)
{
    uint16_t h, i;
    h = hash(key);

    for (i = 0; i < MAX_INSTR; i++) {
        instr_entry *ent;
        ent = &map->ent[h];
        if (ent->func != NULL && ent->key == key) {
            return h;
        }
        h += 1;
        h %= MAX_INSTR;
    }

    return -1;
}

instr_func instr_map_entry(instr_map *map, int ent)
{
    if (ent < 0 || ent >= MAX_INSTR) return NULL;
    return map->ent[ent].func;
}
