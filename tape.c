#include <stdint.h>
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include "tape.h"

void tape_track_init(tape_track *trk)
{
    /* TODO: implement */
}

int tape_track_open(tape_track *trk)
{
    /* TODO: implement */
    return 1;
}

int tape_track_close(tape_track *trk)
{
    /* TODO: implement */
    return 1;
}

int tape_track_process(tape_track *trk, uint32_t *mem)
{
    /* TODO: implement */
    return 1;
}

int tape_track_bind(tape_track *trk, uint16_t sink)
{
    /* TODO: implement */
    return 1;
}
