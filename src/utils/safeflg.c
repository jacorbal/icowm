/**
 * @file safeflg.c
 *
 * @brief Implementation of safe flag management functions
 */

/* System includes */
#include <stdbool.h>    /* bool, false, true */

/* Local includes */
#include <utils/safeflg.h>


/* Validate if a flag is within the allowed range */
bool safeflg_is_valid(unsigned int flag, unsigned int max_flags)
{
    if (max_flags == 0) {
        return false;
    }

    /* Check whether the specified flag is a valid, single-bit flag.
     * A valid flag must be a power of 2 and should also fall within the
     * defined range of allowed flags. */
    return (flag != 0 && (flag & (flag - 1)) == 0 &&
            flag < (1U << max_flags));
}


/* Set (enable) the specified flag in the given flag set */
int safeflg_set(unsigned int *flags, unsigned int flag,
        unsigned int max_flags)
{
    if (!safeflg_is_valid(flag, max_flags)) {
        return 1;
    }

    *flags |= flag;
    return 0;
}


/* Unset (clear) the specified flag in the given flag set */
int safeflg_unset(unsigned int *flags, unsigned int flag,
        unsigned int max_flags)
{
    if (!safeflg_is_valid(flag, max_flags)) {
        return 1;
    }

    *flags &= ~flag;
    return 0;
}


/* Toggle the specified flag in the given flag set */
int safeflg_toggle(unsigned int *flags, unsigned int flag,
        unsigned int max_flags)
{
    if (!safeflg_is_valid(flag, max_flags)) {
        return 1;
    }

    *flags ^= flag;
    return 0;
}
