/**
 * @file tests/cctl/test_adopt.c
 *
 * @brief Test battery for the window manager startup scan's own
 *        dispatch logic (cctl/adopt.c)
 *
 * cctl/adopt.c holds exactly one non-static, externally reachable
 * function, cctl_adopt_scan, and two file-local static helpers,
 * s_adopt_one_window and s_adopt_scan_stage, that are only ever
 * reached through it.  Both static helpers are genuinely X-bound:
 * s_adopt_scan_stage issues a real xcb_query_tree round trip and a
 * batch of xcb_get_window_attributes requests against a live X
 * connection before it ever calls s_adopt_one_window, and
 * s_adopt_one_window itself goes on to call client_init (which itself
 * performs several more X requests) and client_send_synthetic_
 * configure_notify.  Neither static helper's own logic can be reached
 * or observed without a real xcb_connection_t answering real
 * requests, so neither is exercised here; this file instead covers
 * exactly what is genuinely cctl_adopt_scan's own: the null-wm guard
 * clause, and the per-stage loop's own skip logic for a null stage
 * pointer or a stage with a null screen, neither of which ever
 * reaches s_adopt_scan_stage at all.
 *
 * cctl/adopt.c's own translation unit is linked for real.  wm/
 * instance.c is also linked for real, since wm_connection and
 * wm_stages are themselves tiny, pure, null-tolerant accessors
 * whose own behavior (in particular, wm_connection(NULL) safely
 * returning NULL, which is exactly what lets cctl_adopt_scan's own
 * guard clause be reached at all despite dereferencing wm through
 * wm_connection before its own null check) is worth proving directly
 * rather than assuming.  adt/list.c is linked for real to build an
 * actual stages list.  Every other external symbol cctl/adopt.c's
 * translation unit references is a harmless link-only stand-in below,
 * since none of them is ever reached by any scenario this file
 * actually drives: s_adopt_scan_stage, the only call site for
 * xcb_query_tree, xcb_get_window_attributes, and client_init, is never
 * entered by a stage this file ever supplies (every one either has
 * a null screen, so cctl_adopt_scan's own loop skips it outright, or
 * the stages list is empty), so those stand-ins exist purely so the
 * translation unit links, never because a scenario here invokes them.
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
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <rules.h>
#include <stage.h>
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <cctl/adopt.h>
#include <harness/tap.h>


/* Recording state for the logger_msg stand-in */
static int s_logger_call_count;

/** Link-only stand-in for logger_msg (logger.c): records that it was
 *  reached instead of formatting or emitting anything, matching the
 *  real logger's own silent-no-op behavior whenever logger_start has
 *  never run */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    s_logger_call_count++;
    return 0;
}


/* Every stand-in below is reached only if cctl_adopt_scan's own loop
 * ever entered s_adopt_scan_stage, which no scenario in this file
 * ever causes; each is a harmless, never-invoked link-only stand-in
 * purely so cctl/adopt.c's one translation unit links at all */

/** Link-only stand-in for lookup_current_desktop (lookup.c) */
desktop_td *lookup_current_desktop(stage_td *stage)
{
    (void) stage;
    return NULL;
}


/** Link-only stand-in for client_init (client.c) */
client_td *client_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        const config_td *config)
{
    (void) connection;
    (void) ewmh;
    (void) window;
    (void) config;
    return NULL;
}


/** Link-only stand-in for client_send_synthetic_configure_notify
 *  (client.c) */
void client_send_synthetic_configure_notify(xcb_connection_t *connection,
        const client_td *client)
{
    (void) connection;
    (void) client;
}


/** Link-only stand-in for desktop_action_client_add (desktop.c) */
int desktop_action_client_add(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;
    return 0;
}


/** Link-only stand-in for rules_apply (rules.c) */
bool rules_apply(const wm_td *wm, client_td *client, stage_td **stage,
        desktop_td **desktop, enum rules_trigger_e trigger)
{
    (void) wm;
    (void) client;
    (void) stage;
    (void) desktop;
    (void) trigger;
    return false;
}


/** Link-only stand-in for stage_workarea_refresh_all (stage.c) */
void stage_workarea_refresh_all(stage_td *stage)
{
    (void) stage;
}


/** Link-only stand-in for xcb_change_property (libxcb) */
xcb_void_cookie_t xcb_change_property(xcb_connection_t *c, uint8_t mode,
        xcb_window_t window, xcb_atom_t property, xcb_atom_t type,
        uint8_t format, uint32_t data_len, const void *data)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) mode;
    (void) window;
    (void) property;
    (void) type;
    (void) format;
    (void) data_len;
    (void) data;
    return cookie;
}


/** Link-only stand-in for xcb_query_tree (libxcb) */
xcb_query_tree_cookie_t xcb_query_tree(xcb_connection_t *c,
        xcb_window_t window)
{
    xcb_query_tree_cookie_t cookie = {0};

    (void) c;
    (void) window;
    return cookie;
}


/** Link-only stand-in for xcb_query_tree_reply (libxcb) */
xcb_query_tree_reply_t *xcb_query_tree_reply(xcb_connection_t *c,
        xcb_query_tree_cookie_t cookie, xcb_generic_error_t **e)
{
    (void) c;
    (void) cookie;
    (void) e;
    return NULL;
}


/** Link-only stand-in for xcb_flush (libxcb) */
int xcb_flush(xcb_connection_t *c)
{
    (void) c;
    return 1;
}


/** Link-only stand-in for xcb_reply_log_error (utils/xcb/reply.c) */
void xcb_reply_log_error(xcb_generic_error_t *error, const char *what)
{
    (void) error;
    (void) what;
}


/** Link-only stand-in for xcb_query_tree_children (libxcb) */
xcb_window_t *xcb_query_tree_children(const xcb_query_tree_reply_t *reply)
{
    (void) reply;
    return NULL;
}


/** Link-only stand-in for xcb_query_tree_children_length (libxcb) */
int xcb_query_tree_children_length(const xcb_query_tree_reply_t *reply)
{
    (void) reply;
    return 0;
}


/** Link-only stand-in for xcb_get_window_attributes (libxcb) */
xcb_get_window_attributes_cookie_t xcb_get_window_attributes(
        xcb_connection_t *c, xcb_window_t window)
{
    xcb_get_window_attributes_cookie_t cookie = {0};

    (void) c;
    (void) window;
    return cookie;
}


/** Link-only stand-in for xcb_get_window_attributes_reply (libxcb) */
xcb_get_window_attributes_reply_t *xcb_get_window_attributes_reply(
        xcb_connection_t *c, xcb_get_window_attributes_cookie_t cookie,
        xcb_generic_error_t **e)
{
    (void) c;
    (void) cookie;
    (void) e;
    return NULL;
}


/**
 * @brief Build a minimal, real 'wm_td' on the stack
 */
static wm_td s_make_wm(void)
{
    wm_td local_wm;

    memset(&local_wm, 0, sizeof(local_wm));
    return local_wm;
}


/* cctl_adopt_scan on a null wm returns immediately, through
 * wm_connection(NULL) and wm_stages(NULL) both safely reporting
 * NULL first, then the guard clause itself catching it before either
 * the logging call or the loop ever runs */
static void s_test_null_wm_guard(void)
{
    s_logger_call_count = 0;

    cctl_adopt_scan(NULL);

    TAP_EQ_INT(s_logger_call_count, 0,
            "cctl_adopt_scan on a null wm never reaches its own"
            " LOGGER_DEBUG calls");
}


/* cctl_adopt_scan on an initialized wm with an empty stages list
 * runs its own loop zero times, logging its start and end messages
 * but touching nothing else */
static void s_test_empty_stages_list(void)
{
    wm_td local_wm = s_make_wm();
    list_td *stages = list_init(NULL);

    local_wm.stages = stages;
    s_logger_call_count = 0;

    cctl_adopt_scan(&local_wm);

    TAP_EQ_INT(s_logger_call_count, 2,
            "cctl_adopt_scan on an empty stages list logs exactly"
            " its own start and end messages");

    list_destroy(stages);
}


/* cctl_adopt_scan's own loop skips a null stage pointer and a
 * stage whose screen is null, in both cases never entering
 * s_adopt_scan_stage (which would otherwise dereference that null
 * screen when building an xcb_query_tree request) */
static void s_test_loop_skips_null_and_screenless_stages(void)
{
    wm_td local_wm = s_make_wm();
    stage_td stage_without_screen;
    list_td *stages = list_init(NULL);

    memset(&stage_without_screen, 0, sizeof(stage_without_screen));
    stage_without_screen.screen = NULL;

    list_ins_next(stages, NULL, NULL);
    list_ins_next(stages, list_tail(stages), &stage_without_screen);

    local_wm.stages = stages;
    s_logger_call_count = 0;

    cctl_adopt_scan(&local_wm);

    TAP_EQ_INT(s_logger_call_count, 2,
            "cctl_adopt_scan skips a null stage entry and a"
            " screenless stage without entering"
            " s_adopt_scan_stage, so only the start and end"
            " messages are logged");

    list_destroy(stages);
}


int main(void)
{
    TAP_PLAN(3);

    s_test_null_wm_guard();
    s_test_empty_stages_list();
    s_test_loop_skips_null_and_screenless_stages();

    return TAP_DONE();
}
