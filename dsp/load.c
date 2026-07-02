#include "sonilo.h"

int ugen_sink(sonilo *s);
int ugen_sine(sonilo *s);
int ugen_arith(sonilo *s);
int ugen_blsaw(sonilo *s);
int ugen_butterworth(sonilo *s);
int ugen_clock(sonilo *s);
int ugen_metro(sonilo *s);
int ugen_seq(sonilo *s);
int ugen_mtof(sonilo *s);
int ugen_smoother(sonilo *s);
int ugen_bigverb(sonilo *s);

/* top-level loader for ugen subroutines */
int sonilo_load_ugens(sonilo *s)
{
    int rc;
    int err;

    err = 1;

    rc = ugen_sink(s); if (rc) return err; err++;
    rc = ugen_sine(s); if (rc) return err; err++;
    rc = ugen_arith(s); if (rc) return err; err++;
    rc = ugen_blsaw(s); if (rc) return err; err++;
    rc = ugen_butterworth(s); if (rc) return err; err++;
    rc = ugen_clock(s); if (rc) return err; err++;
    rc = ugen_metro(s); if (rc) return err; err++;
    rc = ugen_seq(s); if (rc) return err; err++;
    rc = ugen_mtof(s); if (rc) return err; err++;
    rc = ugen_smoother(s); if (rc) return err; err++;
    rc = ugen_bigverb(s); if (rc) return err; err++;

    return 0;
}
