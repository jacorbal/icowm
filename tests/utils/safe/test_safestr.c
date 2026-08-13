/**
 * @file tests/utils/safe/test_safestr.c
 *
 * @brief Test battery for safe string helpers
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
#include <string.h>

/* Local includes */
#include <utils/safe/safestr.h>
#include <harness/tap.h>


/* safe_strnlen: NULL, empty, exact, and truncated-by-maxlen cases */
static void s_test_strnlen(void)
{
    TAP_EQ_INT(safe_strnlen(NULL, 10u), 0, "NULL string is length 0");
    TAP_EQ_INT(safe_strnlen("", 10u), 0, "empty string is length 0");
    TAP_EQ_INT(safe_strnlen("hello", 10u), 5,
            "real length, well under maxlen");
    TAP_EQ_INT(safe_strnlen("hello world", 5u), 5,
            "capped at maxlen, not the real length");
}


/* safe_strlen: NULL and a real string, with no cap at all */
static void s_test_strlen(void)
{
    TAP_EQ_INT(safe_strlen(NULL), 0, "NULL string is length 0");
    TAP_EQ_INT(safe_strlen("Cerrar"), 6, "real length, no cap");
}


/* safe_strncpy: NULL arguments, sz == 0, sz == 1, truncation, and an
 * ordinary in-bounds copy.  'buf' itself is sized generously (16)
 * so every direct 'strcpy' setup below fits safely; the 8-byte
 * limit being tested is passed explicitly to 'safe_strncpy' as its
 * own 'sz' argument, not derived from 'sizeof(buf)' */
static void s_test_strncpy(void)
{
    char buf[16];
    static const size_t s_limit = 8u;

    TAP_NULL(safe_strncpy(NULL, "x", s_limit), "dst == NULL returns NULL");

    strcpy(buf, "unchanged");
    TAP_EQ_STR(safe_strncpy(buf, NULL, s_limit), "unchanged",
            "src == NULL: dst unchanged (buf itself untouched)");

    strcpy(buf, "keep-me!");
    safe_strncpy(buf, "new", 0u);
    TAP_EQ_STR(buf, "keep-me!", "sz == 0: dst left completely alone");

    buf[0] = 'X';
    safe_strncpy(buf, "hello", 1u);
    TAP_EQ_STR(buf, "", "sz == 1: only the null terminator fits");

    safe_strncpy(buf, "hi", s_limit);
    TAP_EQ_STR(buf, "hi", "ordinary in-bounds copy");

    safe_strncpy(buf, "way too long for this buffer", s_limit);
    TAP_EQ_INT((int) strlen(buf), (int) s_limit - 1,
            "truncated to sz - 1 characters");
    TAP_EQ_STR(buf, "way too", "truncated content is the right prefix");
}


/* safe_strcpy: the unsized legacy wrapper still copies correctly */
static void s_test_strcpy(void)
{
    char buf[16];

    safe_strcpy(buf, "Cerrar");
    TAP_EQ_STR(buf, "Cerrar", "safe_strcpy copies the full string");
}


/* safe_strndup: NULL, an ordinary duplicate (independent allocation),
 * and truncation at n */
static void s_test_strndup(void)
{
    char *dup;
    const char *original = "Iconizar";

    TAP_NULL(safe_strndup(NULL, 10u), "NULL source yields NULL");

    dup = safe_strndup(original, 100u);
    TAP_NOT_NULL(dup, "duplicate allocated");
    TAP_EQ_STR(dup, original, "duplicate has the same content");
    TAP_OK(dup != original, "duplicate is its own allocation");
    free(dup);

    dup = safe_strndup(original, 4u);
    TAP_EQ_STR(dup, "Icon", "truncated to the first n characters");
    free(dup);
}


/* safe_strdup: NULL, and an ordinary full duplicate */
static void s_test_strdup(void)
{
    char *dup;

    TAP_NULL(safe_strdup(NULL), "NULL source yields NULL");

    dup = safe_strdup("Terminal");
    TAP_EQ_STR(dup, "Terminal", "full duplicate matches");
    free(dup);
}


/* safe_strncat: NULL arguments, an already-full destination, an
 * ordinary in-bounds append, and truncation */
static void s_test_strncat(void)
{
    char buf[12];

    strcpy(buf, "ab");
    TAP_EQ_STR(safe_strncat(buf, NULL, sizeof(buf)), "ab",
            "src == NULL: dst returned unchanged");
    TAP_NULL(safe_strncat(NULL, "x", 8u),
            "dst == NULL literally returns NULL, not a crash");

    strcpy(buf, "full-value!");  /* already fills all 11 usable chars */
    safe_strncat(buf, "more", sizeof(buf));
    TAP_EQ_STR(buf, "full-value!",
            "dst_len >= sz: no room at all, nothing appended");

    strcpy(buf, "ab");
    safe_strncat(buf, "cd", sizeof(buf));
    TAP_EQ_STR(buf, "abcd", "ordinary in-bounds append");

    strcpy(buf, "12345");
    safe_strncat(buf, "abcdefgh", sizeof(buf));
    TAP_EQ_INT((int) strlen(buf), (int) sizeof(buf) - 1,
            "append truncated to leave room for the terminator");
    TAP_EQ_STR(buf, "12345abcdef", "truncated content is the right"
            " prefix of what was appended");
}


/* safe_strcat: the unsized legacy wrapper still appends correctly */
static void s_test_strcat(void)
{
    char buf[16];

    strcpy(buf, "ab");
    safe_strcat(buf, "cd");
    TAP_EQ_STR(buf, "abcd", "safe_strcat appends the full string");
}


/* safe_strncmp: both-NULL, one-NULL-each-side, n == 0, equal
 * strings, differing strings, and a difference past the n cutoff */
static void s_test_strncmp(void)
{
    TAP_EQ_INT(safe_strncmp(NULL, NULL, 5u), 0,
            "both NULL: equal");
    TAP_EQ_INT(safe_strncmp(NULL, "x", 5u), -1,
            "only s1 NULL: NULL sorts before anything");
    TAP_EQ_INT(safe_strncmp("x", NULL, 5u), 1,
            "only s2 NULL: anything sorts after NULL");
    TAP_EQ_INT(safe_strncmp("abc", "xyz", 0u), 0,
            "n == 0: nothing to compare, always equal");
    TAP_EQ_INT(safe_strncmp("abc", "abc", 5u), 0,
            "identical strings compare equal");
    TAP_OK(safe_strncmp("abc", "abd", 5u) < 0,
            "'abc' sorts before 'abd'");
    TAP_OK(safe_strncmp("abd", "abc", 5u) > 0,
            "'abd' sorts after 'abc'");
    TAP_EQ_INT(safe_strncmp("abcXXX", "abcYYY", 3u), 0,
            "equal within the first n characters: equal, "
            "regardless of what differs past the cutoff");
}


/* safe_strcmp: both-NULL, one-NULL-each-side, and an ordinary
 * unsized comparison with no length limit at all */
static void s_test_strcmp(void)
{
    TAP_EQ_INT(safe_strcmp(NULL, NULL), 0, "both NULL: equal");
    TAP_EQ_INT(safe_strcmp(NULL, "x"), -1,
            "only s1 NULL: NULL sorts before anything");
    TAP_EQ_INT(safe_strcmp("x", NULL), 1,
            "only s2 NULL: anything sorts after NULL");
    TAP_EQ_INT(safe_strcmp("same", "same"), 0,
            "identical strings compare equal");
    TAP_OK(safe_strcmp("apple", "banana") < 0,
            "'apple' sorts before 'banana'");
}


int main(void)
{
    TAP_PLAN(41);

    s_test_strnlen();
    s_test_strlen();
    s_test_strncpy();
    s_test_strcpy();
    s_test_strndup();
    s_test_strdup();
    s_test_strncat();
    s_test_strcat();
    s_test_strncmp();
    s_test_strcmp();

    return TAP_DONE();
}
