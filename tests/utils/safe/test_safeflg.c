/**
 * @file tests/utils/safe/test_safeflg.c
 *
 * @brief Test battery for safe flag (bitset) management
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* Local includes */
#include <utils/safe/safeflg.h>
#include <harness/tap.h>


/* safeflg_is_valid: max_flags == 0 rejects every flag outright,
 * including flag == 0 itself */
static void s_test_is_valid_max_flags_zero(void)
{
    TAP_OK(!safeflg_is_valid(0u, 0u), "max_flags=0 rejects flag=0");
    TAP_OK(!safeflg_is_valid(1u, 0u), "max_flags=0 rejects flag=1");
}


/* safeflg_is_valid: flag == 0 is never valid, regardless of
 * max_flags (it is not a single-bit flag at all) */
static void s_test_is_valid_flag_zero(void)
{
    TAP_OK(!safeflg_is_valid(0u, 16u), "flag=0 is never valid");
}


/* safeflg_is_valid: every power of 2 strictly below max_flags is
 * valid; every non-power-of-2 is not, regardless of range */
static void s_test_is_valid_power_of_two(void)
{
    TAP_OK(safeflg_is_valid(1u, 16u), "1 (2^0) is a valid flag");
    TAP_OK(safeflg_is_valid(2u, 16u), "2 (2^1) is a valid flag");
    TAP_OK(safeflg_is_valid(4u, 16u), "4 (2^2) is a valid flag");
    TAP_OK(safeflg_is_valid(8u, 16u), "8 (2^3) is a valid flag");
    TAP_OK(!safeflg_is_valid(3u, 16u), "3 is not a power of 2");
    TAP_OK(!safeflg_is_valid(5u, 16u), "5 is not a power of 2");
    TAP_OK(!safeflg_is_valid(6u, 16u), "6 is not a power of 2");
    TAP_OK(!safeflg_is_valid(7u, 16u), "7 is not a power of 2");
}


/* safeflg_is_valid: the max_flags bound is exclusive, not inclusive */
static void s_test_is_valid_bound_is_exclusive(void)
{
    TAP_OK(safeflg_is_valid(4u, 8u),
            "4 < 8: valid, one power-of-2 step below the bound");
    TAP_OK(!safeflg_is_valid(8u, 8u),
            "8 == 8: rejected, the bound itself is exclusive");
    TAP_OK(!safeflg_is_valid(16u, 8u),
            "16 > 8: rejected, past the bound entirely");
}


/* safeflg_set: a NULL flags pointer is reported as failure, never a
 * crash */
static void s_test_set_null_flags(void)
{
    TAP_EQ_INT(safeflg_set(NULL, 1u, 16u), 1,
            "set on NULL flags fails cleanly");
}


/* safeflg_set: an invalid flag is rejected, and never mutates
 * *flags even though it's already known-valid before the call */
static void s_test_set_invalid_flag_leaves_flags_untouched(void)
{
    uint32_t flags = 0x00AAu;
    int status = safeflg_set(&flags, 3u, 16u);  /* 3 is not a power of 2 */

    TAP_EQ_INT(status, 1, "set with an invalid flag fails");
    TAP_EQ_INT(flags, 0x00AAu, "flags left completely untouched");
}


/* safeflg_set: setting a valid flag turns that one bit on, without
 * disturbing any other bit already set */
static void s_test_set_ors_in_the_bit(void)
{
    uint32_t flags = 0x0010u;  /* bit 4 already set */
    int status = safeflg_set(&flags, 0x0001u, 0x8000u);

    TAP_EQ_INT(status, 0, "set on a valid flag succeeds");
    TAP_EQ_INT(flags, 0x0011u, "new bit ORed in, old bit untouched");
}


/* safeflg_set: setting a flag that is already set is a harmless,
 * idempotent no-op (still succeeds, value unchanged) */
static void s_test_set_is_idempotent(void)
{
    uint32_t flags = 0x0004u;
    int status = safeflg_set(&flags, 0x0004u, 0x8000u);

    TAP_EQ_INT(status, 0, "re-setting an already-set flag succeeds");
    TAP_EQ_INT(flags, 0x0004u, "value unchanged");
}


/* safeflg_unset: a NULL flags pointer is reported as failure, never
 * a crash */
static void s_test_unset_null_flags(void)
{
    TAP_EQ_INT(safeflg_unset(NULL, 1u, 16u), 1,
            "unset on NULL flags fails cleanly");
}


/* safeflg_unset: clearing a set bit turns it off, without disturbing
 * any other bit */
static void s_test_unset_clears_the_bit(void)
{
    uint32_t flags = 0x0011u;  /* bits 0 and 4 set */
    int status = safeflg_unset(&flags, 0x0001u, 0x8000u);

    TAP_EQ_INT(status, 0, "unset on a valid flag succeeds");
    TAP_EQ_INT(flags, 0x0010u, "only the target bit cleared");
}


/* safeflg_unset: clearing a flag that is already clear is a
 * harmless, idempotent no-op */
static void s_test_unset_is_idempotent(void)
{
    uint32_t flags = 0x0010u;
    int status = safeflg_unset(&flags, 0x0001u, 0x8000u);

    TAP_EQ_INT(status, 0, "unsetting an already-clear flag succeeds");
    TAP_EQ_INT(flags, 0x0010u, "value unchanged");
}


/* safeflg_toggle: flips a clear bit to set, and that same flag back
 * to clear on a second call, restoring the original value */
static void s_test_toggle_flips_both_ways(void)
{
    uint32_t flags = 0x0000u;

    TAP_EQ_INT(safeflg_toggle(&flags, 0x0002u, 0x8000u), 0,
            "first toggle succeeds");
    TAP_EQ_INT(flags, 0x0002u, "flag now set");

    TAP_EQ_INT(safeflg_toggle(&flags, 0x0002u, 0x8000u), 0,
            "second toggle succeeds");
    TAP_EQ_INT(flags, 0x0000u, "flag cleared again, back to original");
}


/* safeflg_toggle: an invalid flag is rejected without mutating
 * *flags, the same as set/unset */
static void s_test_toggle_invalid_flag_leaves_flags_untouched(void)
{
    uint32_t flags = 0x0055u;
    int status = safeflg_toggle(&flags, 6u, 16u);  /* not a power of 2 */

    TAP_EQ_INT(status, 1, "toggle with an invalid flag fails");
    TAP_EQ_INT(flags, 0x0055u, "flags left completely untouched");
}


int main(void)
{
    TAP_PLAN(32);

    s_test_is_valid_max_flags_zero();
    s_test_is_valid_flag_zero();
    s_test_is_valid_power_of_two();
    s_test_is_valid_bound_is_exclusive();
    s_test_set_null_flags();
    s_test_set_invalid_flag_leaves_flags_untouched();
    s_test_set_ors_in_the_bit();
    s_test_set_is_idempotent();
    s_test_unset_null_flags();
    s_test_unset_clears_the_bit();
    s_test_unset_is_idempotent();
    s_test_toggle_flips_both_ways();
    s_test_toggle_invalid_flag_leaves_flags_untouched();

    return TAP_DONE();
}
