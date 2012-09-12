#ifndef RFP_ERRORS_H
#define RFP_ERRORS_H

#define RFPERR_RFS (1 << 30)

enum rfperr {
/* Expected duplications. */

    /* Expected: 3,5 in OF1.1 means both OFPBIC_BAD_EXPERIMENTER and
     * OFPBIC_BAD_EXP_TYPE. */

/* ## ------------------ ## */
/* ## OFPET_HELLO_FAILED ## */
/* ## ------------------ ## */

    /* OF1.0+(0).  Hello protocol failed. */
    RFPERR_RFPET_HELLO_FAILED = RFPERR_RFS,
};

#endif
