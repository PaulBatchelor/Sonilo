enum {
    TAPE_INACTIVE,
    TAPE_WRITE,
    TAPE_READ
};

typedef struct tape_track {
    int id;
    sk_drwav wav;
    uint16_t sink;
    int rw;
} tape_track;

/* initialize the track data structure */
void tape_track_init(tape_track *trk, int id);

/* open / close tracks */
int tape_track_open(tape_track *trk);
int tape_track_close(tape_track *trk);

/* process block of audio */
int tape_track_process(tape_track *trk, uint32_t *mem);

/* bind sink to track */
int tape_track_bind(tape_track *trk, uint16_t sink);
