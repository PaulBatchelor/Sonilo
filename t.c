#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "sonilo.h"

#include "mw.h"

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
