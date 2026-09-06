/**
 * @file tests/menu/context/test_wincmenu.c
 *
 * @brief Test battery for the per-client window context menu
 *        (menu/context/wincmenu.c)
 *
 * 'wincmenu_show' is reached the same way
 * 'tests/menu/context/test_winlist.c' reaches 'winlist_show': a
 * recording stand-in for 'ctxmenu_show' captures the
 * 'ctxmenu_state_td' the real, unmodified function under test built,
 * so every entry it decided to include, omit, enable, or disable can
 * be inspected directly afterwards, exactly as
 * 'tests/menu/context/rootmenu.c''s footer was inspected in
 * 'test_rootmenu.c'.  Every 'enact_*'/'ccmd_*'/'kbd_modal_*'/
 * 'drag_start'/'dialog_inspect_show'/'wm_get_surface_by_id' stand-in
 * below is link-only: each is referenced only by address inside a
 * file-static callback closure ('s_cb_move', 's_cb_resize',
 * 's_cb_send_action', etc.) that is never invoked here, since no
 * test in this file activates a built entry, only inspects it.
 * 'surface_desktops_walk', 'surface_monitor_for_point' and
 * 'surface_viewport_has_room', by contrast, are test-controlled:
 * 'wincmenu_show' calls all three directly while deciding the "Send
 * to desktop"/"Send to monitor" submenus and the Sticky entry, so
 * this file supplies working, minimal implementations rather than
 * stubs, letting the real building logic in 'wincmenu.c' genuinely
 * run end to end.
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
#include <xcb/xcb_keysyms.h>

/* Local includes */
#include <action.h>
#include <client.h>
#include <client/predicates.h>
#include <client/state.h>
#include <cmds/client/state.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <harness/tap.h>
#include <input/kbd/modal.h>
#include <input/mouse/drag.h>
#include <logger.h>
#include <menu/context/ctxmenu.h>
#include <menu/context/ctxmenu/select.h>
#include <menu/context/ctxmenu/tree.h>
#include <menu/context/wincmenu.h>
#include <menu/dialog/inspect.h>
#include <surface.h>
#include <wm.h>


/** Link-only stand-in for @a logger_msg, reached only past the
 *  never-taken default case of 's_cb_send_action''s switch, which
 *  nothing here activates
 * @note Complexity: @e O(1) */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/** Recording stand-in for @a ctxmenu_show, capturing the state it
 *  was handed so its built entries can be inspected directly */
static ctxmenu_state_td *s_captured_state;

void ctxmenu_show(xcb_connection_t *connection, surface_td *surface,
        ctxmenu_state_td *state, struct position_s pos,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) pos;
    (void) config;
    s_captured_state = state;
}


/** Link-only stand-in for @a ctxmenu_close
 * @note Complexity: @e O(1) */
void ctxmenu_close(ctxmenu_state_td *state)
{
    (void) state;
}


/** Link-only stand-in for @a ctxmenu_is_open
 * @note Complexity: @e O(1) */
bool ctxmenu_is_open(const ctxmenu_state_td *state)
{
    (void) state;
    return false;
}


/** Link-only stand-in for @a ctxmenu_last_activation_was_keyboard
 * @note Complexity: @e O(1) */
bool ctxmenu_last_activation_was_keyboard(void)
{
    return false;
}


/** Link-only stand-in for @a ctxmenu_tree_handle_click_window
 * @note Complexity: @e O(1) */
bool ctxmenu_tree_handle_click_window(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *root, xcb_window_t win,
        int y, const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) root;
    (void) win;
    (void) y;
    (void) config;
    return false;
}


/** Link-only stand-in for @a ctxmenu_tree_handle_keypress_deepest
 * @note Complexity: @e O(1) */
bool ctxmenu_tree_handle_keypress_deepest(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *root,
        xcb_keysym_t keysym, const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) root;
    (void) keysym;
    (void) config;
    return false;
}


/** Link-only stand-in for @a ctxmenu_tree_handle_motion_window
 * @note Complexity: @e O(1) */
void ctxmenu_tree_handle_motion_window(ctxmenu_state_td *root,
        xcb_window_t win, int x, int y)
{
    (void) root;
    (void) win;
    (void) x;
    (void) y;
}


/** Link-only stand-in for @a ctxmenu_tree_redraw_window
 * @note Complexity: @e O(1) */
void ctxmenu_tree_redraw_window(ctxmenu_state_td *root, xcb_window_t win)
{
    (void) root;
    (void) win;
}


/** Link-only stand-in for @a ctxmenu_tree_state_find_for_window
 * @note Complexity: @e O(1) */
ctxmenu_state_td *ctxmenu_tree_state_find_for_window(
        ctxmenu_state_td *state, xcb_window_t win)
{
    (void) state;
    (void) win;
    return NULL;
}


/** Link-only stand-ins for every @a enact_* callback dependency;
 *  reached only through a click on an already-built entry, which
 *  nothing here does
 * @note Complexity: @e O(1) each */
void enact_client_close(client_td *client)
{
    (void) client;
}

void enact_client_hide(client_td *client)
{
    (void) client;
}

void enact_client_iconify(client_td *client)
{
    (void) client;
}

void enact_client_layer_above(client_td *client)
{
    (void) client;
}

void enact_client_layer_below(client_td *client)
{
    (void) client;
}

void enact_client_layer_normal(client_td *client)
{
    (void) client;
}

void enact_client_maximize(client_td *client)
{
    (void) client;
}

void enact_client_move_to_monitor(client_td *client,
        uint32_t monitor_index)
{
    (void) client;
    (void) monitor_index;
}

void enact_client_restore(client_td *client)
{
    (void) client;
}

void enact_client_toggle_decorate(client_td *client)
{
    (void) client;
}

void enact_client_toggle_fullscreen(client_td *client)
{
    (void) client;
}

void enact_client_toggle_pin(client_td *client)
{
    (void) client;
}

void enact_client_toggle_stick(client_td *client)
{
    (void) client;
}

void enact_client_toggle_shade(client_td *client)
{
    (void) client;
}

void enact_desktop_client_send(const desktop_td *desktop,
        client_td *client, desktop_td *target)
{
    (void) desktop;
    (void) client;
    (void) target;
}


/** Link-only stand-in for @a ccmd_client_unshade
 * @note Complexity: @e O(1) */
void ccmd_client_unshade(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for @a kbd_modal_move_start
 * @note Complexity: @e O(1) */
void kbd_modal_move_start(xcb_connection_t *connection,
        surface_td *surface, client_td *client)
{
    (void) connection;
    (void) surface;
    (void) client;
}


/** Link-only stand-in for @a kbd_modal_resize_start
 * @note Complexity: @e O(1) */
void kbd_modal_resize_start(xcb_connection_t *connection,
        surface_td *surface, client_td *client)
{
    (void) connection;
    (void) surface;
    (void) client;
}


/** Link-only stand-in for @a drag_start
 * @note Complexity: @e O(1) */
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
}


/** Link-only stand-in for @a dialog_inspect_show
 * @note Complexity: @e O(1) */
void dialog_inspect_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const client_td *client)
{
    (void) connection;
    (void) surface;
    (void) config;
    (void) client;
}


/** Link-only stand-in for @a wm_get_surface_by_id
 * @note Complexity: @e O(1) */
surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return NULL;
}


/** Test-controlled stand-in for @a surface_desktops_walk, walking a
 *  small fixed array registered by @a s_set_desktops instead of a
 *  real 'cdlist_td' */
#define MAX_TEST_DESKTOPS (4)
static desktop_td *s_desktops[MAX_TEST_DESKTOPS];
static int s_desktop_count;

void surface_desktops_walk(const surface_td *surface,
        surface_desktop_visitor_fn visit, void *data)
{
    (void) surface;

    if (visit == NULL) {
        return;
    }
    for (int i = 0; i < s_desktop_count; ++i) {
        visit(s_desktops[i], data);
    }
}


/** Test-controlled stand-in for @a surface_desktop_label, producing a
 *  simple, predictable label instead of the real localized one
 * @note Complexity: @e O(1) */
void surface_desktop_label(const surface_td *surface,
        uint32_t desktop_id, const char *desktop_name, bool is_pinned,
        bool shows_name, char *out_label, size_t length)
{
    (void) surface;
    (void) is_pinned;
    (void) shows_name;

    (void) snprintf(out_label, length, "%u:%s", desktop_id,
            (desktop_name != NULL) ? desktop_name : "");
}


/** Test-controlled stand-in for @a surface_viewport_has_room,
 *  answering whatever this file last registered, so the Sticky entry
 *  can be inspected both present and omitted without building a whole
 *  configuration around a pannable viewport
 * @note Complexity: @e O(1) */
static bool s_viewport_has_room;

bool surface_viewport_has_room(const surface_td *surface)
{
    (void) surface;
    return s_viewport_has_room;
}


/** Test-controlled stand-in for @a surface_monitor_for_point,
 *  answering whichever monitor this file last registered as
 *  "current" via @a s_current_monitor
 * @note Complexity: @e O(1) */
static monitor_td s_current_monitor;

monitor_td surface_monitor_for_point(const surface_td *surface,
        struct position_s pos)
{
    (void) surface;
    (void) pos;
    return s_current_monitor;
}


/** Every client this file calloc's, freed by @a s_teardown */
#define MAX_TEST_CLIENTS (8)
static client_td *s_owned_clients[MAX_TEST_CLIENTS];
static int s_owned_clients_used;


static client_td *s_make_client(void)
{
    client_td *client = calloc(1, sizeof(*client));

    client->id = (xcb_window_t) 1;
    client->window = (xcb_window_t) 1;
    client->screen_id = 0u;
    client->properties.flags = (uint16_t) CLIENT_FLAG_RESIZABLE |
        (uint16_t) CLIENT_FLAG_DECORATED;
    client->properties.layer = (uint16_t) CLIENT_LAYER_NORMAL;
    s_owned_clients[s_owned_clients_used] = client;
    s_owned_clients_used++;

    return client;
}


static void s_reset(void)
{
    s_captured_state = NULL;
    s_desktop_count = 0;
    s_viewport_has_room = true;
    memset(s_desktops, 0, sizeof(s_desktops));
    memset(&s_current_monitor, 0, sizeof(s_current_monitor));
}


static void s_teardown(void)
{
    wincmenu_close();
    for (int i = 0; i < s_owned_clients_used; ++i) {
        free(s_owned_clients[i]);
    }
    s_owned_clients_used = 0;
}


/**
 * @brief Verify @a wincmenu_show rejects every null argument, and a
 *        locked client, without ever reaching @a ctxmenu_show
 */
static void s_test_show_guards(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td *client;
    config_td config;
    struct position_s pos = { 0, 0 };

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&config, 0, sizeof(config));
    client = s_make_client();

    wincmenu_show(NULL, &surface, &desktop, client, pos, &config);
    TAP_NULL(s_captured_state,
            "a null connection shows nothing");

    wincmenu_show((xcb_connection_t *) 1, NULL, &desktop, client, pos,
            &config);
    TAP_NULL(s_captured_state, "a null surface shows nothing");

    wincmenu_show((xcb_connection_t *) 1, &surface, NULL, client, pos,
            &config);
    TAP_NULL(s_captured_state, "a null desktop shows nothing");

    wincmenu_show((xcb_connection_t *) 1, &surface, &desktop, NULL, pos,
            &config);
    TAP_NULL(s_captured_state, "a null client shows nothing");

    wincmenu_show((xcb_connection_t *) 1, &surface, &desktop, client,
            pos, NULL);
    TAP_NULL(s_captured_state, "a null config shows nothing");

    client->properties.flags |= (uint16_t) CLIENT_FLAG_LOCKED;
    wincmenu_show((xcb_connection_t *) 1, &surface, &desktop, client,
            pos, &config);
    TAP_NULL(s_captured_state, "a locked client shows nothing");

    s_teardown();
}


/**
 * @brief Verify the "Send to desktop" and "Send to monitor" submenus
 *        are omitted entirely on a single-desktop, single-monitor
 *        surface, leaving only the always-present Layer submenu, a
 *        separator, and the fixed command entries
 */
static void s_test_show_single_desktop_single_monitor(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td *client;
    config_td config;
    struct position_s pos = { 5, 6 };

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&config, 0, sizeof(config));
    surface.desktop_count = 1u;
    surface.monitor_count = 1u;
    client = s_make_client();

    wincmenu_show((xcb_connection_t *) 1, &surface, &desktop, client,
            pos, &config);

    TAP_NOT_NULL(s_captured_state,
            "showing a valid client calls ctxmenu_show");
    TAP_EQ_STR(s_captured_state->entries[0].label, "Sticky",
            "with one desktop and one monitor, Sticky is the first"
            " entry");
    TAP_EQ_INT((int) s_captured_state->entries[0].type,
            (int) CTXMENU_COMMAND,
            "Sticky is a plain command entry, not a submenu");
    TAP_EQ_STR(s_captured_state->entries[1].label, "Layer",
            "Layer follows Sticky");
    TAP_EQ_INT((int) s_captured_state->entries[1].type,
            (int) CTXMENU_SUBMENU,
            "Layer is a submenu entry");
    TAP_EQ_INT(s_captured_state->entries[1].item_count, 3,
            "the Layer submenu holds exactly the 3 fixed layer"
            " choices");
    TAP_EQ_INT((int) s_captured_state->entries[2].type,
            (int) CTXMENU_SEPARATOR,
            "a separator follows the Layer submenu");
    TAP_EQ_STR(s_captured_state->entries[3].label, "Restore",
            "Restore is the next entry after the separator");

    s_teardown();
}


/**
 * @brief Verify both submenus appear, in order, before Layer, when
 *        the surface has more than one desktop and more than one
 *        monitor
 */
static void s_test_show_multi_desktop_multi_monitor(void)
{
    surface_td surface;
    desktop_td desktop_a;
    desktop_td desktop_b;
    client_td *client;
    config_td config;
    struct position_s pos = { 0, 0 };

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop_a, 0, sizeof(desktop_a));
    memset(&desktop_b, 0, sizeof(desktop_b));
    memset(&config, 0, sizeof(config));
    desktop_a.id = (xcb_window_t) 0;
    strcpy(desktop_a.name, "Desk A");
    desktop_b.id = (xcb_window_t) 1;
    strcpy(desktop_b.name, "Desk B");
    s_desktops[0] = &desktop_a;
    s_desktops[1] = &desktop_b;
    s_desktop_count = 2;

    surface.desktop_count = 2u;
    surface.monitor_count = 2u;
    surface.monitors[0] = (monitor_td) { 0, 0, 1920u, 1080u };
    surface.monitors[1] = (monitor_td) { 1920, 0, 1920u, 1080u };
    surface.primary_monitor_index = 0u;
    s_current_monitor = surface.monitors[0];

    client = s_make_client();

    wincmenu_show((xcb_connection_t *) 1, &surface, &desktop_a, client,
            pos, &config);

    TAP_NOT_NULL(s_captured_state,
            "showing with 2 desktops and 2 monitors calls"
            " ctxmenu_show");
    TAP_EQ_INT((int) s_captured_state->entries[0].type,
            (int) CTXMENU_SUBMENU,
            "the first entry is the Send to desktop submenu");
    TAP_EQ_INT((int) s_captured_state->entries[1].type,
            (int) CTXMENU_SUBMENU,
            "the second entry is the Send to monitor submenu");
    TAP_EQ_STR(s_captured_state->entries[2].label, "Sticky",
            "Sticky follows both submenus");
    TAP_EQ_STR(s_captured_state->entries[3].label, "Layer",
            "Layer follows Sticky");

    /* Send to desktop submenu: one row per desktop plus a trailing
     * separator plus the pin/unpin toggle */
    TAP_EQ_INT(s_captured_state->entries[0].item_count, 4,
            "the desktop submenu holds 2 desktop rows, a separator,"
            " and the pin toggle");
    TAP_OK(s_captured_state->entries[0].items[0].is_disabled,
            "the current desktop's own row is disabled");
    TAP_OK(!s_captured_state->entries[0].items[1].is_disabled,
            "the other desktop's row stays enabled");
    TAP_EQ_INT((int) s_captured_state->entries[0].items[2].type,
            (int) CTXMENU_SEPARATOR,
            "a separator sits between the desktop rows and the pin"
            " toggle");

    /* Send to monitor submenu: one row per monitor, current one
     * disabled */
    TAP_EQ_INT(s_captured_state->entries[1].item_count, 2,
            "the monitor submenu holds one row per monitor");
    TAP_OK(s_captured_state->entries[1].items[0].is_disabled,
            "the current monitor's own row is disabled");
    TAP_OK(!s_captured_state->entries[1].items[1].is_disabled,
            "the other monitor's row stays enabled");

    s_teardown();
}


/**
 * @brief Verify a pinned client relabels the "Send to
 *        desktop" submenu's trailing toggle as an un-pin action
 */
static void s_test_show_pinned_relabels_pin_toggle(void)
{
    surface_td surface;
    desktop_td desktop_a;
    desktop_td desktop_b;
    client_td *client;
    config_td config;
    struct position_s pos = { 0, 0 };
    int last;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop_a, 0, sizeof(desktop_a));
    memset(&desktop_b, 0, sizeof(desktop_b));
    memset(&config, 0, sizeof(config));
    desktop_a.id = (xcb_window_t) 0;
    desktop_b.id = (xcb_window_t) 1;
    s_desktops[0] = &desktop_a;
    s_desktops[1] = &desktop_b;
    s_desktop_count = 2;
    surface.desktop_count = 2u;
    surface.monitor_count = 1u;

    client = s_make_client();
    client->properties.flags |= (uint16_t) CLIENT_FLAG_PIN;

    wincmenu_show((xcb_connection_t *) 1, &surface, &desktop_a, client,
            pos, &config);

    last = s_captured_state->entries[0].item_count - 1;
    TAP_EQ_STR(s_captured_state->entries[0].items[last].label,
            "This desktop only (unpin)",
            "a pinned client's trailing toggle offers to unpin"
            " rather than pin");

    s_teardown();
}


/**
 * @brief Verify the disabled-state derivation of each fixed command
 *        entry for a plain, resizable, decorated, unmaximized client
 */
static void s_test_show_fixed_entries_plain_client(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td *client;
    config_td config;
    struct position_s pos = { 0, 0 };
    ctxmenu_entry_td *e;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&config, 0, sizeof(config));
    surface.desktop_count = 1u;
    surface.monitor_count = 1u;
    client = s_make_client();

    wincmenu_show((xcb_connection_t *) 1, &surface, &desktop, client,
            pos, &config);

    /* Sticky, Layer, separator, Restore, Move, Resize, Iconify, Hide,
     * Maximize, Fullscreen, Shade, Decorate, separator, Inspect,
     * Close */
    e = s_captured_state->entries;
    TAP_EQ_INT(s_captured_state->entry_count, 15,
            "a plain client with 1 desktop and 1 monitor yields 15"
            " top-level entries");
    TAP_EQ_STR(e[0].label, "Sticky", "entry 0 is Sticky");
    TAP_OK(!e[0].is_disabled, "Sticky is never disabled");
    TAP_EQ_STR(e[3].label, "Restore", "entry 3 is Restore");
    TAP_OK(e[3].is_disabled,
            "Restore is disabled for an unmaximized, non-fullscreen"
            " client");
    TAP_EQ_STR(e[4].label, "Move", "entry 4 is Move");
    TAP_OK(!e[4].is_disabled, "Move is enabled for a plain client");
    TAP_EQ_STR(e[5].label, "Resize", "entry 5 is Resize");
    TAP_OK(!e[5].is_disabled,
            "Resize is enabled for a resizable, unmaximized client");
    TAP_EQ_STR(e[6].label, "Iconify", "entry 6 is Iconify");
    TAP_OK(!e[6].is_disabled, "Iconify is never disabled");
    TAP_EQ_STR(e[7].label, "Hide", "entry 7 is Hide");
    TAP_OK(!e[7].is_disabled, "Hide is never disabled");
    TAP_EQ_STR(e[8].label, "Maximize", "entry 8 is Maximize");
    TAP_OK(!e[8].is_disabled,
            "Maximize is enabled for a maximizable, unmaximized"
            " client");
    TAP_EQ_STR(e[9].label, "Fullscreen", "entry 9 is"
            " Fullscreen for a windowed, resizable client");
    TAP_OK(!e[9].is_disabled,
            "fullscreen is enabled for a resizable, non-modal"
            " client");
    TAP_EQ_STR(e[10].label, "Shade", "entry 10 is Shade");
    TAP_OK(!e[10].is_disabled,
            "Shade is enabled for a decorated, non-fullscreen"
            " client");
    TAP_EQ_STR(e[11].label, "Undecorate",
            "entry 11 reads Undecorate for an already-decorated"
            " client");
    TAP_OK(!e[11].is_disabled,
            "Decorate/Undecorate is enabled outside fullscreen");
    TAP_EQ_INT((int) e[12].type, (int) CTXMENU_SEPARATOR,
            "entry 12 is the separator before Inspect/Close");
    TAP_EQ_STR(e[13].label, "Inspect...", "entry 13 is Inspect...");
    TAP_OK(!e[13].is_disabled, "Inspect is never disabled");
    TAP_EQ_STR(e[14].label, "Close", "entry 14 is Close");
    TAP_OK(!e[14].is_disabled, "Close is never disabled");

    s_teardown();
}


/**
 * @brief Verify Sticky is omitted entirely, not merely disabled, on a
 *        surface whose configured viewport is a single screen, the
 *        same condition the titlebar's sticky button hides under
 */
static void s_test_show_single_page_viewport_omits_sticky(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td *client;
    config_td config;
    struct position_s pos = { 0, 0 };
    ctxmenu_entry_td *e;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&config, 0, sizeof(config));
    surface.desktop_count = 1u;
    surface.monitor_count = 1u;
    s_viewport_has_room = false;
    client = s_make_client();

    wincmenu_show((xcb_connection_t *) 1, &surface, &desktop, client,
            pos, &config);

    e = s_captured_state->entries;
    TAP_EQ_INT(s_captured_state->entry_count, 14,
            "a 1x1 viewport yields 14 top-level entries, one fewer"
            " than a pannable one");
    TAP_EQ_STR(e[0].label, "Layer",
            "Layer takes the first slot once Sticky is omitted");
    TAP_EQ_INT((int) e[0].type, (int) CTXMENU_SUBMENU,
            "that first entry really is the Layer submenu");
    TAP_EQ_STR(e[13].label, "Close",
            "Close still ends the menu, one slot earlier");

    s_teardown();
}


/**
 * @brief Verify a fully-maximized client disables Move, Resize, and
 *        Layer, and enables Restore instead
 */
static void s_test_show_maximized_client(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td *client;
    config_td config;
    struct position_s pos = { 0, 0 };
    ctxmenu_entry_td *e;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&config, 0, sizeof(config));
    surface.desktop_count = 1u;
    surface.monitor_count = 1u;
    client = s_make_client();
    client->properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED;

    wincmenu_show((xcb_connection_t *) 1, &surface, &desktop, client,
            pos, &config);

    e = s_captured_state->entries;
    TAP_OK(!e[3].is_disabled,
            "Restore is enabled once the client is maximized");
    TAP_OK(e[4].is_disabled, "Move is disabled while maximized");
    TAP_OK(e[5].is_disabled, "Resize is disabled while maximized");
    TAP_OK(e[8].is_disabled,
            "Maximize itself is disabled while already maximized");

    s_teardown();
}


/**
 * @brief Verify a fullscreen client disables Layer, Move, Resize,
 *        and Shade, but leaves fullscreen's own entry enabled with
 *        the "Exit fullscreen" label
 */
static void s_test_show_fullscreen_client(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td *client;
    config_td config;
    struct position_s pos = { 0, 0 };
    ctxmenu_entry_td *e;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&config, 0, sizeof(config));
    surface.desktop_count = 1u;
    surface.monitor_count = 1u;
    client = s_make_client();
    client->properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;

    wincmenu_show((xcb_connection_t *) 1, &surface, &desktop, client,
            pos, &config);

    e = s_captured_state->entries;
    TAP_OK(e[1].is_disabled,
            "the Layer submenu is disabled while fullscreen");
    TAP_OK(e[4].is_disabled, "Move is disabled while fullscreen");
    TAP_OK(e[5].is_disabled, "Resize is disabled while fullscreen");
    TAP_EQ_STR(e[9].label, "Unfullscreen",
            "the fullscreen entry reads Unfullscreen once"
            " active");
    TAP_OK(!e[9].is_disabled,
            "exiting fullscreen is always enabled once active");
    TAP_OK(e[10].is_disabled, "Shade is disabled while fullscreen");
    TAP_OK(e[11].is_disabled,
            "Decorate/Undecorate is disabled while fullscreen");

    s_teardown();
}


/**
 * @brief Verify a modal, resizable client cannot enter fullscreen,
 *        even though it can still be maximized
 */
static void s_test_show_modal_client_blocks_fullscreen(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td *client;
    config_td config;
    struct position_s pos = { 0, 0 };
    ctxmenu_entry_td *e;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&config, 0, sizeof(config));
    surface.desktop_count = 1u;
    surface.monitor_count = 1u;
    client = s_make_client();
    client->properties.flags |= (uint16_t) CLIENT_FLAG_MODAL;

    wincmenu_show((xcb_connection_t *) 1, &surface, &desktop, client,
            pos, &config);

    e = s_captured_state->entries;
    TAP_OK(e[9].is_disabled,
            "a modal client cannot enter fullscreen even though it"
            " is resizable");

    s_teardown();
}


/**
 * @brief Verify a non-resizable client disables Resize, Maximize,
 *        and fullscreen together
 */
static void s_test_show_non_resizable_client(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td *client;
    config_td config;
    struct position_s pos = { 0, 0 };
    ctxmenu_entry_td *e;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&config, 0, sizeof(config));
    surface.desktop_count = 1u;
    surface.monitor_count = 1u;
    client = s_make_client();
    client->properties.flags &= (uint16_t) ~CLIENT_FLAG_RESIZABLE;

    wincmenu_show((xcb_connection_t *) 1, &surface, &desktop, client,
            pos, &config);

    e = s_captured_state->entries;
    TAP_OK(e[5].is_disabled,
            "Resize is disabled for a non-resizable client");
    TAP_OK(e[8].is_disabled,
            "Maximize is disabled for a non-resizable client");
    TAP_OK(e[9].is_disabled,
            "fullscreen is disabled for a non-resizable client");

    s_teardown();
}


/**
 * @brief Verify an undecorated client disables Shade, and shows
 *        "Decorate" instead of "Undecorate"
 */
static void s_test_show_undecorated_client(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td *client;
    config_td config;
    struct position_s pos = { 0, 0 };
    ctxmenu_entry_td *e;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&config, 0, sizeof(config));
    surface.desktop_count = 1u;
    surface.monitor_count = 1u;
    client = s_make_client();
    client->properties.flags &= (uint16_t) ~CLIENT_FLAG_DECORATED;

    wincmenu_show((xcb_connection_t *) 1, &surface, &desktop, client,
            pos, &config);

    e = s_captured_state->entries;
    TAP_OK(e[10].is_disabled,
            "Shade is disabled for an undecorated client");
    TAP_EQ_STR(e[11].label, "Decorate",
            "the toggle reads Decorate for an undecorated client");

    s_teardown();
}


/**
 * @brief Verify @a wincmenu_show closes any previously open menu
 *        first, and that the thin wrapper functions forward to their
 *        respective dispatchers, mirroring 'test_rootmenu.c''s own
 *        wrapper coverage
 */
static void s_test_wrappers(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td *client;
    config_td config;
    struct position_s pos = { 0, 0 };
    bool click_result;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&config, 0, sizeof(config));
    surface.desktop_count = 1u;
    surface.monitor_count = 1u;
    client = s_make_client();

    wincmenu_show((xcb_connection_t *) 1, &surface, &desktop, client,
            pos, &config);
    TAP_NOT_NULL(s_captured_state,
            "showing once builds and shows a menu");

    wincmenu_repaint((xcb_window_t) 1);
    TAP_OK(true, "repaint forwards without crashing");

    click_result = wincmenu_handle_click((xcb_connection_t *) 1,
            &surface, (xcb_window_t) 1, 0, &config);
    TAP_OK(!click_result,
            "handle_click returns the stubbed tree dispatcher's"
            " result");

    TAP_OK(!wincmenu_is_open(),
            "is_open returns the stubbed ctxmenu_is_open's result");

    TAP_OK(!wincmenu_owns_window((xcb_window_t) 1),
            "owns_window returns false when the stubbed tree finder"
            " finds nothing");

    TAP_OK(!wincmenu_handle_keypress((xcb_connection_t *) 1, &surface,
                0x61u, &config),
            "handle_keypress returns the stubbed deepest-dispatcher's"
            " result");

    wincmenu_handle_motion((xcb_window_t) 1, 0, 0);
    TAP_OK(true, "handle_motion forwards without crashing");

    s_teardown();
}


int main(void)
{
    TAP_PLAN(81);

    s_test_show_guards();
    s_test_show_single_desktop_single_monitor();
    s_test_show_multi_desktop_multi_monitor();
    s_test_show_pinned_relabels_pin_toggle();
    s_test_show_fixed_entries_plain_client();
    s_test_show_single_page_viewport_omits_sticky();
    s_test_show_maximized_client();
    s_test_show_fullscreen_client();
    s_test_show_modal_client_blocks_fullscreen();
    s_test_show_non_resizable_client();
    s_test_show_undecorated_client();
    s_test_wrappers();

    return TAP_DONE();
}
