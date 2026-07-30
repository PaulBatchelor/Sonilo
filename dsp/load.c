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
int ugen_env(sonilo *s);
int ugen_rephasor(sonilo *s);
int ugen_curve(sonilo *s);
int ugen_terp(sonilo *s);
int ugen_bez(sonilo *s);
int ugen_phasor(sonilo *s);
int ugen_warp(sonilo *s);
int ugen_scale(sonilo *s);
int ugen_vib(sonilo *s);
int ugen_powerwave(sonilo *s);
int ugen_formant(sonilo *s);
int ugen_dcblk(sonilo *s);
int ugen_glot(sonilo *s);

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
    rc = ugen_env(s); if (rc) return err; err++;
    rc = ugen_rephasor(s); if (rc) return err; err++;
    rc = ugen_curve(s); if (rc) return err; err++;
    rc = ugen_terp(s); if (rc) return err; err++;
    rc = ugen_bez(s); if (rc) return err; err++;
    rc = ugen_phasor(s); if (rc) return err; err++;
    rc = ugen_warp(s); if (rc) return err; err++;
    rc = ugen_scale(s); if (rc) return err; err++;
    rc = ugen_vib(s); if (rc) return err; err++;
    rc = ugen_powerwave(s); if (rc) return err; err++;
    rc = ugen_formant(s); if (rc) return err; err++;
    rc = ugen_dcblk(s); if (rc) return err; err++;
    rc = ugen_glot(s); if (rc) return err; err++;

    return 0;
}
