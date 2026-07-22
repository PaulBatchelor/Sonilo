typedef struct memwrite {
    /* previous character */
    int8_t prev;
    /* 5-bit encoding mode */
    int encode;
    sonilo *s;
    sonilo_vm *vm;
    sonilo_host *host;
} memwrite;

void parse_memwrite(memwrite *mw, char c);
void memwrite_init(memwrite *mw);
