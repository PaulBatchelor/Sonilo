#include "sonilo.h"

int ugen_sink(sonilo *s);
int ugen_sine(sonilo *s);
int ugen_arith(sonilo *s);
int ugen_blsaw(sonilo *s);
int ugen_butterworth(sonilo *s);

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

    return 0;
}
