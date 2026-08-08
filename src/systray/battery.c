/**
 * @file systray/battery.c
 *
 * @brief Battery status reading and formatting implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Local includes */
#include <systray/battery.h>


/**
 * @brief Read one small text file into @p out, trimming the trailing
 *        newline if present
 *
 * @param path    File to read
 * @param out     Buffer to receive the file's contents
 * @param out_size Size of @p out in bytes
 *
 * @return @c true if the file was opened and at least one byte read
 *
 * @note Complexity: @e O(1), a single short read
 */
static bool s_battery_read_line(const char *path, char *out,
        size_t out_size)
{
    FILE *f;
    size_t len;

    if (path == NULL || out == NULL || out_size == 0u) {
        return false;
    }

    f = fopen(path, "r");
    if (f == NULL) {
        return false;
    }

    if (fgets(out, (int) out_size, f) == NULL) {
        fclose(f);
        return false;
    }
    fclose(f);

    len = safe_strlen(out);
    if (len > 0u && out[len - 1u] == '\n') {
        out[len - 1u] = '\0';
    }

    return true;
}


/**
 * @brief Read an integer percentage (0-100) from a small sysfs file
 *
 * @param path   File expected to hold a plain integer
 * @param out    Receives the parsed value
 *
 * @return @c true if the file was read and held a valid integer
 *
 * @note Complexity: @e O(1)
 */
static bool s_battery_read_uint(const char *path, uint32_t *out)
{
    char line[BATTERY_LINE_MAX_LEN];
    char *end;
    long value;

    if (out == NULL || !s_battery_read_line(path, line, sizeof(line))) {
        return false;
    }

    value = strtol(line, &end, 10);
    if (end == line || value < 0) {
        return false;
    }

    *out = (uint32_t) value;
    return true;
}


/**
 * @brief Whether any @c /sys/class/power_supply entry of type
 *        @c "Mains" currently reports @c online = 1
 *
 * Scans every entry rather than assuming a fixed name (@c AC,
 * @c ACAD, @c ADP0, @c ADP1, and others are all in use across
 * different hardware and kernel versions), stopping as soon as one
 * online supply is found.
 *
 * @return @c true if AC power is connected
 *
 * @note Complexity: @e O(n), where @e n is the number of power supply
 *       entries on the system (typically well under 10)
 */
static bool s_battery_acpi_ac_online(void)
{
    DIR *dir;
    struct dirent *entry;
    bool online = false;

    dir = opendir(BATTERY_ACPI_BASE_DIR);
    if (dir == NULL) {
        return false;
    }

    while (!online && (entry = readdir(dir)) != NULL) {
        char type_path[256];
        char type_line[BATTERY_LINE_MAX_LEN];
        char online_path[256];
        uint32_t online_val;

        if (entry->d_name[0] == '.') {
            continue;
        }

        (void) snprintf(type_path, sizeof(type_path), "%s/%s/type",
                BATTERY_ACPI_BASE_DIR, entry->d_name);
        if (!s_battery_read_line(type_path, type_line,
                    sizeof(type_line))) {
            continue;
        }
        if (safe_strcmp(type_line, "Mains") != 0) {
            continue;
        }

        (void) snprintf(online_path, sizeof(online_path),
                "%s/%s/online", BATTERY_ACPI_BASE_DIR,
                entry->d_name);
        if (s_battery_read_uint(online_path, &online_val) &&
                online_val != 0u) {
            online = true;
        }
    }

    closedir(dir);
    return online;
}


/**
 * @brief Read the ACPI battery at @p backend_number's charge
 *        percentage
 *
 * @param backend_number Which @c BAT<N> to read
 * @param out_percent    Receives the battery's capacity, 0-100
 *
 * @return @c true if that battery exists and its capacity was read
 *
 * @note Complexity: @e O(1)
 */
static bool s_battery_acpi_read(uint32_t backend_number,
        uint32_t *out_percent)
{
    char path[256];

    (void) snprintf(path, sizeof(path), "%s/BAT%u/capacity",
            BATTERY_ACPI_BASE_DIR, backend_number);

    return s_battery_read_uint(path, out_percent);
}


/**
 * @brief Read the legacy APM battery percentage and AC status
 *
 * @c /proc/apm holds one line of whitespace-separated fields:
 * @c "driver_version apm_version apm_flags ac_line_status
 * battery_status battery_flags battery_percentage battery_time units".
 * Only @c ac_line_status (@c 0x01 means AC connected) and
 * @c battery_percentage (a bare integer, optionally followed by
 * @c '%') are used here.
 *
 * @param out_percent Receives the battery's capacity, 0-100
 * @param out_ac       Receives whether AC power is connected
 *
 * @return @c true if @c /proc/apm was read and both fields parsed
 *
 * @note Complexity: @e O(1)
 */
static bool s_battery_apm_read(uint32_t *out_percent, bool *out_ac)
{
    char line[BATTERY_LINE_MAX_LEN * 2];
    char driver_version[16];
    char apm_version[16];
    char apm_flags[16];
    char ac_line_status[16];
    char battery_status[16];
    char battery_flags[16];
    char battery_percentage[16];
    long ac_value;
    long percent_value;
    char *end;

    if (out_percent == NULL || out_ac == NULL ||
            !s_battery_read_line(BATTERY_APM_PROC_FILE, line,
                sizeof(line))) {
        return false;
    }

    if (sscanf(line, "%15s %15s %15s %15s %15s %15s %15s",
                driver_version, apm_version, apm_flags,
                ac_line_status, battery_status, battery_flags,
                battery_percentage) != 7) {
        return false;
    }

    ac_value = strtol(ac_line_status, &end, 16);
    if (end == ac_line_status) {
        return false;
    }
    *out_ac = (ac_value == 1);

    percent_value = strtol(battery_percentage, &end, 10);
    if (end == battery_percentage || percent_value < 0) {
        return false;
    }
    *out_percent = (uint32_t) percent_value;

    return true;
}


/* Read and format the current battery/AC status */
void battery_status_read(enum config_battery_backend_type_e backend_type,
        uint32_t backend_number,
        uint32_t threshold_charged, uint32_t threshold_low,
        uint32_t threshold_critical,
        char *out, size_t out_size)
{
    uint32_t percent = 0u;
    bool ac = false;
    bool found;

    if (out == NULL || out_size == 0u) {
        return;
    }

    if (backend_type == CONFIG_BATTERY_BACKEND_APM) {
        found = s_battery_apm_read(&percent, &ac);
    } else {
        found = s_battery_acpi_read(backend_number, &percent);
        if (found) {
            ac = s_battery_acpi_ac_online();
        }
    }

    if (!found) {
        (void) safe_strncpy(out, "N/A", out_size - 1u);
        out[out_size - 1u] = '\0';
        return;
    }

    if (percent >= threshold_charged) {
        (void) snprintf(out, out_size, "%s", (ac) ? "Full AC" : "Full");
        return;
    }

    if (ac) {
        (void) snprintf(out, out_size, "%u%% AC", percent);
    } else if (percent <= threshold_critical) {
        (void) snprintf(out, out_size, "%u%%!!", percent);
    } else if (percent <= threshold_low) {
        (void) snprintf(out, out_size, "%u%%!", percent);
    } else {
        (void) snprintf(out, out_size, "%u%%", percent);
    }
}
