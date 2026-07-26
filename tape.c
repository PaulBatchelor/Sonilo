#include <stdint.h>
#include "sonilo.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include "tape.h"
#include "ugen.h"

void tape_track_init(tape_track *trk, int id)
{
    trk->id = id;
    trk->rw = TAPE_INACTIVE;
    trk->sink = 0;
}

int tape_track_open(tape_track *trk)
{
    char filename[8];
    sk_drwav_data_format format;

    trk->rw = TAPE_WRITE;

    format.container = sk_drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = 1;
    /* It would be nice to not hard-code the samplerate? */
    format.sampleRate = 44100;
    format.bitsPerSample = 32;

    sprintf(filename, "%02d.wav", trk->id);

    sk_drwav_init_file_write(&trk->wav, filename, &format, NULL);

    return 0;
}

int tape_track_close(tape_track *trk)
{
    if (trk->rw == TAPE_INACTIVE) return 1;
    sk_drwav_uninit(&trk->wav);
    trk->rw = TAPE_INACTIVE;
    return 0;
}

int tape_track_process(tape_track *trk, uint32_t *mem)
{
    float *out;
    uint16_t *blk;

    blk = (uint16_t *)ugen_state(mem, trk->sink);
    out = (float *)&mem[*blk];
    sk_drwav_write_pcm_frames(&trk->wav, 64, out);
    return 0;
}

int tape_track_bind(tape_track *trk, uint16_t sink)
{
    trk->sink = sink;
    return 0;
}
