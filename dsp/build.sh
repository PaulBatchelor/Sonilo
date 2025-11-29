gcc -std=c89 -Wall -pedantic \
    -I ../ \
    -g \
    -o test_dsp \
    phasor.c \
    blsaw.c \
    util.c \
    ../mem.c \
    test_dsp.c \
    -l m
