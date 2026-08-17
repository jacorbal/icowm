/**
 * @file utils/safe/safestr.c
 *
 * @brief Implementation for enhanced safe string functions
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdint.h>     /* SIZE_MAX */
#include <stdlib.h>     /* size_t, malloc */
#include <string.h>     /* memcpy */

/* Local includes */
#include <utils/safe/safestr.h>


/* Calculate the length of a string, up to a maximum length */
size_t safe_strnlen(const char *str, size_t maxlen)
{
    size_t len;
    const char *s;

    if (str == NULL) {
        return 0;
    }

    s = str;
    len = 0;

    while (len < maxlen && *s) {
        s++;
        len++;
    }

    return len;
}


/* Calculate the length of a string */
size_t safe_strlen(const char *str)
{
    return safe_strnlen(str, SIZE_MAX);
}


/* Safely copies a string from 'src' to 'dst' */
char *safe_strncpy(char *restrict dst, const char *restrict src,
        size_t sz)
{
    char *dst_s;

    if (dst == NULL || src == NULL || sz == 0) {
        return dst;
    }

    dst_s = dst;

    /* Copy until n-th character */
    while (sz - 1 > 0 && *src != '\0') {
        *dst = *src;
        dst++;
        src++;
        sz--;
    }

    /* Null-terminate the destination string */
    *dst = '\0';

    return dst_s;
}


/* Legacy unsized copy helper (prefer 'safe_strncpy' for bounded writes) */
char *safe_strcpy(char *restrict dst, const char *restrict src)
{
    return safe_strncpy(dst, src, SIZE_MAX);
}


/* Creates a duplicate of a string with a specified maximum length */
char *safe_strndup(const char *s, size_t n)
{
    size_t len;
    char *copy;

    if (s == NULL) {
        return NULL;
    }

    len = safe_strnlen(s, n);
    copy = (char *) malloc(len + 1);

    if (copy) {
        memcpy(copy, s, len);
        copy[len] = '\0';
    }

    return copy;
}


/* Safely duplicates a string */
char *safe_strdup(const char *s)
{
    return safe_strndup(s, SIZE_MAX);
}


/* Concatenates at most 'n' characters from one string to another */
char *safe_strncat(char *restrict dst, const char *restrict src,
        size_t sz)
{
    size_t dst_len;
    size_t remaining_space;
    size_t i;

    if (dst == NULL || src == NULL || sz == 0) {
        return dst; /* Return 'dst' if there's nothing to concatenate */
    }

    dst_len = safe_strlen(dst);

    if (dst_len >= sz) {
        /* No enough space, nothing done */
        return dst;
    }

    remaining_space = sz - dst_len;

    /* Concatenate the maximum number of characters from 'src' */
    for (i = 0; i < remaining_space - 1 && src[i] != '\0'; ++i) {
        dst[dst_len + i] = src[i];  /* Copy the character */
    }

    /* Ensure that 'dst' is null-terminated */
    dst[dst_len + i] = '\0';

    return dst;
}


/* Legacy unsized concatenation helper (prefer 'safe_strncat') */
char *safe_strcat(char *restrict dst, const char *restrict src)
{
    return safe_strncat(dst, src, SIZE_MAX);
}


/* Safely compares two strings up to a specific length */
int safe_strncmp(const char *s1, const char *s2, size_t n)
{
    /* Nothing to compare */
    if (n == 0) {
        return 0;
    }

    /* Both strings are equal if both are null */
    if (s1 == NULL && s2 == NULL) {
        return 0;
    }

    /* Null is less than any non-null string */
    if (s1 == NULL) {
        return -1;
    }

    /* Any non-null string is greater than null */
    if (s2 == NULL) {
        return 1;
    }

    while (n-- > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return (n == (size_t) (-1)) ?
        0 :
        *(const unsigned char *) s1 - *(const unsigned char *) s2;
}


/* Safely compares two strings */
int safe_strcmp(const char *s1, const char *s2)
{
    /* 'n == (size_t) -1' in 'safe_strncmp' only signals its own limit
     * was reached without finding a difference, which cannot happen
     * here: exhausting 'SIZE_MAX' comparisons would require a string
     * that size, far beyond any real allocation. */
    return safe_strncmp(s1, s2, SIZE_MAX);
}
