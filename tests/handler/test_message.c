/**
 * @file tests/handler/test_message.c
 *
 * @brief Unit tests for @c handler/message.c
 *
 * Covers @a handler_message_client, exercised entirely through
 * synthetic @c xcb_client_message_event_t structs built by hand, a
 * hand-built @c wm_td (via the transparent @c struct wm_s from
 * include/wm/internal.h, linking the tiny real accessor leaf file
 * src/wm/instance.c rather than the heavy src/wm.c), and a hand-built
 * @c xcb_ewmh_connection_t with sentinel atom values, the same
 * approach already used for tests/handler/test_focus.c's
 * PropertyNotify atom dispatch.  No live X connection is ever needed.
 *
 * @a lookup_find_client and @a lookup_current_desktop are replaced by
 * controlled stand-ins (same pattern as test_focus.c/test_crossing.c/
 * test_configure.c) so each scenario can hand back exactly the
 * client/stage/desktop triple it wants to exercise.  Every other
 * collaborator this dispatcher calls is a link-only stand-in defined
 * below: all eight @c hi_handle_net_* functions declared in
 * include/handler/internal.h (implemented in handler/ewmh.c, a
 * separate translation unit this file is not testing), the
 * notification-startup, systray, EWMH-state-mutation, desktop-move,
 * focus, visibility, and responsiveness collaborators, and the real
 * xcb_change_property calls this dispatcher itself makes directly.
 *
 * Deliberately out of scope: the exact internals of every
 * @c hi_handle_net_* handler (each gets its own test file when
 * handler/ewmh.c is tested) - here only "was the right one called,
 * with the right client/stage/desktop resolved" is checked, which is
 * everything @c handler_message_client itself is responsible for.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L


/* System includes */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/list.h>

/* Utils includes */
#include <utils/xcb/atom.h>
#include <utils/xcb/connection.h>

/* Command includes */
#include <cmds/client/flags.h>
#include <cmds/client/focus.h>
#include <cmds/client/layer.h>
#include <cmds/client/visibility.h>
#include <cmds/stage.h>

/* Policy includes */
#include <policy/focus.h>

/* Definition includes */
#include <defs/ewmh.h>

/* Project includes */
#include <cctl/sn.h>
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <stage.h>
#include <systray.h>
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <handler/message.h>
#include <handler/internal.h>
#include <harness/tap.h>


/* What desktop_find_client_by_id should hand back for the next call */
static client_td *s_desktop_find_client_by_id_result = NULL;

/* What stage_desktop_get should hand back for the next call */
static desktop_td *s_stage_desktop_get_result = NULL;

/* What lookup_find_client/lookup_current_desktop should hand back for
 * the next call */
static client_td *s_lookup_result = NULL;
static stage_td *s_lookup_stage_out = NULL;
static desktop_td *s_lookup_desktop_out = NULL;
static desktop_td *s_lookup_current_desktop_result = NULL;

/* What systray_owns_window should report for the next call */
static bool s_systray_owns_window_result = false;

/* Call counters/recorders for every link-only stand-in below */
static unsigned int s_call_cctl_sn = 0u;
static unsigned int s_call_systray_handle_message = 0u;
static unsigned int s_call_hi_wm_state = 0u;
static unsigned int s_call_hi_restack_window = 0u;
static unsigned int s_call_hi_fullscreen_monitors = 0u;
static unsigned int s_call_hi_moveresize = 0u;
static unsigned int s_call_hi_current_desktop = 0u;
static unsigned int s_call_hi_desktop_viewport = 0u;
static unsigned int s_call_hi_wm_desktop = 0u;
static unsigned int s_call_hi_moveresize_window = 0u;
static unsigned int s_call_hi_showing_desktop = 0u;
static unsigned int s_call_ccmd_urge = 0u;
static unsigned int s_call_desktop_move = 0u;
static unsigned int s_call_stage_switch = 0u;
static unsigned int s_call_ccmd_restore = 0u;
static unsigned int s_call_ccmd_unhide = 0u;
static unsigned int s_call_ccmd_unshade = 0u;
static unsigned int s_call_focus_apply = 0u;
static unsigned int s_call_ccmd_close = 0u;
static unsigned int s_call_change_property = 0u;
static unsigned int s_call_ewmh_set_showing_desktop = 0u;
static unsigned int s_call_ccmd_iconify = 0u;
static client_td *s_last_hi_wm_state_client = NULL;
static client_td *s_last_hi_restack_client = NULL;
static uint32_t s_last_change_property_value = 0u;


/** Controlled stand-in for lookup_find_client */
client_td *lookup_find_client(list_td *stages, xcb_window_t window,
        stage_td **stage, desktop_td **desktop)
{
    (void) stages;
    (void) window;

    if (stage != NULL) {
        *stage = s_lookup_stage_out;
    }
    if (desktop != NULL) {
        *desktop = s_lookup_desktop_out;
    }

    return s_lookup_result;
}


/** Controlled stand-in for lookup_current_desktop */
desktop_td *lookup_current_desktop(stage_td *stage)
{
    (void) stage;

    return s_lookup_current_desktop_result;
}


/** Controlled stand-in for desktop_find_client_by_id */
client_td *desktop_find_client_by_id(const desktop_td *desktop,
        uint32_t id)
{
    (void) desktop;
    (void) id;

    return s_desktop_find_client_by_id_result;
}


/** Controlled stand-in for stage_desktop_get */
desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;

    return s_stage_desktop_get_result;
}


/** Controlled stand-in for systray_owns_window */
bool systray_owns_window(xcb_window_t window)
{
    (void) window;

    return s_systray_owns_window_result;
}


/** Link-only stand-in for systray_handle_client_message */
void systray_handle_client_message(wm_td *wm,
        const xcb_client_message_event_t *event)
{
    (void) wm;
    (void) event;

    s_call_systray_handle_message++;
}


/** Link-only stand-in for cctl_sn_handle_client_message */
void cctl_sn_handle_client_message(xcb_connection_t *connection,
        list_td *stages, const xcb_client_message_event_t *event)
{
    (void) connection;
    (void) stages;
    (void) event;

    s_call_cctl_sn++;
}


/** Link-only stand-in for hi_handle_net_wm_state */
void hi_handle_net_wm_state(client_td *client,
        xcb_client_message_event_t *event,
        const xcb_ewmh_connection_t *ewmh,
        stage_td *stage, desktop_td *desktop)
{
    (void) event;
    (void) ewmh;
    (void) stage;
    (void) desktop;

    s_call_hi_wm_state++;
    s_last_hi_wm_state_client = client;
}


/** Link-only stand-in for hi_handle_net_current_desktop */
void hi_handle_net_current_desktop(const wm_td *wm,
        xcb_client_message_event_t *event)
{
    (void) wm;
    (void) event;

    s_call_hi_current_desktop++;
}


/** Link-only stand-in for hi_handle_net_desktop_viewport */
void hi_handle_net_desktop_viewport(const wm_td *wm,
        xcb_client_message_event_t *event)
{
    (void) wm;
    (void) event;

    s_call_hi_desktop_viewport++;
}


/** Link-only stand-in for hi_handle_net_wm_desktop */
void hi_handle_net_wm_desktop(const wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, stage_td *stage, desktop_td *src_desktop)
{
    (void) wm;
    (void) event;
    (void) client;
    (void) stage;
    (void) src_desktop;

    s_call_hi_wm_desktop++;
}


/** Link-only stand-in for hi_handle_net_moveresize_window */
void hi_handle_net_moveresize_window(const wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, stage_td *stage, desktop_td *desktop)
{
    (void) wm;
    (void) event;
    (void) client;
    (void) stage;
    (void) desktop;

    s_call_hi_moveresize_window++;
}


/** Link-only stand-in for hi_handle_net_showing_desktop */
void hi_handle_net_showing_desktop(stage_td *stage, bool show)
{
    (void) stage;
    (void) show;

    s_call_hi_showing_desktop++;
}


/** Link-only stand-in for hi_handle_net_restack_window */
void hi_handle_net_restack_window(const wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, stage_td *stage, desktop_td *desktop)
{
    (void) wm;
    (void) event;
    (void) stage;
    (void) desktop;

    s_call_hi_restack_window++;
    s_last_hi_restack_client = client;
}


/** Link-only stand-in for hi_handle_net_wm_fullscreen_monitors */
void hi_handle_net_wm_fullscreen_monitors(const wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, stage_td *stage, desktop_td *desktop)
{
    (void) wm;
    (void) event;
    (void) client;
    (void) stage;
    (void) desktop;

    s_call_hi_fullscreen_monitors++;
}


/** Link-only stand-in for hi_handle_net_wm_moveresize */
void hi_handle_net_wm_moveresize(const wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, stage_td *stage, desktop_td *desktop)
{
    (void) wm;
    (void) event;
    (void) client;
    (void) stage;
    (void) desktop;

    s_call_hi_moveresize++;
}


/** Link-only stand-in for ccmd_client_urge */
void ccmd_client_urge(client_td *client)
{
    (void) client;

    s_call_ccmd_urge++;
}


/** Link-only stand-in for desktop_action_client_move */
int desktop_action_client_move(desktop_td *from, desktop_td *to,
        client_td *client)
{
    (void) from;
    (void) to;
    (void) client;

    s_call_desktop_move++;
    return 0;
}


/** Link-only stand-in for scmd_stage_desktop_switch */
void scmd_stage_desktop_switch(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;

    s_call_stage_switch++;
}


/** Link-only stand-in for ccmd_client_restore */
void ccmd_client_restore(client_td *client)
{
    (void) client;

    s_call_ccmd_restore++;
}


/** Link-only stand-in for ccmd_client_unhide */
void ccmd_client_unhide(client_td *client)
{
    (void) client;

    s_call_ccmd_unhide++;
}


/** Link-only stand-in for ccmd_client_unshade */
void ccmd_client_unshade(client_td *client)
{
    (void) client;

    s_call_ccmd_unshade++;
}


/** Link-only stand-in for focus_apply */
void focus_apply(list_td *stages, stage_td *stage,
        desktop_td *desktop, client_td *client,
        bool raise, const config_td *cfg)
{
    (void) stages;
    (void) stage;
    (void) desktop;
    (void) client;
    (void) raise;
    (void) cfg;

    s_call_focus_apply++;
}


/** Link-only stand-in for ccmd_client_close */
void ccmd_client_close(client_td *client)
{
    (void) client;

    s_call_ccmd_close++;
}


/** Link-only stand-in for ccmd_client_iconify */
void ccmd_client_iconify(client_td *client)
{
    (void) client;

    s_call_ccmd_iconify++;
}


/** Link-only stand-in for xcb_ewmh_connection_get; always reports a
 *  live connection so the frame-extents branch's guard passes */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    static xcb_ewmh_connection_t dummy;

    return &dummy;
}


/** Link-only stand-in for xcb_change_property; records the property
 *  value list's first 32-bit word */
xcb_void_cookie_t xcb_change_property(xcb_connection_t *c, uint8_t mode,
        xcb_window_t window, xcb_atom_t property, xcb_atom_t type,
        uint8_t format, uint32_t data_len, const void *data)
{
    xcb_void_cookie_t cookie = {0};
    const uint32_t *values = (const uint32_t *) data;

    (void) c;
    (void) mode;
    (void) window;
    (void) property;
    (void) type;
    (void) format;

    s_call_change_property++;
    if (data_len > 0u) {
        s_last_change_property_value = values[0];
    }

    return cookie;
}


/** Link-only stand-in for xcb_ewmh_set_showing_desktop */
xcb_void_cookie_t xcb_ewmh_set_showing_desktop(
        xcb_ewmh_connection_t *ewmh, int screen_nbr, uint32_t show)
{
    xcb_void_cookie_t cookie = {0};

    (void) ewmh;
    (void) screen_nbr;
    (void) show;

    s_call_ewmh_set_showing_desktop++;

    return cookie;
}


/** Link-only stand-in for atom_intern; hands back a distinct,
 *  never-colliding sentinel atom value for each requested name so
 *  a test can compare an event's type against exactly one of them */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    (void) connection;
    (void) only_if_exists;

    if (strcmp(name, "_NET_RESTACK_WINDOW") == 0) {
        return (xcb_atom_t) 9001;
    }
    if (strcmp(name, "_NET_WM_FULLSCREEN_MONITORS") == 0) {
        return (xcb_atom_t) 9002;
    }
    if (strcmp(name, "_NET_WM_MOVERESIZE") == 0) {
        return (xcb_atom_t) 9003;
    }
    if (strcmp(name, "WM_CHANGE_STATE") == 0) {
        return (xcb_atom_t) 9004;
    }

    return (xcb_atom_t) 0;
}


/** Link-only stand-in for logger_msg */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    va_list args;

    (void) level;
    (void) prefix;

    va_start(args, fmt);
    va_end(args);

    return 0;
}


/**
 * @brief Reset every stand-in's recorded state and lookup fixture
 */
static void s_test_reset_state(void)
{
    s_lookup_result = NULL;
    s_lookup_stage_out = NULL;
    s_lookup_desktop_out = NULL;
    s_lookup_current_desktop_result = NULL;
    s_desktop_find_client_by_id_result = NULL;
    s_stage_desktop_get_result = NULL;
    s_systray_owns_window_result = false;
    s_call_cctl_sn = 0u;
    s_call_systray_handle_message = 0u;
    s_call_hi_wm_state = 0u;
    s_call_hi_restack_window = 0u;
    s_call_hi_fullscreen_monitors = 0u;
    s_call_hi_moveresize = 0u;
    s_call_hi_current_desktop = 0u;
    s_call_hi_desktop_viewport = 0u;
    s_call_hi_wm_desktop = 0u;
    s_call_hi_moveresize_window = 0u;
    s_call_hi_showing_desktop = 0u;
    s_call_ccmd_urge = 0u;
    s_call_desktop_move = 0u;
    s_call_stage_switch = 0u;
    s_call_ccmd_restore = 0u;
    s_call_ccmd_unhide = 0u;
    s_call_ccmd_unshade = 0u;
    s_call_focus_apply = 0u;
    s_call_ccmd_close = 0u;
    s_call_change_property = 0u;
    s_call_ewmh_set_showing_desktop = 0u;
    s_call_ccmd_iconify = 0u;
    s_last_hi_wm_state_client = NULL;
    s_last_hi_restack_client = NULL;
    s_last_change_property_value = 0u;
}


/**
 * @brief Build a wm_td on the stack wired to the given ewmh/stages
 *
 * @param wm       Struct to fill
 * @param ewmh     EWMH connection to expose via wm_ewmh
 * @param stages Stage list to expose via wm_stages
 * @param config   Config to expose via wm_config
 */
static void s_test_build_wm(wm_td *wm, xcb_ewmh_connection_t *ewmh,
        list_td *stages, config_td *config)
{
    memset(wm, 0, sizeof(*wm));
    wm->connection = (xcb_connection_t *) 0x1234;
    wm->ewmh = ewmh;
    wm->stages = stages;
    wm->config = config;
}


/**
 * @brief Build a plain client with a given window ID
 *
 * @param client Client struct to initialize
 * @param window Window ID and id field to assign
 */
static void s_test_build_client(client_td *client, xcb_window_t window)
{
    memset(client, 0, sizeof(*client));
    client->window = window;
    client->id = window;
}


/**
 * @brief Build a plain xcb_client_message_event_t
 *
 * @param event  Event struct to fill
 * @param window Target window
 * @param type   Message type atom
 */
static void s_test_build_event(xcb_client_message_event_t *event,
        xcb_window_t window, xcb_atom_t type)
{
    memset(event, 0, sizeof(*event));
    event->response_type = XCB_CLIENT_MESSAGE;
    event->format = 32u;
    event->window = window;
    event->type = type;
}


/* A null wm never crashes the dispatcher */
static void s_test_null_wm(void)
{
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&event, 0, sizeof(event));

    handler_message_client(NULL, &event);

    TAP_OK(s_call_cctl_sn == 0u,
            "a null wm triggers no further dispatch work at all");
}


/* A null event never crashes the dispatcher */
static void s_test_null_event(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, stages, &config);

    handler_message_client(&wm, NULL);

    TAP_OK(s_call_cctl_sn == 0u,
            "a null event triggers no further dispatch work at all");

    list_destroy(stages);
}


/* A null ewmh connection is treated the same way: bail out early */
static void s_test_null_ewmh(void)
{
    list_td *stages;
    config_td config;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, NULL, stages, &config);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 5000);

    handler_message_client(&wm, &event);

    TAP_OK(s_call_cctl_sn == 0u,
            "a null EWMH connection triggers no further dispatch" \
            " work at all");

    list_destroy(stages);
}


/* Every message, even one this dispatcher will not otherwise
 * recognize, is first offered to the startup-notification tracker */
static void s_test_cctl_sn_always_offered_first(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 424242);

    handler_message_client(&wm, &event);

    TAP_OK(s_call_cctl_sn == 1u,
            "every client message is offered to the startup-" \
            "notification tracker exactly once");

    list_destroy(stages);
}


/* A window the systray owns is dispatched to the systray exclusively;
 * nothing further runs for it */
static void s_test_systray_owned_window(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE = (xcb_atom_t) 100;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_event(&event, 0x100, ewmh._NET_WM_STATE);
    s_systray_owns_window_result = true;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_systray_handle_message == 1u,
            "a systray-owned window's message is handed to the" \
            " systray");
    TAP_OK(s_call_hi_wm_state == 0u,
            "a systray-owned window's _NET_WM_STATE is never also" \
            " routed to the EWMH state handler");

    list_destroy(stages);
}


/* _NET_WM_STATE for a window with no matching managed client calls
 * no handler at all */
static void s_test_wm_state_no_client(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE = (xcb_atom_t) 100;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_event(&event, 0x100, ewmh._NET_WM_STATE);
    s_lookup_result = NULL;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_hi_wm_state == 0u,
            "an unmanaged window's _NET_WM_STATE message calls no" \
            " state handler");

    list_destroy(stages);
}


/* _NET_WM_STATE for a managed client's window calls the state
 * handler with that exact client */
static void s_test_wm_state_with_client(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE = (xcb_atom_t) 100;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, ewmh._NET_WM_STATE);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_hi_wm_state == 1u,
            "a managed window's _NET_WM_STATE message calls the" \
            " state handler exactly once");
    TAP_OK(s_last_hi_wm_state_client == &client,
            "the state handler receives the exact client that was" \
            " looked up");

    list_destroy(stages);
}


/* _NET_RESTACK_WINDOW dispatches through s_dispatch_to_client_handler
 * to hi_handle_net_restack_window when a client is found */
static void s_test_restack_window_with_client(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0x200);
    s_test_build_event(&event, 0x200, (xcb_atom_t) 9001);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_hi_restack_window == 1u,
            "_NET_RESTACK_WINDOW with a matching client calls the" \
            " restack handler exactly once");
    TAP_OK(s_last_hi_restack_client == &client,
            "the restack handler receives the exact client that was" \
            " looked up");

    list_destroy(stages);
}


/* _NET_RESTACK_WINDOW for an unmanaged window calls no handler */
static void s_test_restack_window_no_client(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_event(&event, 0x999, (xcb_atom_t) 9001);
    s_lookup_result = NULL;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_hi_restack_window == 0u,
            "_NET_RESTACK_WINDOW for an unmanaged window calls no" \
            " restack handler");

    list_destroy(stages);
}


/* _NET_WM_FULLSCREEN_MONITORS dispatches to its own handler */
static void s_test_fullscreen_monitors_dispatch(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0x300);
    s_test_build_event(&event, 0x300, (xcb_atom_t) 9002);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_hi_fullscreen_monitors == 1u,
            "_NET_WM_FULLSCREEN_MONITORS dispatches to its handler" \
            " exactly once");

    list_destroy(stages);
}


/* _NET_WM_MOVERESIZE dispatches to its own handler */
static void s_test_wm_moveresize_dispatch(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0x400);
    s_test_build_event(&event, 0x400, (xcb_atom_t) 9003);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_hi_moveresize == 1u,
            "_NET_WM_MOVERESIZE dispatches to its handler exactly" \
            " once");

    list_destroy(stages);
}


/* _NET_ACTIVE_WINDOW: an older, non-user-sourced request against
 * a different already-focused client is refused and only marks the
 * requester urgent */
static void s_test_active_window_urged_not_activated(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    client_td active_client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_ACTIVE_WINDOW = (xcb_atom_t) 200;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0x500);
    client.user_time = 10u;
    s_test_build_client(&active_client, 0x600);
    active_client.user_time = 999u;
    desktop.client_active_id = active_client.id;
    desktop.clients = NULL;
    s_desktop_find_client_by_id_result = &active_client;
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_lookup_current_desktop_result = &desktop;
    s_test_build_event(&event, 0x500, ewmh._NET_ACTIVE_WINDOW);
    event.data.data32[0] = 0u; /* Not WM_SOURCE_USER */
    event.data.data32[1] = 10u; /* asked_at, matches client's own */
    event.data.data32[2] = XCB_WINDOW_NONE; /* asker has no active */

    handler_message_client(&wm, &event);

    TAP_OK(s_call_ccmd_urge == 1u,
            "an older, program-sourced _NET_ACTIVE_WINDOW request" \
            " against a different focused client is marked urgent");
    TAP_OK(s_call_focus_apply == 0u,
            "an urged _NET_ACTIVE_WINDOW request never also gets" \
            " real focus applied");

    list_destroy(stages);
}


/* _NET_ACTIVE_WINDOW: a WM_SOURCE_USER request bypasses the
 * focus-stealing check entirely and proceeds to real focus */
static void s_test_active_window_user_source_bypasses_urge(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    client_td active_client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_ACTIVE_WINDOW = (xcb_atom_t) 200;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0x500);
    client.user_time = 10u;
    s_test_build_client(&active_client, 0x600);
    active_client.user_time = 999u;
    desktop.client_active_id = active_client.id;
    desktop.id = 5;
    stage.desktop_cur = 5;
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_lookup_current_desktop_result = &desktop;
    s_test_build_event(&event, 0x500, ewmh._NET_ACTIVE_WINDOW);
    event.data.data32[0] = (uint32_t) WM_SOURCE_USER;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_ccmd_urge == 0u,
            "a WM_SOURCE_USER _NET_ACTIVE_WINDOW request is never" \
            " marked urgent regardless of user_time");
    TAP_OK(s_call_focus_apply == 1u,
            "a WM_SOURCE_USER _NET_ACTIVE_WINDOW request proceeds" \
            " to real focus");

    list_destroy(stages);
}


/* _NET_ACTIVE_WINDOW: a hidden client on a different desktop is moved
 * to the current desktop via desktop_action_client_move, not switched
 * to */
static void s_test_active_window_hidden_moves_desktop(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    desktop_td cur_desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_ACTIVE_WINDOW = (xcb_atom_t) 200;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&cur_desktop, 0, sizeof(cur_desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0x500);
    client.properties.flags = (uint16_t) CLIENT_FLAG_HIDDEN;
    desktop.id = 1;
    cur_desktop.id = 0;
    stage.desktop_cur = 0;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_lookup_current_desktop_result = &cur_desktop;
    s_stage_desktop_get_result = &cur_desktop;
    s_lookup_result = &client;
    s_test_build_event(&event, 0x500, ewmh._NET_ACTIVE_WINDOW);
    event.data.data32[0] = (uint32_t) WM_SOURCE_USER;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_desktop_move == 1u,
            "a hidden client requesting activation from a" \
            " different desktop is moved to the current desktop");
    TAP_OK(s_call_stage_switch == 0u,
            "moving a hidden client to the current desktop never" \
            " also switches desktops");
    TAP_OK(s_call_change_property == 1u,
            "moving a hidden client updates its published" \
            " _NET_WM_DESKTOP property");

    list_destroy(stages);
}


/* _NET_ACTIVE_WINDOW: a non-hidden client on a different desktop
 * switches the stage to that desktop instead of moving the client */
static void s_test_active_window_visible_switches_desktop(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_ACTIVE_WINDOW = (xcb_atom_t) 200;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0x500);
    client.properties.flags = 0u;
    desktop.id = 3;
    stage.desktop_cur = 0;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_lookup_current_desktop_result = &desktop;
    s_lookup_result = &client;
    s_test_build_event(&event, 0x500, ewmh._NET_ACTIVE_WINDOW);
    event.data.data32[0] = (uint32_t) WM_SOURCE_USER;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_stage_switch >= 1u,
            "a visible client requesting activation from a" \
            " different desktop switches the stage to it");
    TAP_OK(s_call_desktop_move == 0u,
            "switching desktops for a visible client never also" \
            " moves the client between desktops");

    list_destroy(stages);
}


/* _NET_ACTIVE_WINDOW: an iconified client is restored rather than
 * unhidden */
static void s_test_active_window_iconified_restores(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_ACTIVE_WINDOW = (xcb_atom_t) 200;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0x500);
    client.properties.state = (uint16_t) CLIENT_STATE_ICONIFIED;
    desktop.id = 0;
    stage.desktop_cur = 0;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_lookup_current_desktop_result = &desktop;
    s_lookup_result = &client;
    s_test_build_event(&event, 0x500, ewmh._NET_ACTIVE_WINDOW);
    event.data.data32[0] = (uint32_t) WM_SOURCE_USER;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_ccmd_restore == 1u,
            "activating an iconified client restores it exactly" \
            " once");
    TAP_OK(s_call_ccmd_unhide == 0u,
            "restoring an iconified client never also calls unhide");

    list_destroy(stages);
}


/* _NET_ACTIVE_WINDOW: a hidden but not iconified client is unhidden
 * rather than restored */
static void s_test_active_window_hidden_unhides(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_ACTIVE_WINDOW = (xcb_atom_t) 200;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0x500);
    client.properties.flags = (uint16_t) CLIENT_FLAG_HIDDEN;
    client.properties.state = 0u;
    desktop.id = 0;
    stage.desktop_cur = 0;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_lookup_current_desktop_result = &desktop;
    s_lookup_result = &client;
    s_test_build_event(&event, 0x500, ewmh._NET_ACTIVE_WINDOW);
    event.data.data32[0] = (uint32_t) WM_SOURCE_USER;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_ccmd_unhide == 1u,
            "activating a hidden, non-iconified client unhides it" \
            " exactly once");
    TAP_OK(s_call_ccmd_restore == 0u,
            "unhiding a hidden client never also calls restore");

    list_destroy(stages);
}


/* _NET_ACTIVE_WINDOW: a shaded, non-iconified, non-hidden client is
 * unshaded so the whole window comes back into view, not left rolled
 * up under its own titlebar after being raised and focused */
static void s_test_active_window_shaded_unshades(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_ACTIVE_WINDOW = (xcb_atom_t) 200;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0x500);
    client.properties.flags = (uint16_t) CLIENT_FLAG_SHADED;
    client.properties.state = 0u;
    desktop.id = 0;
    stage.desktop_cur = 0;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_lookup_current_desktop_result = &desktop;
    s_lookup_result = &client;
    s_test_build_event(&event, 0x500, ewmh._NET_ACTIVE_WINDOW);
    event.data.data32[0] = (uint32_t) WM_SOURCE_USER;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_ccmd_unshade == 1u,
            "activating a shaded client unshades it exactly once");
    TAP_OK(s_call_ccmd_restore == 0u && s_call_ccmd_unhide == 0u,
            "unshading it never also calls restore or unhide");

    list_destroy(stages);
}


/* _NET_CLOSE_WINDOW for a managed client closes it */
static void s_test_close_window_with_client(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_CLOSE_WINDOW = (xcb_atom_t) 300;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0x700);
    s_test_build_event(&event, 0x700, ewmh._NET_CLOSE_WINDOW);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_ccmd_close == 1u,
            "_NET_CLOSE_WINDOW for a managed client closes it" \
            " exactly once");

    list_destroy(stages);
}


/* _NET_CLOSE_WINDOW for an unmanaged window closes nothing */
static void s_test_close_window_no_client(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_CLOSE_WINDOW = (xcb_atom_t) 300;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_event(&event, 0x999, ewmh._NET_CLOSE_WINDOW);
    s_lookup_result = NULL;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_ccmd_close == 0u,
            "_NET_CLOSE_WINDOW for an unmanaged window closes" \
            " nothing");

    list_destroy(stages);
}


/* _NET_WM_DESKTOP dispatches to its own handler */
static void s_test_wm_desktop_dispatch(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_DESKTOP = (xcb_atom_t) 400;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0x800);
    s_test_build_event(&event, 0x800, ewmh._NET_WM_DESKTOP);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_hi_wm_desktop == 1u,
            "_NET_WM_DESKTOP dispatches to its handler exactly once");

    list_destroy(stages);
}


/* _NET_CURRENT_DESKTOP is dispatched directly, without going through
 * s_dispatch_to_client_handler (it needs no client lookup at all) */
static void s_test_current_desktop_dispatch(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_CURRENT_DESKTOP = (xcb_atom_t) 500;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_event(&event, 0x1, ewmh._NET_CURRENT_DESKTOP);
    s_lookup_result = NULL;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_hi_current_desktop == 1u,
            "_NET_CURRENT_DESKTOP dispatches to its handler exactly" \
            " once, with no client lookup required");

    list_destroy(stages);
}


/* _NET_DESKTOP_VIEWPORT is dispatched directly, without going
 * through s_dispatch_to_client_handler (it needs no client lookup) */
static void s_test_desktop_viewport_dispatch(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_DESKTOP_VIEWPORT = (xcb_atom_t) 501;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_event(&event, 0x1, ewmh._NET_DESKTOP_VIEWPORT);
    s_lookup_result = NULL;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_hi_desktop_viewport == 1u,
            "_NET_DESKTOP_VIEWPORT dispatches to its handler exactly" \
            " once, with no client lookup required");

    list_destroy(stages);
}


/* _NET_MOVERESIZE_WINDOW dispatches to its own handler */
static void s_test_moveresize_window_dispatch(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_MOVERESIZE_WINDOW = (xcb_atom_t) 600;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0x900);
    s_test_build_event(&event, 0x900, ewmh._NET_MOVERESIZE_WINDOW);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_hi_moveresize_window == 1u,
            "_NET_MOVERESIZE_WINDOW dispatches to its handler" \
            " exactly once");

    list_destroy(stages);
}


/* _NET_REQUEST_FRAME_EXTENTS replies with the client's real frame
 * extents via a synthesized property */
static void s_test_request_frame_extents_replies(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_REQUEST_FRAME_EXTENTS = (xcb_atom_t) 700;
    ewmh._NET_FRAME_EXTENTS = (xcb_atom_t) 701;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0xa00);
    client.layout.frame_extents.left = 4;
    client.layout.frame_extents.right = 4;
    client.layout.frame_extents.top = 24;
    client.layout.frame_extents.bottom = 4;
    s_test_build_event(&event, 0xa00, ewmh._NET_REQUEST_FRAME_EXTENTS);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_change_property == 1u,
            "_NET_REQUEST_FRAME_EXTENTS replies with a property" \
            " change exactly once");
    TAP_EQ_INT((int) s_last_change_property_value, 4,
            "the frame-extents reply's first value is the client's" \
            " real left extent");

    list_destroy(stages);
}


/* _NET_REQUEST_FRAME_EXTENTS for an unmanaged window replies with
 * nothing */
static void s_test_request_frame_extents_no_client(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_REQUEST_FRAME_EXTENTS = (xcb_atom_t) 700;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_event(&event, 0x999, ewmh._NET_REQUEST_FRAME_EXTENTS);
    s_lookup_result = NULL;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_change_property == 0u,
            "_NET_REQUEST_FRAME_EXTENTS for an unmanaged window" \
            " replies with nothing");

    list_destroy(stages);
}


/* _NET_SHOWING_DESKTOP walks every stage, applying the request to
 * each one; publishing the resulting state is left to 'wm_ewmh_sync'
 * on the next refresh, not done here directly, since
 * 'hi_handle_net_showing_desktop' can decide there is nothing to show
 * and leave a stage's actual state different from what was asked
 * for */
static void s_test_showing_desktop_walks_all_stages(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    stage_td stage_a;
    stage_td stage_b;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_SHOWING_DESKTOP = (xcb_atom_t) 800;
    memset(&stage_a, 0, sizeof(stage_a));
    memset(&stage_b, 0, sizeof(stage_b));
    stage_a.id = 0;
    stage_b.id = 1;
    stages = list_init(NULL);
    (void) list_ins_next(stages, NULL, &stage_a);
    (void) list_ins_next(stages, list_head(stages), &stage_b);
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_event(&event, 0x1, ewmh._NET_SHOWING_DESKTOP);
    event.data.data32[0] = 1u;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_hi_showing_desktop == 2u,
            "_NET_SHOWING_DESKTOP applies to every stage in the" \
            " list");
    TAP_OK(s_call_ewmh_set_showing_desktop == 0u,
            "...without publishing the state directly itself, since" \
            " a later refresh does that from the real result");

    list_destroy(stages);
}


/* A WM_PROTOCOLS/_NET_WM_PING pong reply updates the client's ping
 * bookkeeping and marks it responsive */
static void s_test_wm_ping_pong_updates_client(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh.WM_PROTOCOLS = (xcb_atom_t) 900;
    ewmh._NET_WM_PING = (xcb_atom_t) 901;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0xb00);
    client.hints_ewmh.ping.is_waiting = true;
    client.hints_ewmh.ping.pending_ticks = 3u;
    client.properties.flags = (uint16_t) CLIENT_FLAG_UNRESPONSIVE;
    s_test_build_event(&event, 0x1, ewmh.WM_PROTOCOLS);
    event.data.data32[0] = (uint32_t) ewmh._NET_WM_PING;
    event.data.data32[1] = 12345u;
    event.data.data32[2] = 0xb00;
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;

    handler_message_client(&wm, &event);

    TAP_EQ_INT((int) client.hints_ewmh.ping.last_reply, 12345,
            "a _NET_WM_PING pong records the reply timestamp it" \
            " carried");
    TAP_OK(!client.hints_ewmh.ping.is_waiting,
            "a _NET_WM_PING pong clears the is_waiting flag");
    TAP_EQ_INT((int) client.hints_ewmh.ping.pending_ticks, 0,
            "a _NET_WM_PING pong resets the pending-ticks counter");
    TAP_OK(!(client.properties.flags &
                (uint16_t) CLIENT_FLAG_UNRESPONSIVE),
            "a _NET_WM_PING pong marks the client responsive again");
    TAP_OK(client.is_outdated && stage.is_outdated &&
            desktop.is_outdated,
            "a _NET_WM_PING pong outdates the client, its stage," \
            " and its desktop");

    list_destroy(stages);
}


/* A pong from a client already known to be responsive changes nothing
 * that is drawn, so nothing is marked outdated: every ping-capable
 * client answers one every interval, and outdating on each had the
 * render pass redraw every desktop that often, clearing each
 * decorated client's window with exposures as it went */
static void s_test_wm_ping_pong_responsive_does_not_outdate(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh.WM_PROTOCOLS = (xcb_atom_t) 900;
    ewmh._NET_WM_PING = (xcb_atom_t) 901;
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0xb01);
    client.hints_ewmh.ping.is_waiting = true;
    client.hints_ewmh.ping.pending_ticks = 1u;
    client.properties.flags = 0u;    /* already responsive */
    s_test_build_event(&event, 0x1, ewmh.WM_PROTOCOLS);
    event.data.data32[0] = (uint32_t) ewmh._NET_WM_PING;
    event.data.data32[1] = 54321u;
    event.data.data32[2] = 0xb01;
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;

    handler_message_client(&wm, &event);

    TAP_EQ_INT((int) client.hints_ewmh.ping.last_reply, 54321,
            "the reply timestamp is still recorded");
    TAP_OK(!client.hints_ewmh.ping.is_waiting,
            "and the is_waiting flag still cleared");
    TAP_OK(!client.is_outdated && !stage.is_outdated &&
            !desktop.is_outdated,
            "but nothing is outdated, the client having been"
            " responsive all along");

    list_destroy(stages);
}


/* WM_CHANGE_STATE to IconicState iconifies the matching client */
static void s_test_wm_change_state_iconic(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0xc00);
    s_test_build_event(&event, 0xc00, (xcb_atom_t) 9004);
    event.data.data32[0] = (uint32_t) ICCCM_ICONIC_STATE;
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_ccmd_iconify == 1u,
            "WM_CHANGE_STATE to IconicState iconifies the matching" \
            " client exactly once");
    TAP_OK(stage.is_outdated && desktop.is_outdated,
            "iconifying via WM_CHANGE_STATE outdates the stage" \
            " and desktop");

    list_destroy(stages);
}


/* WM_CHANGE_STATE with a non-iconic state value iconifies nothing */
static void s_test_wm_change_state_non_iconic_ignored(void)
{
    xcb_ewmh_connection_t ewmh;
    list_td *stages;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;
    wm_td wm;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    stages = list_init(NULL);
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, stages, &config);
    s_test_build_client(&client, 0xc00);
    s_test_build_event(&event, 0xc00, (xcb_atom_t) 9004);
    event.data.data32[0] = 0u; /* NormalState, not IconicState */
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;

    handler_message_client(&wm, &event);

    TAP_OK(s_call_ccmd_iconify == 0u,
            "WM_CHANGE_STATE with a non-iconic state value" \
            " iconifies nothing");

    list_destroy(stages);
}


int main(void)
{
    TAP_PLAN(51);

    s_test_null_wm();
    s_test_null_event();
    s_test_null_ewmh();
    s_test_cctl_sn_always_offered_first();
    s_test_systray_owned_window();
    s_test_wm_state_no_client();
    s_test_wm_state_with_client();
    s_test_restack_window_with_client();
    s_test_restack_window_no_client();
    s_test_fullscreen_monitors_dispatch();
    s_test_wm_moveresize_dispatch();
    s_test_active_window_urged_not_activated();
    s_test_active_window_user_source_bypasses_urge();
    s_test_active_window_hidden_moves_desktop();
    s_test_active_window_visible_switches_desktop();
    s_test_active_window_iconified_restores();
    s_test_active_window_hidden_unhides();
    s_test_active_window_shaded_unshades();
    s_test_close_window_with_client();
    s_test_close_window_no_client();
    s_test_wm_desktop_dispatch();
    s_test_current_desktop_dispatch();
    s_test_desktop_viewport_dispatch();
    s_test_moveresize_window_dispatch();
    s_test_request_frame_extents_replies();
    s_test_request_frame_extents_no_client();
    s_test_showing_desktop_walks_all_stages();
    s_test_wm_ping_pong_updates_client();
    s_test_wm_ping_pong_responsive_does_not_outdate();
    s_test_wm_change_state_iconic();
    s_test_wm_change_state_non_iconic_ignored();

    return TAP_DONE();
}
