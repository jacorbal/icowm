/**
 * @file tests/harness/tap.h
 *
 * @brief A small, dependency-free TAP-style test harness
 *
 * Produces plain Test Anything Protocol output (@c ok/@c not @c ok
 * lines, a leading @c 1..N plan) to stdout, so every test binary this
 * project builds already speaks a format any TAP consumer (@c prove,
 * a CI step, or just a human reading the output directly) already
 * understands, without pulling in a real test framework as a
 * dependency.
 *
 * Usage, in one test file:
 * @code
 * #include <harness/tap.h>
 *
 * int main(void)
 * {
 *     TAP_PLAN(3);
 *     TAP_OK(1 + 1 == 2, "arithmetic still works");
 *     TAP_EQ_INT(cdlist_size(list), 0, "starts empty");
 *     TAP_EQ_STR(safe_strdup("x"), "x", "safe_strdup copies");
 *     TAP_DONE();
 * }
 * @endcode
 *
 * @ingroup tests
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef TESTS_HARNESS_TAP_H
#define TESTS_HARNESS_TAP_H


/* System includes */
#include <stdio.h>
#include <string.h>


/** Running count of tests executed so far in this binary, and of how
 *  many of those failed; both drive @c TAP_DONE's own exit status,
 *  so a failing test binary is always visible as a nonzero exit code
 *  to whatever ran it, not just as text a human has to read closely. */
static int tap_count = 0;
static int tap_failed = 0;

/** How many assertions @c TAP_PLAN announced, compared against
 *  @c tap_count by @c TAP_DONE itself */
static int tap_planned = 0;


/**
 * @brief Announce how many assertions this test binary will make
 *
 * Always the first TAP call in @c main, per the protocol itself: a
 * consumer reading the output knows immediately how many @c ok/@c
 * not @c ok lines to expect, and a mismatch (the binary crashing
 * partway through, skipping an assertion by accident) is visible as
 * a plan/count mismatch rather than silently passing.
 *
 * @param n How many @c TAP_OK-family assertions follow
 */
#define TAP_PLAN(n) \
    (tap_planned = (n), (void) printf("1..%d\n", (n)))

/**
 * @brief Assert that @p cond is true
 *
 * @param cond Condition being asserted
 * @param desc Short, human-readable description of what this checks
 */
#define TAP_OK(cond, desc) \
    tap_ok_impl((cond), (desc), __FILE__, __LINE__)

/**
 * @brief Assert that two @c long values are equal
 *
 * @param a    Actual value
 * @param b    Expected value
 * @param desc Short, human-readable description of what this checks
 */
#define TAP_EQ_INT(a, b, desc) \
    tap_eq_int_impl((long) (a), (long) (b), (desc), __FILE__, __LINE__)

/**
 * @brief Assert that two null-terminated strings are equal
 *
 * @param a    Actual value; @c NULL is a failure, never a crash
 * @param b    Expected value; @c NULL is a failure, never a crash
 * @param desc Short, human-readable description of what this checks
 */
#define TAP_EQ_STR(a, b, desc) \
    tap_eq_str_impl((a), (b), (desc), __FILE__, __LINE__)

/**
 * @brief Assert that a pointer is @c NULL
 *
 * @param p    Pointer being asserted
 * @param desc Short, human-readable description of what this checks
 */
#define TAP_NULL(p, desc) \
    tap_ok_impl((p) == NULL, (desc), __FILE__, __LINE__)

/**
 * @brief Assert that a pointer is not @c NULL
 *
 * @param p    Pointer being asserted
 * @param desc Short, human-readable description of what this checks
 */
#define TAP_NOT_NULL(p, desc) \
    tap_ok_impl((p) != NULL, (desc), __FILE__, __LINE__)

/**
 * @brief Finish this test binary, printing a one-line summary and
 *        returning the exit status @c main itself should return
 *
 * @c 0 when every assertion this binary made passed, @c 1 when at
 * least one failed; a POSIX-style exit status, so @c make test
 * (or any script) can tell success from failure without parsing the
 * TAP text itself.
 */
#define TAP_DONE() tap_done_impl()


/**
 * @brief Implementation behind @c TAP_OK; not meant to be called
 *        directly, use the macro instead
 */
static inline int tap_ok_impl(int cond, const char *desc,
        const char *file, int line)
{
    tap_count += 1;
    if (cond) {
        printf("ok %d - %s\n", tap_count, desc);
    } else {
        tap_failed += 1;
        printf("not ok %d - %s\n", tap_count, desc);
        printf("#   at %s:%d\n", file, line);
    }
    return cond;
}

/**
 * @brief Implementation behind @c TAP_EQ_INT; not meant to be called
 *        directly, use the macro instead
 */
static inline int tap_eq_int_impl(long a, long b, const char *desc,
        const char *file, int line)
{
    int cond = (a == b);

    tap_count += 1;
    if (cond) {
        printf("ok %d - %s\n", tap_count, desc);
    } else {
        tap_failed += 1;
        printf("not ok %d - %s\n", tap_count, desc);
        printf("#   at %s:%d\n", file, line);
        printf("#   expected: %ld\n", b);
        printf("#   got:      %ld\n", a);
    }
    return cond;
}

/**
 * @brief Implementation behind @c TAP_EQ_STR; not meant to be called
 *        directly, use the macro instead
 */
static inline int tap_eq_str_impl(const char *a, const char *b,
        const char *desc, const char *file, int line)
{
    int cond = (a != NULL && b != NULL && strcmp(a, b) == 0);

    tap_count += 1;
    if (cond) {
        printf("ok %d - %s\n", tap_count, desc);
    } else {
        tap_failed += 1;
        printf("not ok %d - %s\n", tap_count, desc);
        printf("#   at %s:%d\n", file, line);
        printf("#   expected: %s\n", (b != NULL) ? b : "(null)");
        printf("#   got:      %s\n", (a != NULL) ? a : "(null)");
    }
    return cond;
}

/**
 * @brief Implementation behind @c TAP_DONE; not meant to be called
 *        directly, use the macro instead
 */
static inline int tap_done_impl(void)
{
    int status = 0;

    if (tap_count != tap_planned) {
        printf("# planned %d but ran %d\n", tap_planned, tap_count);
        status = 1;
    }
    if (tap_failed > 0) {
        printf("# %d/%d failed\n", tap_failed, tap_count);
        status = 1;
    }
    if (status == 0) {
        printf("# all %d passed\n", tap_count);
    }
    return status;
}


#endif  /* ! TESTS_HARNESS_TAP_H */
