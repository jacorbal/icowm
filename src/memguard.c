/**
 * @file memguard.c
 *
 * @brief Restricted-memory mode's runtime watchdog and dynamic
 *        client-count cap implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* CLOCK_MONOTONIC, clock_gettime */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* NULL, snprintf */
#include <time.h>       /* clock_gettime, struct timespec, time_t */

/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/dialog.h>
#include <defs/memguard.h>
#include <defs/uistr.h>

/* Utils includes */
#include <utils/sysmem.h>
#include <utils/time/clock.h>

/* Menu includes */
#include <menu/dialog/message.h>

/* Project includes */
#include <config.h>
#include <i18n.h>
#include <logger.h>
#include <stage.h>

/* Local includes */
#include <memguard.h>


/** Ceiling the watchdog checks against; @c 0 means it is off */
static uint32_t s_ceiling_mib = 0u;

/** Client cap computed for the current ceiling by @c memguard_init;
 *  @c 0 means it is off, the same as @c s_ceiling_mib */
static uint32_t s_max_clients = 0u;

/** Wall-clock time of the last actual usage check */
static struct timespec s_last_check;

/** Whether a warning is currently showing for the present climb
 *  above the ceiling; see @c MEMGUARD_HYSTERESIS_PERCENT */
static bool s_warned = false;


/**
 * @brief Show a message dialog on this module's behalf, unless one is
 *        already open or a required parameter is missing
 *
 * Both @a memguard_tick and @a memguard_warn_client_cap need exactly
 * this same guard-then-show sequence around a message that is
 * otherwise entirely their (built with a different format and
 * arguments, logged with a different message); this is the part that
 * was actually identical between the two.
 *
 * @param connection XCB connection
 * @param stage      Stage to center the dialog on
 * @param config     Active configuration, for the dialog
 * @param level      Alert level to show the dialog at
 * @param message    Already-built message text
 *
 * @note Complexity: @e O(1)
 */
static void s_memguard_show_dialog(xcb_connection_t *connection,
        stage_td *stage, const config_td *config,
        menu_msg_level_e level, const char *message)
{
    if (connection == NULL || stage == NULL || config == NULL ||
            menu_message_dialog_is_open()) {
        return;
    }
    menu_message_dialog_show(connection, stage, config,
            message, level);
}


/* Set the ceiling this module checks against, and compute the client
 * cap that ceiling affords */
void memguard_init(uint32_t ceiling_mib)
{
    uint32_t budget_mib;
    uint32_t n;

    s_ceiling_mib = ceiling_mib;
    s_warned = false;
    if (clock_gettime(CLOCK_MONOTONIC, &s_last_check) != 0) {
        s_last_check.tv_sec = 0;
        s_last_check.tv_nsec = 0;
    }

    if (ceiling_mib == 0u) {
        s_max_clients = 0u;
        return;
    }

    budget_mib = (ceiling_mib > MEMGUARD_BASELINE_MIB)
        ? (ceiling_mib - MEMGUARD_BASELINE_MIB) : 0u;
    n = (budget_mib * 1024u) / MEMGUARD_KIB_PER_CLIENT;

    if (n < MEMGUARD_MIN_CLIENTS) {
        n = MEMGUARD_MIN_CLIENTS;
    } else if (n > MEMGUARD_ABSOLUTE_MAX_CLIENTS) {
        n = MEMGUARD_ABSOLUTE_MAX_CLIENTS;
    }
    s_max_clients = n;

    LOGGER_NOTICE("Restricted-memory mode: computed a client cap of" \
            " %u window(s) for a %u MiB ceiling",
            (unsigned int) s_max_clients, (unsigned int) ceiling_mib);
}


/* The maximum number of clients any one desktop should hold under
 * the current ceiling */
uint32_t memguard_max_clients(void)
{
    return s_max_clients;
}


/* Periodically check memory usage against the configured ceiling */
void memguard_tick(xcb_connection_t *connection,
        stage_td *stage, const config_td *config)
{
    struct timespec now;
    uint32_t rss_mib;
    uint32_t hysteresis_floor_mib;
    char message[DIALOG_MSG_RAW_MAX_LENGTH];

    if (s_ceiling_mib == 0u || connection == NULL || stage == NULL ||
            config == NULL) {
        return;
    }

    /* Never compete with a dialog already on screen, restricted-memory
     * warning or otherwise: showing a second one on top would be
     * confusing, and 's_memguard_show_dialog' below would just silently
     * refuse it anyway (only one instance at a time).  Caught here too,
     * ahead of that, so the whole rest of this function (the clock
     * read, the RSS read, &c.) is skipped as well, not just the dialog
     * itself. */
    if (menu_message_dialog_is_open()) {
        return;
    }

    if (clock_ms_since(&s_last_check) <
            (long) MEMGUARD_CHECK_INTERVAL_SECONDS * 1000L) {
        return;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return;
    }
    s_last_check = now;

    if (!sysmem_self_rss_mib(&rss_mib)) {
        return;
    }

    hysteresis_floor_mib =
        (s_ceiling_mib * MEMGUARD_HYSTERESIS_PERCENT) / 100u;
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
            _(STR_MEMGUARD_CEILING_REACHED_FMT),
            (unsigned int) rss_mib, (unsigned int) s_ceiling_mib);
    s_memguard_show_dialog(connection, stage, config,
            MENU_MSG_LEVEL_ERROR, message);
    s_warned = true;
}


/* Warn through a message dialog that 'memguard_max_clients' has been
 * reached on a desktop */
void memguard_warn_client_cap(xcb_connection_t *connection,
        stage_td *stage, const config_td *config)
{
    char message[DIALOG_MSG_RAW_MAX_LENGTH];

    if (connection == NULL || stage == NULL || config == NULL) {
        return;
    }

    LOGGER_WARNING("Restricted-memory mode: reached the computed" \
            " limit of %u window(s) on this desktop",
            (unsigned int) s_max_clients);

    (void) snprintf(message, sizeof(message),
            _(STR_MEMGUARD_CLIENT_CAP_REACHED_FMT),
            (unsigned int) s_max_clients);
    s_memguard_show_dialog(connection, stage, config,
            MENU_MSG_LEVEL_WARNING, message);
}
