#include <stdio.h>
#include "sonilo.h"

static int a_array(sonilo_vm *vm, uint8_t data)
{
    int rc;
    rc = -1;
    switch (data) {
        case 0:
            /* TODO: array create */
            break;
        case 1:
            /* TODO: array write */
            break;
        default:
            rc = -1;
    }
    return rc;
}

static int a_iter(sonilo_vm *vm, uint8_t data)
{
    int rc;
    rc = -1;
    switch (data) {
        case 0:
            /* TODO: create array iterator */
            break;
         default:
            rc = -1;
    }
    return rc;
}

static int a_ugen(sonilo_vm *vm, uint32_t data)
{
    int rc;
    rc = -1;

    switch (data) {
        case 0:
            /* TODO: last ugen */
            break;
        default:
            rc = -1;
            break;
    }
    return rc;
}

static int b_ugen(sonilo_vm *vm, uint8_t data)
{
    /* TODO: implement */
    return 1;
}

static int b_const(sonilo_vm *vm, uint32_t data)
{
    /* TODO: implement */
    return 1;
}

static int b_word(sonilo_vm *vm, uint32_t data)
{
    /* TODO: implement */
    return 1;
}

static int a_ctx(sonilo_vm *vm, uint32_t data)
{
    /* TODO: implement */
    return 1;
}

int sonilo_op_a(sonilo_vm *vm, char type, uint8_t data)
{
    int rc;

    rc = 0;
    switch (type) {
        case '!': /* print rw */
            printf("%x\n", sonilo_vm_rw_get(vm));
            break;
        case 'a': /* array */
            rc = a_array(vm, data);
            break;
        case 'i': /* iterator */
            rc = a_iter(vm, data);
            break;
        case 'u': /* ugens */
            rc = a_ugen(vm, data);
            break;
        case 'C': /* create context */
            rc = a_ctx(vm, data);
            break;
        default:
            rc = -1;
            break;
    }

    return rc;
}

int sonilo_op_b(sonilo_vm *vm, char type, uint32_t data)
{
    int rc;

    rc = 0;
    switch (type) {
        case '!':
            /* set rw */
            sonilo_vm_rw_set(vm, data);
            break;
        case 'u':
            rc = b_ugen(vm, data);
            break;
        case 'w':
            rc = b_word(vm, data);
            break;
        case 'c':
            rc = b_const(vm, data);
            break;
        default:
            rc = 1;
            break;
    }

    return rc;
}
