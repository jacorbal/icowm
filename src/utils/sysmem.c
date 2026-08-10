/**
 * @file utils/sysmem.c
 *
 * @brief System-wide available memory reading implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Local includes */
#include <utils/sysmem.h>


/**
 * @brief Read a single "Label: <n> kB" line from a @c /proc pseudo-
 *        file and convert it to mebibytes
 *
 * Both @c sysmem_available_mib and @c sysmem_self_rss_mib need
 * exactly this same read-a-file, scan-its-lines-for-one-label,
 * parse-the-number-after-it sequence; this is the part that was
 * actually identical between the two, parameterized by which file
 * and which label each one cares about.
 *
 * @param filepath Path to the pseudo-file to read
 * @param label    Line prefix to look for, colon included (e.g.,
 *                 @c "VmRSS:"); its length is measured here rather
 *                 than passed in, so there is nothing for a caller to
 *                 keep in sync with the string itself
 * @param out_mib  Receives the value, in mebibytes, rounded down
 *
 * @return @c true if @p filepath was read and @p label was found in
 *         it, @c false otherwise; @p out_mib is left untouched on
 *         @c false
 *
 * @note Complexity: @e O(n), where @e n is the file's size in bytes
 */
static bool s_sysmem_read_kib_field(const char *filepath,
        const char *label, uint32_t *out_mib)
{
    FILE *f;
    char buf[SYSMEM_LINE_MAX_LEN];
    size_t label_len;
    bool found = false;

    f = fopen(filepath, "r");
    if (f == NULL) {
        return false;
    }

    label_len = strlen(label);
    while (fgets(buf, (int) sizeof(buf), f) != NULL) {
        unsigned long kib;

        /* Every line in either file starts with a label immediately
         * followed by ':', so a plain prefix match is enough; no
         * need to skip past leading whitespace first. */
        if (strncmp(buf, label, label_len) == 0 &&
                sscanf(buf + label_len, "%lu", &kib) == 1) {
            *out_mib = (uint32_t) (kib / 1024ul);
            found = true;
            break;
        }
    }

    fclose(f);
    return found;
}


/* Read the kernel's own estimate of currently available system
 * memory */
bool sysmem_available_mib(uint32_t *out_mib)
{
    if (out_mib == NULL) {
        return false;
    }
    return s_sysmem_read_kib_field(SYSMEM_MEMINFO_FILE, "MemAvailable:",
            out_mib);
}


/* Read the calling process's own current resident memory usage */
bool sysmem_self_rss_mib(uint32_t *out_mib)
{
    if (out_mib == NULL) {
        return false;
    }
    return s_sysmem_read_kib_field(SYSMEM_SELF_STATUS_FILE, "VmRSS:",
            out_mib);
}
