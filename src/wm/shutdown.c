/**
 * @file wm/shutdown.c
 *
 * @brief Coordinated shutdown implementation
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
#include <stdlib.h>     /* malloc, free */
#include <time.h>       /* clock_gettime, struct timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Project includes */
#include <client.h>
#include <cmds/client/focus.h>
#include <cmds/surface.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Utils includes */
#include <utils/time/clock.h>

/* Local includes */
#include <wm/internal.h>
#include <wm/shutdown.h>


/** Whether a coordinated shutdown is currently in progress */
static bool s_shutdown_in_progress = false;

/** Absolute deadline (@c CLOCK_MONOTONIC) past which remaining clients
 * are force-closed regardless */
static struct timespec s_shutdown_deadline;


/**
 * @brief What @a s_shutdown_gather_collect is filling in
 */
struct s_gather_ctx_s {
    client_td **clients;    /**< Room for @p capacity pointers */
    uint32_t capacity;      /**< How many @p clients can hold */
    uint32_t count;         /**< How many it holds so far */
};


/**
 * @brief Record one client, rather than acting on it there and then
 *
 * @a wm_for_each_client walks each desktop's own client table live, and
 * bringing a client over moves it from one of those tables to another,
 * which would reorder the very table being walked underneath it: with
 * open addressing that skips entries and revisits others, so some
 * clients would never be asked to close and the shutdown would sit out
 * its whole timeout waiting for them.  The pointers are collected first
 * and acted on afterwards, once the walk is over.
 *
 * @param client   Client to record
 * @param userdata The @c s_gather_ctx_s being filled
 *
 * @note Silently drops anything past @p capacity, which cannot happen
 *       while the caller sizes the array from the same walk
 * @note Complexity: @e O(1)
 */
static void s_shutdown_gather_collect(client_td *client, void *userdata)
{
    struct s_gather_ctx_s *const ctx = userdata;

    if (ctx == NULL || client == NULL || ctx->count >= ctx->capacity) {
        return;
    }

    ctx->clients[ctx->count] = client;
    ctx->count++;
}


/**
 * @brief Adapts @c ccmd_client_close to @c wm_for_each_client's
 *        action signature
 *
 * @param client   Client to close
 * @param userdata Unused
 *
 * @note Complexity: @e O(1)
 */
static void s_shutdown_close_client(client_td *client, void *userdata)
{
    (void) userdata;
    ccmd_client_close(client);
}


/**
 * @brief Adapts @c ccmd_client_kill to @c wm_for_each_client's
 *        action signature
 *
 * @param client   Client to kill
 * @param userdata Unused
 *
 * @note Complexity: @e O(1)
 */
static void s_shutdown_kill_client(client_td *client, void *userdata)
{
    (void) userdata;
    ccmd_client_kill(client);
}


/* Bring one client to the desktop and viewport page being looked at */
void wm_shutdown_gather_client(client_td *client)
{
    surface_td *surface;
    desktop_td *desktop;
    uint32_t col;
    uint32_t row;

    if (client == NULL) {
        return;
    }

    surface = wm_get_surface_by_id(client->screen_id);
    if (surface == NULL) {
        return;
    }

    desktop = surface_desktop_get(surface, surface->desktop_cur);
    if (desktop == NULL) {
        return;
    }

    /* An iconified client shows nothing at all, dialog included, so
     * it is put back on screen before being asked to close */
    if (client_is_iconified(client)) {
        enact_client_restore(client);
    }

    if (!client_is_pinned(client)) {
        const desktop_td *const from = wm_get_client_desktop(client);

        if (from != NULL && from != desktop) {
            enact_desktop_client_send(from, client, desktop);
        }
    }

    if (!client_is_sticky(client) &&
            scmd_surface_viewport_desktop_page(surface, desktop,
                &col, &row)) {
        enact_client_send_to_page(surface, client, col, row);
    }
}


/* Begin a coordinated shutdown */
void wm_shutdown_begin(const wm_td *wm)
{
    struct s_gather_ctx_s ctx;
    uint32_t client_count;
    uint32_t timeout_seconds;
    config_td *config;

    if (s_shutdown_in_progress) {
        return;
    }

    client_count = wm_for_each_client(wm, NULL, NULL);
    if (client_count == 0u) {
        LOGGER_DEBUG("No managed clients to wait for;" \
                " stopping right away", L_NARG);
        (void) wm_request_stop();
        return;
    }

    LOGGER_INFO("Coordinated shutdown started;" \
            " asking every managed client to close", L_NARG);

    /* Gathered before anything is asked to close, so that a client
     * putting up a "save your work?" dialog does so with its own
     * window already in front of the user.  In two passes, since the
     * gathering moves clients between the very tables the walk reads;
     * see 's_shutdown_gather_collect'.  A failed allocation skips the
     * gathering alone: the shutdown itself still proceeds, with
     * whatever is off screen staying there, which is what it did
     * before any of this existed. */
    ctx.clients = malloc(client_count * sizeof(*ctx.clients));
    if (ctx.clients != NULL) {
        ctx.capacity = client_count;
        ctx.count = 0u;
        (void) wm_for_each_client(wm, s_shutdown_gather_collect, &ctx);
        for (uint32_t i = 0u; i < ctx.count; ++i) {
            wm_shutdown_gather_client(ctx.clients[i]);
        }
        free(ctx.clients);
    } else {
        LOGGER_WARNING("Out of memory gathering %u client(s) before" \
                " shutdown; closing them where they are",
                client_count);
    }

    (void) wm_for_each_client(wm, s_shutdown_close_client, NULL);

    config = wm_config(wm);
    timeout_seconds = (config != NULL)
        ? config->base.shutdown.timeout_seconds : 15u;

    s_shutdown_in_progress = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_shutdown_deadline);
    s_shutdown_deadline.tv_sec += (time_t) timeout_seconds;
}


/* Whether a coordinated shutdown is under way */
bool wm_shutdown_is_in_progress(void)
{
    return s_shutdown_in_progress;
}


/* Milliseconds remaining before the shutdown timeout forces closure */
int wm_shutdown_ms_remaining(void)
{
    if (!s_shutdown_in_progress) {
        return -1;
    }

    return (int) clock_ms_until(&s_shutdown_deadline);
}


/* Advance the shutdown state machine */
void wm_shutdown_tick(const wm_td *wm)
{
    uint32_t remaining;

    if (!s_shutdown_in_progress) {
        return;
    }

    remaining = wm_for_each_client(wm, NULL, NULL);
    if (remaining == 0u) {
        LOGGER_INFO("Every managed client closed;" \
                " finishing shutdown", L_NARG);
        s_shutdown_in_progress = false;
        (void) wm_request_stop();
        return;
    }

    if (clock_ms_until(&s_shutdown_deadline) > 0) {
        return;
    }

    LOGGER_NOTICE("Shutdown timeout elapsed with %u client(s)" \
            " still open; forcing them closed", remaining);
    (void) wm_for_each_client(wm, s_shutdown_kill_client, NULL);
    s_shutdown_in_progress = false;
    (void) wm_request_stop();
}
