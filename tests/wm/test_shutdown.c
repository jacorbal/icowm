/**
 * @file tests/wm/test_shutdown.c
 *
 * @brief Test battery for the coordinated shutdown state machine
 *
 * wm_shutdown_begin/wm_shutdown_tick/wm_shutdown_ms_remaining live on
 * top of four cross-module dependencies, each stubbed here as a
 * controllable, recording link-only stand-in rather than linked for
 * real: wm_for_each_client (its own real walk over cdlist/ohtbl
 * structures already has dedicated coverage in
 * tests/wm/test_clients.c, so reimplementing that walk here would
 * only duplicate it), ccmd_client_close and ccmd_client_kill (each a
 * whole client-teardown subsystem, e.g., a live ICCCM
 * WM_DELETE_WINDOW round trip or a real process signal, neither of
 * which this file has any live X connection or client process for),
 * and wm_request_stop (whose real body, wm.c, shares a translation
 * unit with wm_start/s_wm_cleanup's roughly forty unrelated external
 * symbols, none of which have anything to do with the two-line
 * null-check-and-field-write this file needs to exercise).  This
 * file's own job is the timeout/countdown/re-entrancy state machine
 * that wm/shutdown.c itself owns, driven entirely through those four
 * seams.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* Local includes */
#include <client.h>
#include <config.h>
#include <harness/tap.h>
#include <logger.h>
#include <wm/internal.h>
#include <wm/shutdown.h>


/** This file owns the one wm_td singleton this whole file works
 *  through, in place of the real global wm.c defines; wm_request_stop
 *  itself is stubbed too (see below), so nothing here needs wm.c's
 *  own real definition of this pointer */
wm_td *wm = NULL;

/** How many times wm_request_stop was called while wm was non-NULL,
 *  i.e., how many times a real call would have asked the main loop
 *  to stop */
static int s_request_stop_calls;

/** How many managed clients wm_for_each_client's own stand-in
 *  reports remain, and how it is told to change that count once
 *  s_stub_action runs on each of them */
static uint32_t s_remaining_clients;

/** Which action wm_shutdown_begin/wm_shutdown_tick last asked
 *  wm_for_each_client to run: 0 none, 1 close, 2 kill */
static int s_last_action_kind;

/** How many times wm_for_each_client's counting-only form
 *  (a NULL action) was called */
static int s_count_only_calls;

/** How many times ccmd_client_close was called */
static int s_close_calls;

/** How many times ccmd_client_kill was called */
static int s_kill_calls;

/**
 * @brief Link-only stand-in for wm_for_each_client
 *
 * Stubbed because its own real walking logic, over real cdlist/ohtbl
 * structures, already has dedicated coverage in
 * tests/wm/test_clients.c; reimplementing that walk here would only
 * duplicate it while obscuring wm/shutdown.c's own state-machine logic
 * under test.  Records which action shutdown.c asked for
 * (s_shutdown_close_client vs s_shutdown_kill_client, both static to
 * wm/shutdown.c and therefore only distinguishable here by pointer
 * identity against the two real link seams below), and reports the
 * test-controlled remaining-client count.
 *
 * @param wm_param Unused, other than matching the real signature
 * @param action   NULL for a counting-only call, otherwise one of
 *                 wm/shutdown.c's own two static per-client actions
 * @param userdata Unused
 *
 * @return The test-controlled remaining-client count
 *
 * @note Complexity: O(1)
 */
uint32_t wm_for_each_client(const wm_td *wm_param,
        void (*action)(client_td *client, void *userdata),
        void *userdata)
{
    (void) wm_param;
    (void) userdata;

    if (action == NULL) {
        s_count_only_calls++;
        return s_remaining_clients;
    }

    /* wm/shutdown.c's own two static adapters, s_shutdown_close_client
     * and s_shutdown_kill_client, each call exactly one of the two
     * real link seams below once per client this stand-in "walks";
     * since s_remaining_clients is this stand-in's own fictional
     * count rather than a real list, one representative call per
     * wm_for_each_client invocation, kind identified by which
     * adapter ran, is enough to prove which close-vs-kill path
     * shutdown.c chose without reimplementing a real walk */
    action((client_td *) NULL, userdata);

    return s_remaining_clients;
}


/**
 * @brief Link-only stand-in for ccmd_client_close
 *
 * Reached only through wm/shutdown.c's own static
 * s_shutdown_close_client adapter, itself only reachable through the
 * wm_for_each_client stand-in above; a real close call is a whole
 * ICCCM WM_DELETE_WINDOW round trip this file has no live X
 * connection for.
 *
 * @param client Unused; the stand-in above always passes NULL
 *
 * @note Complexity: O(1)
 */
void ccmd_client_close(client_td *client)
{
    (void) client;

    s_close_calls++;
    s_last_action_kind = 1;
}


/** Link-only stand-ins for the two desktop lookups the gathering
 *  step makes; @c NULL leaves it with nothing to move between
 * @note Complexity: O(1) */
surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;

    return NULL;
}


desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;

    return NULL;
}


desktop_td *wm_get_client_desktop(const client_td *client)
{
    (void) client;

    return NULL;
}


/** Recording stand-ins for the gathering step: this file has no
 *  desktops or viewport of its own, so what is recorded is that the
 *  gather asked for each move, and on which client
 * @note Complexity: O(1) */
static int s_gather_restore_calls;
static int s_gather_desktop_calls;
static int s_gather_page_calls;

void enact_client_restore(client_td *client)
{
    (void) client;
    s_gather_restore_calls++;
}


void enact_desktop_client_send(const desktop_td *desktop,
        client_td *client, desktop_td *target)
{
    (void) desktop;
    (void) client;
    (void) target;
    s_gather_desktop_calls++;
}


void enact_client_send_to_page(surface_td *surface, client_td *client,
        uint32_t col, uint32_t row)
{
    (void) surface;
    (void) client;
    (void) col;
    (void) row;
    s_gather_page_calls++;
}


bool scmd_surface_viewport_desktop_page(const surface_td *surface,
        const desktop_td *desktop, uint32_t *col_out, uint32_t *row_out)
{
    (void) surface;
    (void) desktop;
    (void) col_out;
    (void) row_out;

    /* No viewport in this file: the page move is never asked for */
    return false;
}


/**
 * @brief Link-only stand-in for ccmd_client_kill
 *
 * Reached only through wm/shutdown.c's own static
 * s_shutdown_kill_client adapter; a real kill call sends a real
 * signal to a real client process, which this file has none of.
 *
 * @param client Unused; the stand-in above always passes NULL
 *
 * @note Complexity: O(1)
 */
void ccmd_client_kill(client_td *client)
{
    (void) client;

    s_kill_calls++;
    s_last_action_kind = 2;
}


/**
 * @brief Link-only stand-in for wm_request_stop
 *
 * The real wm_request_stop (wm.c) shares its translation unit with
 * wm_start/s_wm_cleanup, whose own bodies alone pull in roughly forty
 * unrelated external symbols (config, session, rendering, IPC,
 * systray, xsettings, and more) that have nothing to do with
 * wm/shutdown.c's own state machine; linking the real one here would
 * mean stubbing all of those just to reach a two-line null check and
 * field write.  This stand-in reproduces that exact contract: a
 * NULL wm is a no-op that reports failure, otherwise the running flag
 * is cleared and the call is tallied.
 *
 * @return 1 if wm is NULL, 0 otherwise, matching the real function
 *
 * @note Complexity: O(1)
 */
int wm_request_stop(void)
{
    if (wm == NULL) {
        return 1;
    }

    wm->is_running = false;
    s_request_stop_calls++;
    return 0;
}


/**
 * @brief Link-only stand-in for logger_msg
 *
 * A silent no-op, matching the real logger's own behavior whenever
 * logger_start has never run (its first check is 'logger == NULL'),
 * the same reasoning tests/wm/test_lifecycle.c already documents for
 * never calling logger_start at all here either.
 *
 * @note Complexity: O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/**
 * @brief Reset every recording stand-in's state ahead of one scenario
 *
 * @note Complexity: O(1)
 */
static void s_reset(void)
{
    s_remaining_clients = 0u;
    s_request_stop_calls = 0;
    s_last_action_kind = 0;
    s_count_only_calls = 0;
    s_close_calls = 0;
    s_kill_calls = 0;
}


/* With no managed clients at all, wm_shutdown_begin stops the window
 * manager immediately, with no wait and nothing published to the
 * shutdown-in-progress state at all */
static void s_test_begin_no_clients_stops_immediately(void)
{
    wm_td local_wm;
    config_td config;

    s_reset();
    memset(&local_wm, 0, sizeof(local_wm));
    memset(&config, 0, sizeof(config));
    config.base.shutdown.timeout_seconds = 15u;
    local_wm.config = &config;
    local_wm.is_running = true;
    wm = &local_wm;
    s_remaining_clients = 0u;

    wm_shutdown_begin(wm);

    TAP_EQ_INT(s_count_only_calls, 1,
            "wm_shutdown_begin first counts clients with a NULL"
            " action before deciding anything");
    TAP_EQ_INT(s_close_calls, 0,
            "with nothing to wait for, no close is ever requested");
    TAP_OK(!wm->is_running,
            "and the window manager is asked to stop right away");
    TAP_EQ_INT(wm_shutdown_ms_remaining(), -1,
            "no shutdown was ever put in progress, so there is"
            " nothing left to count down");

    wm = NULL;
}


/* With managed clients present, wm_shutdown_begin asks every one of
 * them to close and starts the countdown, without stopping the
 * window manager yet */
static void s_test_begin_with_clients_starts_countdown(void)
{
    wm_td local_wm;
    config_td config;

    s_reset();
    memset(&local_wm, 0, sizeof(local_wm));
    memset(&config, 0, sizeof(config));
    config.base.shutdown.timeout_seconds = 30u;
    local_wm.config = &config;
    local_wm.is_running = true;
    wm = &local_wm;
    s_remaining_clients = 2u;

    wm_shutdown_begin(wm);

    TAP_EQ_INT(s_close_calls, 1,
            "every managed client is asked to close through the"
            " real ccmd_client_close seam");
    TAP_EQ_INT(s_last_action_kind, 1,
            "specifically the close action, not the kill one");
    TAP_OK(wm->is_running,
            "the window manager itself is not stopped yet, since"
            " clients still need time to close on their own");
    TAP_OK(wm_shutdown_ms_remaining() > 0,
            "a positive countdown is now in progress, derived from"
            " the configured timeout");

    wm = NULL;
}


/* Calling wm_shutdown_begin a second time while one is already in
 * progress is a complete no-op: no second round of closes, no
 * countdown reset */
static void s_test_begin_reentrant_call_is_noop(void)
{
    wm_td local_wm;
    config_td config;
    int first_remaining;

    s_reset();
    memset(&local_wm, 0, sizeof(local_wm));
    memset(&config, 0, sizeof(config));
    config.base.shutdown.timeout_seconds = 30u;
    local_wm.config = &config;
    local_wm.is_running = true;
    wm = &local_wm;
    s_remaining_clients = 2u;

    wm_shutdown_begin(wm);
    first_remaining = wm_shutdown_ms_remaining();
    s_close_calls = 0;

    wm_shutdown_begin(wm);

    TAP_EQ_INT(s_close_calls, 0,
            "a second wm_shutdown_begin call while one is already"
            " running never asks any client to close again");
    TAP_OK(wm_shutdown_ms_remaining() <= first_remaining,
            "and the countdown is never reset back up by the"
            " redundant call");

    /* Drains the in-progress state this scenario left behind so the
     * next scenario starts from a clean slate: wm_shutdown_tick with
     * zero remaining clients finishes the shutdown outright */
    s_remaining_clients = 0u;
    wm_shutdown_tick(wm);
    wm = NULL;
}


/* wm_shutdown_tick before any shutdown was ever begun is a pure
 * no-op: no counting, no closes, no kills, no stop request */
static void s_test_tick_without_begin_is_noop(void)
{
    wm_td local_wm;

    s_reset();
    memset(&local_wm, 0, sizeof(local_wm));
    local_wm.is_running = true;
    wm = &local_wm;

    wm_shutdown_tick(wm);

    TAP_EQ_INT(s_count_only_calls, 0,
            "wm_shutdown_tick never even counts clients when no"
            " shutdown is currently in progress");
    TAP_OK(wm->is_running,
            "and the window manager is left running untouched");

    wm = NULL;
}


/* wm_shutdown_tick, called while clients remain open and the
 * timeout has not yet elapsed, does nothing: no kill, no stop */
static void s_test_tick_clients_remaining_time_left(void)
{
    wm_td local_wm;
    config_td config;

    s_reset();
    memset(&local_wm, 0, sizeof(local_wm));
    memset(&config, 0, sizeof(config));
    config.base.shutdown.timeout_seconds = 30u;
    local_wm.config = &config;
    local_wm.is_running = true;
    wm = &local_wm;
    s_remaining_clients = 2u;

    wm_shutdown_begin(wm);
    s_close_calls = 0;
    s_count_only_calls = 0;

    wm_shutdown_tick(wm);

    TAP_EQ_INT(s_count_only_calls, 1,
            "the tick counts how many clients remain");
    TAP_EQ_INT(s_kill_calls, 0,
            "with time left on the clock, nothing is force-killed");
    TAP_OK(wm->is_running,
            "and the window manager keeps waiting rather than"
            " stopping yet");
    TAP_OK(wm_shutdown_ms_remaining() > 0,
            "the countdown is still positive, unaffected by this tick");

    s_remaining_clients = 0u;
    wm_shutdown_tick(wm);
    wm = NULL;
}


/* Once every client has closed on its own, the next tick finishes
 * the shutdown and stops the window manager, without ever force
 * killing anything */
static void s_test_tick_all_clients_closed_stops(void)
{
    wm_td local_wm;
    config_td config;

    s_reset();
    memset(&local_wm, 0, sizeof(local_wm));
    memset(&config, 0, sizeof(config));
    config.base.shutdown.timeout_seconds = 30u;
    local_wm.config = &config;
    local_wm.is_running = true;
    wm = &local_wm;
    s_remaining_clients = 1u;

    wm_shutdown_begin(wm);
    s_remaining_clients = 0u;

    wm_shutdown_tick(wm);

    TAP_EQ_INT(s_kill_calls, 0,
            "clients that already closed themselves are never"
            " force-killed");
    TAP_OK(!wm->is_running,
            "the window manager is stopped as soon as the last"
            " client is gone");
    TAP_EQ_INT(wm_shutdown_ms_remaining(), -1,
            "and the shutdown is no longer considered in progress"
            " afterward");

    wm = NULL;
}


/* Once the configured timeout has actually elapsed with clients
 * still open, the next tick force-kills every remaining one and
 * stops the window manager regardless */
static void s_test_tick_timeout_elapsed_force_kills(void)
{
    wm_td local_wm;
    config_td config;

    s_reset();
    memset(&local_wm, 0, sizeof(local_wm));
    memset(&config, 0, sizeof(config));
    /* A timeout of zero seconds makes clock_gettime's own deadline,
     * computed the instant wm_shutdown_begin runs, already in the
     * past by the time wm_shutdown_tick checks it moments later:
     * the real clock_ms_until (utils/time/clock.c) clamps a past
     * deadline to zero, which wm_shutdown_tick treats as elapsed */
    config.base.shutdown.timeout_seconds = 0u;
    local_wm.config = &config;
    local_wm.is_running = true;
    wm = &local_wm;
    s_remaining_clients = 3u;

    wm_shutdown_begin(wm);
    s_close_calls = 0;

    wm_shutdown_tick(wm);

    TAP_EQ_INT(s_kill_calls, 1,
            "a client still open past the deadline is force-killed"
            " through the real ccmd_client_kill seam");
    TAP_EQ_INT(s_last_action_kind, 2,
            "specifically the kill action, not another close");
    TAP_OK(!wm->is_running,
            "and the window manager is stopped regardless of any"
            " clients that refused to close in time");
    TAP_EQ_INT(wm_shutdown_ms_remaining(), -1,
            "with the shutdown no longer in progress afterward");

    wm = NULL;
}


/* A NULL config falls back to a fifteen-second default timeout
 * rather than crashing or leaving the deadline uninitialized */
static void s_test_begin_null_config_uses_default_timeout(void)
{
    wm_td local_wm;

    s_reset();
    memset(&local_wm, 0, sizeof(local_wm));
    local_wm.config = NULL;
    local_wm.is_running = true;
    wm = &local_wm;
    s_remaining_clients = 1u;

    wm_shutdown_begin(wm);

    TAP_OK(wm_shutdown_ms_remaining() > 0,
            "a NULL config still starts a real countdown, using the"
            " fifteen-second default rather than leaving the"
            " deadline unset");

    s_remaining_clients = 0u;
    wm_shutdown_tick(wm);
    wm = NULL;
}


int main(void)
{
    TAP_PLAN(24);

    s_test_begin_no_clients_stops_immediately();
    s_test_begin_with_clients_starts_countdown();
    s_test_begin_reentrant_call_is_noop();
    s_test_tick_without_begin_is_noop();
    s_test_tick_clients_remaining_time_left();
    s_test_tick_all_clients_closed_stops();
    s_test_tick_timeout_elapsed_force_kills();
    s_test_begin_null_config_uses_default_timeout();

    return TAP_DONE();
}
