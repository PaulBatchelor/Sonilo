/* squished into remaining space of system megablock */
#define MAX_INSTR 252

typedef struct {
    uint32_t key;
    /* TODO: split the C function array into its own thing */
    instr_func func;
} instr_entry;

typedef struct {
    /* TODO: make this an array of 32-bit ints
     * This will be possible once the C callback is moved
     * elsewhere.
     */
    instr_entry ent[MAX_INSTR];
    uint32_t nent;
} instr_map;

typedef struct {
    uint32_t *key;
    instr_func *func;
    uint32_t *nent;
} instr_map_NEW;

void instr_map_init(instr_map *map);
void instr_map_set(instr_map *map, uint16_t key, instr_func func);
void instr_map_NEW_set(instr_map *map, uint16_t key, instr_func func);
instr_func instr_map_get(instr_map *map, uint16_t key);
instr_func instr_map_NEW_get(instr_map *map, uint16_t key);
int instr_char_sym(char c);
int instr_sym_char(int sym);
uint16_t instr_key(const char *str);
int instr_ex_direct(uint32_t *mem, instr_map *map, uint32_t i, uint32_t *rw);
int instr_ex(uint32_t *mem, instr_map *map, uint32_t i, uint32_t *rw);
int instr_block(uint32_t *mem, instr_map *map, uint16_t p, uint32_t *rw);
int instr_map_index(instr_map *map, uint16_t key);
instr_func instr_map_entry(instr_map *map, int ent);
