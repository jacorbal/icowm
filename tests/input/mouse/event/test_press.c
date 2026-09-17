/**
 * @file tests/input/mouse/event/test_press.c
 *
 * @brief Test battery for input/mouse/event/press.c
 *
 * mouse_handle_press itself is the largest, most deeply cross-cutting
 * entry point in this subsystem: its static helpers reach into raw
 * XCB geometry queries, the drag subsystem, every context menu and
 * dialog, manual placement, resize-bounds math and focus policy, each
 * already covered by its own dedicated tests elsewhere.  Exercising
 * that whole dispatcher end-to-end here would mean re-deriving all of
 * those subsystems' own test doubles a second time for no additional
 * coverage, so this file instead gives full, direct coverage to the
 * two exported entry points declared in internal.h that are genuinely
 * self-contained: @a im_sync_pinned_active (pure desktop-list
 * traversal logic over a pinned client) and @a im_allow_and_flush (a
 * two-line wrapper over raw XCB calls).  Every other press.c
 * collaborator below exists purely so the translation unit links;
 * none of it is exercised by any test in this file, and each is
 * documented as unreached.
 *
 * The real src/adt/cdlist.c is linked rather than stubbed, since it
 * is a small, side-effect-free general-purpose data structure, exactly
 * the kind of leaf utility tests/input/mouse/event/test_enter.c
 * already established as safe to link directly instead of stubbing.
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

/* Type includes */
#include <types/pair.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Policy includes */
#include <policy/focus.h>
#include <policy/placement/manual.h>

/* Menu includes */
#include <menu/context/iconmenu.h>
#include <menu/context/rootmenu.h>
#include <menu/context/wincmenu.h>
#include <menu/context/winlist.h>

/* Command includes */
#include <cmds/client/state.h>

/* Project includes */
#include <client.h>
#include <client/predicates.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <stage.h>
#include <wm.h>

/* Default initial values */
#include <defs/client.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/bounds.h>
#include <input/mouse/drag.h>
#include <input/mouse/drag/background.h>
#include <input/mouse/drag/icon.h>
#include <input/mouse/event.h>
#include <input/mouse/internal.h>


/** Recorded call count for every link-only stand-in this file never
 *  actually exercises; kept only to prove none of them fire */
static int s_unreached_calls;


/**
 * @brief Link-only stand-in for @a logger_msg, never exercised
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;

    s_unreached_calls++;

    return 0;
}


/**
 * @brief Link-only stand-in for @a lookup_stage_for_root, never
 *        exercised
 * @note Complexity: @e O(1)
 */
stage_td *lookup_stage_for_root(list_td *stages, xcb_window_t root)
{
    (void) stages;
    (void) root;

    s_unreached_calls++;

    return NULL;
}


/**
 * @brief Link-only stand-in for @a lookup_find_client, never
 *        exercised
 * @note Complexity: @e O(1)
 */
client_td *lookup_find_client(list_td *stages, xcb_window_t window,
        stage_td **out_stage, desktop_td **out_desktop)
{
    (void) stages;
    (void) window;
    (void) out_stage;
    (void) out_desktop;

    s_unreached_calls++;

    return NULL;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_unshade, never exercised
 * @note Complexity: @e O(1)
 */
void ccmd_client_unshade(client_td *client)
{
    (void) client;

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a drag_icon_start, never exercised
 * @note Complexity: @e O(1)
 */
void drag_icon_start(xcb_connection_t *connection, xcb_window_t root,
        client_td *client, desktop_td *desktop,
        struct position_s icon_pos, xcb_timestamp_t event_time,
        struct position_s root_pos, struct dimensions_s screen_dim)
{
    (void) connection;
    (void) root;
    (void) client;
    (void) desktop;
    (void) icon_pos;
    (void) event_time;
    (void) root_pos;
    (void) screen_dim;

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a drag_background_start, never
 *        exercised
 * @note Complexity: @e O(1)
 */
void drag_background_start(xcb_connection_t *connection,
        stage_td *stage, xcb_window_t root,
        xcb_timestamp_t event_time, struct position_s root_pos)
{
    (void) connection;
    (void) stage;
    (void) root;
    (void) event_time;
    (void) root_pos;

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a drag_start, never exercised
 * @note Complexity: @e O(1)
 */
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

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a drag_start_resize_axis_locked,
 *        never exercised
 * @note Complexity: @e O(1)
 */
void drag_start_resize_axis_locked(xcb_connection_t *connection,
        xcb_window_t root, client_td *client, desktop_td *desktop,
        xcb_timestamp_t event_time, struct position_s root_pos,
        struct dimensions_s screen_dim, bool axis_w_locked,
        bool axis_h_locked)
{
    (void) connection;
    (void) root;
    (void) client;
    (void) desktop;
    (void) event_time;
    (void) root_pos;
    (void) screen_dim;
    (void) axis_w_locked;
    (void) axis_h_locked;

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a enact_client_lower, never exercised
 * @note Complexity: @e O(1)
 */
void enact_client_lower(client_td *client)
{
    (void) client;

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a enact_client_restore, never
 *        exercised
 * @note Complexity: @e O(1)
 */
void enact_client_restore(client_td *client)
{
    (void) client;

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a iconmenu_show, never exercised
 * @note Complexity: @e O(1)
 */
void iconmenu_show(xcb_connection_t *connection, stage_td *stage,
        desktop_td *desktop, client_td *client, struct position_s pos,
        const config_td *config)
{
    (void) connection;
    (void) stage;
    (void) desktop;
    (void) client;
    (void) pos;
    (void) config;

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a enact_client_unfocus, never
 *        exercised
 * @note Complexity: @e O(1)
 */
void enact_client_unfocus(client_td *client)
{
    (void) client;

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a focus_apply, never exercised
 * @note Complexity: @e O(1)
 */
void focus_apply(list_td *stages, stage_td *stage,
        desktop_td *desktop, client_td *client, bool raise,
        const config_td *config)
{
    (void) stages;
    (void) stage;
    (void) desktop;
    (void) client;
    (void) raise;
    (void) config;

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a im_bounds_resize, never exercised
 * @note Complexity: @e O(1)
 */
im_resize_bounds_td im_bounds_resize(const client_td *client)
{
    im_resize_bounds_td bounds;

    (void) client;

    memset(&bounds, 0, sizeof(bounds));
    s_unreached_calls++;

    return bounds;
}


/**
 * @brief Link-only stand-in for @a im_press_close_overlays, never
 *        exercised
 * @note Complexity: @e O(1)
 */
bool im_press_close_overlays(xcb_connection_t *connection,
        list_td *stages, xcb_button_press_event_t *event,
        const config_td *config)
{
    (void) connection;
    (void) stages;
    (void) event;
    (void) config;

    s_unreached_calls++;

    return false;
}


/**
 * @brief Link-only stand-in for @a im_press_scroll_binding, never
 *        exercised
 * @note Complexity: @e O(1)
 */
void im_press_scroll_binding(xcb_connection_t *connection,
        list_td *stages, xcb_button_press_event_t *event,
        client_td *client, desktop_td *desktop,
        enum wm_mousebind_type_e type, const config_td *config)
{
    (void) connection;
    (void) stages;
    (void) event;
    (void) client;
    (void) desktop;
    (void) type;
    (void) config;

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a im_press_titlebar, never exercised
 * @note Complexity: @e O(1)
 */
bool im_press_titlebar(xcb_connection_t *connection, list_td *stages,
        xcb_button_press_event_t *event, client_td *client,
        desktop_td *desktop, stage_td *stage,
        const config_td *config)
{
    (void) connection;
    (void) stages;
    (void) event;
    (void) client;
    (void) desktop;
    (void) stage;
    (void) config;

    s_unreached_calls++;

    return false;
}


/**
 * @brief Link-only stand-in for @a im_resolve_binding, never exercised
 * @note Complexity: @e O(1)
 */
enum wm_mousebind_type_e im_resolve_binding(xcb_button_index_t button,
        uint16_t state)
{
    (void) button;
    (void) state;

    s_unreached_calls++;

    return MOUSEBIND_NONE;
}


/**
 * @brief Link-only stand-in for @a place_manual_handle_press, never
 *        exercised
 * @note Complexity: @e O(1)
 */
void place_manual_handle_press(xcb_connection_t *connection)
{
    (void) connection;

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a place_manual_is_active, never
 *        exercised
 * @note Complexity: @e O(1)
 */
bool place_manual_is_active(void)
{
    s_unreached_calls++;

    return false;
}


/**
 * @brief Link-only stand-in for @a rootmenu_show, never exercised
 * @note Complexity: @e O(1)
 */
void rootmenu_show(wm_td *wm, xcb_connection_t *connection,
        stage_td *stage, struct position_s pos,
        const config_td *config)
{
    (void) wm;
    (void) connection;
    (void) stage;
    (void) pos;
    (void) config;

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a stage_desktop_get, never
 *        exercised
 * @note Complexity: @e O(1)
 */
desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;

    s_unreached_calls++;

    return NULL;
}


/**
 * @brief Link-only stand-in for @a wincmenu_show, never exercised
 * @note Complexity: @e O(1)
 */
void wincmenu_show(xcb_connection_t *connection, stage_td *stage,
        desktop_td *desktop, client_td *client, struct position_s pos,
        const config_td *config)
{
    (void) connection;
    (void) stage;
    (void) desktop;
    (void) client;
    (void) pos;
    (void) config;

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a winlist_show, never exercised
 * @note Complexity: @e O(1)
 */
void winlist_show(xcb_connection_t *connection, stage_td *stage,
        struct position_s pos, const config_td *config)
{
    (void) connection;
    (void) stage;
    (void) pos;
    (void) config;

    s_unreached_calls++;
}


/**
 * @brief Link-only stand-in for @a wm_get_stage_by_id, never
 *        exercised
 * @note Complexity: @e O(1)
 */
stage_td *wm_get_stage_by_id(uint32_t stage_id)
{
    (void) stage_id;

    s_unreached_calls++;

    return NULL;
}


/** Recorded arguments the fake xcb_allow_events/xcb_flush last saw */
static int s_allow_events_calls;
static uint8_t s_allow_events_mode;
static xcb_timestamp_t s_allow_events_time;
static int s_flush_calls;


/**
 * @brief Recording stand-in for the raw @a xcb_allow_events request,
 *        so @a im_allow_and_flush can be exercised without a live X
 *        connection
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_allow_events(xcb_connection_t *c, uint8_t mode,
        xcb_timestamp_t time)
{
    xcb_void_cookie_t cookie;

    (void) c;

    memset(&cookie, 0, sizeof(cookie));
    s_allow_events_calls++;
    s_allow_events_mode = mode;
    s_allow_events_time = time;

    return cookie;
}


/**
 * @brief Recording stand-in for the raw @a xcb_flush request
 * @note Complexity: @e O(1)
 */
int xcb_flush(xcb_connection_t *c)
{
    (void) c;

    s_flush_calls++;

    return 1;
}


static void s_reset(void)
{
    s_unreached_calls = 0;
    s_allow_events_calls = 0;
    s_allow_events_mode = 0u;
    s_allow_events_time = 0;
    s_flush_calls = 0;
}


/* im_allow_and_flush forwards its mode and timestamp to
 * xcb_allow_events verbatim, then flushes the connection once */
static void s_test_allow_and_flush_forwards_mode_and_time(void)
{
    s_reset();

    im_allow_and_flush((xcb_connection_t *) 1, XCB_ALLOW_REPLAY_POINTER,
            (xcb_timestamp_t) 4242);

    TAP_EQ_INT(s_allow_events_calls, 1,
            "xcb_allow_events is called exactly once");
    TAP_OK(s_allow_events_mode == (uint8_t) XCB_ALLOW_REPLAY_POINTER,
            "the exact mode requested is forwarded");
    TAP_OK(s_allow_events_time == (xcb_timestamp_t) 4242,
            "the exact timestamp requested is forwarded");
    TAP_EQ_INT(s_flush_calls, 1,
            "the connection is flushed exactly once afterwards");
}


/* A second, independent call with a different mode confirms nothing
 * is cached between calls */
static void s_test_allow_and_flush_async_mode(void)
{
    s_reset();

    im_allow_and_flush((xcb_connection_t *) 1, XCB_ALLOW_ASYNC_POINTER,
            (xcb_timestamp_t) 1);

    TAP_OK(s_allow_events_mode == (uint8_t) XCB_ALLOW_ASYNC_POINTER,
            "a different mode on a separate call is forwarded"
            " correctly too");
}


/* im_sync_pinned_active is a no-op when the stage is null */
static void s_test_sync_pinned_null_stage_is_noop(void)
{
    client_td client;
    desktop_td desktop;

    memset(&client, 0, sizeof(client));
    memset(&desktop, 0, sizeof(desktop));
    client.properties.flags |= CLIENT_FLAG_PIN;
    client.id = 5u;

    /* Nothing to assert on directly beyond "does not crash"; ASan
     * would catch any null-stage dereference regardless */
    im_sync_pinned_active(NULL, &desktop, &client);

    TAP_OK(true, "null stage: im_sync_pinned_active returns"
            " without touching anything");
}


/* im_sync_pinned_active is a no-op when the client is null */
static void s_test_sync_pinned_null_client_is_noop(void)
{
    stage_td stage;
    desktop_td desktop;

    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    stage.desktops = NULL;

    im_sync_pinned_active(&stage, &desktop, NULL);

    TAP_OK(true, "null client: im_sync_pinned_active returns without"
            " touching anything");
}


/* im_sync_pinned_active is a no-op for a non-pinned client, even with
 * a real desktop list behind the stage */
static void s_test_sync_pinned_unpinned_client_is_noop(void)
{
    stage_td stage;
    desktop_td d1;
    desktop_td d2;
    client_td client;
    cdlist_td *desktops = cdlist_init(NULL);

    memset(&stage, 0, sizeof(stage));
    memset(&d1, 0, sizeof(d1));
    memset(&d2, 0, sizeof(d2));
    memset(&client, 0, sizeof(client));
    client.id = 9u;
    d1.client_active_id = 0u;
    d2.client_active_id = 0u;

    cdlist_ins_next(desktops, NULL, &d1);
    cdlist_ins_next(desktops, cdlist_head(desktops), &d2);
    stage.desktops = desktops;

    im_sync_pinned_active(&stage, &d1, &client);

    TAP_EQ_INT((int) d1.client_active_id, 0,
            "unpinned client: the passed-in desktop is untouched");
    TAP_EQ_INT((int) d2.client_active_id, 0,
            "and the other desktop on the stage is untouched too");

    cdlist_destroy(desktops);
}


/* im_sync_pinned_active is a no-op when the stage's desktop list
 * itself is null, even for a pinned client */
static void s_test_sync_pinned_null_desktop_list_is_noop(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;

    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_PIN;
    client.id = 3u;
    stage.desktops = NULL;

    im_sync_pinned_active(&stage, &desktop, &client);

    TAP_OK(true, "null desktop list on the stage: returns cleanly"
            " without dereferencing it");
}


/* A pinned client propagates its id onto every other desktop on the
 * stage, skipping only the one desktop already passed in (the one
 * focus_apply is assumed to have just updated itself) */
static void s_test_sync_pinned_pinned_client_propagates(void)
{
    stage_td stage;
    desktop_td d1;
    desktop_td d2;
    desktop_td d3;
    client_td client;
    cdlist_td *desktops = cdlist_init(NULL);
    cdlist_item_td *head;

    memset(&stage, 0, sizeof(stage));
    memset(&d1, 0, sizeof(d1));
    memset(&d2, 0, sizeof(d2));
    memset(&d3, 0, sizeof(d3));
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_PIN;
    client.id = 7u;
    d1.client_active_id = 7u;
    d1.is_focus_dirty = false;
    d2.client_active_id = 0u;
    d2.is_focus_dirty = false;
    d3.client_active_id = 99u;
    d3.is_focus_dirty = false;

    cdlist_ins_next(desktops, NULL, &d1);
    head = cdlist_head(desktops);
    cdlist_ins_next(desktops, head, &d2);
    cdlist_ins_next(desktops, cdlist_next(head), &d3);
    stage.desktops = desktops;

    im_sync_pinned_active(&stage, &d1, &client);

    TAP_EQ_INT((int) d1.client_active_id, 7,
            "the desktop already passed in (already handled by"
            " focus_apply) keeps its own value unmodified");
    TAP_OK(!d1.is_focus_dirty,
            "and is never marked focus-dirty by this function");
    TAP_EQ_INT((int) d2.client_active_id, 7,
            "a different desktop with no active client is set to the"
            " pinned client's id");
    TAP_OK(d2.is_focus_dirty,
            "and marked focus-dirty so the next repass picks it up");
    TAP_EQ_INT((int) d3.client_active_id, 7,
            "a desktop whose active client was something else"
            " entirely is overwritten to the pinned client's id too");
    TAP_OK(d3.is_focus_dirty, "and is also marked focus-dirty");

    cdlist_destroy(desktops);
}


/* A desktop already showing the pinned client as active is left with
 * is_focus_dirty untouched, since nothing about it actually changed */
static void s_test_sync_pinned_already_correct_stays_clean(void)
{
    stage_td stage;
    desktop_td d1;
    desktop_td d2;
    client_td client;
    cdlist_td *desktops = cdlist_init(NULL);

    memset(&stage, 0, sizeof(stage));
    memset(&d1, 0, sizeof(d1));
    memset(&d2, 0, sizeof(d2));
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_PIN;
    client.id = 11u;
    d1.client_active_id = 11u;
    d2.client_active_id = 11u;
    d2.is_focus_dirty = false;

    cdlist_ins_next(desktops, NULL, &d1);
    cdlist_ins_next(desktops, cdlist_head(desktops), &d2);
    stage.desktops = desktops;

    im_sync_pinned_active(&stage, &d1, &client);

    TAP_OK(!d2.is_focus_dirty,
            "a desktop that already shows the pinned client active is"
            " left completely untouched, not merely re-set to the"
            " same value");

    cdlist_destroy(desktops);
}


/* An empty desktop list (head is null) is a no-op, not a crash */
static void s_test_sync_pinned_empty_desktop_list_is_noop(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;
    cdlist_td *desktops = cdlist_init(NULL);

    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_PIN;
    client.id = 1u;
    stage.desktops = desktops;

    im_sync_pinned_active(&stage, &desktop, &client);

    TAP_OK(true, "empty desktop list: returns cleanly without"
            " iterating anything");

    cdlist_destroy(desktops);
}


int main(void)
{
    TAP_PLAN(19);

    s_test_allow_and_flush_forwards_mode_and_time();
    s_test_allow_and_flush_async_mode();
    s_test_sync_pinned_null_stage_is_noop();
    s_test_sync_pinned_null_client_is_noop();
    s_test_sync_pinned_unpinned_client_is_noop();
    s_test_sync_pinned_null_desktop_list_is_noop();
    s_test_sync_pinned_pinned_client_propagates();
    s_test_sync_pinned_already_correct_stays_clean();
    s_test_sync_pinned_empty_desktop_list_is_noop();

    TAP_EQ_INT(s_unreached_calls, 0,
            "none of the many press.c collaborators outside"
            " im_sync_pinned_active/im_allow_and_flush were ever"
            " actually invoked by this file");

    return TAP_DONE();
}
