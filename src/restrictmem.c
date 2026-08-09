/**
 * @file restrictmem.c
 *
 * @brief Restricted-memory mode's runtime watchdog implementation
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
#include <stdio.h>      /* snprintf */
#include <time.h>       /* clock_gettime, struct timespec, time_t */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <surface.h>

/* Utils includes */
#include <utils/sysmem.h>

/* Menu includes */
#include <menu/dialog/message.h>

/* Local includes */
#include <restrictmem.h>


/** Ceiling the watchdog checks against; @c 0 means it is off */
static uint32_t s_ceiling_mib = 0u;

/** Wall-clock time of the last actual usage check */
static struct timespec s_last_check;

/** Whether a warning is currently showing for the present climb
 *  above the ceiling; see @c RESTRICTMEM_HYSTERESIS_PERCENT */
static bool s_warned = false;


/* Set the ceiling the runtime watchdog checks against */
void restrictmem_init(uint32_t ceiling_mib)
{
    s_ceiling_mib = ceiling_mib;
    s_warned = false;
    if (clock_gettime(CLOCK_MONOTONIC, &s_last_check) != 0) {
        s_last_check.tv_sec = 0;
        s_last_check.tv_nsec = 0;
    }
}


/* Periodically check memory usage against the configured ceiling */
void restrictmem_tick(xcb_connection_t *connection,
        surface_td *surface, const config_td *config)
{
    struct timespec now;
    uint32_t rss_mib;
    uint32_t hysteresis_floor_mib;
    char message[DIALOG_MSG_RAW_MAX_LEN];

    if (s_ceiling_mib == 0u || connection == NULL || surface == NULL ||
            config == NULL) {
        return;
    }

    /* Never compete with a dialog already on screen, restricted-
     * memory warning or otherwise: showing a second one on top would
     * be confusing, and menu_message_dialog_show would just silently
     * refuse it anyway (only one instance at a time). */
    if (menu_message_dialog_is_open()) {
        return;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return;
    }
    if ((now.tv_sec - s_last_check.tv_sec) <
            RESTRICTMEM_CHECK_INTERVAL_SECONDS) {
        return;
    }
    s_last_check = now;

    if (!sysmem_self_rss_mib(&rss_mib)) {
        return;
    }

    hysteresis_floor_mib =
        (s_ceiling_mib * RESTRICTMEM_HYSTERESIS_PERCENT) / 100u;
    if (rss_mib < hysteresis_floor_mib) {
        s_warned = false;
        return;
    }

    if (rss_mib < s_ceiling_mib || s_warned) {
        return;
    }

    LOGGER_WARNING("Restricted-memory mode: reached the configured" \
            " ceiling (using %u MiB of %u MiB)",
            (unsigned int) rss_mib, (unsigned int) s_ceiling_mib);

    (void) snprintf(message, sizeof(message),
            "IcoWM has reached its configured memory ceiling: using" \
            " %u MiB of the %u MiB allowed (see the -M command-line" \
            " option).  Close some windows to free up memory before" \
            " opening more.",
            (unsigned int) rss_mib, (unsigned int) s_ceiling_mib);
    menu_message_dialog_show(connection, surface, config, message,
            MENU_MSG_LEVEL_ERROR);
    s_warned = true;
}


/* Warn through a message dialog that RESTRICTMEM_MAX_CLIENTS has been
 * reached on a desktop */
void restrictmem_warn_client_cap(xcb_connection_t *connection,
        surface_td *surface, const config_td *config)
{
    char message[DIALOG_MSG_RAW_MAX_LEN];

    if (connection == NULL || surface == NULL || config == NULL ||
            menu_message_dialog_is_open()) {
        return;
    }

    LOGGER_WARNING("Restricted-memory mode: reached the" \
            " RESTRICTMEM_MAX_CLIENTS limit of %u window(s) on this" \
            " desktop", (unsigned int) RESTRICTMEM_MAX_CLIENTS);

    (void) snprintf(message, sizeof(message),
            "IcoWM is running in restricted-memory mode and will not" \
            " manage more than %u window(s) at once (see the -M" \
            " command-line option).  Close a window before opening" \
            " another.",
            (unsigned int) RESTRICTMEM_MAX_CLIENTS);
    menu_message_dialog_show(connection, surface, config, message,
            MENU_MSG_LEVEL_WARNING);
}
