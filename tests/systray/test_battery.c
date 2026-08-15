/**
 * @file tests/systray/test_battery.c
 *
 * @brief Test battery for battery status reading
 *
 * Deliberately limited: essentially all of battery_status_read's own
 * real logic (the full/AC/critical/low percentage formatting) only
 * runs once real data is found under BATTERY_ACPI_BASE_DIR or
 * BATTERY_APM_PROC_FILE, both fixed, real system paths read by
 * static functions private to this file, with no way to inject a
 * synthetic fixture the way, say, utils/sysmem.c's own tests do
 * against a real /proc.  This sandbox has neither a real battery nor
 * /proc/apm, so only the deterministic "not found" path and the two
 * initial guard clauses are covered here.
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
#include <harness/tap.h>
#include <systray/battery.h>


/* A NULL output buffer, or a zero-sized one, is a no-op, not a crash */
static void s_test_guards(void)
{
    char buf[32] = "unchanged";

    battery_status_read(CONFIG_BATTERY_BACKEND_ACPI, 0, 95, 20, 10,
            NULL, sizeof(buf));
    TAP_OK(true, "a NULL output buffer is safely ignored, no crash");

    battery_status_read(CONFIG_BATTERY_BACKEND_ACPI, 0, 95, 20, 10,
            buf, 0);
    TAP_EQ_STR(buf, "unchanged",
            "a zero-sized output buffer is left untouched");
}


/* With no real ACPI or APM battery present at all (this sandbox has
 * neither), both backends report "N/A" */
static void s_test_no_battery_present(void)
{
    char buf[32];

    memset(buf, 0, sizeof(buf));
    battery_status_read(CONFIG_BATTERY_BACKEND_ACPI, 0, 95, 20, 10,
            buf, sizeof(buf));
    TAP_EQ_STR(buf, "N/A",
            "ACPI backend reports \"N/A\" when nothing is found");

    memset(buf, 0, sizeof(buf));
    battery_status_read(CONFIG_BATTERY_BACKEND_APM, 0, 95, 20, 10,
            buf, sizeof(buf));
    TAP_EQ_STR(buf, "N/A",
            "APM backend reports \"N/A\" when nothing is found");
}


/* The "not found" message truncates cleanly to fit a small buffer,
 * rather than overflowing it: battery_status_read reserves the
 * buffer's own last byte for the terminator itself, on top of
 * safe_strncpy's own one-byte reservation, so a 4-byte buffer holds
 * only 2 real characters of "N/A" ("N/") plus the terminator, one
 * byte more conservative than the smallest buffer that could still
 * hold it */
static void s_test_not_found_message_is_bounded(void)
{
    char buf[4];

    battery_status_read(CONFIG_BATTERY_BACKEND_ACPI, 0, 95, 20, 10,
            buf, sizeof(buf));
    TAP_EQ_INT((long) strlen(buf), 2,
            "the message truncates to fit, never overflowing the buffer");
    TAP_EQ_STR(buf, "N/",
            "...keeping whichever leading characters did fit");
}


int main(void)
{
    TAP_PLAN(6);

    s_test_guards();
    s_test_no_battery_present();
    s_test_not_found_message_is_bounded();

    return TAP_DONE();
}
