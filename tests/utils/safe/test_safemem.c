/**
 * @file tests/utils/safe/test_safemem.c
 *
 * @brief Test battery for safe memory-freeing helpers
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>

/* Local includes */
#include <utils/safe/safemem.h>
#include <harness/tap.h>


/* safe_free on a NULL 'ptr' argument itself (not a pointer to a
 * NULL block, the pointer-to-pointer argument itself) is a no-op,
 * never a crash */
static void s_test_free_null_ptr_argument(void)
{
    safe_free(NULL);
    TAP_OK(1, "safe_free(NULL) does not crash");
}


/* safe_free on a pointer whose target is already NULL is a no-op,
 * never a call to free(NULL) */
static void s_test_free_already_null_target(void)
{
    void *p = NULL;

    safe_free(&p);
    TAP_NULL(p, "target stays NULL, no crash freeing an empty slot");
}


/* safe_free on a real allocation frees it and resets the pointer to
 * NULL, so a second call (or any later use) can never see a
 * dangling pointer */
static void s_test_free_real_allocation_resets_pointer(void)
{
    void *p = malloc(16);

    TAP_NOT_NULL(p, "test allocation itself succeeded");
    safe_free(&p);
    TAP_NULL(p, "pointer reset to NULL after freeing");
}


/* Calling safe_free twice in a row on the same pointer is safe: the
 * first call frees and nulls it, so the second call sees an
 * already-NULL target and does nothing, never a double free
 * (verified for real here, under AddressSanitizer, not just by
 * reading the code) */
static void s_test_free_twice_is_safe(void)
{
    void *p = malloc(16);

    safe_free(&p);
    safe_free(&p);
    TAP_NULL(p, "still NULL after a second safe_free call");
}


/* safe_free_var with a NULL 'first' argument reports failure and
 * touches nothing, rather than crashing on an unusable list */
static void s_test_free_var_null_first(void)
{
    TAP_EQ_INT(safe_free_var(NULL), -1,
            "safe_free_var(NULL) reports failure, not a crash");
}


/* safe_free_var frees every pointer in the list and resets each one
 * to NULL, stopping at the SAFE_FREE_VAR_END sentinel */
static void s_test_free_var_frees_every_pointer(void)
{
    void *a = malloc(8);
    void *b = malloc(8);
    void *c = malloc(8);
    int status;

    status = safe_free_var(&a, &b, &c, SAFE_FREE_VAR_END);

    TAP_EQ_INT(status, 0, "reports success");
    TAP_NULL(a, "first pointer freed and reset");
    TAP_NULL(b, "second pointer freed and reset");
    TAP_NULL(c, "third pointer freed and reset");
}


/* safe_free_var silently skips any pointer in the list whose own
 * target is already NULL, rather than treating it as an error or
 * calling free(NULL) */
static void s_test_free_var_skips_already_null_targets(void)
{
    void *a = malloc(8);
    void *b = NULL;         /* already empty */
    void *c = malloc(8);
    int status;

    status = safe_free_var(&a, &b, &c, SAFE_FREE_VAR_END);

    TAP_EQ_INT(status, 0, "reports success even with a null slot");
    TAP_NULL(a, "real allocation still freed and reset");
    TAP_NULL(b, "already-null slot stays null, no crash");
    TAP_NULL(c, "real allocation still freed and reset");
}


/* A single pointer, with nothing after it but the terminator, is
 * the smallest valid call */
static void s_test_free_var_single_pointer(void)
{
    void *a = malloc(8);
    int status = safe_free_var(&a, SAFE_FREE_VAR_END);

    TAP_EQ_INT(status, 0, "single-pointer call succeeds");
    TAP_NULL(a, "that one pointer freed and reset");
}


int main(void)
{
    TAP_PLAN(16);

    s_test_free_null_ptr_argument();
    s_test_free_already_null_target();
    s_test_free_real_allocation_resets_pointer();
    s_test_free_twice_is_safe();
    s_test_free_var_null_first();
    s_test_free_var_frees_every_pointer();
    s_test_free_var_skips_already_null_targets();
    s_test_free_var_single_pointer();

    return TAP_DONE();
}
