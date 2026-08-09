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


/* Read the kernel's own estimate of currently available system
 * memory */
bool sysmem_available_mib(uint32_t *out_mib)
{
    FILE *f;
    char buf[SYSMEM_MEMINFO_MAX_LEN];
    bool found = false;

    if (out_mib == NULL) {
        return false;
    }

    f = fopen(SYSMEM_MEMINFO_FILE, "r");
    if (f == NULL) {
        return false;
    }

    while (fgets(buf, (int) sizeof(buf), f) != NULL) {
        unsigned long kib;

        /* Every line in this file starts with a label immediately
         * followed by ':', so a plain prefix match is enough; no
         * need to skip past leading whitespace first. */
        if (strncmp(buf, "MemAvailable:", 13) == 0 &&
                sscanf(buf + 13, "%lu", &kib) == 1) {
            *out_mib = (uint32_t) (kib / 1024ul);
            found = true;
            break;
        }
    }

    fclose(f);
    return found;
}


/* Read the calling process's own current resident memory usage */
bool sysmem_self_rss_mib(uint32_t *out_mib)
{
    FILE *f;
    char buf[SYSMEM_SELF_STATUS_MAX_LEN];
    bool found = false;

    if (out_mib == NULL) {
        return false;
    }

    f = fopen(SYSMEM_SELF_STATUS_FILE, "r");
    if (f == NULL) {
        return false;
    }

    while (fgets(buf, (int) sizeof(buf), f) != NULL) {
        unsigned long kib;

        if (strncmp(buf, "VmRSS:", 6) == 0 &&
                sscanf(buf + 6, "%lu", &kib) == 1) {
            *out_mib = (uint32_t) (kib / 1024ul);
            found = true;
            break;
        }
    }

    fclose(f);
    return found;
}
