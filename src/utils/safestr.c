/**
 * @file safestr.c
 *
 * @brief Implementation of safe string handling functions
 */

/* System includes */
#include <stdlib.h>     /* size_t, malloc */
#include <string.h>     /* memcpy */

/* Local includes */
#include <utils/safestr.h>


/* Calculate the length of a string, up to a maximum length */
size_t safe_strnlen(const char *str, size_t maxlen)
{
    if (str == NULL) {
        return 0;
    }

    const char *s = str;
    size_t len = 0;

    while (len < maxlen && *s) {
        s++;
        len++;
    }

    return len;
}


/* Calculate the length of a string */
size_t safe_strlen(const char *str)
{
    if (str == NULL) {
        return 0;
    }

    const char *s = str;

    while (*s) {
        s++;
    }

    return (size_t) (s - str);
}


/* Safely copies a string from 'src' to 'dst' */
char *safe_strncpy(char *restrict dst, const char *restrict src,
        size_t size)
{
    if (dst == NULL || src == NULL || size == 0) {
        return dst;
    }

    char *dst_s = dst;

    /* Copy until n-th character */
    while (size - 1 > 0 && *src != '\0') {
        *dst = *src;
        dst++;
        src++;
        size--;
    }

    /* Null-terminate the destination string */
    *dst = '\0';

    return dst_s;
}


/* Copies a string from 'src' to 'dst' without exceeding buffer size */
char *safe_strcpy(char *restrict dst, const char *restrict src)
{
    if (dst == NULL || src == NULL) {
        return dst;
    }

    char *dst_s = dst;

    while (*src != '\0') {
        *dst = *src;
        dst++;
        src++;
    }
    *dst = '\0';

    return dst_s;
}


/* Creates a duplicate of a string with a specified maximum length */
char *safe_strndup(const char *s, size_t n)
{
    if (s == NULL) {
        return NULL;
    }

    size_t len = safe_strnlen(s, n);
    char *copy = (char *) malloc(len + 1);

    if (copy) {
        memcpy(copy, s, len);
        copy[len] = '\0';
    }

    return copy;
}


/* Safely duplicates a string */
char *safe_strdup(const char *s)
{
    if (s == NULL) {
        return NULL;
    }

    size_t len = safe_strlen(s) + 1;
    char *copy = (char *) malloc(len);

    if (copy) {
        memcpy(copy, s, len);
    }

    return copy;
}


/* Safely compares two strings up to a specific length */
int safe_strncmp(const char *s1, const char *s2, size_t n)
{
    /* Nothing to compare */
    if (n == 0) {
        return 0;
    }

     /* Both strings are equal if both are 'NULL' */
    if (s1 == NULL && s2 == NULL) {
        return 0;
    }

    /* 'NULL' is less than any non-'NULL' string */
    if (s1 == NULL) {
        return -1;
    }

    /* Any non-'NULL' string is greater than 'NULL' */
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
    /* Both strings are equal if both are 'NULL' */
    if (s1 == NULL && s2 == NULL) {
        return 0;
    }

    /* 'NULL' is less than any non-'NULL' string */
    if (s1 == NULL) {
        return -1;
    }

    /* Any non-'NULL' string is greater than 'NULL' */
    if (s2 == NULL) {
        return 1;
    }

    /* Standard string comparison */
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return *(const unsigned char *) s1 - *(const unsigned char *) s2;
}
