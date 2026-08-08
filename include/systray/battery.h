/**
 * @file systray/battery.h
 *
 * @brief Battery status reading and formatting, compatible with
 *        either the Linux ACPI (@c /sys/class/power_supply) or the
 *        legacy APM (@c /proc/apm) kernel interface
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

/** Maximum bytes read from any one single-line sysfs/procfs file */
#define BATTERY_LINE_MAX_LEN (64)


/**
 * @brief Read the current battery/AC state and format it into @p out
 *
 * The formatted text follows a fixed set of shapes depending on
 * whether AC power is connected and how the battery's charge compares
 * to @p threshold_charged / @p threshold_low / @p threshold_critical:
 *
 * | State                          | Text     |
 * |---------------------------------|----------|
 * | On battery, above @c low        | @c "X%"  |
 * | On battery, at/below @c low     | @c "X%!" |
 * | On battery, at/below @c critical | @c "X%!!" |
 * | On AC, not fully charged         | @c "X% AC" |
 * | Fully charged, on battery        | @c "Full" |
 * | Fully charged, on AC             | @c "Full AC" |
 * | No battery found for @p backend_type / @p backend_number | @c "N/A" |
 *
 * A battery is "fully charged" once its percentage is at or above
 * @p threshold_charged, regardless of its actual charging/discharging
 * state as reported by the kernel; some hardware never reports
 * "Full" even sitting at 100% on AC, so going by the percentage alone
 * is more reliable across backends.
 *
 * @param backend_type      Which kernel interface to read
 * @param backend_number    Which battery to read when a system has
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
 * @note Complexity: @e O(1), a small, fixed number of short file
 *       reads regardless of system state
 */
void battery_status_read(enum config_battery_backend_type_e backend_type,
        uint32_t backend_number,
        uint32_t threshold_charged, uint32_t threshold_low,
        uint32_t threshold_critical,
        char *out, size_t out_size);


#endif  /* ! SYSTRAY_BATTERY_H */
