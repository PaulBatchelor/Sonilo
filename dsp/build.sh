gcc -std=c89 -Wall -pedantic \
    -I ../ \
    -g \
    -o test_dsp \
    phasor.c \
    util.c \
    ../mem.c \
    test_dsp.c
