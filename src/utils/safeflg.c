/**
 * @file safeflg.c
 *
 * @brief Implementation of safe flag management functions
 */

/* System includes */
#include <stdbool.h>    /* bool */
#include <stdint.h>     /* uint32_t */

/* Local includes */
#include <utils/safeflg.h>


/* Validate if a flag is within the allowed range */
bool safeflg_is_valid(uint32_t flag, uint32_t max_flags)
{
    /* Check if the flag is a single bit (power of 2) and is within the
     * valid range */
    return (flag > 0 && (flag & (flag - 1)) == 0 && flag < (1 << max_flags));

}


/* Set (enable) the specified flag in the given flag set */
int safeflg_set(uint32_t *flags, uint32_t flag, uint32_t max_flags)
{
    if (!safeflg_is_valid(flag, (1 << max_flags))) {
        return 1;
    }

    *flags |= flag;
    return 0;
}


/* Unset (clear) the specified flag in the given flag set */
int safeflg_unset(uint32_t *flags, uint32_t flag, uint32_t max_flags)
{
    if (!safeflg_is_valid(flag, (1 << max_flags))) {
        return 1;
    }

    *flags &= ~flag;
    return 0;
}


/* Toggle the specified flag in the given flag set */
int safeflg_toggle(uint32_t *flags, uint32_t flag, uint32_t max_flags)
{
    if (!safeflg_is_valid(flag, (1 << max_flags))) {
        return 1;
    }

    *flags ^= flag;
    return 0;
}
