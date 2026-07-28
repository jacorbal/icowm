/**
 * @file utils/safeflg.c
 *
 * @brief Implementation of safe flag management functions
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>    /* bools, true, false */
#include <stdint.h>     /* uint16_t */

/* Local includes */
#include <utils/safeflg.h>


/* Validate if a flag is within the allowed range */
bool safeflg_is_valid(uint16_t flag, uint16_t max_flags)
{
    if (max_flags == 0) {
        return false;
    }

    /* Check whether the specified flag is a valid, single-bit flag.
     * A valid flag must be a power of 2 and must be strictly less than
     * 'max_flags' (which callers pass as '1 << CLIENT_FLAG_MAX',
     * i.e., the exclusive upper bound on valid flag values). */
    return (flag != 0 && (flag & (flag - 1)) == 0 &&
            flag < max_flags);
}


/* Set (enable) the specified flag in the given flag set */
int safeflg_set(uint16_t *flags, uint16_t flag, uint16_t max_flags)
{
    if (!safeflg_is_valid(flag, max_flags)) {
        return 1;
    }

    *flags |= flag;
    return 0;
}


/* Unset (clear) the specified flag in the given flag set */
int safeflg_unset(uint16_t *flags, uint16_t flag, uint16_t max_flags)
{
    if (!safeflg_is_valid(flag, max_flags)) {
        return 1;
    }

    *flags &= ~flag;
    return 0;
}


/* Toggle the specified flag in the given flag set */
int safeflg_toggle(uint16_t *flags, uint16_t flag, uint16_t max_flags)
{
    if (!safeflg_is_valid(flag, max_flags)) {
        return 1;
    }

    *flags ^= flag;
    return 0;
}
