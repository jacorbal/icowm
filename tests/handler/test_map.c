/**
 * @file tests/handler/test_map.c
 *
 * @brief Unit tests for @c handler/map.c
 *
 * Covers @c handler_map_notify, @c handler_gravity_notify,
 * @c handler_circulate_notify, @c handler_circulate_request,
 * @c handler_unmap_notify, and @c handler_destroy_notify in full,
 * plus @c handler_map_request narrowed to its guard clauses and
 * early-return "unmanaged" paths, all exercised through synthetic
 * XCB event structs and hand-built @c client_td/@c surface_td/
 * @c desktop_td/@c wm_td fixtures.  No live X connection is ever
 * needed.
 *
 * @c handler_map_request's deep body, from the point it decides to
 * actually adopt a window (the @c client_init call onward through
 * @c desktop_action_client_add, @c client_link_transient,
 * @c scratchpad_position, @c rules_apply, @c place_window_apply,
 * @c place_manual_enqueue, and finally @c s_map_finish's own
 * iconify/focus/fullscreen/maximize/shade/hide cascade) is
 * deliberately out of scope here.  That single function's fully
 * managed path pulls in upward of twenty distinct collaborators
 * (client lifecycle, rules engine, two placement policies, the
 * scratchpad, EWMH publishing, and IPC broadcasting all at once),
 * each of which either already has, or is a natural candidate for,
 * its own dedicated test file (e.g., a future @c tests/client/
 * test_init.c for @c client_init itself, @c tests/rules/test_apply.c
 * for @c rules_apply); turning every one of them into a link-only
 * stand-in here would only prove that this file's stand-ins call each
 * other in the order the source dictates, not that
 * @c handler_map_request's own logic is correct, while the guard
 * clauses and early "leave it unmanaged" returns tested below are
 * exactly the part of this function's own responsibility that does
 * not depend on any of those twenty collaborators' real behavior.
 *
 * @c handler_destroy_notify and @c handler_unmap_notify are each
 * tested to the same depth as every other handler file in this
 * family: every guard clause, both branches of every conditional, and
 * every collaborator call this function's own body is responsible
 * for making, with the called collaborators themselves (rather than
 * their own internals) replaced by link-only stand-ins.
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
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/list.h>

/* Command includes */
#include <cmds/client/ewmh.h>
#include <cmds/client/focus.h>
#include <cmds/client/layer.h>
#include <cmds/client/screen.h>

/* Input includes */
#include <input/mouse/cursor.h>
#include <input/mouse/drag.h>

/* Policy includes */
#include <policy/focus.h>
#include <policy/placement/manual.h>
#include <policy/placement/window.h>

/* Utils includes */
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* IPC includes */
#include <ipc.h>

/* Project includes */
#include <cmds/client/transient.h>
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <memguard.h>
#include <rules.h>
#include <scratchpad.h>
#include <surface.h>
#include <systray.h>
#include <utils/xcb/atom.h>
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <handler.h>
#include <harness/tap.h>


/* Call counters/recorders for every link-only stand-in below */
static unsigned int s_call_lookup_find_client = 0u;
static unsigned int s_call_lookup_surface_for_root = 0u;
static unsigned int s_call_lookup_current_desktop = 0u;
static unsigned int s_call_systray_icon_map_request = 0u;
static unsigned int s_call_systray_handle_destroy = 0u;
static unsigned int s_call_cctl_sn_desktop_for_window = 0u;
static unsigned int s_call_memguard_max_clients = 0u;
static unsigned int s_call_memguard_warn_client_cap = 0u;
static unsigned int s_call_client_init = 0u;
static unsigned int s_call_client_destroy = 0u;
static unsigned int s_call_desktop_action_client_add = 0u;
static unsigned int s_call_desktop_action_client_rem = 0u;
static unsigned int s_call_client_focus_fallback = 0u;
static unsigned int s_call_drag_is_active = 0u;
static unsigned int s_call_drag_cancel = 0u;
static unsigned int s_call_drag_client = 0u;
static unsigned int s_call_place_manual_cancel_client = 0u;
static unsigned int s_call_surface_refresh_workareas = 0u;
static unsigned int s_call_ccmd_clear_wm_state = 0u;
static unsigned int s_call_ccmd_set_wm_state = 0u;
static unsigned int s_call_ccmd_client_sync_states = 0u;
static unsigned int s_call_ipc_broadcast_event = 0u;
static unsigned int s_call_wm_request_client_redraw = 0u;
static unsigned int s_call_client_send_synthetic_configure_notify = 0u;
static unsigned int s_call_client_decoration_layout_sync = 0u;
static unsigned int s_call_ccmd_desktop_enforce_layers = 0u;
static unsigned int s_call_ccmd_target_win = 0u;
static unsigned int s_call_map_window = 0u;
static unsigned int s_call_unmap_window = 0u;
static unsigned int s_call_destroy_window = 0u;
static unsigned int s_call_xcb_window_hide = 0u;
static unsigned int s_call_xcb_window_destroy = 0u;
static unsigned int s_call_configure_window = 0u;
static unsigned int s_call_clear_area = 0u;
static unsigned int s_call_change_window_attributes = 0u;
static unsigned int s_call_mouse_plain_cursor = 0u;

/* Controlled return values for the next call */
static client_td *s_lookup_find_client_result = NULL;
static surface_td *s_lookup_find_client_surface_out = NULL;
static desktop_td *s_lookup_find_client_desktop_out = NULL;
static surface_td *s_lookup_surface_for_root_result = NULL;
static desktop_td *s_lookup_current_desktop_result = NULL;
static bool s_systray_icon_map_request_result = false;
static bool s_drag_is_active_result = false;
static client_td *s_drag_client_result = NULL;
static uint32_t s_last_configure_window_stack_mode = 0u;
static xcb_window_t s_last_configure_window_target = 0u;


/** Link-only stand-in for lookup_find_client */
client_td *lookup_find_client(list_td *surfaces, xcb_window_t window,
        surface_td **surface_out, desktop_td **desktop_out)
{
    (void) surfaces;
    (void) window;

    s_call_lookup_find_client++;
    if (surface_out != NULL) {
        *surface_out = s_lookup_find_client_surface_out;
    }
    if (desktop_out != NULL) {
        *desktop_out = s_lookup_find_client_desktop_out;
    }
    return s_lookup_find_client_result;
}


/** Link-only stand-in for lookup_surface_for_root */
surface_td *lookup_surface_for_root(list_td *surfaces,
        xcb_window_t root)
{
    (void) surfaces;
    (void) root;

    s_call_lookup_surface_for_root++;
    return s_lookup_surface_for_root_result;
}


/** Link-only stand-in for lookup_current_desktop */
desktop_td *lookup_current_desktop(surface_td *surface)
{
    (void) surface;

    s_call_lookup_current_desktop++;
    return s_lookup_current_desktop_result;
}


/** Link-only stand-in for systray_icon_map_request */
bool systray_icon_map_request(xcb_window_t window)
{
    (void) window;

    s_call_systray_icon_map_request++;
    return s_systray_icon_map_request_result;
}


/** Link-only stand-in for systray_handle_destroy */
void systray_handle_destroy(wm_td *wm, xcb_window_t window)
{
    (void) wm;
    (void) window;

    s_call_systray_handle_destroy++;
}


/** Link-only stand-in for cctl_sn_desktop_for_window */
bool cctl_sn_desktop_for_window(xcb_connection_t *connection,
        xcb_window_t window, uint32_t *desktop_id_out)
{
    (void) connection;
    (void) window;
    (void) desktop_id_out;

    s_call_cctl_sn_desktop_for_window++;
    return false;
}


/** Link-only stand-in for memguard_max_clients */
uint32_t memguard_max_clients(void)
{
    s_call_memguard_max_clients++;
    return 0u;
}


/** Link-only stand-in for memguard_warn_client_cap */
void memguard_warn_client_cap(xcb_connection_t *connection,
        surface_td *surface, const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) config;

    s_call_memguard_warn_client_cap++;
}


/** Link-only stand-in for client_init; never returns a real client in
 *  this file's narrowed scope, so handler_map_request's body always
 *  takes the "left unmanaged" early return right after this call */
client_td *client_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        const config_td *config)
{
    (void) connection;
    (void) ewmh;
    (void) window;
    (void) config;

    s_call_client_init++;
    return NULL;
}


/** Link-only stand-in for client_destroy */
void client_destroy(client_td *client)
{
    (void) client;

    s_call_client_destroy++;
}


/** Link-only stand-in for atom_intern; never reached in this file's
 *  narrowed scope, since client_init always answers NULL before
 *  s_map_finish (the only caller of s_map_refresh_initial_state,
 *  which is the only caller of this) is ever reached, kept only so
 *  the whole translation unit links */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    (void) connection;
    (void) name;
    (void) only_if_exists;

    return (xcb_atom_t) 0;
}


/** Link-only stand-in for xcb_ewmh_get_wm_state; never reached, see
 *  atom_intern's comment just above */
xcb_get_property_cookie_t xcb_ewmh_get_wm_state(
        xcb_ewmh_connection_t *ewmh, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie = {0};

    (void) ewmh;
    (void) window;

    return cookie;
}


/** Link-only stand-in for xcb_ewmh_connection_get; never reached, see
 *  atom_intern's comment above */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return NULL;
}


/** Link-only stand-in for xcb_get_property_reply; never reached, see
 *  atom_intern's comment above */
xcb_get_property_reply_t *xcb_get_property_reply(xcb_connection_t *c,
        xcb_get_property_cookie_t cookie, xcb_generic_error_t **e)
{
    (void) c;
    (void) cookie;
    (void) e;

    return NULL;
}


/** Link-only stand-in for xcb_get_property_value; never reached, see
 *  atom_intern's comment above */
void *xcb_get_property_value(const xcb_get_property_reply_t *reply)
{
    (void) reply;

    return NULL;
}


/** Link-only stand-in for xcb_get_property_value_length; never
 *  reached, see atom_intern's comment above */
int xcb_get_property_value_length(const xcb_get_property_reply_t *reply)
{
    (void) reply;

    return 0;
}


/** Link-only stand-in for ccmd_client_iconify; never reached in this
 *  file's narrowed scope (see atom_intern's comment above) */
void ccmd_client_iconify(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for focus_apply; never reached, see
 *  ccmd_client_iconify's comment above */
void focus_apply(list_td *surfaces, surface_td *surface,
        desktop_td *desktop, client_td *client, bool raise,
        const config_td *cfg)
{
    (void) surfaces;
    (void) surface;
    (void) desktop;
    (void) client;
    (void) raise;
    (void) cfg;
}


/** Link-only stand-in for ccmd_client_fullscreen; never reached, see
 *  ccmd_client_iconify's comment above */
void ccmd_client_fullscreen(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for ccmd_client_maximize; never reached, see
 *  ccmd_client_iconify's comment above */
void ccmd_client_maximize(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for ccmd_client_maximize_horz; never reached,
 *  see ccmd_client_iconify's comment above */
void ccmd_client_maximize_horz(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for ccmd_client_maximize_vert; never reached,
 *  see ccmd_client_iconify's comment above */
void ccmd_client_maximize_vert(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for ccmd_client_unshade; never reached, see
 *  ccmd_client_iconify's comment above */
void ccmd_client_unshade(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for ccmd_client_shade; never reached, see
 *  ccmd_client_iconify's comment above */
void ccmd_client_shade(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for ccmd_client_hide; never reached, see
 *  ccmd_client_iconify's comment above */
void ccmd_client_hide(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for ccmd_client_unhide; never reached, see
 *  ccmd_client_iconify's comment above */
void ccmd_client_unhide(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for surface_desktop_get; used by
 *  handler_map_request's own body for a real, reachable branch (the
 *  startup-notification origin desktop lookup), unlike the
 *  s_map_finish-only collaborators above */
desktop_td *surface_desktop_get(surface_td *surface,
        uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;

    return NULL;
}


/** Link-only stand-in for client_link_transient; never reached in
 *  this file's narrowed scope, see ccmd_client_iconify's comment
 *  above */
void client_link_transient(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for ccmd_client_transient_top_parent; never
 *  reached, see client_link_transient's comment above */
client_td *ccmd_client_transient_top_parent(client_td *client)
{
    (void) client;

    return NULL;
}


/** Link-only stand-in for ccmd_client_pin; never reached, see
 *  client_link_transient's comment above */
void ccmd_client_pin(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for scratchpad_position; never reached, see
 *  client_link_transient's comment above */
void scratchpad_position(client_td *client, const desktop_td *desktop,
        surface_td *surface)
{
    (void) client;
    (void) desktop;
    (void) surface;
}


/** Link-only stand-in for rules_apply; never reached, see
 *  client_link_transient's comment above */
bool rules_apply(const wm_td *wm, client_td *client,
        surface_td **surface_io, desktop_td **desktop_io,
        enum rules_trigger_e trigger)
{
    (void) wm;
    (void) client;
    (void) surface_io;
    (void) desktop_io;
    (void) trigger;

    return false;
}


/** Link-only stand-in for place_window_apply; never reached, see
 *  client_link_transient's comment above */
void place_window_apply(const wm_td *wm, surface_td *surface,
        client_td *client)
{
    (void) wm;
    (void) surface;
    (void) client;
}


/** Link-only stand-in for mouse_cursor_move; never reached, see
 *  client_link_transient's comment above */
xcb_cursor_t mouse_cursor_move(void)
{
    return (xcb_cursor_t) 0;
}


/** Link-only stand-in for xcb_change_property (real xcb prototype,
 *  but never the real library: only headers are included, never
 *  linked); never reached, see client_link_transient's comment
 *  above */
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


/** Link-only stand-in for place_manual_enqueue; never reached, see
 *  client_link_transient's comment above */
bool place_manual_enqueue(xcb_connection_t *connection, const wm_td *wm,
        surface_td *surface, desktop_td *desktop, client_td *client,
        xcb_cursor_t cursor, place_manual_done_fn done)
{
    (void) connection;
    (void) wm;
    (void) surface;
    (void) desktop;
    (void) client;
    (void) cursor;
    (void) done;

    return false;
}


/** Link-only stand-in for desktop_action_client_add */
int desktop_action_client_add(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;

    s_call_desktop_action_client_add++;
    return 0;
}


/** Link-only stand-in for desktop_action_client_rem */
int desktop_action_client_rem(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;

    s_call_desktop_action_client_rem++;
    return 0;
}


/** Link-only stand-in for client_focus_fallback */
void client_focus_fallback(desktop_td *desktop, surface_td *surface,
        client_td *exclude)
{
    (void) desktop;
    (void) surface;
    (void) exclude;

    s_call_client_focus_fallback++;
}


/** Link-only stand-in for drag_is_active */
bool drag_is_active(void)
{
    s_call_drag_is_active++;
    return s_drag_is_active_result;
}


/** Link-only stand-in for drag_client */
client_td *drag_client(void)
{
    s_call_drag_client++;
    return s_drag_client_result;
}


/** Link-only stand-in for drag_cancel */
void drag_cancel(xcb_connection_t *connection, const client_td *client)
{
    (void) connection;
    (void) client;

    s_call_drag_cancel++;
}


/** Link-only stand-in for place_manual_cancel_client */
void place_manual_cancel_client(xcb_connection_t *connection,
        const client_td *client)
{
    (void) connection;
    (void) client;

    s_call_place_manual_cancel_client++;
}


/** Link-only stand-in for surface_refresh_workareas */
void surface_refresh_workareas(surface_td *surface)
{
    (void) surface;

    s_call_surface_refresh_workareas++;
}


/** Link-only stand-in for ccmd_clear_wm_state */
void ccmd_clear_wm_state(client_td *client)
{
    (void) client;

    s_call_ccmd_clear_wm_state++;
}


/** Link-only stand-in for ccmd_set_wm_state */
void ccmd_set_wm_state(client_td *client, uint32_t state,
        xcb_window_t icon_window)
{
    (void) client;
    (void) state;
    (void) icon_window;

    s_call_ccmd_set_wm_state++;
}


/** Link-only stand-in for ccmd_client_sync_states */
void ccmd_client_sync_states(client_td *client)
{
    (void) client;

    s_call_ccmd_client_sync_states++;
}


/** Link-only stand-in for ipc_broadcast_event; frees the fields
 *  object it is handed, mirroring the ownership the real function
 *  documents, so a test that triggers this path leaves nothing
 *  leaked for ASan to catch */
void ipc_broadcast_event(uint32_t type, cJSON *fields)
{
    (void) type;

    s_call_ipc_broadcast_event++;
    cJSON_Delete(fields);
}


/** Link-only stand-in for wm_request_client_redraw */
/** Test-controlled stand-ins for the shutdown gathering: a client
 *  mapping while everything is closing is brought to the desktop and
 *  page being looked at, which this file records rather than performs
 * @note Complexity: @e O(1) */
static bool s_shutdown_in_progress;
static int s_call_shutdown_gather;

bool wm_shutdown_is_in_progress(void)
{
    return s_shutdown_in_progress;
}


void wm_shutdown_gather_client(client_td *client)
{
    (void) client;

    s_call_shutdown_gather++;
}


void wm_request_client_redraw(client_td *client)
{
    (void) client;

    s_call_wm_request_client_redraw++;
}


/** Link-only stand-in for client_send_synthetic_configure_notify */
void client_send_synthetic_configure_notify(
        xcb_connection_t *connection, const client_td *client)
{
    (void) connection;
    (void) client;

    s_call_client_send_synthetic_configure_notify++;
}


/** Link-only stand-in for client_decoration_layout_sync */
void client_decoration_layout_sync(client_td *client)
{
    (void) client;

    s_call_client_decoration_layout_sync++;
}


/** Link-only stand-in for ccmd_desktop_enforce_layers */
void ccmd_desktop_enforce_layers(desktop_td *desktop)
{
    (void) desktop;

    s_call_ccmd_desktop_enforce_layers++;
}


/** Link-only stand-in for ccmd_target_win */
xcb_window_t ccmd_target_win(client_td *client)
{
    s_call_ccmd_target_win++;
    return client->window;
}


/** Link-only stand-in for xcb_map_window (real xcb prototype, but
 *  never the real library: only headers are included, never linked) */
xcb_void_cookie_t xcb_map_window(xcb_connection_t *c,
        xcb_window_t window)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) window;

    s_call_map_window++;

    return cookie;
}


/** Link-only stand-in for xcb_unmap_window */
xcb_void_cookie_t xcb_unmap_window(xcb_connection_t *c,
        xcb_window_t window)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) window;

    s_call_unmap_window++;

    return cookie;
}


/** Link-only stand-in for xcb_destroy_window */
xcb_void_cookie_t xcb_destroy_window(xcb_connection_t *c,
        xcb_window_t window)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) window;

    s_call_destroy_window++;

    return cookie;
}


/** Link-only stand-in for xcb_window_hide, a project-internal helper
 *  (not a real xcb library function) declared in
 *  include/utils/xcb/window.h */
void xcb_window_hide(xcb_window_t window)
{
    (void) window;

    s_call_xcb_window_hide++;
}


/** Link-only stand-in for xcb_window_destroy, a project-internal
 *  helper (not a real xcb library function) declared in
 *  include/utils/xcb/window.h */
void xcb_window_destroy(xcb_window_t window)
{
    (void) window;

    s_call_xcb_window_destroy++;
}


/** Link-only stand-in for xcb_configure_window; records the target
 *  window and the requested stack mode for scenarios that need to
 *  check what was asked for */
xcb_void_cookie_t xcb_configure_window(xcb_connection_t *c,
        xcb_window_t window, uint16_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = {0};
    const uint32_t *values = (const uint32_t *) value_list;

    (void) c;
    (void) value_mask;

    s_call_configure_window++;
    s_last_configure_window_target = window;
    if (value_list != NULL) {
        s_last_configure_window_stack_mode = values[0];
    }

    return cookie;
}


/** Link-only stand-in for xcb_clear_area */
xcb_void_cookie_t xcb_clear_area(xcb_connection_t *c, uint8_t exposures,
        xcb_window_t window, int16_t x, int16_t y, uint16_t width,
        uint16_t height)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) exposures;
    (void) window;
    (void) x;
    (void) y;
    (void) width;
    (void) height;

    s_call_clear_area++;

    return cookie;
}


/** Link-only stand-in for xcb_change_window_attributes */
xcb_void_cookie_t xcb_change_window_attributes(xcb_connection_t *c,
        xcb_window_t window, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) window;
    (void) value_mask;
    (void) value_list;

    s_call_change_window_attributes++;

    return cookie;
}


/** Link-only stand-in for mouse_plain_cursor */
xcb_cursor_t mouse_plain_cursor(void)
{
    s_call_mouse_plain_cursor++;
    return (xcb_cursor_t) 42;
}


/** Link-only stand-in for xcb_connection_get */
xcb_connection_t *xcb_connection_get(void)
{
    static int dummy;

    return (xcb_connection_t *) &dummy;
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
 * @brief Reset every stand-in's recorded state and fixture
 */
static void s_test_reset_state(void)
{
    s_call_lookup_find_client = 0u;
    s_call_lookup_surface_for_root = 0u;
    s_call_lookup_current_desktop = 0u;
    s_call_systray_icon_map_request = 0u;
    s_call_systray_handle_destroy = 0u;
    s_call_cctl_sn_desktop_for_window = 0u;
    s_call_memguard_max_clients = 0u;
    s_call_memguard_warn_client_cap = 0u;
    s_call_client_init = 0u;
    s_call_client_destroy = 0u;
    s_call_desktop_action_client_add = 0u;
    s_call_desktop_action_client_rem = 0u;
    s_call_client_focus_fallback = 0u;
    s_call_drag_is_active = 0u;
    s_call_drag_cancel = 0u;
    s_call_drag_client = 0u;
    s_call_place_manual_cancel_client = 0u;
    s_call_surface_refresh_workareas = 0u;
    s_call_ccmd_clear_wm_state = 0u;
    s_call_ccmd_set_wm_state = 0u;
    s_call_ccmd_client_sync_states = 0u;
    s_call_ipc_broadcast_event = 0u;
    s_call_wm_request_client_redraw = 0u;
    s_call_client_send_synthetic_configure_notify = 0u;
    s_call_client_decoration_layout_sync = 0u;
    s_call_ccmd_desktop_enforce_layers = 0u;
    s_call_ccmd_target_win = 0u;
    s_call_map_window = 0u;
    s_call_unmap_window = 0u;
    s_call_destroy_window = 0u;
    s_call_xcb_window_hide = 0u;
    s_call_xcb_window_destroy = 0u;
    s_call_configure_window = 0u;
    s_call_clear_area = 0u;
    s_call_change_window_attributes = 0u;
    s_call_mouse_plain_cursor = 0u;
    s_lookup_find_client_result = NULL;
    s_lookup_find_client_surface_out = NULL;
    s_lookup_find_client_desktop_out = NULL;
    s_lookup_surface_for_root_result = NULL;
    s_lookup_current_desktop_result = NULL;
    s_systray_icon_map_request_result = false;
    s_drag_is_active_result = false;
    s_drag_client_result = NULL;
    s_last_configure_window_stack_mode = 0u;
    s_last_configure_window_target = 0u;
}


/**
 * @brief Build a plain client fixture with a given window ID
 *
 * @param client Client struct to initialize
 * @param window Window ID to assign
 */
static void s_test_build_client(client_td *client, xcb_window_t window)
{
    memset(client, 0, sizeof(*client));
    client->window = window;
    client->id = window;
}


/**
 * @brief Build a wm_td on the stack wired to the given ewmh/config
 *
 * @param wm     Struct to fill
 * @param ewmh   EWMH connection to expose via wm_ewmh
 * @param config Config to expose via wm_config
 */
static void s_test_build_wm(wm_td *wm, xcb_ewmh_connection_t *ewmh,
        config_td *config)
{
    memset(wm, 0, sizeof(*wm));
    wm->connection = (xcb_connection_t *) 0x1234;
    wm->ewmh = ewmh;
    wm->config = config;
}


/* handler_map_request: a null wm or event is a no-op that never even
 * reaches the already-managed lookup */
static void s_test_map_request_null_guards(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    xcb_map_request_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, &config);
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    event.parent = 0x1;

    handler_map_request(NULL, &event);
    handler_map_request(&wm, NULL);

    TAP_OK(s_call_lookup_find_client == 0u,
            "a null wm or event never reaches the already-managed" \
            " lookup");
}


/* handler_map_request: a window already tracked as a managed client
 * is mapped directly, without going through adoption again */
static void s_test_map_request_already_managed(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td existing_client;
    xcb_map_request_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&existing_client, 0x100);
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    event.parent = 0x1;
    s_lookup_find_client_result = &existing_client;

    handler_map_request(&wm, &event);

    TAP_OK(s_call_map_window == 1u,
            "a window already tracked as a managed client is mapped" \
            " directly exactly once");
    TAP_OK(s_call_client_init == 0u,
            "an already-managed window never re-enters client_init");
}


/* handler_map_request: a docked systray icon is left to the systray
 * subsystem instead of being adopted as a top-level client */
static void s_test_map_request_systray_icon(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    xcb_map_request_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, &config);
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    event.parent = 0x1;
    s_lookup_find_client_result = NULL;
    s_systray_icon_map_request_result = true;

    handler_map_request(&wm, &event);

    TAP_OK(s_call_client_init == 0u,
            "a docked systray icon is never adopted as a top-level" \
            " client");
}


/* handler_map_request: no resolvable surface at all (empty surface
 * list too) leaves the window entirely unmapped and unmanaged */
static void s_test_map_request_no_surface(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    list_td *surfaces;
    xcb_map_request_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    surfaces = list_init(NULL);
    s_test_build_wm(&wm, &ewmh, &config);
    wm.surfaces = surfaces;
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    event.parent = 0x1;
    s_lookup_find_client_result = NULL;
    s_systray_icon_map_request_result = false;
    s_lookup_surface_for_root_result = NULL;

    handler_map_request(&wm, &event);

    TAP_OK(s_call_map_window == 0u,
            "a window with no resolvable surface and an empty" \
            " surface list is left entirely unmapped");
    TAP_OK(s_call_lookup_current_desktop == 0u,
            "with no surface resolved, the current-desktop lookup" \
            " is never reached");

    list_destroy(surfaces);
}


/* handler_map_request: a resolvable surface with no current desktop
 * maps the window as unmanaged (visible, but untracked) */
static void s_test_map_request_no_current_desktop(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    surface_td surface;
    xcb_map_request_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&surface, 0, sizeof(surface));
    s_test_build_wm(&wm, &ewmh, &config);
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    event.parent = 0x1;
    s_lookup_find_client_result = NULL;
    s_systray_icon_map_request_result = false;
    s_lookup_surface_for_root_result = &surface;
    s_lookup_current_desktop_result = NULL;

    handler_map_request(&wm, &event);

    TAP_OK(s_call_map_window == 1u,
            "a resolvable surface with no current desktop maps the" \
            " window unmanaged exactly once");
    TAP_OK(s_call_client_init == 0u,
            "with no current desktop resolved, client_init is never" \
            " reached");
}


/* handler_map_notify: a null event is a no-op */
static void s_test_map_notify_null_event(void)
{
    s_test_reset_state();

    handler_map_notify((xcb_connection_t *) 0x1234, NULL, NULL);

    TAP_OK(s_call_lookup_find_client == 0u,
            "a null event never reaches the client lookup");
}


/* handler_map_notify: an override-redirect window (tooltips, menus)
 * is ignored outright */
static void s_test_map_notify_override_redirect_ignored(void)
{
    xcb_map_notify_event_t event;

    s_test_reset_state();
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    event.override_redirect = 1;

    handler_map_notify((xcb_connection_t *) 0x1234, NULL, &event);

    TAP_OK(s_call_lookup_find_client == 0u,
            "an override-redirect window never even reaches the" \
            " client lookup");
}


/* handler_map_notify: an unresolvable window is a no-op past the
 * lookup */
static void s_test_map_notify_unresolvable(void)
{
    xcb_map_notify_event_t event;

    s_test_reset_state();
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = NULL;

    handler_map_notify((xcb_connection_t *) 0x1234, NULL, &event);

    TAP_OK(s_call_wm_request_client_redraw == 0u,
            "an unresolvable window never requests a redraw");
}


/* handler_map_notify: a resolved, decorated client whose own window
 * mapped gets redrawn, re-synced with a synthetic ConfigureNotify,
 * and its cursor re-asserted */
static void s_test_map_notify_decorated_client(void)
{
    client_td client;
    xcb_map_notify_event_t event;

    s_test_reset_state();
    s_test_build_client(&client, 0x100);
    client.frame = 0x200;
    client.properties.flags = (uint16_t) CLIENT_FLAG_DECORATED;
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = &client;

    handler_map_notify((xcb_connection_t *) 0x1234, NULL, &event);

    TAP_OK(s_call_wm_request_client_redraw == 1u,
            "a resolved client is redrawn exactly once");
    TAP_OK(s_call_client_send_synthetic_configure_notify == 1u,
            "a decorated client whose own window just mapped" \
            " receives a synthetic ConfigureNotify");
    TAP_OK(s_call_clear_area == 1u,
            "the client's own window just mapping forces an Expose" \
            " via xcb_clear_area");
    TAP_OK(s_call_change_window_attributes == 1u,
            "the client's own window mapping re-asserts the plain" \
            " cursor");
}


/* handler_map_notify: a resolved but undecorated client's own window
 * mapping never sends a synthetic ConfigureNotify (only decorated
 * clients need the re-sync) */
static void s_test_map_notify_undecorated_client_no_resync(void)
{
    client_td client;
    xcb_map_notify_event_t event;

    s_test_reset_state();
    s_test_build_client(&client, 0x100);
    client.frame = 0x200;
    client.properties.flags = 0u;
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = &client;

    handler_map_notify((xcb_connection_t *) 0x1234, NULL, &event);

    TAP_OK(s_call_client_send_synthetic_configure_notify == 0u,
            "an undecorated client's own window mapping sends no" \
            " synthetic ConfigureNotify");
}


/* handler_gravity_notify: a null event is a no-op */
static void s_test_gravity_notify_null_event(void)
{
    s_test_reset_state();

    handler_gravity_notify((xcb_connection_t *) 0x1234, NULL, NULL);

    TAP_OK(s_call_lookup_find_client == 0u,
            "a null event never reaches the client lookup");
}


/* handler_gravity_notify: an unresolvable window is a no-op */
static void s_test_gravity_notify_unresolvable(void)
{
    xcb_gravity_notify_event_t event;

    s_test_reset_state();
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = NULL;

    handler_gravity_notify((xcb_connection_t *) 0x1234, NULL, &event);

    TAP_OK(s_call_client_decoration_layout_sync == 0u,
            "an unresolvable window never re-syncs any decoration" \
            " layout");
}


/* handler_gravity_notify: a resolved client has its cached position
 * updated and its decoration re-synced */
static void s_test_gravity_notify_updates_position(void)
{
    client_td client;
    xcb_gravity_notify_event_t event;

    s_test_reset_state();
    s_test_build_client(&client, 0x100);
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    event.x = 42;
    event.y = -7;
    s_lookup_find_client_result = &client;

    handler_gravity_notify((xcb_connection_t *) 0x1234, NULL, &event);

    TAP_EQ_INT((int) client.layout.geometry.cur.pos.x, 42,
            "the frame's new X position is cached on the client");
    TAP_EQ_INT((int) client.layout.geometry.cur.pos.y, -7,
            "the frame's new Y position is cached on the client");
    TAP_OK(s_call_client_decoration_layout_sync == 1u,
            "a resolved client's decoration layout is re-synced" \
            " exactly once");
    TAP_OK(s_call_wm_request_client_redraw == 1u,
            "a resolved client is redrawn exactly once");
}


/* handler_gravity_notify: a decorated, framed client also gets a
 * synthetic ConfigureNotify; an undecorated one does not */
static void s_test_gravity_notify_decorated_resync(void)
{
    client_td client;
    xcb_gravity_notify_event_t event;

    s_test_reset_state();
    s_test_build_client(&client, 0x100);
    client.frame = 0x200;
    client.properties.flags = (uint16_t) CLIENT_FLAG_DECORATED;
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = &client;

    handler_gravity_notify((xcb_connection_t *) 0x1234, NULL, &event);

    TAP_OK(s_call_client_send_synthetic_configure_notify == 1u,
            "a decorated, framed client receives a synthetic" \
            " ConfigureNotify after a gravity move");

    s_test_reset_state();
    s_test_build_client(&client, 0x100);
    client.frame = 0x200;
    client.properties.flags = 0u;
    s_lookup_find_client_result = &client;

    handler_gravity_notify((xcb_connection_t *) 0x1234, NULL, &event);

    TAP_OK(s_call_client_send_synthetic_configure_notify == 0u,
            "an undecorated, framed client receives no synthetic" \
            " ConfigureNotify after a gravity move");
}


/* handler_circulate_notify: a null event is a no-op */
static void s_test_circulate_notify_null_event(void)
{
    s_test_reset_state();

    handler_circulate_notify((xcb_connection_t *) 0x1234, NULL, NULL);

    TAP_OK(s_call_lookup_surface_for_root == 0u,
            "a null event never reaches the surface lookup");
}


/* handler_circulate_notify: a resolved root outdates its surface */
static void s_test_circulate_notify_outdates_surface(void)
{
    surface_td surface;
    xcb_circulate_notify_event_t event;

    s_test_reset_state();
    memset(&surface, 0, sizeof(surface));
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    event.event = 0x1;
    s_lookup_surface_for_root_result = &surface;

    handler_circulate_notify((xcb_connection_t *) 0x1234, NULL, &event);

    TAP_OK(surface.is_outdated,
            "a resolved circulate-notify root outdates its surface");
}


/* handler_circulate_request: a null event is a no-op */
static void s_test_circulate_request_null_event(void)
{
    s_test_reset_state();

    handler_circulate_request((xcb_connection_t *) 0x1234, NULL, NULL);

    TAP_OK(s_call_lookup_find_client == 0u,
            "a null event never reaches the client lookup");
}


/* handler_circulate_request: an unresolvable window is a no-op */
static void s_test_circulate_request_unresolvable(void)
{
    xcb_circulate_request_event_t event;

    s_test_reset_state();
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = NULL;

    handler_circulate_request((xcb_connection_t *) 0x1234, NULL,
            &event);

    TAP_OK(s_call_configure_window == 0u,
            "an unresolvable window never configures anything");
}


/* handler_circulate_request: placing on top asks for
 * XCB_STACK_MODE_ABOVE, and layer ordering is re-enforced afterward */
static void s_test_circulate_request_place_on_top(void)
{
    client_td client;
    desktop_td desktop;
    xcb_circulate_request_event_t event;

    s_test_reset_state();
    s_test_build_client(&client, 0x100);
    memset(&desktop, 0, sizeof(desktop));
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    event.place = XCB_PLACE_ON_TOP;
    s_lookup_find_client_result = &client;
    s_lookup_find_client_desktop_out = &desktop;

    handler_circulate_request((xcb_connection_t *) 0x1234, NULL,
            &event);

    TAP_EQ_INT((int) s_last_configure_window_stack_mode,
            (int) XCB_STACK_MODE_ABOVE,
            "PlaceOnTop asks for XCB_STACK_MODE_ABOVE");
    TAP_EQ_INT((int) s_last_configure_window_target, (int) 0x100,
            "the configure request targets the resolved client's" \
            " own target window");
    TAP_OK(s_call_ccmd_desktop_enforce_layers == 1u,
            "layer ordering is re-enforced exactly once after" \
            " honoring the raw circulate request");
    TAP_OK(s_call_wm_request_client_redraw == 1u,
            "a resolved client is redrawn exactly once");
}


/* handler_circulate_request: placing on the bottom asks for
 * XCB_STACK_MODE_BELOW */
static void s_test_circulate_request_place_on_bottom(void)
{
    client_td client;
    desktop_td desktop;
    xcb_circulate_request_event_t event;

    s_test_reset_state();
    s_test_build_client(&client, 0x100);
    memset(&desktop, 0, sizeof(desktop));
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    event.place = XCB_PLACE_ON_BOTTOM;
    s_lookup_find_client_result = &client;
    s_lookup_find_client_desktop_out = &desktop;

    handler_circulate_request((xcb_connection_t *) 0x1234, NULL,
            &event);

    TAP_EQ_INT((int) s_last_configure_window_stack_mode,
            (int) XCB_STACK_MODE_BELOW,
            "PlaceOnBottom asks for XCB_STACK_MODE_BELOW");
}


/* handler_unmap_notify: a null event is a no-op */
static void s_test_unmap_notify_null_event(void)
{
    s_test_reset_state();

    handler_unmap_notify((xcb_connection_t *) 0x1234, NULL, NULL);

    TAP_OK(s_call_lookup_find_client == 0u,
            "a null event never reaches the client lookup");
}


/* handler_unmap_notify: an unresolvable window is a no-op */
static void s_test_unmap_notify_unresolvable(void)
{
    xcb_unmap_notify_event_t event;

    s_test_reset_state();
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = NULL;

    handler_unmap_notify((xcb_connection_t *) 0x1234, NULL, &event);

    TAP_OK(s_call_ccmd_set_wm_state == 0u,
            "an unresolvable window never publishes any WM_STATE");
}


/* handler_unmap_notify: an unmap of a resolved client's non-primary
 * window (e.g., a stray decoration unmap event) decrements the ignore
 * counter without withdrawing the client, when the counter was
 * already positive, and is a silent no-op past that when it was not */
static void s_test_unmap_notify_non_primary_window(void)
{
    client_td client;
    xcb_unmap_notify_event_t event;

    s_test_reset_state();
    s_test_build_client(&client, 0x100);
    client.ignore.unmap = 2u;
    memset(&event, 0, sizeof(event));
    event.window = 0x999; /* Not client.window */
    s_lookup_find_client_result = &client;

    handler_unmap_notify((xcb_connection_t *) 0x1234, NULL, &event);

    TAP_EQ_INT((int) client.ignore.unmap, 1,
            "an unmap of a non-primary window decrements a positive" \
            " ignore counter");
    TAP_OK(s_call_ccmd_set_wm_state == 0u,
            "an unmap of a non-primary window never withdraws the" \
            " client");
}


/* handler_unmap_notify: an ignored unmap of the primary window (the
 * window manager's own synthetic unmap) is swallowed without
 * withdrawing the client */
static void s_test_unmap_notify_ignored_primary(void)
{
    client_td client;
    xcb_unmap_notify_event_t event;

    s_test_reset_state();
    s_test_build_client(&client, 0x100);
    client.ignore.unmap = 1u;
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = &client;

    handler_unmap_notify((xcb_connection_t *) 0x1234, NULL, &event);

    TAP_EQ_INT((int) client.ignore.unmap, 0,
            "a self-inflicted unmap of the primary window consumes" \
            " one ignore credit");
    TAP_OK(s_call_ccmd_set_wm_state == 0u,
            "a self-inflicted unmap of the primary window never" \
            " withdraws the client");
}


/* handler_unmap_notify: a genuine self-withdrawal of the primary
 * window hides the client, falls back focus when it was active,
 * hides its decorations, resets its state, and withdraws it via
 * EWMH */
static void s_test_unmap_notify_genuine_withdrawal(void)
{
    client_td client;
    surface_td surface;
    desktop_td desktop;
    xcb_unmap_notify_event_t event;

    s_test_reset_state();
    s_test_build_client(&client, 0x100);
    client.frame = 0x200;
    client.titlebar = 0x300;
    client.properties.state = (uint16_t) CLIENT_STATE_FULLSCREEN;
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    desktop.client_active_id = client.id;
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = &client;
    s_lookup_find_client_surface_out = &surface;
    s_lookup_find_client_desktop_out = &desktop;

    handler_unmap_notify((xcb_connection_t *) 0x1234, NULL, &event);

    TAP_OK(s_call_client_focus_fallback == 1u,
            "withdrawing the currently active client falls back" \
            " focus exactly once");
    TAP_OK(s_call_xcb_window_hide == 2u,
            "withdrawing a client with both a frame and a titlebar" \
            " unmaps both decoration windows");
    TAP_EQ_INT((int) client.ignore.unmap, 2,
            "unmapping both decoration windows credits one ignore" \
            " count each");
    TAP_EQ_INT((int) client.properties.state,
            (int) CLIENT_STATE_NORMAL,
            "a genuine self-withdrawal resets the client's own" \
            " state to plain normal, not iconified");
    TAP_OK(s_call_ccmd_set_wm_state == 1u,
            "a genuine self-withdrawal publishes WM_STATE exactly" \
            " once");
    TAP_OK(s_call_ccmd_client_sync_states == 1u,
            "a genuine self-withdrawal syncs the client's EWMH" \
            " states exactly once");
    TAP_OK(client.is_outdated && surface.is_outdated &&
            desktop.is_outdated,
            "a genuine self-withdrawal outdates the client," \
            " surface, and desktop directly");
}


/* handler_unmap_notify: withdrawing a client that was not the active
 * one on its desktop never triggers a focus fallback */
static void s_test_unmap_notify_withdrawal_not_active(void)
{
    client_td client;
    client_td other_client;
    surface_td surface;
    desktop_td desktop;
    xcb_unmap_notify_event_t event;

    s_test_reset_state();
    s_test_build_client(&client, 0x100);
    s_test_build_client(&other_client, 0x777);
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    desktop.client_active_id = other_client.id;
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = &client;
    s_lookup_find_client_surface_out = &surface;
    s_lookup_find_client_desktop_out = &desktop;

    handler_unmap_notify((xcb_connection_t *) 0x1234, NULL, &event);

    TAP_OK(s_call_client_focus_fallback == 0u,
            "withdrawing a client that was not the desktop's active" \
            " one triggers no focus fallback");
}


/* handler_destroy_notify: a null event is a no-op past
 * systray_handle_destroy, which unconditionally still runs */
static void s_test_destroy_notify_null_event(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, &config);

    handler_destroy_notify(&wm, (xcb_connection_t *) 0x1234, NULL,
            NULL);

    TAP_OK(s_call_lookup_find_client == 0u,
            "a null event never reaches the client lookup");
}


/* handler_destroy_notify: systray_handle_destroy runs unconditionally
 * before the managed-client lookup, so a destroyed docked icon (never
 * a managed client) still gets cleaned out of the tray */
static void s_test_destroy_notify_always_notifies_systray(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    xcb_destroy_notify_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, &config);
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = NULL;

    handler_destroy_notify(&wm, (xcb_connection_t *) 0x1234, NULL,
            &event);

    TAP_OK(s_call_systray_handle_destroy == 1u,
            "systray is notified of a destroyed window exactly once" \
            " even when it is not a managed client");
    TAP_OK(s_call_client_destroy == 0u,
            "an unresolvable window is never itself destroyed as a" \
            " client");
}


/* handler_destroy_notify: a destroy of a resolved client's non-primary
 * window (e.g., a stray child) is a no-op past the lookup */
static void s_test_destroy_notify_non_primary_window(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    xcb_destroy_notify_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    memset(&event, 0, sizeof(event));
    event.window = 0x999; /* Not client.window */
    s_lookup_find_client_result = &client;

    handler_destroy_notify(&wm, (xcb_connection_t *) 0x1234, NULL,
            &event);

    TAP_OK(s_call_client_destroy == 0u,
            "a destroy notification for a non-primary window never" \
            " destroys the resolved client");
}


/* handler_destroy_notify: an active drag on the destroyed client is
 * cancelled first */
static void s_test_destroy_notify_cancels_active_drag(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    surface_td surface;
    xcb_destroy_notify_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    memset(&surface, 0, sizeof(surface));
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = &client;
    s_lookup_find_client_surface_out = &surface;
    s_drag_is_active_result = true;
    s_drag_client_result = &client;

    handler_destroy_notify(&wm, (xcb_connection_t *) 0x1234, NULL,
            &event);

    TAP_OK(s_call_drag_cancel == 1u,
            "an active drag on the destroyed client is cancelled" \
            " exactly once");
}


/* handler_destroy_notify: an active drag on a DIFFERENT client is left
 * alone */
static void s_test_destroy_notify_leaves_unrelated_drag(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    client_td other_client;
    surface_td surface;
    xcb_destroy_notify_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_client(&other_client, 0x777);
    memset(&surface, 0, sizeof(surface));
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = &client;
    s_lookup_find_client_surface_out = &surface;
    s_drag_is_active_result = true;
    s_drag_client_result = &other_client;

    handler_destroy_notify(&wm, (xcb_connection_t *) 0x1234, NULL,
            &event);

    TAP_OK(s_call_drag_cancel == 0u,
            "an active drag on an unrelated client is never" \
            " cancelled by this destroy");
}


/* handler_destroy_notify: destroying the content window destroys the
 * now-orphaned frame for real and increments the ignore counter to
 * swallow its own resulting UnmapNotify.  Only this content-window
 * branch is reachable at all: the guard clause a few lines above in
 * the real function, "if (event->window != client->window) return;",
 * already forces 'event->window' to equal 'client->window' by the
 * time the frame-vs-content check below it runs, so its "the frame
 * itself was destroyed" arm, "if (event->window == client->frame)",
 * can only ever be taken in the degenerate case where 'client->frame'
 * happens to equal 'client->window', which never occurs, since
 * 'include/client.h' documents them as two always-distinct windows
 * (the actual client window versus its optional decoration frame).
 * That arm is therefore dead code under the real object model, and is
 * not exercised here since doing so would only prove a stand-in
 * mirrors an unreachable branch, not that this function's own logic
 * is correct */
static void s_test_destroy_notify_frame_vs_content(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    desktop_td desktop;
    surface_td surface;
    xcb_destroy_notify_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    client.frame = 0x200;
    client.titlebar = 0x300;
    memset(&desktop, 0, sizeof(desktop));
    memset(&surface, 0, sizeof(surface));
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = &client;
    s_lookup_find_client_surface_out = &surface;
    s_lookup_find_client_desktop_out = &desktop;

    handler_destroy_notify(&wm, (xcb_connection_t *) 0x1234, NULL,
            &event);

    TAP_EQ_INT((int) client.frame, 0,
            "destroying the content window still clears the frame" \
            " field afterward");
    TAP_EQ_INT((int) client.titlebar, 0,
            "destroying the content window still clears the" \
            " titlebar field afterward");
    TAP_OK(s_call_xcb_window_destroy == 1u,
            "destroying the content window (not the frame) also" \
            " really destroys the now-orphaned frame exactly once");
    TAP_EQ_INT((int) client.ignore.unmap, 1,
            "really destroying the frame credits one ignore count" \
            " for its own resulting UnmapNotify");
}


/* handler_destroy_notify: removing a client from a resolved desktop
 * refreshes the surface's work areas, and falls back focus only when
 * that client was the desktop's active one */
static void s_test_destroy_notify_removes_from_desktop(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    desktop_td desktop;
    surface_td surface;
    xcb_destroy_notify_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    memset(&desktop, 0, sizeof(desktop));
    desktop.client_active_id = client.id;
    memset(&surface, 0, sizeof(surface));
    memset(&event, 0, sizeof(event));
    event.window = 0x100;
    s_lookup_find_client_result = &client;
    s_lookup_find_client_surface_out = &surface;
    s_lookup_find_client_desktop_out = &desktop;

    handler_destroy_notify(&wm, (xcb_connection_t *) 0x1234, NULL,
            &event);

    TAP_OK(s_call_desktop_action_client_rem == 1u,
            "a client destroyed on a resolved desktop is removed" \
            " from it exactly once");
    TAP_OK(s_call_surface_refresh_workareas == 1u,
            "removing a destroyed client refreshes its surface's" \
            " work areas exactly once");
    TAP_OK(s_call_client_focus_fallback == 1u,
            "destroying the desktop's own active client falls back" \
            " focus exactly once");
    TAP_OK(s_call_ccmd_clear_wm_state == 1u,
            "a destroyed client has its WM_STATE cleared exactly" \
            " once");
    TAP_OK(s_call_client_destroy == 1u,
            "a fully resolved, destroyed client is itself torn down" \
            " exactly once");
    TAP_OK(surface.is_outdated && desktop.is_outdated,
            "destroying a client outdates its surface and desktop");
}


int main(void)
{
    TAP_PLAN(63);

    s_test_map_request_null_guards();
    s_test_map_request_already_managed();
    s_test_map_request_systray_icon();
    s_test_map_request_no_surface();
    s_test_map_request_no_current_desktop();
    s_test_map_notify_null_event();
    s_test_map_notify_override_redirect_ignored();
    s_test_map_notify_unresolvable();
    s_test_map_notify_decorated_client();
    s_test_map_notify_undecorated_client_no_resync();
    s_test_gravity_notify_null_event();
    s_test_gravity_notify_unresolvable();
    s_test_gravity_notify_updates_position();
    s_test_gravity_notify_decorated_resync();
    s_test_circulate_notify_null_event();
    s_test_circulate_notify_outdates_surface();
    s_test_circulate_request_null_event();
    s_test_circulate_request_unresolvable();
    s_test_circulate_request_place_on_top();
    s_test_circulate_request_place_on_bottom();
    s_test_unmap_notify_null_event();
    s_test_unmap_notify_unresolvable();
    s_test_unmap_notify_non_primary_window();
    s_test_unmap_notify_ignored_primary();
    s_test_unmap_notify_genuine_withdrawal();
    s_test_unmap_notify_withdrawal_not_active();
    s_test_destroy_notify_null_event();
    s_test_destroy_notify_always_notifies_systray();
    s_test_destroy_notify_non_primary_window();
    s_test_destroy_notify_cancels_active_drag();
    s_test_destroy_notify_leaves_unrelated_drag();
    s_test_destroy_notify_frame_vs_content();
    s_test_destroy_notify_removes_from_desktop();

    return TAP_DONE();
}
