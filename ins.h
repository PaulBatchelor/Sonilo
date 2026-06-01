#ifndef INS_H
#define INS_H
/* squished into remaining space of system megablock */
#define MAX_INSTR 251

typedef struct {
    uint32_t *key;
    instr_func *func;
    uint32_t *nent;
} instr_map;

void instr_map_set(instr_map *map, uint16_t key, instr_func func);
instr_func instr_map_get(instr_map *map, uint16_t key);
int instr_char_sym(char c);
int instr_sym_char(int sym);
uint16_t instr_key(const char *str);
int instr_ex(uint32_t *mem, instr_map *map, uint32_t i, uint32_t *rw);
int instr_block(uint32_t *mem, instr_map *map, uint16_t p, uint32_t *rw);
int instr_map_index(instr_map *map, uint16_t key);
instr_func instr_map_entry(instr_map *map, int ent);
#endif
