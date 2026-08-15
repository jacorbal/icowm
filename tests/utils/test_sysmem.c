/**
 * @file tests/utils/test_sysmem.c
 *
 * @brief Test battery for /proc-based system memory reading
 *
 * Both public functions read fixed, real paths ('/proc/meminfo',
 * '/proc/self/status') rather than an injectable one, so there is no
 * way to point either at a synthetic test fixture.  These tests run
 * against the real /proc filesystem instead, which is present and
 * meaningful on any Linux system this project actually targets (it
 * is a Linux/X11 window manager); they check general properties
 * (success, a sane value) rather than exact numbers, since available
 * and resident memory are inherently not deterministic.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* Local includes */
#include <harness/tap.h>
#include <utils/sysmem.h>


/* sysmem_available_mib succeeds on a real Linux system, and reports
 * a plausible, nonzero value */
static void s_test_available_mib_succeeds(void)
{
    uint32_t mib = 0;
    bool ok = sysmem_available_mib(&mib);

    TAP_OK(ok, "sysmem_available_mib succeeds against the real /proc");
    TAP_OK(mib > 0, "reported available memory is a nonzero value");
}


/* sysmem_self_rss_mib succeeds on a real Linux system; this test
 * binary itself is a running process with some nonzero resident
 * memory */
static void s_test_self_rss_mib_succeeds(void)
{
    uint32_t mib = 0;
    bool ok = sysmem_self_rss_mib(&mib);

    TAP_OK(ok, "sysmem_self_rss_mib succeeds against the real /proc");
    TAP_OK(mib > 0, "this process's own reported RSS is a nonzero value");
}


/* Both functions reject a NULL output pointer rather than crashing */
static void s_test_null_out_mib_rejected(void)
{
    TAP_OK(!sysmem_available_mib(NULL),
            "sysmem_available_mib(NULL) fails cleanly, no crash");
    TAP_OK(!sysmem_self_rss_mib(NULL),
            "sysmem_self_rss_mib(NULL) fails cleanly, no crash");
}


int main(void)
{
    TAP_PLAN(6);

    s_test_available_mib_succeeds();
    s_test_self_rss_mib_succeeds();
    s_test_null_out_mib_rejected();

    return TAP_DONE();
}
