/**
 * @file tests/handler/test_ewmh.c
 *
 * @brief Unit tests for @c handler/ewmh.c
 *
 * Covers all 8 public @c hi_handle_net_* sub-handlers that
 * @a handler_message_client (handler/message.c, see
 * tests/handler/test_message.c) dispatches to for a specific EWMH
 * atom, exercised entirely through synthetic
 * @c xcb_client_message_event_t structs, hand-built @c client_td/
 * @c stage_td/@c desktop_td/@c wm_td fixtures (all transparent
 * structs, safe on the stack), and a hand-built
 * @c xcb_ewmh_connection_t with sentinel state atoms.  No live X
 * connection is ever needed.
 *
 * Given the breadth of @a hi_handle_net_wm_state's per-atom dispatch
 * (12 distinct EWMH state atoms, each with add/remove/toggle), this
 * file tests a representative cross-section covering each of the
 * three resolution outcomes (add/remove/toggle) at least once, one
 * scenario per remaining atom to prove the correct collaborator is
 * reached, and the "unrecognized atom" no-op, rather than every one of
 * the 36 add/remove/toggle combinations across every atom, since each
 * atom's own dispatch arm is structurally identical (compare, resolve,
 * branch) and @a s_wm_state_resolve_add's own three outcomes are
 * already fully covered by the first few scenarios.
 *
 * Deliberately out of scope: the internals of every collaborator
 * command this module calls (each already has, or will have, its own
 * dedicated test file under @c tests/cmds/, @c tests/policy/, etc.);
 * here only "did the right collaborator get called, was the outdate
 * bookkeeping done, and were the guard clauses honored" is checked,
 * which is everything each @c hi_handle_net_* function itself is
 * responsible for.  @a hi_handle_net_wm_desktop's transient-family
 * cascade snapshot allocation path is exercised only for the
 * no-siblings (@c NULL snapshot) case, since a real heap-allocated
 * @c client_td** snapshot array is otherwise just a stand-in
 * returning whatever this file hands it back, adding no further
 * coverage of @a hi_handle_net_wm_desktop's own logic beyond what the
 * single-client path already proves.
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

/* Command includes */
#include <cmds/client/ewmh.h>
#include <cmds/client/flags.h>
#include <cmds/client/focus.h>
#include <cmds/client/layer.h>
#include <cmds/client/maximize.h>
#include <cmds/client/state.h>
#include <cmds/client/transient.h>
#include <cmds/client/visibility.h>
#include <cmds/stage.h>

/* Input includes */
#include <input/mouse/drag.h>

/* Policy includes */
#include <policy/focus.h>
#include <policy/stacking.h>

/* Utils includes */
#include <utils/xcb/atom.h>
#include <utils/xcb/connection.h>

/* Definition includes */
#include <defs/client.h>
#include <defs/ewmh.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <stage.h>
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <handler/internal.h>
#include <harness/tap.h>


/* Call counters/recorders for every link-only stand-in below */
static unsigned int s_call_fullscreen = 0u;
static unsigned int s_call_unfullscreen = 0u;
static unsigned int s_call_toggle_fullscreen = 0u;
static unsigned int s_call_maximize_horz = 0u;
static unsigned int s_call_maximize_vert = 0u;
static unsigned int s_call_layer_above = 0u;
static unsigned int s_call_layer_below = 0u;
static unsigned int s_call_layer_normal = 0u;
static unsigned int s_call_pin = 0u;
static unsigned int s_call_unpin = 0u;
static unsigned int s_call_shade = 0u;
static unsigned int s_call_unshade = 0u;
static unsigned int s_call_iconify = 0u;
static unsigned int s_call_restore = 0u;
static unsigned int s_call_urge = 0u;
static unsigned int s_call_unurge = 0u;
static unsigned int s_call_sync_states = 0u;
static unsigned int s_call_desktop_move = 0u;
static unsigned int s_call_unmap_decorated = 0u;
static unsigned int s_call_publish_wm_desktop = 0u;
static unsigned int s_call_transient_top_parent = 0u;
static unsigned int s_call_transient_family_snapshot = 0u;
static unsigned int s_call_lookup_stage_for_root = 0u;
static unsigned int s_call_stage_switch = 0u;
static unsigned int s_call_stage_viewport_set = 0u;
static int32_t s_last_viewport_set_x = 0;
static int32_t s_last_viewport_set_y = 0;
static unsigned int s_call_stage_desktop_get = 0u;
static unsigned int s_call_configure_window = 0u;
static unsigned int s_call_atom_intern = 0u;
static unsigned int s_call_change_property = 0u;
static unsigned int s_call_lookup_current_desktop = 0u;
static unsigned int s_call_stacking_count = 0u;
static unsigned int s_call_stacking_walk = 0u;
static unsigned int s_call_stage_clients_show = 0u;
static unsigned int s_call_stage_clients_hide = 0u;
static unsigned int s_call_set_input_focus = 0u;
static unsigned int s_call_drag_start = 0u;
static unsigned int s_call_drag_start_directed = 0u;
static unsigned int s_call_drag_cancel = 0u;
static unsigned int s_call_set_wm_state = 0u;
static unsigned int s_call_decoration_layout_sync = 0u;
static unsigned int s_call_wm_get_client_desktop = 0u;

/* Controlled return values for the next call */
static desktop_td *s_wm_get_client_desktop_result = NULL;
static client_td *s_transient_top_parent_result = NULL;
static client_td **s_transient_family_snapshot_result = NULL;
static size_t s_transient_family_snapshot_count = 0u;
static stage_td *s_lookup_stage_for_root_result = NULL;
static desktop_td *s_stage_desktop_get_result = NULL;
static desktop_td *s_lookup_current_desktop_result = NULL;
static uint32_t s_stacking_count_result = 0u;
static bool s_drag_is_active_result = false;
static uint32_t s_last_configure_window_value0 = 0u;
static uint16_t s_last_configure_window_mask = 0u;


/** Link-only stand-in for ccmd_client_fullscreen */
void ccmd_client_fullscreen(client_td *client)
{
    (void) client;

    s_call_fullscreen++;
}


/** Link-only stand-in for ccmd_client_unfullscreen */
void ccmd_client_unfullscreen(client_td *client)
{
    (void) client;

    s_call_unfullscreen++;
}


/** Link-only stand-in for ccmd_client_toggle_fullscreen */
void ccmd_client_toggle_fullscreen(client_td *client)
{
    (void) client;

    s_call_toggle_fullscreen++;
}


/** Link-only stand-in for enact_client_maximize_horz */
void enact_client_maximize_horz(client_td *client)
{
    (void) client;

    s_call_maximize_horz++;
}


/** Link-only stand-in for enact_client_maximize_vert */
void enact_client_maximize_vert(client_td *client)
{
    (void) client;

    s_call_maximize_vert++;
}


/** Link-only stand-in for ccmd_client_layer_above */
void ccmd_client_layer_above(client_td *client)
{
    (void) client;

    s_call_layer_above++;
}


/** Link-only stand-in for ccmd_client_layer_below */
void ccmd_client_layer_below(client_td *client)
{
    (void) client;

    s_call_layer_below++;
}


/** Link-only stand-in for ccmd_client_layer_normal */
void ccmd_client_layer_normal(client_td *client)
{
    (void) client;

    s_call_layer_normal++;
}


/** Link-only stand-in for ccmd_client_pin */
void ccmd_client_pin(client_td *client)
{
    (void) client;

    s_call_pin++;
}


/** Link-only stand-in for ccmd_client_unpin */
void ccmd_client_unpin(client_td *client)
{
    (void) client;

    s_call_unpin++;
}


/** Link-only stand-in for ccmd_client_shade */
void ccmd_client_shade(client_td *client)
{
    (void) client;

    s_call_shade++;
}


/** Link-only stand-in for ccmd_client_unshade */
void ccmd_client_unshade(client_td *client)
{
    (void) client;

    s_call_unshade++;
}


/** Link-only stand-in for ccmd_client_iconify */
void ccmd_client_iconify(client_td *client)
{
    (void) client;

    s_call_iconify++;
}


/** Link-only stand-in for ccmd_client_restore */
void ccmd_client_restore(client_td *client)
{
    (void) client;

    s_call_restore++;
}


/** Link-only stand-in for ccmd_client_urge */
void ccmd_client_urge(client_td *client)
{
    (void) client;

    s_call_urge++;
}


/** Link-only stand-in for ccmd_client_unurge */
void ccmd_client_unurge(client_td *client)
{
    (void) client;

    s_call_unurge++;
}


/** Link-only stand-in for ccmd_client_sync_states */
void ccmd_client_sync_states(client_td *client)
{
    (void) client;

    s_call_sync_states++;
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


/** Link-only stand-in for ccmd_client_unmap_decorated */
void ccmd_client_unmap_decorated(client_td *client, xcb_window_t target)
{
    (void) client;
    (void) target;

    s_call_unmap_decorated++;
}


/** Link-only stand-in for ccmd_publish_wm_desktop */
void ccmd_publish_wm_desktop(client_td *client, uint32_t desktop_id)
{
    (void) client;
    (void) desktop_id;

    s_call_publish_wm_desktop++;
}


/** Link-only stand-in for ccmd_client_transient_top_parent */
client_td *ccmd_client_transient_top_parent(client_td *client)
{
    (void) client;

    s_call_transient_top_parent++;
    return s_transient_top_parent_result;
}


/** Link-only stand-in for ccmd_client_transient_family_snapshot */
client_td **ccmd_client_transient_family_snapshot(
        const desktop_td *desktop, client_td *top, size_t *count_out)
{
    (void) desktop;
    (void) top;

    s_call_transient_family_snapshot++;
    if (count_out != NULL) {
        *count_out = s_transient_family_snapshot_count;
    }
    return s_transient_family_snapshot_result;
}


/** Link-only stand-in for wm_get_client_desktop */
desktop_td *wm_get_client_desktop(const client_td *client)
{
    (void) client;

    s_call_wm_get_client_desktop++;
    return s_wm_get_client_desktop_result;
}


/** Link-only stand-in for lookup_stage_for_root */
stage_td *lookup_stage_for_root(list_td *stages,
        xcb_window_t root)
{
    (void) stages;
    (void) root;

    s_call_lookup_stage_for_root++;
    return s_lookup_stage_for_root_result;
}


/** Link-only stand-in for scmd_stage_desktop_switch */
void scmd_stage_desktop_switch(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;

    s_call_stage_switch++;
}


/** Recording stand-in for scmd_stage_viewport_set */
void scmd_stage_viewport_set(stage_td *stage, int32_t x, int32_t y)
{
    (void) stage;

    s_call_stage_viewport_set++;
    s_last_viewport_set_x = x;
    s_last_viewport_set_y = y;
}


/** Link-only stand-in for stage_desktop_get */
desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;

    s_call_stage_desktop_get++;
    return s_stage_desktop_get_result;
}


/** Link-only stand-in for client_decoration_layout_sync */
void client_decoration_layout_sync(client_td *client)
{
    (void) client;

    s_call_decoration_layout_sync++;
}


/** Link-only stand-in for xcb_configure_window; records the first
 *  requested value and the value mask for scenarios that need to
 *  check what was asked for */
xcb_void_cookie_t xcb_configure_window(xcb_connection_t *c,
        xcb_window_t window, uint16_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = {0};
    const uint32_t *values = (const uint32_t *) value_list;

    (void) c;
    (void) window;

    s_call_configure_window++;
    s_last_configure_window_mask = value_mask;
    if (value_list != NULL) {
        s_last_configure_window_value0 = values[0];
    }

    return cookie;
}


/** Link-only stand-in for atom_intern */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    (void) connection;
    (void) name;
    (void) only_if_exists;

    s_call_atom_intern++;
    return (xcb_atom_t) 12345;
}


/** Link-only stand-in for xcb_change_property */
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

    s_call_change_property++;

    return cookie;
}


/** Link-only stand-in for xcb_connection_get */
xcb_connection_t *xcb_connection_get(void)
{
    static int dummy;

    return (xcb_connection_t *) &dummy;
}


/** Link-only stand-in for lookup_current_desktop */
desktop_td *lookup_current_desktop(stage_td *stage)
{
    (void) stage;

    s_call_lookup_current_desktop++;
    return s_lookup_current_desktop_result;
}


/** Link-only stand-in for stacking_count */
uint32_t stacking_count(const desktop_td *desktop)
{
    (void) desktop;

    s_call_stacking_count++;
    return s_stacking_count_result;
}


/** Fixture clients 'stacking_walk' below actually visits, so a
 *  scenario testing 'hi_handle_net_showing_desktop' can make the
 *  real 's_any_visible_visit' it walks with see one; empty by
 *  default, so every other scenario's walk stays a no-op exactly as
 *  before */
static client_td *s_stacking_walk_clients[4];
static uint8_t s_stacking_walk_client_count = 0u;

/** Link-only stand-in for stacking_walk; invokes @p visit against
 *  whichever fixture clients a scenario populated in
 *  's_stacking_walk_clients' beforehand, exactly as the real function
 *  would against a live stacking list, and always records the call so
 *  a scenario can prove the walk phase was reached */
void stacking_walk(const desktop_td *desktop, stacking_visitor_fn visit,
        void *data)
{
    (void) desktop;

    s_call_stacking_walk++;
    if (visit == NULL) {
        return;
    }
    for (uint8_t i = 0u; i < s_stacking_walk_client_count; ++i) {
        visit(s_stacking_walk_clients[i], data);
    }
}


/** Link-only stand-in for stage_client_show_all */
void stage_client_show_all(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;

    s_call_stage_clients_show++;
}


/** Link-only stand-in for stage_client_hide_all */
void stage_client_hide_all(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;

    s_call_stage_clients_hide++;
}


/** Link-only stand-in for xcb_set_input_focus */
xcb_void_cookie_t xcb_set_input_focus(xcb_connection_t *c,
        uint8_t revert_to, xcb_window_t focus, xcb_timestamp_t time)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) revert_to;
    (void) focus;
    (void) time;

    s_call_set_input_focus++;

    return cookie;
}


/** Link-only stand-in for client_last_user_time */
uint32_t client_last_user_time(void)
{
    return 0u;
}


/** Link-only stand-in for drag_is_active */
bool drag_is_active(void)
{
    return s_drag_is_active_result;
}


/** Link-only stand-in for drag_cancel */
void drag_cancel(xcb_connection_t *connection, const client_td *client)
{
    (void) connection;
    (void) client;

    s_call_drag_cancel++;
}


/** Link-only stand-in for drag_start */
void drag_start(xcb_connection_t *connection, xcb_window_t root,
        client_td *client, desktop_td *desktop,
        enum window_operation_e operation, xcb_timestamp_t event_time,
        struct position_s root_pos, struct dimensions_s screen_dim)
{
    (void) connection;
    (void) root;
    (void) client;
    (void) desktop;
    (void) operation;
    (void) event_time;
    (void) root_pos;
    (void) screen_dim;

    s_call_drag_start++;
}


/** Link-only stand-in for drag_start_directed */
void drag_start_directed(xcb_connection_t *connection, xcb_window_t root,
        client_td *client, desktop_td *desktop,
        xcb_timestamp_t event_time, struct position_s root_pos,
        struct dimensions_s screen_dim, bool anchor_right,
        bool anchor_bottom, bool resize_w, bool resize_h)
{
    (void) connection;
    (void) root;
    (void) client;
    (void) desktop;
    (void) event_time;
    (void) root_pos;
    (void) screen_dim;
    (void) anchor_right;
    (void) anchor_bottom;
    (void) resize_w;
    (void) resize_h;

    s_call_drag_start_directed++;
}


/** Link-only stand-in for ccmd_set_wm_state */
void ccmd_set_wm_state(client_td *client, uint32_t state,
        xcb_window_t icon_window)
{
    (void) client;
    (void) state;
    (void) icon_window;

    s_call_set_wm_state++;
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
    s_call_fullscreen = 0u;
    s_call_unfullscreen = 0u;
    s_call_toggle_fullscreen = 0u;
    s_call_maximize_horz = 0u;
    s_call_maximize_vert = 0u;
    s_call_layer_above = 0u;
    s_call_layer_below = 0u;
    s_call_layer_normal = 0u;
    s_call_pin = 0u;
    s_call_unpin = 0u;
    s_call_shade = 0u;
    s_call_unshade = 0u;
    s_call_iconify = 0u;
    s_call_restore = 0u;
    s_call_urge = 0u;
    s_call_unurge = 0u;
    s_call_sync_states = 0u;
    s_call_desktop_move = 0u;
    s_call_unmap_decorated = 0u;
    s_call_publish_wm_desktop = 0u;
    s_call_transient_top_parent = 0u;
    s_call_transient_family_snapshot = 0u;
    s_call_lookup_stage_for_root = 0u;
    s_call_stage_switch = 0u;
    s_call_stage_viewport_set = 0u;
    s_last_viewport_set_x = 0;
    s_last_viewport_set_y = 0;
    s_call_stage_desktop_get = 0u;
    s_call_configure_window = 0u;
    s_call_atom_intern = 0u;
    s_call_change_property = 0u;
    s_call_lookup_current_desktop = 0u;
    s_call_stacking_count = 0u;
    s_call_stacking_walk = 0u;
    s_stacking_walk_client_count = 0u;
    s_call_stage_clients_show = 0u;
    s_call_stage_clients_hide = 0u;
    s_call_set_input_focus = 0u;
    s_call_drag_start = 0u;
    s_call_drag_start_directed = 0u;
    s_call_drag_cancel = 0u;
    s_call_set_wm_state = 0u;
    s_call_decoration_layout_sync = 0u;
    s_call_wm_get_client_desktop = 0u;
    s_wm_get_client_desktop_result = NULL;
    s_transient_top_parent_result = NULL;
    s_transient_family_snapshot_result = NULL;
    s_transient_family_snapshot_count = 0u;
    s_lookup_stage_for_root_result = NULL;
    s_stage_desktop_get_result = NULL;
    s_lookup_current_desktop_result = NULL;
    s_stacking_count_result = 0u;
    s_drag_is_active_result = false;
    s_last_configure_window_value0 = 0u;
    s_last_configure_window_mask = 0u;
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


/* hi_handle_net_wm_state: a null client/event/ewmh is a silent no-op */
static void s_test_wm_state_null_guards(void)
{
    xcb_ewmh_connection_t ewmh;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);

    hi_handle_net_wm_state(NULL, &event, &ewmh, &stage, &desktop);
    hi_handle_net_wm_state(&client, NULL, &ewmh, &stage, &desktop);
    hi_handle_net_wm_state(&client, &event, NULL, &stage, &desktop);

    TAP_OK(s_call_fullscreen == 0u,
            "a null client, event, or ewmh never reaches any state" \
            " collaborator");
}


/* hi_handle_net_wm_state: ADD action on _NET_WM_STATE_FULLSCREEN
 * fullscreens the client and outdates both stage and desktop */
static void s_test_wm_state_fullscreen_add(void)
{
    xcb_ewmh_connection_t ewmh;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE_FULLSCREEN = (xcb_atom_t) 10;
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 999);
    event.data.data32[0] = (uint32_t) WM_STATE_ACTION_ADD;
    event.data.data32[1] = ewmh._NET_WM_STATE_FULLSCREEN;

    hi_handle_net_wm_state(&client, &event, &ewmh, &stage, &desktop);

    TAP_OK(s_call_fullscreen == 1u,
            "ADD on _NET_WM_STATE_FULLSCREEN calls the fullscreen" \
            " collaborator exactly once");
    TAP_OK(stage.is_outdated && desktop.is_outdated,
            "handling _NET_WM_STATE outdates both the stage and" \
            " the desktop");
}


/* hi_handle_net_wm_state: REMOVE action on fullscreen unfullscreens */
static void s_test_wm_state_fullscreen_remove(void)
{
    xcb_ewmh_connection_t ewmh;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE_FULLSCREEN = (xcb_atom_t) 10;
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 999);
    event.data.data32[0] = (uint32_t) WM_STATE_ACTION_REMOVE;
    event.data.data32[1] = ewmh._NET_WM_STATE_FULLSCREEN;

    hi_handle_net_wm_state(&client, &event, &ewmh, &stage, &desktop);

    TAP_OK(s_call_unfullscreen == 1u,
            "REMOVE on _NET_WM_STATE_FULLSCREEN calls the" \
            " unfullscreen collaborator exactly once");
    TAP_OK(s_call_fullscreen == 0u,
            "REMOVE on fullscreen never also calls the fullscreen" \
            " collaborator");
}


/* hi_handle_net_wm_state: TOGGLE action on fullscreen toggles it */
static void s_test_wm_state_fullscreen_toggle(void)
{
    xcb_ewmh_connection_t ewmh;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE_FULLSCREEN = (xcb_atom_t) 10;
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 999);
    event.data.data32[0] = (uint32_t) WM_STATE_ACTION_TOGGLE;
    event.data.data32[1] = ewmh._NET_WM_STATE_FULLSCREEN;

    hi_handle_net_wm_state(&client, &event, &ewmh, &stage, &desktop);

    TAP_OK(s_call_toggle_fullscreen == 1u,
            "TOGGLE on _NET_WM_STATE_FULLSCREEN calls the toggle" \
            " collaborator exactly once");
}


/* hi_handle_net_wm_state: a second atom in data32[2] is processed too,
 * when present */
static void s_test_wm_state_two_atoms(void)
{
    xcb_ewmh_connection_t ewmh;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE_MAXIMIZED_HORZ = (xcb_atom_t) 20;
    ewmh._NET_WM_STATE_MAXIMIZED_VERT = (xcb_atom_t) 21;
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_client(&client, 0x100);
    client.properties.flags = (uint32_t) CLIENT_FLAG_RESIZABLE;
    s_test_build_event(&event, 0x100, (xcb_atom_t) 999);
    event.data.data32[0] = (uint32_t) WM_STATE_ACTION_ADD;
    event.data.data32[1] = ewmh._NET_WM_STATE_MAXIMIZED_HORZ;
    event.data.data32[2] = ewmh._NET_WM_STATE_MAXIMIZED_VERT;

    hi_handle_net_wm_state(&client, &event, &ewmh, &stage, &desktop);

    TAP_OK(s_call_maximize_horz == 1u,
            "the first state atom in data32[1] is processed");
    TAP_OK(s_call_maximize_vert == 1u,
            "a second state atom present in data32[2] is processed" \
            " too");
}


/* hi_handle_net_wm_state: a maximize request on an unmaximizable
 * client is silently ignored */
static void s_test_wm_state_maximize_gated(void)
{
    xcb_ewmh_connection_t ewmh;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE_MAXIMIZED_HORZ = (xcb_atom_t) 20;
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_client(&client, 0x100);
    client.properties.flags = 0u; /* Not resizable, so unmaximizable */
    s_test_build_event(&event, 0x100, (xcb_atom_t) 999);
    event.data.data32[0] = (uint32_t) WM_STATE_ACTION_ADD;
    event.data.data32[1] = ewmh._NET_WM_STATE_MAXIMIZED_HORZ;

    hi_handle_net_wm_state(&client, &event, &ewmh, &stage, &desktop);

    TAP_OK(s_call_maximize_horz == 0u,
            "a maximize request on an unmaximizable (non-resizable)" \
            " client is gated off");
}


/* hi_handle_net_wm_state: above/below/sticky/shaded/hidden/urgent/
 * modal each dispatch to their own pair of collaborators */
static void s_test_wm_state_remaining_atoms(void)
{
    xcb_ewmh_connection_t ewmh;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE_ABOVE = (xcb_atom_t) 30;
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_client(&client, 0x100);
    client.properties.layer = CLIENT_LAYER_NORMAL;
    s_test_build_event(&event, 0x100, (xcb_atom_t) 999);
    event.data.data32[0] = (uint32_t) WM_STATE_ACTION_ADD;
    event.data.data32[1] = ewmh._NET_WM_STATE_ABOVE;
    hi_handle_net_wm_state(&client, &event, &ewmh, &stage, &desktop);
    TAP_OK(s_call_layer_above == 1u,
            "ADD on _NET_WM_STATE_ABOVE calls the layer-above" \
            " collaborator");

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE_BELOW = (xcb_atom_t) 31;
    s_test_build_client(&client, 0x100);
    client.properties.layer = CLIENT_LAYER_NORMAL;
    s_test_build_event(&event, 0x100, (xcb_atom_t) 999);
    event.data.data32[0] = (uint32_t) WM_STATE_ACTION_ADD;
    event.data.data32[1] = ewmh._NET_WM_STATE_BELOW;
    hi_handle_net_wm_state(&client, &event, &ewmh, &stage, &desktop);
    TAP_OK(s_call_layer_below == 1u,
            "ADD on _NET_WM_STATE_BELOW calls the layer-below" \
            " collaborator");

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE_STICKY = (xcb_atom_t) 32;
    s_test_build_client(&client, 0x100);
    client.properties.flags = 0u;
    s_test_build_event(&event, 0x100, (xcb_atom_t) 999);
    event.data.data32[0] = (uint32_t) WM_STATE_ACTION_ADD;
    event.data.data32[1] = ewmh._NET_WM_STATE_STICKY;
    hi_handle_net_wm_state(&client, &event, &ewmh, &stage, &desktop);
    TAP_OK(s_call_pin == 1u,
            "ADD on _NET_WM_STATE_STICKY calls the pin collaborator");

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE_SHADED = (xcb_atom_t) 33;
    s_test_build_client(&client, 0x100);
    client.properties.flags = 0u;
    s_test_build_event(&event, 0x100, (xcb_atom_t) 999);
    event.data.data32[0] = (uint32_t) WM_STATE_ACTION_ADD;
    event.data.data32[1] = ewmh._NET_WM_STATE_SHADED;
    hi_handle_net_wm_state(&client, &event, &ewmh, &stage, &desktop);
    TAP_OK(s_call_shade == 1u,
            "ADD on _NET_WM_STATE_SHADED calls the shade" \
            " collaborator");

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE_HIDDEN = (xcb_atom_t) 34;
    s_test_build_client(&client, 0x100);
    client.properties.flags = 0u;
    s_test_build_event(&event, 0x100, (xcb_atom_t) 999);
    event.data.data32[0] = (uint32_t) WM_STATE_ACTION_ADD;
    event.data.data32[1] = ewmh._NET_WM_STATE_HIDDEN;
    hi_handle_net_wm_state(&client, &event, &ewmh, &stage, &desktop);
    TAP_OK(s_call_iconify == 1u,
            "ADD on _NET_WM_STATE_HIDDEN calls the iconify" \
            " collaborator");

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE_DEMANDS_ATTENTION = (xcb_atom_t) 35;
    s_test_build_client(&client, 0x100);
    client.properties.flags = 0u;
    s_test_build_event(&event, 0x100, (xcb_atom_t) 999);
    event.data.data32[0] = (uint32_t) WM_STATE_ACTION_ADD;
    event.data.data32[1] = ewmh._NET_WM_STATE_DEMANDS_ATTENTION;
    hi_handle_net_wm_state(&client, &event, &ewmh, &stage, &desktop);
    TAP_OK(s_call_urge == 1u,
            "ADD on _NET_WM_STATE_DEMANDS_ATTENTION calls the urge" \
            " collaborator");

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    ewmh._NET_WM_STATE_MODAL = (xcb_atom_t) 36;
    s_test_build_client(&client, 0x100);
    client.properties.flags = 0u;
    s_test_build_event(&event, 0x100, (xcb_atom_t) 999);
    event.data.data32[0] = (uint32_t) WM_STATE_ACTION_ADD;
    event.data.data32[1] = ewmh._NET_WM_STATE_MODAL;
    hi_handle_net_wm_state(&client, &event, &ewmh, &stage, &desktop);
    TAP_OK(s_call_sync_states == 1u,
            "ADD on _NET_WM_STATE_MODAL syncs the client's states" \
            " afterward");
}


/* hi_handle_net_wm_state: an atom matching none of the recognized
 * EWMH states is a silent no-op */
static void s_test_wm_state_unrecognized_atom(void)
{
    xcb_ewmh_connection_t ewmh;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 999);
    event.data.data32[0] = (uint32_t) WM_STATE_ACTION_ADD;
    event.data.data32[1] = (xcb_atom_t) 777777;

    hi_handle_net_wm_state(&client, &event, &ewmh, &stage, &desktop);

    TAP_OK(s_call_fullscreen == 0u && s_call_maximize_horz == 0u &&
            s_call_layer_above == 0u && s_call_pin == 0u &&
            s_call_shade == 0u && s_call_iconify == 0u &&
            s_call_urge == 0u && s_call_sync_states == 0u,
            "an unrecognized state atom reaches no state" \
            " collaborator at all");
}


/* hi_handle_net_current_desktop: null wm/event is a no-op */
static void s_test_current_desktop_null_guards(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_event(&event, 0x1, (xcb_atom_t) 1);

    hi_handle_net_current_desktop(NULL, &event);
    hi_handle_net_current_desktop(&wm, NULL);

    TAP_OK(s_call_lookup_stage_for_root == 0u,
            "a null wm or event never reaches the stage lookup");
}


/* hi_handle_net_current_desktop: an unresolved root window is a
 * no-op */
static void s_test_current_desktop_no_stage(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_event(&event, 0x1, (xcb_atom_t) 1);
    s_lookup_stage_for_root_result = NULL;

    hi_handle_net_current_desktop(&wm, &event);

    TAP_OK(s_call_stage_switch == 0u,
            "an unresolved root window never switches any desktop");
}


/* hi_handle_net_current_desktop: a resolved stage switches to the
 * requested desktop and gets outdated */
static void s_test_current_desktop_switches(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    stage_td stage;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_event(&event, 0x1, (xcb_atom_t) 1);
    event.data.data32[0] = 3u;
    s_lookup_stage_for_root_result = &stage;

    hi_handle_net_current_desktop(&wm, &event);

    TAP_OK(s_call_stage_switch == 1u,
            "a resolved stage switches its current desktop exactly" \
            " once");
    TAP_OK(stage.is_outdated,
            "switching desktops outdates the stage");
}


/* hi_handle_net_desktop_viewport: null wm/event is a no-op */
static void s_test_desktop_viewport_null_guards(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_event(&event, 0x1, (xcb_atom_t) 1);

    hi_handle_net_desktop_viewport(NULL, &event);
    hi_handle_net_desktop_viewport(&wm, NULL);

    TAP_OK(s_call_lookup_stage_for_root == 0u,
            "a null wm or event never reaches the stage lookup");
}


/* hi_handle_net_desktop_viewport: an unresolved root window is a
 * no-op */
static void s_test_desktop_viewport_no_stage(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_event(&event, 0x1, (xcb_atom_t) 1);
    s_lookup_stage_for_root_result = NULL;

    hi_handle_net_desktop_viewport(&wm, &event);

    TAP_OK(s_call_stage_viewport_set == 0u,
            "an unresolved root window never moves any viewport");
}


/* hi_handle_net_desktop_viewport: a resolved stage moves its
 * current desktop's viewport to the requested origin and gets
 * outdated */
static void s_test_desktop_viewport_moves(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    stage_td stage;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_event(&event, 0x1, (xcb_atom_t) 1);
    event.data.data32[0] = 1024u;
    event.data.data32[1] = 768u;
    s_lookup_stage_for_root_result = &stage;

    hi_handle_net_desktop_viewport(&wm, &event);

    TAP_OK(s_call_stage_viewport_set == 1u,
            "a resolved stage moves its viewport exactly once");
    TAP_EQ_INT(s_last_viewport_set_x, 1024,
            "the requested X origin is forwarded unchanged");
    TAP_EQ_INT(s_last_viewport_set_y, 768,
            "the requested Y origin is forwarded unchanged");
    TAP_OK(stage.is_outdated,
            "moving the viewport outdates the stage");
}


/* hi_handle_net_wm_desktop: null wm/event/client is a no-op */
static void s_test_wm_desktop_null_guards(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);

    hi_handle_net_wm_desktop(NULL, &event, &client, &stage, &desktop);
    hi_handle_net_wm_desktop(&wm, NULL, &client, &stage, &desktop);
    hi_handle_net_wm_desktop(&wm, &event, NULL, &stage, &desktop);

    TAP_OK(s_call_stage_desktop_get == 0u,
            "a null wm, event, or client never reaches the target" \
            " desktop lookup");
}


/* hi_handle_net_wm_desktop: a target desktop equal to the source, or
 * one that cannot be resolved at all, is a no-op */
static void s_test_wm_desktop_same_or_missing_target(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    s_stage_desktop_get_result = &desktop; /* Same as src_desktop */

    hi_handle_net_wm_desktop(&wm, &event, &client, &stage, &desktop);

    TAP_OK(s_call_transient_top_parent == 0u,
            "a target desktop identical to the source desktop never" \
            " reaches the transient-family cascade");

    s_test_reset_state();
    s_stage_desktop_get_result = NULL; /* Unresolvable target */

    hi_handle_net_wm_desktop(&wm, &event, &client, &stage, &desktop);

    TAP_OK(s_call_transient_top_parent == 0u,
            "an unresolvable target desktop never reaches the" \
            " transient-family cascade either");
}





/* hi_handle_net_wm_desktop: a client with no resolvable transient top
 * parent (or no desktop for it) never moves anything */
static void s_test_wm_desktop_no_top_parent(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    desktop_td target_desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&target_desktop, 0, sizeof(target_desktop));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    s_stage_desktop_get_result = &target_desktop;
    s_transient_top_parent_result = NULL;

    hi_handle_net_wm_desktop(&wm, &event, &client, &stage, &desktop);

    TAP_OK(s_call_desktop_move == 0u,
            "an unresolvable transient top parent never moves any" \
            " client");
}


/* hi_handle_net_wm_desktop: a resolvable client with no transient
 * siblings moves exactly the one client and outdates the stage */
static void s_test_wm_desktop_single_client_moves(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    desktop_td target_desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&target_desktop, 0, sizeof(target_desktop));
    target_desktop.id = 7;
    stage.desktop_cur = 7;
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    event.data.data32[0] = 7u;
    s_stage_desktop_get_result = &target_desktop;
    s_transient_top_parent_result = &client;
    s_wm_get_client_desktop_result = &desktop;
    s_transient_family_snapshot_result = NULL;
    s_transient_family_snapshot_count = 0u;

    hi_handle_net_wm_desktop(&wm, &event, &client, &stage, &desktop);

    TAP_OK(s_call_desktop_move == 1u,
            "a resolvable client with no transient family moves" \
            " exactly once");
    TAP_OK(s_call_publish_wm_desktop == 1u,
            "moving a client publishes its new _NET_WM_DESKTOP" \
            " exactly once");
    TAP_OK(stage.is_outdated,
            "hi_handle_net_wm_desktop outdates the stage when done");
}


/* hi_handle_net_moveresize_window: an iconified client is left alone */
static void s_test_moveresize_window_iconified_noop(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    client.properties.state = (uint16_t) CLIENT_STATE_ICONIFIED;
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);

    hi_handle_net_moveresize_window(&wm, &event, &client, &stage,
            &desktop);

    TAP_OK(s_call_configure_window == 0u,
            "an iconified client's _NET_MOVERESIZE_WINDOW request" \
            " never configures its window");
}


/* hi_handle_net_moveresize_window: an X/Y move applies the requested
 * position and outdates the client/stage/desktop */
static void s_test_moveresize_window_move_only(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    event.data.data32[0] =
        (uint32_t) (WM_MOVERESIZE_FLAG_X | WM_MOVERESIZE_FLAG_Y);
    event.data.data32[1] = 50u;
    event.data.data32[2] = 60u;

    hi_handle_net_moveresize_window(&wm, &event, &client, &stage,
            &desktop);

    TAP_OK(s_call_configure_window == 1u,
            "an X/Y-only _NET_MOVERESIZE_WINDOW request configures" \
            " the window exactly once");
    TAP_EQ_INT((int) client.layout.geometry.cur.pos.x, 50,
            "the requested X position is recorded on the client");
    TAP_OK(s_call_decoration_layout_sync == 0u,
            "a move without a size change never re-syncs the" \
            " decoration layout");
    TAP_OK(client.is_outdated && stage.is_outdated &&
            desktop.is_outdated,
            "a _NET_MOVERESIZE_WINDOW move outdates the client," \
            " stage, and desktop");
}


/* hi_handle_net_moveresize_window: a size change on a decorated
 * client re-syncs the decoration layout */
static void s_test_moveresize_window_resize_syncs_decoration(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    client.frame = 0x200;
    client.properties.flags = (uint32_t) CLIENT_FLAG_DECORATED;
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    event.data.data32[0] =
        (uint32_t) (WM_MOVERESIZE_FLAG_WIDTH | WM_MOVERESIZE_FLAG_HEIGHT);
    event.data.data32[3] = 300u;
    event.data.data32[4] = 200u;

    hi_handle_net_moveresize_window(&wm, &event, &client, &stage,
            &desktop);

    TAP_OK(s_call_decoration_layout_sync == 1u,
            "a decorated client's resize re-syncs its decoration" \
            " layout exactly once");
}


/* hi_handle_net_moveresize_window: a request too small is clamped up
 * to the minimum window dimension */
static void s_test_moveresize_window_clamps_minimum(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    event.data.data32[0] = (uint32_t) WM_MOVERESIZE_FLAG_WIDTH;
    event.data.data32[3] = 1u; /* Below WM_MIN_WINDOW_DIMENSION */

    hi_handle_net_moveresize_window(&wm, &event, &client, &stage,
            &desktop);

    TAP_EQ_INT((int) client.layout.geometry.cur.dim.w,
            (int) WM_MIN_WINDOW_DIMENSION,
            "a requested width below the minimum is clamped up to" \
            " WM_MIN_WINDOW_DIMENSION");
}


/* hi_handle_net_showing_desktop: a null stage, or no live X
 * connection, is a no-op */
static void s_test_showing_desktop_null_guard(void)
{
    s_test_reset_state();

    hi_handle_net_showing_desktop(NULL, true);

    TAP_OK(s_call_lookup_current_desktop == 0u,
            "a null stage never reaches the current-desktop lookup");
}


/* hi_handle_net_showing_desktop: a stage with no resolvable current
 * desktop is a no-op */
static void s_test_showing_desktop_no_desktop(void)
{
    stage_td stage;

    s_test_reset_state();
    memset(&stage, 0, sizeof(stage));
    s_lookup_current_desktop_result = NULL;

    hi_handle_net_showing_desktop(&stage, true);

    TAP_OK(s_call_stacking_count == 0u,
            "an unresolvable current desktop never reaches the" \
            " stacking count");
}


/* hi_handle_net_showing_desktop: an empty desktop leaves nothing to
 * show or hide */
static void s_test_showing_desktop_empty_desktop(void)
{
    stage_td stage;
    desktop_td desktop;

    s_test_reset_state();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_lookup_current_desktop_result = &desktop;
    s_stacking_count_result = 0u;

    hi_handle_net_showing_desktop(&stage, true);

    TAP_OK(s_call_stacking_walk == 0u,
            "an empty desktop's showing-desktop request never walks" \
            " the (empty) stack");
}


/* hi_handle_net_showing_desktop: hiding a non-empty desktop hides its
 * clients and never touches input focus via unhide */
static void s_test_showing_desktop_hides(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;

    s_test_reset_state();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_client(&client, 0x100);
    stage.is_showing_desktop = false;
    stage.desktop_cur = 2u;
    s_lookup_current_desktop_result = &desktop;
    s_stacking_count_result = 1u;
    s_stacking_walk_clients[0] = &client;
    s_stacking_walk_client_count = 1u;

    hi_handle_net_showing_desktop(&stage, true);

    TAP_OK(s_call_stage_clients_hide == 1u,
            "showing the desktop (from not already showing it)" \
            " hides the stage's clients exactly once");
    TAP_OK(s_call_set_input_focus == 1u,
            "showing the desktop sends input focus to the root" \
            " window exactly once");
    TAP_OK(stage.is_outdated && desktop.is_outdated,
            "hi_handle_net_showing_desktop outdates the stage and" \
            " desktop when done");
}


/* hi_handle_net_showing_desktop: un-showing the desktop restores
 * clients rather than hiding them, with no focus repositioning */
static void s_test_showing_desktop_unhides(void)
{
    stage_td stage;
    desktop_td desktop;

    s_test_reset_state();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    stage.is_showing_desktop = true;
    stage.desktop_cur = 2u;
    s_lookup_current_desktop_result = &desktop;
    s_stacking_count_result = 1u;

    hi_handle_net_showing_desktop(&stage, false);

    TAP_OK(s_call_stage_clients_show == 1u,
            "un-showing the desktop restores the stage's clients" \
            " exactly once");
    TAP_OK(s_call_set_input_focus == 0u,
            "un-showing the desktop never repositions input focus");
}


/* hi_handle_net_showing_desktop: a redundant show request, issued
 * while the stage already reports showing the desktop and every
 * client is already hidden from that earlier request, leaves
 * 'is_showing_desktop' true rather than resetting it to false */
static void s_test_showing_desktop_redundant_show_stays_showing(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;

    s_test_reset_state();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_client(&client, 0x100);
    client.properties.flags = CLIENT_FLAG_HIDDEN;
    stage.is_showing_desktop = true;
    stage.desktop_cur = 2u;
    s_lookup_current_desktop_result = &desktop;
    s_stacking_count_result = 1u;
    s_stacking_walk_clients[0] = &client;
    s_stacking_walk_client_count = 1u;

    hi_handle_net_showing_desktop(&stage, true);

    TAP_OK(stage.is_showing_desktop,
            "a redundant show request leaves 'is_showing_desktop'" \
            " true instead of resetting it to false");
}


/* hi_handle_net_showing_desktop: a redundant restore request, issued
 * while the stage already reports not showing the desktop, neither
 * hides nor unhides anything */
static void s_test_showing_desktop_redundant_restore_is_noop(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;

    s_test_reset_state();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_client(&client, 0x100);
    stage.is_showing_desktop = false;
    stage.desktop_cur = 2u;
    s_lookup_current_desktop_result = &desktop;
    s_stacking_count_result = 1u;
    s_stacking_walk_clients[0] = &client;
    s_stacking_walk_client_count = 1u;

    hi_handle_net_showing_desktop(&stage, false);

    TAP_OK(s_call_stage_clients_hide == 0u,
            "a redundant restore request never hides the stage's" \
            " clients");
    TAP_OK(s_call_stage_clients_show == 0u,
            "...nor does it restore them, since there is nothing to" \
            " restore");
    TAP_OK(!stage.is_showing_desktop,
            "...and 'is_showing_desktop' stays false");
}


/* hi_handle_net_restack_window: null wm/event/client is a no-op */
static void s_test_restack_window_null_guards(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);

    hi_handle_net_restack_window(NULL, &event, &client, &stage,
            &desktop);
    hi_handle_net_restack_window(&wm, NULL, &client, &stage,
            &desktop);
    hi_handle_net_restack_window(&wm, &event, NULL, &stage,
            &desktop);

    TAP_OK(s_call_configure_window == 0u,
            "a null wm, event, or client never reaches the restack" \
            " configure call");
}


/* hi_handle_net_restack_window: a sibling in data32[1] adds the
 * sibling mask alongside the stack-mode mask */
static void s_test_restack_window_with_sibling(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    event.data.data32[1] = 0x300u; /* Sibling window */
    event.data.data32[2] = WM_RESTACK_DETAIL_BELOW;

    hi_handle_net_restack_window(&wm, &event, &client, &stage,
            &desktop);

    TAP_OK((s_last_configure_window_mask &
                (uint16_t) XCB_CONFIG_WINDOW_SIBLING) != 0u,
            "a sibling window in the request sets the sibling" \
            " configure mask bit");
    TAP_OK((s_last_configure_window_mask &
                (uint16_t) XCB_CONFIG_WINDOW_STACK_MODE) != 0u,
            "every restack request also sets the stack-mode" \
            " configure mask bit");
    TAP_OK(stage.is_outdated && desktop.is_outdated,
            "a restack request outdates the stage and desktop");
}


/* hi_handle_net_restack_window: an unrecognized detail value falls
 * back to XCB_STACK_MODE_ABOVE */
static void s_test_restack_window_unknown_detail_defaults_above(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    event.data.data32[2] = 999u; /* Not any recognized detail */

    hi_handle_net_restack_window(&wm, &event, &client, &stage,
            &desktop);

    TAP_EQ_INT((int) s_last_configure_window_value0,
            (int) XCB_STACK_MODE_ABOVE,
            "an unrecognized restack detail defaults to" \
            " XCB_STACK_MODE_ABOVE");
}


/* hi_handle_net_wm_fullscreen_monitors: null wm/event/client/ewmh is a
 * no-op */
static void s_test_fullscreen_monitors_null_guards(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);

    hi_handle_net_wm_fullscreen_monitors(NULL, &event, &client,
            &stage, &desktop);
    hi_handle_net_wm_fullscreen_monitors(&wm, NULL, &client,
            &stage, &desktop);
    hi_handle_net_wm_fullscreen_monitors(&wm, &event, NULL,
            &stage, &desktop);

    TAP_OK(s_call_change_property == 0u,
            "a null wm, event, or client never publishes fullscreen" \
            " monitors");
}


/* hi_handle_net_wm_fullscreen_monitors: a well-formed request
 * publishes the monitor indices and re-applies fullscreen if the
 * client already is fullscreen */
static void s_test_fullscreen_monitors_publishes_and_reapplies(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    client.properties.state = (uint16_t) CLIENT_STATE_FULLSCREEN;
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    event.data.data32[0] = 0u;
    event.data.data32[1] = 0u;
    event.data.data32[2] = 0u;
    event.data.data32[3] = 0u;

    hi_handle_net_wm_fullscreen_monitors(&wm, &event, &client,
            &stage, &desktop);

    TAP_OK(s_call_change_property == 1u,
            "a well-formed request publishes the fullscreen" \
            " monitors property exactly once");
    TAP_OK(s_call_fullscreen == 1u,
            "an already-fullscreen client re-applies fullscreen so" \
            " the new monitor spans take effect");
    TAP_OK(stage.is_outdated && desktop.is_outdated,
            "handling _NET_WM_FULLSCREEN_MONITORS outdates the" \
            " stage and desktop");
}


/* hi_handle_net_wm_fullscreen_monitors: a non-fullscreen client is
 * left alone (property published, but fullscreen not re-applied) */
static void s_test_fullscreen_monitors_non_fullscreen_client(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    client.properties.flags = 0u;
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);

    hi_handle_net_wm_fullscreen_monitors(&wm, &event, &client,
            &stage, &desktop);

    TAP_OK(s_call_fullscreen == 0u,
            "a non-fullscreen client's monitor spans are published" \
            " without re-applying fullscreen");
}


/* hi_handle_net_wm_moveresize: null wm/event/client/stage/screen/
 * config is a no-op */
static void s_test_wm_moveresize_null_guards(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    stage.screen = NULL; /* No screen resolved yet */
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    event.data.data32[2] = XCB_EWMH_WM_MOVERESIZE_MOVE;

    hi_handle_net_wm_moveresize(NULL, &event, &client, &stage,
            &desktop);
    hi_handle_net_wm_moveresize(&wm, NULL, &client, &stage,
            &desktop);
    hi_handle_net_wm_moveresize(&wm, &event, NULL, &stage,
            &desktop);
    hi_handle_net_wm_moveresize(&wm, &event, &client, NULL,
            &desktop);
    hi_handle_net_wm_moveresize(&wm, &event, &client, &stage,
            &desktop); /* Null screen */

    TAP_OK(s_call_drag_start == 0u,
            "a null wm, event, client, stage, or screen never" \
            " starts a drag");
}


/* hi_handle_net_wm_moveresize: XCB_EWMH_WM_MOVERESIZE_CANCEL cancels
 * an active drag, and does nothing when no drag is active */
static void s_test_wm_moveresize_cancel(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_screen_t screen;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&screen, 0, sizeof(screen));
    stage.screen = &screen;
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    event.data.data32[2] = XCB_EWMH_WM_MOVERESIZE_CANCEL;
    s_drag_is_active_result = true;

    hi_handle_net_wm_moveresize(&wm, &event, &client, &stage,
            &desktop);

    TAP_OK(s_call_drag_cancel == 1u,
            "a cancel request while a drag is active cancels it" \
            " exactly once");

    s_test_reset_state();
    stage.screen = &screen;
    s_drag_is_active_result = false;
    hi_handle_net_wm_moveresize(&wm, &event, &client, &stage,
            &desktop);

    TAP_OK(s_call_drag_cancel == 0u,
            "a cancel request while no drag is active cancels" \
            " nothing");
}


/* hi_handle_net_wm_moveresize: a keyboard-driven direction is ignored
 * (not implemented via synthetic pointer drag) */
static void s_test_wm_moveresize_keyboard_ignored(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_screen_t screen;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&screen, 0, sizeof(screen));
    stage.screen = &screen;
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    event.data.data32[2] = XCB_EWMH_WM_MOVERESIZE_MOVE_KEYBOARD;

    hi_handle_net_wm_moveresize(&wm, &event, &client, &stage,
            &desktop);

    TAP_OK(s_call_drag_start == 0u && s_call_drag_start_directed == 0u,
            "a keyboard move/resize direction starts no pointer" \
            " drag at all");
}


/* hi_handle_net_wm_moveresize: a plain MOVE direction starts an
 * undirected drag */
static void s_test_wm_moveresize_move_starts_drag(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_screen_t screen;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&screen, 0, sizeof(screen));
    stage.screen = &screen;
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    event.data.data32[0] = 100u;
    event.data.data32[1] = 200u;
    event.data.data32[2] = XCB_EWMH_WM_MOVERESIZE_MOVE;

    hi_handle_net_wm_moveresize(&wm, &event, &client, &stage,
            &desktop);

    TAP_OK(s_call_drag_start == 1u,
            "a plain MOVE direction starts an undirected drag" \
            " exactly once");
    TAP_OK(s_call_drag_start_directed == 0u,
            "a plain MOVE direction never also starts a directed" \
            " resize drag");
}


/* hi_handle_net_wm_moveresize: a non-move direction on a
 * non-resizable client starts no drag at all */
static void s_test_wm_moveresize_unresizable_gated(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_screen_t screen;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&screen, 0, sizeof(screen));
    stage.screen = &screen;
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    client.properties.flags = 0u; /* Not resizable */
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    event.data.data32[2] = XCB_EWMH_WM_MOVERESIZE_SIZE_RIGHT;

    hi_handle_net_wm_moveresize(&wm, &event, &client, &stage,
            &desktop);

    TAP_OK(s_call_drag_start_directed == 0u,
            "a non-resizable client's resize direction starts no" \
            " directed drag");
}


/* hi_handle_net_wm_moveresize: a resizable client's directional
 * resize starts a directed drag with the correct anchor */
static void s_test_wm_moveresize_resize_starts_directed_drag(void)
{
    wm_td wm;
    xcb_ewmh_connection_t ewmh;
    config_td config;
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_screen_t screen;
    xcb_client_message_event_t event;

    s_test_reset_state();
    memset(&ewmh, 0, sizeof(ewmh));
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&screen, 0, sizeof(screen));
    stage.screen = &screen;
    s_test_build_wm(&wm, &ewmh, &config);
    s_test_build_client(&client, 0x100);
    client.properties.flags = (uint32_t) CLIENT_FLAG_RESIZABLE;
    s_test_build_event(&event, 0x100, (xcb_atom_t) 1);
    event.data.data32[2] = XCB_EWMH_WM_MOVERESIZE_SIZE_RIGHT;

    hi_handle_net_wm_moveresize(&wm, &event, &client, &stage,
            &desktop);

    TAP_OK(s_call_drag_start_directed == 1u,
            "a resizable client's directional resize starts a" \
            " directed drag exactly once");
}


int main(void)
{
    TAP_PLAN(71);

    s_test_wm_state_null_guards();
    s_test_wm_state_fullscreen_add();
    s_test_wm_state_fullscreen_remove();
    s_test_wm_state_fullscreen_toggle();
    s_test_wm_state_two_atoms();
    s_test_wm_state_maximize_gated();
    s_test_wm_state_remaining_atoms();
    s_test_wm_state_unrecognized_atom();
    s_test_current_desktop_null_guards();
    s_test_current_desktop_no_stage();
    s_test_current_desktop_switches();
    s_test_desktop_viewport_null_guards();
    s_test_desktop_viewport_no_stage();
    s_test_desktop_viewport_moves();
    s_test_wm_desktop_null_guards();
    s_test_wm_desktop_same_or_missing_target();
    s_test_wm_desktop_no_top_parent();
    s_test_wm_desktop_single_client_moves();
    s_test_moveresize_window_iconified_noop();
    s_test_moveresize_window_move_only();
    s_test_moveresize_window_resize_syncs_decoration();
    s_test_moveresize_window_clamps_minimum();
    s_test_showing_desktop_null_guard();
    s_test_showing_desktop_no_desktop();
    s_test_showing_desktop_empty_desktop();
    s_test_showing_desktop_hides();
    s_test_showing_desktop_unhides();
    s_test_showing_desktop_redundant_show_stays_showing();
    s_test_showing_desktop_redundant_restore_is_noop();
    s_test_restack_window_null_guards();
    s_test_restack_window_with_sibling();
    s_test_restack_window_unknown_detail_defaults_above();
    s_test_fullscreen_monitors_null_guards();
    s_test_fullscreen_monitors_publishes_and_reapplies();
    s_test_fullscreen_monitors_non_fullscreen_client();
    s_test_wm_moveresize_null_guards();
    s_test_wm_moveresize_cancel();
    s_test_wm_moveresize_keyboard_ignored();
    s_test_wm_moveresize_move_starts_drag();
    s_test_wm_moveresize_unresizable_gated();
    s_test_wm_moveresize_resize_starts_directed_drag();

    return TAP_DONE();
}
