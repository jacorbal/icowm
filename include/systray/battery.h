/**
 * @file systray/battery.h
 *
 * @brief Battery status reading and formatting, compatible with either
 *        the Linux ACPI (@c /sys/class/power_supply) or the legacy APM
 *        (@c /proc/apm) kernel interface
 *
 * @ingroup systray
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SYSTRAY_BATTERY_H
#define SYSTRAY_BATTERY_H


/* System includes */
#include <stddef.h>
#include <stdint.h>

/* Project includes */
#include <config.h>


/** Base sysfs directory the Linux ACPI battery interface lives under */
#define BATTERY_ACPI_BASE_DIR "/sys/class/power_supply"

/** Legacy APM battery/AC status pseudo-file */
#define BATTERY_APM_PROC_FILE "/proc/apm"

/** Maximum bytes read from any one single-line @c sysfs/procfs file */
#define BATTERY_LINE_MAX_LEN (64)

/**
 * @brief Buffer size for one line read from @c BATTERY_APM_PROC_FILE
 *
 * @c /proc/apm holds several whitespace-separated fields on one line
 * (driver version, APM version, flags, AC line status, battery status,
 * battery flags, and battery percentage), unlike the single bare value
 * @c BATTERY_LINE_MAX_LEN is sized for; generous enough for that field
 * list with real-world values, well short of actually needing to be.
 */
#define BATTERY_APM_LINE_MAX_LEN (128)

/**
 * @brief Buffer size for one constructed sysfs path under
 *        @c BATTERY_ACPI_BASE_DIR
 *
 * Sized generously above the worst case a compiler's static
 * truncation analysis can prove for @c ("<base>/<d_name>/<suffix>")
 * (the base directory's own length, plus a full directory entry name
 * with size @c NAME_MAX, plus the longest suffix used, @c /online, plus
 * the terminating null), so building such a path can never be flagged
 * as a possible truncation regardless of what the C library's own
 * @p d_name field declares itself capable of holding.
 */
#define BATTERY_PATH_MAX_LEN (320)


/**
 * @brief Read the current battery/AC state and format it into @p out
 *
 * The formatted text follows a fixed set of shapes depending on
 * whether AC power is connected and how the battery's charge compares
 * to @p threshold_charged / @p threshold_low / @p threshold_critical:
 *
 * @verbatim
 * | State                                                | Text       |
 * |------------------------------------------------------|------------|
 * | On battery, above 'low'                              | "X%"       |
 * | On battery, at/below 'low'                           | "X%!"      |
 * | On battery, at/below 'critical'                      | "X%!!"     |
 * | On AC, not fully charged                             | "X% AC"    |
 * | Fully charged, on battery                            | "Full"     |
 * | Fully charged, on AC                                 | "Full, AC" |
 * | No battery found for 'backend_type'/'backend_number' | "N/A"      |
 * @endverbatim
 *
 * @c "X%" here is itself translatable (@c STR_BATTERY_PERCENT, in
 * @c uistr.h).  Whether the '%' sign sits flush against the number or
 * has a space before it is a per-language typographic convention, so
 * the exact rendered text (e.g., "50%" versus "50 %") varies by locale
 * even though the shapes above hold for every one of them.
 *
 * A battery is "fully charged" once its percentage is at or above
 * @p threshold_charged, regardless of its actual charging/discharging
 * state as reported by the kernel; some hardware never reports "Full"
 * even sitting at 100% on AC, so going by the percentage alone is more
 * reliable across backends.
 *
 * @param backend_type       Which kernel interface to read
 * @param backend_number     Which battery to read when a system has
 *                           more than one (0-indexed); ignored for
 *                           @c CONFIG_BATTERY_BACKEND_APM, which only
 *                           ever exposes one aggregate battery
 * @param threshold_charged  Percentage at/above which the status
 *                           reads "Full" instead of a percentage
 * @param threshold_low      Percentage at/below which a single '!' is
 *                           appended while on battery power
 * @param threshold_critical Percentage at/below which two '!' are
 *                           appended instead of one, while on battery
 *                           power
 * @param out                Buffer to receive the formatted, null
 *                           terminated status text
 * @param out_size           Size of @p out in bytes
 *
 * @note Complexity: @e O(1), a small, fixed number of short file reads
 *       regardless of system state
 */
void battery_status_read(enum config_battery_backend_type_e backend_type,
        uint32_t backend_number,
        uint32_t threshold_charged, uint32_t threshold_low,
        uint32_t threshold_critical,
        char *out, size_t out_size);


#endif  /* ! SYSTRAY_BATTERY_H */
