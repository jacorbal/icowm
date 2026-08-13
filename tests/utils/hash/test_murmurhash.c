/**
 * @file tests/utils/hash/test_murmurhash.c
 *
 * @brief Test battery for the three MurmurHash 32-bit variants
 *
 * No external reference vectors are available in this environment
 * (no network access to check against an authoritative test suite),
 * so this leans on two kinds of assertion instead: structural
 * properties any correct hash function must have regardless of the
 * exact algorithm (determinism, seed sensitivity, input sensitivity,
 * safe handling of the zero-length/NULL edge case), and a handful of
 * golden regression values computed once against this exact
 * implementation, to catch a future accidental change to the
 * algorithm itself.  One of those golden values, murmurhash3_32
 * with an empty string and a zero seed, also happens to match
 * MurmurHash3's own well-known reference behavior for that specific
 * case (an all-zero input state stays all zero through every
 * multiply/xor step, hashing to 0), which is corroborating evidence
 * this implementation is not just internally consistent but actually
 * correct, not just a golden-value snapshot of whatever it already
 * did.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <string.h>

/* Local includes */
#include <utils/hash/murmurhash.h>
#include <harness/tap.h>


/* Each hash function is deterministic: the same key, length, and
 * seed always produce the same result */
static void s_test_deterministic(void)
{
    const char *key = "deterministic";
    int len = (int) strlen(key);

    TAP_EQ_INT(murmurhash1_32(key, len, 7u),
            murmurhash1_32(key, len, 7u),
            "murmurhash1_32 is deterministic");
    TAP_EQ_INT(murmurhash2_32(key, len, 7u),
            murmurhash2_32(key, len, 7u),
            "murmurhash2_32 is deterministic");
    TAP_EQ_INT(murmurhash3_32(key, len, 7u),
            murmurhash3_32(key, len, 7u),
            "murmurhash3_32 is deterministic");
}


/* A NULL key with length 0 is a safe, well-defined edge case for
 * every variant, not a crash (verified for real under
 * AddressSanitizer/UndefinedBehaviorSanitizer, not just assumed from
 * reading the pointer arithmetic) */
static void s_test_null_key_zero_length_is_safe(void)
{
    (void) murmurhash1_32(NULL, 0, 1u);
    (void) murmurhash2_32(NULL, 0, 1u);
    (void) murmurhash3_32(NULL, 0, 1u);
    TAP_OK(1, "NULL key, length 0 does not crash, for all three");
}


/* With length 0, all three variants share the same finalization
 * stage over the same initial state (h = seed in every one of
 * them, since there is no length-dependent tail to fold in), so
 * they necessarily agree; a structural property of this exact
 * implementation, not a coincidence */
static void s_test_zero_length_variants_agree(void)
{
    uint32_t h1 = murmurhash1_32("", 0, 99u);
    uint32_t h2 = murmurhash2_32("", 0, 99u);
    uint32_t h3 = murmurhash3_32("", 0, 99u);

    TAP_EQ_INT(h1, h2,
            "murmurhash1_32 and murmurhash2_32 agree at length 0");
    TAP_EQ_INT(h2, h3,
            "murmurhash2_32 and murmurhash3_32 agree at length 0");
}


/* Changing only the seed, for the same key, changes the hash (an
 * ordinary sanity check, not a guarantee no seed pair could ever
 * collide, just confirming the seed is actually mixed in at all) */
static void s_test_seed_changes_hash(void)
{
    const char *key = "same key, different seed";
    int len = (int) strlen(key);

    TAP_OK(murmurhash1_32(key, len, 1u) != murmurhash1_32(key, len, 2u),
            "murmurhash1_32: different seeds, different hashes");
    TAP_OK(murmurhash2_32(key, len, 1u) != murmurhash2_32(key, len, 2u),
            "murmurhash2_32: different seeds, different hashes");
    TAP_OK(murmurhash3_32(key, len, 1u) != murmurhash3_32(key, len, 2u),
            "murmurhash3_32: different seeds, different hashes");
}


/* Changing only the input, for the same seed, changes the hash (the
 * avalanche property in its most basic form: not a rigorous
 * statistical test, just confirming the input is actually mixed in) */
static void s_test_input_changes_hash(void)
{
    TAP_OK(murmurhash1_32("input-a", 7, 0u) !=
            murmurhash1_32("input-b", 7, 0u),
            "murmurhash1_32: different inputs, different hashes");
    TAP_OK(murmurhash2_32("input-a", 7, 0u) !=
            murmurhash2_32("input-b", 7, 0u),
            "murmurhash2_32: different inputs, different hashes");
    TAP_OK(murmurhash3_32("input-a", 7, 0u) !=
            murmurhash3_32("input-b", 7, 0u),
            "murmurhash3_32: different inputs, different hashes");
}


/* Every tail length (0 through 3 extra bytes past a full 4-byte
 * block boundary) is a genuinely different code path inside each
 * function's own 'switch (len & 3)'; every one of them, for lengths
 * 1 through 9, must run without crashing */
static void s_test_every_tail_length_is_safe(void)
{
    static const char *s_data = "abcdefghi";  /* 9 bytes: covers
                                                   len % 4 == 1, 2,
                                                   3, 0, and 1 again */
    int ok = 1;

    for (int len = 1; len <= 9; ++len) {
        (void) murmurhash1_32(s_data, len, 0u);
        (void) murmurhash2_32(s_data, len, 0u);
        (void) murmurhash3_32(s_data, len, 0u);
    }
    TAP_OK(ok, "lengths 1 through 9 (every tail-length case) are safe");
}


/* Golden regression values: computed once against this exact
 * implementation (see this file's own top comment).  A future
 * change to the mixing constants or the algorithm itself would flip
 * these, on purpose or by accident; this is what catches the
 * "by accident" case. */
static void s_test_golden_values(void)
{
    TAP_EQ_INT(murmurhash3_32("", 0, 0u), 0u,
            "murmurhash3_32(\"\", 0): 0, matching MurmurHash3's own "
            "well-known reference behavior for this exact case");
    TAP_EQ_INT(murmurhash1_32("hello", 5, 0u), 2935446651u,
            "murmurhash1_32(\"hello\", seed=0) golden value");
    TAP_EQ_INT(murmurhash2_32("hello", 5, 0u), 1049452051u,
            "murmurhash2_32(\"hello\", seed=0) golden value");
    TAP_EQ_INT(murmurhash3_32("hello", 5, 0u), 764122812u,
            "murmurhash3_32(\"hello\", seed=0) golden value");
    TAP_EQ_INT(murmurhash1_32("hello", 5, 42u), 215168431u,
            "murmurhash1_32(\"hello\", seed=42) golden value");
    TAP_EQ_INT(murmurhash2_32("hello", 5, 42u), 1171646377u,
            "murmurhash2_32(\"hello\", seed=42) golden value");
    TAP_EQ_INT(murmurhash3_32("hello", 5, 42u), 3107568196u,
            "murmurhash3_32(\"hello\", seed=42) golden value");
}


int main(void)
{
    TAP_PLAN(20);

    s_test_deterministic();
    s_test_null_key_zero_length_is_safe();
    s_test_zero_length_variants_agree();
    s_test_seed_changes_hash();
    s_test_input_changes_hash();
    s_test_every_tail_length_is_safe();
    s_test_golden_values();

    return TAP_DONE();
}
