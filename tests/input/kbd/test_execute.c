/**
 * @file tests/input/kbd/test_execute.c
 *
 * @brief Test battery for carrying out a resolved keyboard binding
 *        (input/kbd/execute.c)
 *
 * 'ik_execute_binding' is one long switch dispatching each of the
 * seventy-four 'wm_keybind_type_e' values to exactly one real action,
 * with its own file-static 's_dispatch_client_action' (a second,
 * narrower switch reached only through the fourteen 'KEYBIND_CLIENT_*'
 * cases that need a resolved client first) and 's_menu_position_
 * resolve' (reached only through the two menu-opening cases) both
 * exercised indirectly the same way, since neither is reachable except
 * through the public entry point this file actually calls.  Every
 * action past the switch itself, every 'enact_client_*'/'enact_
 * desktop_*'/'enact_surface_*' function, every dialog and context-menu
 * 'show' function, 'popup_show', 'search_init', 'scratchpad_toggle',
 * 'wm_request_full_redraw', 'wm_action_config_reload', 'ik_get_active_
 * client'/'ik_handle_launch'/'ik_handle_move'/'ik_handle_resize'
 * (input/kbd/interact.c's own logic, already covered on its own by
 * 'tests/input/kbd/test_interact.c'), 'lookup_current_desktop',
 * 'ccmd_client_unshade', and 'xcb_query_pointer'/its reply, is a heavy
 * side-effecting primitive or a dependency this file has no business
 * re-verifying, so each is a recording or controlled stand-in here,
 * letting every scenario below assert directly on which single real
 * action a given binding type reaches and with which arguments,
 * following the same "stub every heavy dependency, link only the file
 * under test" pattern 'tests/rules/test_apply.c' documents.  'wm_td'
 * is opaque outside wm.c, so a fixed non-null pointer this file never
 * dereferences stands in for it everywhere the switch requires one.
 *
 * @note KEYBIND_WM_EMERGENCY_EXIT is, by 'execute.c's own comment,
 *       "already handled above via the hardcoded shortcut" (in
 *       input/kbd/event.c, covered by tests/input/kbd/test_event.c):
 *       reaching it here is a genuine no-op by design, verified below
 *       as exactly that.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* strtok_r, needed transitively */

/* System includes */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <scratchpad.h>
#include <surface.h>
#include <wm.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/context/wincmenu.h>
#include <menu/context/winlist.h>
#include <menu/dialog/fortune.h>
#include <menu/dialog/inspect.h>
#include <menu/dialog/quit.h>
#include <menu/dialog/shortcuts.h>
#include <menu/popup.h>
#include <menu/search.h>

/* Command includes */
#include <cmds/client/state.h>

/* Local includes */
#include <harness/tap.h>
#include <input/kbd/bind.h>
#include <input/kbd/internal.h>
#include <utils/xcb/connection.h>


/** Fixed opaque wm_td handle, never dereferenced (opaque outside wm.c) */
static int s_fake_wm_storage;
static wm_td *const s_fake_wm = (wm_td *) &s_fake_wm_storage;

/** Fixed opaque xcb_connection_t handle, never dereferenced */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
        (xcb_connection_t *) &s_fake_connection_storage;

/** Fixed desktop lookup_current_desktop and surface_desktop_get answer
 *  with, shared by every scenario that needs a resolvable desktop */
static desktop_td s_fixture_desktop;

/** Fixed client ik_get_active_client answers with, or null */
static client_td s_fixture_client;
static client_td *s_active_client_result;

/** lookup_current_desktop controllable result, independent of the
 *  fixture above so "no desktop" scenarios stay simple */
static desktop_td *s_lookup_desktop_result;

/** Call-counting and last-argument-capturing stand-ins, one static
 *  int per enact_client, enact_desktop, enact_surface, dialog, and
 *  menu-show function this file never links for real; reset to zero
 *  by s_reset before every scenario */
static int s_call_enact_client_iconify;
static int s_call_enact_client_hide;
static int s_call_enact_client_close;
static int s_call_enact_client_kill;
static int s_call_enact_client_maximize;
static int s_call_enact_client_center;
static int s_call_enact_client_move_monitor_north;
static int s_call_enact_client_move_monitor_south;
static int s_call_enact_client_move_monitor_east;
static int s_call_enact_client_move_monitor_west;
static int s_call_enact_client_send_to_desktop_north;
static int s_call_enact_client_send_to_desktop_south;
static int s_call_enact_client_send_to_desktop_east;
static int s_call_enact_client_send_to_desktop_west;
static int s_call_enact_client_toggle_shade;
static int s_call_enact_client_toggle_fullscreen;
static int s_call_enact_client_toggle_pin;
static int s_call_enact_client_toggle_decorate;
static int s_call_enact_client_cycle_layer;
static int s_call_ccmd_client_unshade;

static int s_call_enact_surface_desktop_switch_north;
static int s_call_enact_surface_desktop_switch_south;
static int s_call_enact_surface_desktop_switch_east;
static int s_call_enact_surface_desktop_switch_west;
static int s_call_enact_surface_desktop_switch;
static int s_call_enact_surface_viewport_pan_north;
static int s_call_enact_surface_viewport_pan_south;
static int s_call_enact_surface_viewport_pan_east;
static int s_call_enact_surface_viewport_pan_west;
static int s_call_enact_surface_viewport_goto;
static uint32_t s_last_desktop_switch_id;
static uint32_t s_last_viewport_goto_page;
static int s_call_enact_surface_desktop_add;
static int s_call_enact_surface_desktop_remove;
static int s_call_enact_surface_toggle_strutless_maximize;

static int s_call_enact_desktop_show;
static bool s_last_desktop_show_flag;
static int s_call_scratchpad_toggle;
static int s_call_enact_desktop_clients_iconify_all;
static int s_call_enact_desktop_clients_deiconify_all;
static int s_call_enact_desktop_clients_rearrange;
static int s_call_enact_desktop_cycle_clients_active;
static int s_call_enact_desktop_cycle_clients_prev;
static int s_call_enact_desktop_cycle_clients_icons_next;
static int s_call_enact_desktop_cycle_clients_icons_prev;

static int s_call_dialog_fortune_show;
static int s_call_wm_request_full_redraw;
static int s_call_dialog_quit_show;
static int s_call_dialog_shortcuts_show;
static int s_call_wm_action_config_reload;
static int s_call_rootmenu_show;
static int s_call_winlist_show;
static int s_call_search_init;
static int s_call_wincmenu_show;
static int s_call_popup_show;
static int s_call_dialog_inspect_show;

static int s_call_ik_handle_launch;
static enum ik_launch_e s_last_launch_program;
static int s_call_ik_handle_move;
static enum ik_move_e s_last_move_direction;
static int s_call_ik_handle_resize;
static enum ik_resize_e s_last_resize_edge;

/** xcb_query_pointer controllable outcome: whether the reply is null
 *  and, when not, which root coordinates it reports */
static bool s_query_pointer_reply_is_null;
static int16_t s_query_pointer_root_x;
static int16_t s_query_pointer_root_y;


static void s_reset(void)
{
    memset(&s_fixture_desktop, 0, sizeof(s_fixture_desktop));
    memset(&s_fixture_client, 0, sizeof(s_fixture_client));
    s_active_client_result = NULL;
    s_lookup_desktop_result = NULL;

    s_call_enact_client_iconify = 0;
    s_call_enact_client_hide = 0;
    s_call_enact_client_close = 0;
    s_call_enact_client_kill = 0;
    s_call_enact_client_maximize = 0;
    s_call_enact_client_center = 0;
    s_call_enact_client_move_monitor_north = 0;
    s_call_enact_client_move_monitor_south = 0;
    s_call_enact_client_move_monitor_east = 0;
    s_call_enact_client_move_monitor_west = 0;
    s_call_enact_client_send_to_desktop_north = 0;
    s_call_enact_client_send_to_desktop_south = 0;
    s_call_enact_client_send_to_desktop_east = 0;
    s_call_enact_client_send_to_desktop_west = 0;
    s_call_enact_client_toggle_shade = 0;
    s_call_enact_client_toggle_fullscreen = 0;
    s_call_enact_client_toggle_pin = 0;
    s_call_enact_client_toggle_decorate = 0;
    s_call_enact_client_cycle_layer = 0;
    s_call_ccmd_client_unshade = 0;

    s_call_enact_surface_desktop_switch_north = 0;
    s_call_enact_surface_desktop_switch_south = 0;
    s_call_enact_surface_desktop_switch_east = 0;
    s_call_enact_surface_desktop_switch_west = 0;
    s_call_enact_surface_desktop_switch = 0;
    s_call_enact_surface_viewport_pan_north = 0;
    s_call_enact_surface_viewport_pan_south = 0;
    s_call_enact_surface_viewport_pan_east = 0;
    s_call_enact_surface_viewport_pan_west = 0;
    s_call_enact_surface_viewport_goto = 0;
    s_last_desktop_switch_id = 0;
    s_last_viewport_goto_page = 0;
    s_call_enact_surface_desktop_add = 0;
    s_call_enact_surface_desktop_remove = 0;
    s_call_enact_surface_toggle_strutless_maximize = 0;

    s_call_enact_desktop_show = 0;
    s_last_desktop_show_flag = false;
    s_call_scratchpad_toggle = 0;
    s_call_enact_desktop_clients_iconify_all = 0;
    s_call_enact_desktop_clients_deiconify_all = 0;
    s_call_enact_desktop_clients_rearrange = 0;
    s_call_enact_desktop_cycle_clients_active = 0;
    s_call_enact_desktop_cycle_clients_prev = 0;
    s_call_enact_desktop_cycle_clients_icons_next = 0;
    s_call_enact_desktop_cycle_clients_icons_prev = 0;

    s_call_dialog_fortune_show = 0;
    s_call_wm_request_full_redraw = 0;
    s_call_dialog_quit_show = 0;
    s_call_dialog_shortcuts_show = 0;
    s_call_wm_action_config_reload = 0;
    s_call_rootmenu_show = 0;
    s_call_winlist_show = 0;
    s_call_search_init = 0;
    s_call_wincmenu_show = 0;
    s_call_popup_show = 0;
    s_call_dialog_inspect_show = 0;

    s_call_ik_handle_launch = 0;
    s_call_ik_handle_move = 0;
    s_call_ik_handle_resize = 0;

    s_query_pointer_reply_is_null = true;
    s_query_pointer_root_x = 0;
    s_query_pointer_root_y = 0;
}


/* Stand-ins: input/kbd/interact.c entry points (own logic covered by
 * tests/input/kbd/test_interact.c) */

client_td *ik_get_active_client(surface_td *surface, list_td *surfaces,
        surface_td **cs_out, desktop_td **cd_out)
{
    (void) surface;
    (void) surfaces;
    if (cs_out != NULL) {
        *cs_out = NULL;
    }
    if (cd_out != NULL) {
        *cd_out = NULL;
    }
    return s_active_client_result;
}


void ik_handle_launch(enum ik_launch_e program, surface_td *surface,
        const config_td *config)
{
    (void) surface;
    (void) config;
    s_call_ik_handle_launch++;
    s_last_launch_program = program;
}


void ik_handle_move(enum ik_move_e direction, surface_td *surface,
        list_td *surfaces, const config_td *config)
{
    (void) surface;
    (void) surfaces;
    (void) config;
    s_call_ik_handle_move++;
    s_last_move_direction = direction;
}


void ik_handle_resize(enum ik_resize_e edge, surface_td *surface,
        list_td *surfaces, const config_td *config)
{
    (void) surface;
    (void) surfaces;
    (void) config;
    s_call_ik_handle_resize++;
    s_last_resize_edge = edge;
}


/* Stand-ins: lookup.h */

desktop_td *lookup_current_desktop(surface_td *surface)
{
    (void) surface;
    return s_lookup_desktop_result;
}


desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;
    return s_lookup_desktop_result;
}


/* Stand-ins: enact.h client actions */

void enact_client_iconify(client_td *client)
{
    (void) client;
    s_call_enact_client_iconify++;
}


void enact_client_hide(client_td *client)
{
    (void) client;
    s_call_enact_client_hide++;
}


void enact_client_close(client_td *client)
{
    (void) client;
    s_call_enact_client_close++;
}


void enact_client_kill(client_td *client)
{
    (void) client;
    s_call_enact_client_kill++;
}


void enact_client_maximize(client_td *client)
{
    (void) client;
    s_call_enact_client_maximize++;
}


void enact_client_center(client_td *client)
{
    (void) client;
    s_call_enact_client_center++;
}


void enact_client_move_monitor_north(client_td *client)
{
    (void) client;
    s_call_enact_client_move_monitor_north++;
}


void enact_client_move_monitor_south(client_td *client)
{
    (void) client;
    s_call_enact_client_move_monitor_south++;
}


void enact_client_move_monitor_east(client_td *client)
{
    (void) client;
    s_call_enact_client_move_monitor_east++;
}


void enact_client_move_monitor_west(client_td *client)
{
    (void) client;
    s_call_enact_client_move_monitor_west++;
}


void enact_client_send_to_desktop_north(client_td *client,
        list_td *surfaces, const config_td *config)
{
    (void) client;
    (void) surfaces;
    (void) config;
    s_call_enact_client_send_to_desktop_north++;
}


void enact_client_send_to_desktop_south(client_td *client,
        list_td *surfaces, const config_td *config)
{
    (void) client;
    (void) surfaces;
    (void) config;
    s_call_enact_client_send_to_desktop_south++;
}


void enact_client_send_to_desktop_east(client_td *client,
        list_td *surfaces, const config_td *config)
{
    (void) client;
    (void) surfaces;
    (void) config;
    s_call_enact_client_send_to_desktop_east++;
}


void enact_client_send_to_desktop_west(client_td *client,
        list_td *surfaces, const config_td *config)
{
    (void) client;
    (void) surfaces;
    (void) config;
    s_call_enact_client_send_to_desktop_west++;
}


void enact_client_toggle_shade(client_td *client)
{
    (void) client;
    s_call_enact_client_toggle_shade++;
}


void enact_client_toggle_fullscreen(client_td *client)
{
    (void) client;
    s_call_enact_client_toggle_fullscreen++;
}


void enact_client_toggle_pin(client_td *client)
{
    (void) client;
    s_call_enact_client_toggle_pin++;
}


void enact_client_toggle_decorate(client_td *client)
{
    (void) client;
    s_call_enact_client_toggle_decorate++;
}


void enact_client_cycle_layer(client_td *client)
{
    (void) client;
    s_call_enact_client_cycle_layer++;
}


void ccmd_client_unshade(client_td *client)
{
    (void) client;
    s_call_ccmd_client_unshade++;
}


/* Stand-ins: enact.h surface/desktop-switch actions */

void enact_surface_desktop_switch_north(surface_td *surface)
{
    (void) surface;
    s_call_enact_surface_desktop_switch_north++;
}


void enact_surface_desktop_switch_south(surface_td *surface)
{
    (void) surface;
    s_call_enact_surface_desktop_switch_south++;
}


void enact_surface_desktop_switch_east(surface_td *surface)
{
    (void) surface;
    s_call_enact_surface_desktop_switch_east++;
}


void enact_surface_desktop_switch_west(surface_td *surface)
{
    (void) surface;
    s_call_enact_surface_desktop_switch_west++;
}


void enact_surface_desktop_switch(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    s_call_enact_surface_desktop_switch++;
    s_last_desktop_switch_id = desktop_id;
}


void enact_surface_desktop_add(surface_td *surface)
{
    (void) surface;
    s_call_enact_surface_desktop_add++;
}


void enact_surface_desktop_remove(surface_td *surface)
{
    (void) surface;
    s_call_enact_surface_desktop_remove++;
}


void enact_surface_toggle_strutless_maximize(surface_td *surface)
{
    (void) surface;
    s_call_enact_surface_toggle_strutless_maximize++;
}


/* Stand-ins: enact.h viewport-pan actions */

void enact_surface_viewport_pan_north(surface_td *surface)
{
    (void) surface;
    s_call_enact_surface_viewport_pan_north++;
}


void enact_surface_viewport_pan_south(surface_td *surface)
{
    (void) surface;
    s_call_enact_surface_viewport_pan_south++;
}


void enact_surface_viewport_pan_east(surface_td *surface)
{
    (void) surface;
    s_call_enact_surface_viewport_pan_east++;
}


void enact_surface_viewport_pan_west(surface_td *surface)
{
    (void) surface;
    s_call_enact_surface_viewport_pan_west++;
}


void enact_surface_viewport_goto(surface_td *surface, uint32_t page)
{
    (void) surface;
    s_call_enact_surface_viewport_goto++;
    s_last_viewport_goto_page = page;
}


/* Stand-ins: enact.h desktop-wide actions */

void enact_desktop_show(desktop_td *desktop, bool show)
{
    (void) desktop;
    s_call_enact_desktop_show++;
    s_last_desktop_show_flag = show;
}


void enact_desktop_clients_iconify_all(desktop_td *desktop)
{
    (void) desktop;
    s_call_enact_desktop_clients_iconify_all++;
}


void enact_desktop_clients_deiconify_all(desktop_td *desktop)
{
    (void) desktop;
    s_call_enact_desktop_clients_deiconify_all++;
}


void enact_desktop_clients_rearrange(const wm_td *wm, surface_td *surface,
        const desktop_td *desktop)
{
    (void) wm;
    (void) surface;
    (void) desktop;
    s_call_enact_desktop_clients_rearrange++;
}


void enact_desktop_cycle_clients_active(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop, uint16_t modmask,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) desktop;
    (void) modmask;
    (void) config;
    s_call_enact_desktop_cycle_clients_active++;
}


void enact_desktop_cycle_clients_prev(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop, uint16_t modmask,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) desktop;
    (void) modmask;
    (void) config;
    s_call_enact_desktop_cycle_clients_prev++;
}


void enact_desktop_cycle_clients_icons_next(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop, uint16_t modmask,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) desktop;
    (void) modmask;
    (void) config;
    s_call_enact_desktop_cycle_clients_icons_next++;
}


void enact_desktop_cycle_clients_icons_prev(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop, uint16_t modmask,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) desktop;
    (void) modmask;
    (void) config;
    s_call_enact_desktop_cycle_clients_icons_prev++;
}


/* Stand-ins: scratchpad.h, wm.h misc actions */

void scratchpad_toggle(const wm_td *wm, desktop_td *desktop)
{
    (void) wm;
    (void) desktop;
    s_call_scratchpad_toggle++;
}


void wm_request_full_redraw(void)
{
    s_call_wm_request_full_redraw++;
}


int wm_action_config_reload(const wm_td *wm)
{
    (void) wm;
    s_call_wm_action_config_reload++;
    return 0;
}


/* Stand-ins: dialog and context-menu show functions */

void dialog_fortune_show(xcb_connection_t *connection, surface_td *surface,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) config;
    s_call_dialog_fortune_show++;
}


void dialog_quit_show(xcb_connection_t *connection, surface_td *surface,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) config;
    s_call_dialog_quit_show++;
}


void dialog_shortcuts_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) config;
    s_call_dialog_shortcuts_show++;
}


void rootmenu_show(wm_td *wm, xcb_connection_t *connection,
        surface_td *surface, struct position_s pos, const config_td *config)
{
    (void) wm;
    (void) connection;
    (void) surface;
    (void) pos;
    (void) config;
    s_call_rootmenu_show++;
}


void winlist_show(xcb_connection_t *connection, surface_td *surface,
        struct position_s pos, const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) pos;
    (void) config;
    s_call_winlist_show++;
}


void search_init(list_td *surfaces, xcb_connection_t *connection,
        surface_td *surface, const config_td *cfg)
{
    (void) surfaces;
    (void) connection;
    (void) surface;
    (void) cfg;
    s_call_search_init++;
}


void wincmenu_show(xcb_connection_t *connection, surface_td *surface,
        desktop_td *desktop, client_td *client, struct position_s pos,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) desktop;
    (void) client;
    (void) pos;
    (void) config;
    s_call_wincmenu_show++;
}


void popup_show(xcb_connection_t *connection, surface_td *surface,
        const desktop_td *desktop, client_td *client, uint16_t modifier,
        xcb_keycode_t keycode, const config_td *cfg)
{
    (void) connection;
    (void) surface;
    (void) desktop;
    (void) client;
    (void) modifier;
    (void) keycode;
    (void) cfg;
    s_call_popup_show++;
}


void dialog_inspect_show(xcb_connection_t *connection, surface_td *surface,
        const config_td *config, const client_td *client)
{
    (void) connection;
    (void) surface;
    (void) config;
    (void) client;
    s_call_dialog_inspect_show++;
}


/* Stand-ins: raw libxcb pointer query, only reached by
 * KEYBIND_WM_ROOT_MENU/KEYBIND_WM_WINDOWS_MENU when configured
 * CONFIG_MENU_POSITION_UNDER_MOUSE */

xcb_query_pointer_cookie_t xcb_query_pointer(xcb_connection_t *connection,
        xcb_window_t window)
{
    xcb_query_pointer_cookie_t cookie;

    (void) connection;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


xcb_query_pointer_reply_t *xcb_query_pointer_reply(
        xcb_connection_t *connection, xcb_query_pointer_cookie_t cookie,
        xcb_generic_error_t **error)
{
    xcb_query_pointer_reply_t *reply;

    (void) connection;
    (void) cookie;
    if (error != NULL) {
        *error = NULL;
    }
    if (s_query_pointer_reply_is_null) {
        return NULL;
    }
    reply = malloc(sizeof(*reply));
    memset(reply, 0, sizeof(*reply));
    reply->root_x = s_query_pointer_root_x;
    reply->root_y = s_query_pointer_root_y;
    return reply;
}


/**
 * @brief Link-only stand-in for @a logger_msg
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict msg, ...)
{
    (void) level;
    (void) prefix;
    (void) msg;
    return 0;
}


/* Fixture helpers */

static void s_give_active_client(void)
{
    s_active_client_result = &s_fixture_client;
    s_lookup_desktop_result = &s_fixture_desktop;
}


/* KEYBIND_DESKTOP_NORTH/SOUTH/EAST/WEST: surface-relative desktop
 * switching along the four compass directions */

static void s_test_desktop_north_south_east_west(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_NORTH, 0, 0, &surface,
            NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_SOUTH, 0, 0, &surface,
            NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_EAST, 0, 0, &surface,
            NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_WEST, 0, 0, &surface,
            NULL, NULL);

    TAP_EQ_INT(s_call_enact_surface_desktop_switch_north, 1,
            "KEYBIND_DESKTOP_NORTH reaches" \
            " enact_surface_desktop_switch_north exactly once");
    TAP_EQ_INT(s_call_enact_surface_desktop_switch_south, 1,
            "KEYBIND_DESKTOP_SOUTH reaches" \
            " enact_surface_desktop_switch_south exactly once");
    TAP_EQ_INT(s_call_enact_surface_desktop_switch_east, 1,
            "KEYBIND_DESKTOP_EAST reaches" \
            " enact_surface_desktop_switch_east exactly once");
    TAP_EQ_INT(s_call_enact_surface_desktop_switch_west, 1,
            "KEYBIND_DESKTOP_WEST reaches" \
            " enact_surface_desktop_switch_west exactly once");
}


static void s_test_desktop_north_null_surface_is_noop(void)
{
    s_reset();

    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_NORTH, 0, 0, NULL,
            NULL, NULL);

    TAP_EQ_INT(s_call_enact_surface_desktop_switch_north, 0,
            "a null surface guards KEYBIND_DESKTOP_NORTH from" \
            " reaching enact_surface_desktop_switch_north");
}


/* KEYBIND_VIEWPORT_PAN_NORTH/SOUTH/EAST/WEST: surface-relative
 * viewport panning along the four compass directions */

static void s_test_viewport_pan_north_south_east_west(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    ik_execute_binding(s_fake_wm, KEYBIND_VIEWPORT_PAN_NORTH, 0, 0,
            &surface, NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_VIEWPORT_PAN_SOUTH, 0, 0,
            &surface, NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_VIEWPORT_PAN_EAST, 0, 0,
            &surface, NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_VIEWPORT_PAN_WEST, 0, 0,
            &surface, NULL, NULL);

    TAP_EQ_INT(s_call_enact_surface_viewport_pan_north, 1,
            "KEYBIND_VIEWPORT_PAN_NORTH reaches" \
            " enact_surface_viewport_pan_north exactly once");
    TAP_EQ_INT(s_call_enact_surface_viewport_pan_south, 1,
            "KEYBIND_VIEWPORT_PAN_SOUTH reaches" \
            " enact_surface_viewport_pan_south exactly once");
    TAP_EQ_INT(s_call_enact_surface_viewport_pan_east, 1,
            "KEYBIND_VIEWPORT_PAN_EAST reaches" \
            " enact_surface_viewport_pan_east exactly once");
    TAP_EQ_INT(s_call_enact_surface_viewport_pan_west, 1,
            "KEYBIND_VIEWPORT_PAN_WEST reaches" \
            " enact_surface_viewport_pan_west exactly once");
}


static void s_test_viewport_pan_north_null_surface_is_noop(void)
{
    s_reset();

    ik_execute_binding(s_fake_wm, KEYBIND_VIEWPORT_PAN_NORTH, 0, 0, NULL,
            NULL, NULL);

    TAP_EQ_INT(s_call_enact_surface_viewport_pan_north, 0,
            "a null surface guards KEYBIND_VIEWPORT_PAN_NORTH from" \
            " reaching enact_surface_viewport_pan_north");
}


/* KEYBIND_VIEWPORT_GOTO_1..9: jump the surface's current desktop
 * viewport straight to one of its configured pages, one-based key
 * to zero-based page index */

static void s_test_viewport_goto_n_passes_correct_page(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    ik_execute_binding(s_fake_wm, KEYBIND_VIEWPORT_GOTO_1, 0, 0, &surface,
            NULL, NULL);
    TAP_EQ_INT((int) s_last_viewport_goto_page, 0,
            "KEYBIND_VIEWPORT_GOTO_1 passes page index 0 to" \
            " enact_surface_viewport_goto");

    ik_execute_binding(s_fake_wm, KEYBIND_VIEWPORT_GOTO_5, 0, 0, &surface,
            NULL, NULL);
    TAP_EQ_INT((int) s_last_viewport_goto_page, 4,
            "KEYBIND_VIEWPORT_GOTO_5 passes page index 4 to" \
            " enact_surface_viewport_goto");

    ik_execute_binding(s_fake_wm, KEYBIND_VIEWPORT_GOTO_9, 0, 0, &surface,
            NULL, NULL);
    TAP_EQ_INT((int) s_last_viewport_goto_page, 8,
            "KEYBIND_VIEWPORT_GOTO_9 passes page index 8 to" \
            " enact_surface_viewport_goto");

    TAP_EQ_INT(s_call_enact_surface_viewport_goto, 3,
            "all three KEYBIND_VIEWPORT_GOTO_N scenarios above each" \
            " reached enact_surface_viewport_goto exactly once");
}


static void s_test_viewport_goto_1_null_surface_is_noop(void)
{
    s_reset();

    ik_execute_binding(s_fake_wm, KEYBIND_VIEWPORT_GOTO_1, 0, 0, NULL,
            NULL, NULL);

    TAP_EQ_INT(s_call_enact_surface_viewport_goto, 0,
            "a null surface guards KEYBIND_VIEWPORT_GOTO_1 from" \
            " reaching enact_surface_viewport_goto");
}


static void s_test_desktop_show_toggles_the_flag(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.is_showing_desktop = false;

    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_SHOW, 0, 0, &surface,
            NULL, NULL);

    TAP_EQ_INT(s_call_enact_desktop_show, 1,
            "KEYBIND_DESKTOP_SHOW reaches enact_desktop_show exactly" \
            " once");
    TAP_OK(s_last_desktop_show_flag == true,
            "KEYBIND_DESKTOP_SHOW passes the logical negation of" \
            " surface->is_showing_desktop as the new show state");
}


static void s_test_scratchpad_toggle_needs_a_surface(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    ik_execute_binding(s_fake_wm, KEYBIND_WM_SCRATCHPAD_TOGGLE, 0, 0,
            &surface, NULL, NULL);
    TAP_EQ_INT(s_call_scratchpad_toggle, 1,
            "KEYBIND_WM_SCRATCHPAD_TOGGLE reaches scratchpad_toggle" \
            " exactly once given a surface");

    s_reset();
    ik_execute_binding(s_fake_wm, KEYBIND_WM_SCRATCHPAD_TOGGLE, 0, 0,
            NULL, NULL, NULL);
    TAP_EQ_INT(s_call_scratchpad_toggle, 0,
            "a null surface guards KEYBIND_WM_SCRATCHPAD_TOGGLE from" \
            " reaching scratchpad_toggle");
}


static void s_test_desktop_clients_iconify_deiconify_rearrange(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_CLIENTS_ICONIFY_ALL, 0,
            0, &surface, NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_CLIENTS_DEICONIFY_ALL,
            0, 0, &surface, NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_CLIENTS_REARRANGE, 0,
            0, &surface, NULL, NULL);

    TAP_EQ_INT(s_call_enact_desktop_clients_iconify_all, 1,
            "KEYBIND_DESKTOP_CLIENTS_ICONIFY_ALL reaches" \
            " enact_desktop_clients_iconify_all exactly once");
    TAP_EQ_INT(s_call_enact_desktop_clients_deiconify_all, 1,
            "KEYBIND_DESKTOP_CLIENTS_DEICONIFY_ALL reaches" \
            " enact_desktop_clients_deiconify_all exactly once");
    TAP_EQ_INT(s_call_enact_desktop_clients_rearrange, 1,
            "KEYBIND_DESKTOP_CLIENTS_REARRANGE reaches" \
            " enact_desktop_clients_rearrange exactly once");
}


static void s_test_desktop_goto_n_passes_correct_index(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_GOTO_0, 0, 0, &surface,
            NULL, NULL);
    TAP_EQ_INT((int) s_last_desktop_switch_id, 0,
            "KEYBIND_DESKTOP_GOTO_0 passes desktop index 0 to" \
            " enact_surface_desktop_switch");

    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_GOTO_5, 0, 0, &surface,
            NULL, NULL);
    TAP_EQ_INT((int) s_last_desktop_switch_id, 5,
            "KEYBIND_DESKTOP_GOTO_5 passes desktop index 5 to" \
            " enact_surface_desktop_switch");

    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_GOTO_9, 0, 0, &surface,
            NULL, NULL);
    TAP_EQ_INT((int) s_last_desktop_switch_id, 9,
            "KEYBIND_DESKTOP_GOTO_9 passes desktop index 9 to" \
            " enact_surface_desktop_switch");

    TAP_EQ_INT(s_call_enact_surface_desktop_switch, 3,
            "all three KEYBIND_DESKTOP_GOTO_N scenarios above each" \
            " reached enact_surface_desktop_switch exactly once");
}


static void s_test_desktop_add_remove_and_strutless(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_ADD, 0, 0, &surface,
            NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_REMOVE, 0, 0, &surface,
            NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_WM_TOGGLE_STRUTLESS_MAXIMIZE, 0,
            0, &surface, NULL, NULL);

    TAP_EQ_INT(s_call_enact_surface_desktop_add, 1,
            "KEYBIND_DESKTOP_ADD reaches enact_surface_desktop_add" \
            " exactly once");
    TAP_EQ_INT(s_call_enact_surface_desktop_remove, 1,
            "KEYBIND_DESKTOP_REMOVE reaches" \
            " enact_surface_desktop_remove exactly once");
    TAP_EQ_INT(s_call_enact_surface_toggle_strutless_maximize, 1,
            "KEYBIND_WM_TOGGLE_STRUTLESS_MAXIMIZE reaches" \
            " enact_surface_toggle_strutless_maximize exactly once");
}


static void s_test_client_cycle_next_prev_and_icon_next_prev(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_lookup_desktop_result = &s_fixture_desktop;

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_CYCLE_NEXT, 0, 0,
            &surface, NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_CYCLE_PREV, 0, 0,
            &surface, NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_ICON_NEXT, 0, 0,
            &surface, NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_DESKTOP_ICON_PREV, 0, 0,
            &surface, NULL, NULL);

    TAP_EQ_INT(s_call_enact_desktop_cycle_clients_active, 1,
            "KEYBIND_CLIENT_CYCLE_NEXT reaches" \
            " enact_desktop_cycle_clients_active exactly once");
    TAP_EQ_INT(s_call_enact_desktop_cycle_clients_prev, 1,
            "KEYBIND_CLIENT_CYCLE_PREV reaches" \
            " enact_desktop_cycle_clients_prev exactly once");
    TAP_EQ_INT(s_call_enact_desktop_cycle_clients_icons_next, 1,
            "KEYBIND_DESKTOP_ICON_NEXT reaches" \
            " enact_desktop_cycle_clients_icons_next exactly once");
    TAP_EQ_INT(s_call_enact_desktop_cycle_clients_icons_prev, 1,
            "KEYBIND_DESKTOP_ICON_PREV reaches" \
            " enact_desktop_cycle_clients_icons_prev exactly once");
}


static void s_test_cycle_next_needs_a_resolvable_desktop(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_lookup_desktop_result = NULL;

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_CYCLE_NEXT, 0, 0,
            &surface, NULL, NULL);

    TAP_EQ_INT(s_call_enact_desktop_cycle_clients_active, 0,
            "a null lookup_current_desktop result guards" \
            " KEYBIND_CLIENT_CYCLE_NEXT from reaching" \
            " enact_desktop_cycle_clients_active");
}


static void s_test_emergency_exit_is_a_documented_noop(void)
{
    s_reset();

    ik_execute_binding(s_fake_wm, KEYBIND_WM_EMERGENCY_EXIT, 0, 0, NULL,
            NULL, NULL);

    TAP_OK(true,
            "KEYBIND_WM_EMERGENCY_EXIT falls straight through to" \
            " return, per execute.c's own comment that it is" \
            " \"already handled above via the hardcoded shortcut\"" \
            " in event.c, so no stand-in call is expected here at all");
}


static void s_test_fortune_needs_flag_surface_and_connection(void)
{
    surface_td surface;
    config_td cfg;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&cfg, 0, sizeof(cfg));
    xcb_connection_set(s_fake_connection);
    cfg.base.fortune.is_enabled = true;

    ik_execute_binding(s_fake_wm, KEYBIND_WM_FORTUNE, 0, 0, &surface,
            NULL, &cfg);
    TAP_EQ_INT(s_call_dialog_fortune_show, 1,
            "KEYBIND_WM_FORTUNE reaches dialog_fortune_show exactly" \
            " once when the fortune feature is enabled, a surface is" \
            " given, and a connection exists");

    s_reset();
    cfg.base.fortune.is_enabled = false;
    xcb_connection_set(s_fake_connection);
    ik_execute_binding(s_fake_wm, KEYBIND_WM_FORTUNE, 0, 0, &surface,
            NULL, &cfg);
    TAP_EQ_INT(s_call_dialog_fortune_show, 0,
            "KEYBIND_WM_FORTUNE never reaches dialog_fortune_show" \
            " while the fortune feature is disabled");

    xcb_connection_set(NULL);
}


static void s_test_redraw_reload_quit_shortcuts(void)
{
    surface_td surface;
    config_td cfg;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&cfg, 0, sizeof(cfg));
    xcb_connection_set(s_fake_connection);

    ik_execute_binding(s_fake_wm, KEYBIND_WM_REDRAW, 0, 0, &surface,
            NULL, &cfg);
    ik_execute_binding(s_fake_wm, KEYBIND_WM_RELOAD, 0, 0, &surface,
            NULL, &cfg);
    ik_execute_binding(s_fake_wm, KEYBIND_WM_QUIT, 0, 0, &surface,
            NULL, &cfg);
    ik_execute_binding(s_fake_wm, KEYBIND_WM_SHORTCUTS_LIST, 0, 0,
            &surface, NULL, &cfg);

    TAP_EQ_INT(s_call_wm_request_full_redraw, 1,
            "KEYBIND_WM_REDRAW reaches wm_request_full_redraw exactly" \
            " once");
    TAP_EQ_INT(s_call_wm_action_config_reload, 1,
            "KEYBIND_WM_RELOAD reaches wm_action_config_reload" \
            " exactly once");
    TAP_EQ_INT(s_call_dialog_quit_show, 1,
            "KEYBIND_WM_QUIT reaches dialog_quit_show exactly once");
    TAP_EQ_INT(s_call_dialog_shortcuts_show, 1,
            "KEYBIND_WM_SHORTCUTS_LIST reaches" \
            " dialog_shortcuts_show exactly once");

    xcb_connection_set(NULL);
}


static void s_test_root_menu_center_position(void)
{
    surface_td surface;
    config_td cfg;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&cfg, 0, sizeof(cfg));
    surface.properties.dim.w = 800;
    surface.properties.dim.h = 600;
    xcb_connection_set(s_fake_connection);
    cfg.base.menus.root.position = CONFIG_MENU_POSITION_CENTER;
    s_lookup_desktop_result = NULL;

    ik_execute_binding(s_fake_wm, KEYBIND_WM_ROOT_MENU, 0, 0, &surface,
            NULL, &cfg);

    TAP_EQ_INT(s_call_rootmenu_show, 1,
            "KEYBIND_WM_ROOT_MENU reaches rootmenu_show exactly once" \
            " given a surface and a connection");

    xcb_connection_set(NULL);
}


static void s_test_windows_menu_needs_surface_and_connection(void)
{
    surface_td surface;
    config_td cfg;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&cfg, 0, sizeof(cfg));

    ik_execute_binding(s_fake_wm, KEYBIND_WM_WINDOWS_MENU, 0, 0,
            &surface, NULL, &cfg);
    TAP_EQ_INT(s_call_winlist_show, 0,
            "KEYBIND_WM_WINDOWS_MENU never reaches winlist_show" \
            " while xcb_connection_get returns null");

    xcb_connection_set(s_fake_connection);
    ik_execute_binding(s_fake_wm, KEYBIND_WM_WINDOWS_MENU, 0, 0,
            &surface, NULL, &cfg);
    TAP_EQ_INT(s_call_winlist_show, 1,
            "KEYBIND_WM_WINDOWS_MENU reaches winlist_show exactly" \
            " once given a surface and a connection");

    xcb_connection_set(NULL);
}


static void s_test_root_menu_under_mouse_falls_back_on_null_reply(void)
{
    surface_td surface;
    config_td cfg;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&cfg, 0, sizeof(cfg));
    surface.screen = NULL;
    xcb_connection_set(s_fake_connection);
    cfg.base.menus.root.position = CONFIG_MENU_POSITION_UNDER_MOUSE;
    s_query_pointer_reply_is_null = true;

    ik_execute_binding(s_fake_wm, KEYBIND_WM_ROOT_MENU, 0, 0, &surface,
            NULL, &cfg);

    TAP_EQ_INT(s_call_rootmenu_show, 1,
            "KEYBIND_WM_ROOT_MENU under CONFIG_MENU_POSITION_UNDER_" \
            "MOUSE with a null surface->screen skips the pointer" \
            " query entirely and still reaches rootmenu_show exactly" \
            " once, falling back to the surface center");

    xcb_connection_set(NULL);
}


static void s_test_search_windows_needs_surface_and_connection(void)
{
    surface_td surface;
    config_td cfg;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&cfg, 0, sizeof(cfg));

    ik_execute_binding(s_fake_wm, KEYBIND_WM_SEARCH_WINDOWS, 0, 0,
            &surface, NULL, &cfg);
    TAP_EQ_INT(s_call_search_init, 0,
            "KEYBIND_WM_SEARCH_WINDOWS never reaches search_init" \
            " while xcb_connection_get returns null");

    xcb_connection_set(s_fake_connection);
    ik_execute_binding(s_fake_wm, KEYBIND_WM_SEARCH_WINDOWS, 0, 0,
            &surface, NULL, &cfg);
    TAP_EQ_INT(s_call_search_init, 1,
            "KEYBIND_WM_SEARCH_WINDOWS reaches search_init exactly" \
            " once given a surface and a connection");

    xcb_connection_set(NULL);
}


static void s_test_client_window_menu_needs_active_client(void)
{
    surface_td surface;
    config_td cfg;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&cfg, 0, sizeof(cfg));
    xcb_connection_set(s_fake_connection);
    s_active_client_result = NULL;

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_WINDOW_MENU, 0, 0,
            &surface, NULL, &cfg);
    TAP_EQ_INT(s_call_wincmenu_show, 0,
            "KEYBIND_CLIENT_WINDOW_MENU never reaches wincmenu_show" \
            " without a resolvable active client");

    s_give_active_client();
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_WINDOW_MENU, 0, 0,
            &surface, NULL, &cfg);
    TAP_EQ_INT(s_call_wincmenu_show, 1,
            "KEYBIND_CLIENT_WINDOW_MENU reaches wincmenu_show exactly" \
            " once given a resolvable active client, a surface, and a" \
            " connection");

    xcb_connection_set(NULL);
}


/* KEYBIND_CLIENT_* family routed through s_dispatch_client_action:
 * exercised only through ik_execute_binding since the helper itself
 * is file-static in execute.c */

static void s_test_client_family_no_surface_is_noop(void)
{
    s_reset();

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_ICONIFY, 0, 0, NULL,
            NULL, NULL);

    TAP_EQ_INT(s_call_enact_client_iconify, 0,
            "s_dispatch_client_action's null-surface guard keeps" \
            " KEYBIND_CLIENT_ICONIFY from reaching" \
            " enact_client_iconify");
}


static void s_test_client_family_no_desktop_is_noop(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_lookup_desktop_result = NULL;
    s_active_client_result = &s_fixture_client;

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_ICONIFY, 0, 0, &surface,
            NULL, NULL);

    TAP_EQ_INT(s_call_enact_client_iconify, 0,
            "s_dispatch_client_action's null-desktop guard keeps" \
            " KEYBIND_CLIENT_ICONIFY from reaching" \
            " enact_client_iconify when lookup_current_desktop" \
            " returns null");
}


static void s_test_client_family_no_active_client_is_noop(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_lookup_desktop_result = &s_fixture_desktop;
    s_active_client_result = NULL;

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_ICONIFY, 0, 0, &surface,
            NULL, NULL);

    TAP_EQ_INT(s_call_enact_client_iconify, 0,
            "s_dispatch_client_action's null-active-client guard" \
            " keeps KEYBIND_CLIENT_ICONIFY from reaching" \
            " enact_client_iconify when no client is focused");
}


static void s_test_client_iconify_hide_close_kill(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_give_active_client();

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_ICONIFY, 0, 0, &surface,
            NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_HIDE, 0, 0, &surface,
            NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_CLOSE, 0, 0, &surface,
            NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_KILL, 0, 0, &surface,
            NULL, NULL);

    TAP_EQ_INT(s_call_enact_client_iconify, 1,
            "KEYBIND_CLIENT_ICONIFY reaches enact_client_iconify" \
            " exactly once");
    TAP_EQ_INT(s_call_enact_client_hide, 1,
            "KEYBIND_CLIENT_HIDE reaches enact_client_hide exactly" \
            " once");
    TAP_EQ_INT(s_call_enact_client_close, 1,
            "KEYBIND_CLIENT_CLOSE reaches enact_client_close exactly" \
            " once");
    TAP_EQ_INT(s_call_enact_client_kill, 1,
            "KEYBIND_CLIENT_KILL reaches enact_client_kill exactly" \
            " once");
}


static void s_test_client_maximize_needs_maximizable(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_give_active_client();
    s_fixture_client.properties.flags = 0;

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MAXIMIZE, 0, 0, &surface,
            NULL, NULL);
    TAP_EQ_INT(s_call_enact_client_maximize, 0,
            "a non-resizable client guards KEYBIND_CLIENT_MAXIMIZE" \
            " from reaching enact_client_maximize" \
            " (client_is_maximizable requires resizable and non-modal)");

    s_fixture_client.properties.flags = CLIENT_FLAG_RESIZABLE;
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MAXIMIZE, 0, 0, &surface,
            NULL, NULL);
    TAP_EQ_INT(s_call_enact_client_maximize, 1,
            "a resizable, non-modal client lets KEYBIND_CLIENT_" \
            "MAXIMIZE reach enact_client_maximize exactly once");

    s_fixture_client.properties.flags =
            CLIENT_FLAG_RESIZABLE | CLIENT_FLAG_MODAL;
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MAXIMIZE, 0, 0, &surface,
            NULL, NULL);
    TAP_EQ_INT(s_call_enact_client_maximize, 1,
            "a modal client, even if resizable, still guards" \
            " KEYBIND_CLIENT_MAXIMIZE from reaching" \
            " enact_client_maximize a second time");
}


static void s_test_client_center_and_move_monitor_family(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_give_active_client();

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_CENTER, 0, 0, &surface,
            NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MOVE_MONITOR_NORTH, 0, 0,
            &surface, NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MOVE_MONITOR_SOUTH, 0, 0,
            &surface, NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MOVE_MONITOR_EAST, 0, 0,
            &surface, NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MOVE_MONITOR_WEST, 0, 0,
            &surface, NULL, NULL);

    TAP_EQ_INT(s_call_enact_client_center, 1,
            "KEYBIND_CLIENT_CENTER reaches enact_client_center" \
            " exactly once");
    TAP_EQ_INT(s_call_enact_client_move_monitor_north, 1,
            "KEYBIND_CLIENT_MOVE_MONITOR_NORTH reaches" \
            " enact_client_move_monitor_north exactly once");
    TAP_EQ_INT(s_call_enact_client_move_monitor_south, 1,
            "KEYBIND_CLIENT_MOVE_MONITOR_SOUTH reaches" \
            " enact_client_move_monitor_south exactly once");
    TAP_EQ_INT(s_call_enact_client_move_monitor_east, 1,
            "KEYBIND_CLIENT_MOVE_MONITOR_EAST reaches" \
            " enact_client_move_monitor_east exactly once");
    TAP_EQ_INT(s_call_enact_client_move_monitor_west, 1,
            "KEYBIND_CLIENT_MOVE_MONITOR_WEST reaches" \
            " enact_client_move_monitor_west exactly once");
}


static void s_test_client_send_to_desktop_family(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_give_active_client();

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_SEND_TO_DESKTOP_NORTH, 0,
            0, &surface, NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_SEND_TO_DESKTOP_SOUTH, 0,
            0, &surface, NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_SEND_TO_DESKTOP_EAST, 0,
            0, &surface, NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_SEND_TO_DESKTOP_WEST, 0,
            0, &surface, NULL, NULL);

    TAP_EQ_INT(s_call_enact_client_send_to_desktop_north, 1,
            "KEYBIND_CLIENT_SEND_TO_DESKTOP_NORTH reaches" \
            " enact_client_send_to_desktop_north exactly once");
    TAP_EQ_INT(s_call_enact_client_send_to_desktop_south, 1,
            "KEYBIND_CLIENT_SEND_TO_DESKTOP_SOUTH reaches" \
            " enact_client_send_to_desktop_south exactly once");
    TAP_EQ_INT(s_call_enact_client_send_to_desktop_east, 1,
            "KEYBIND_CLIENT_SEND_TO_DESKTOP_EAST reaches" \
            " enact_client_send_to_desktop_east exactly once");
    TAP_EQ_INT(s_call_enact_client_send_to_desktop_west, 1,
            "KEYBIND_CLIENT_SEND_TO_DESKTOP_WEST reaches" \
            " enact_client_send_to_desktop_west exactly once");
}


static void s_test_client_shade_pin(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_give_active_client();

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_SHADE, 0, 0, &surface,
            NULL, NULL);
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_PIN, 0, 0, &surface,
            NULL, NULL);

    TAP_EQ_INT(s_call_enact_client_toggle_shade, 1,
            "KEYBIND_CLIENT_SHADE reaches enact_client_toggle_shade" \
            " exactly once");
    TAP_EQ_INT(s_call_enact_client_toggle_pin, 1,
            "KEYBIND_CLIENT_PIN reaches enact_client_toggle_pin" \
            " exactly once");
}


static void s_test_client_fullscreen_blocks_non_resizable_or_modal(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_give_active_client();
    s_fixture_client.properties.flags = 0;
    s_fixture_client.properties.state = 0;

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_FULLSCREEN, 0, 0,
            &surface, NULL, NULL);
    TAP_EQ_INT(s_call_enact_client_toggle_fullscreen, 0,
            "a non-resizable, non-fullscreen client guards" \
            " KEYBIND_CLIENT_FULLSCREEN from reaching" \
            " enact_client_toggle_fullscreen");

    s_fixture_client.properties.flags =
            CLIENT_FLAG_RESIZABLE | CLIENT_FLAG_MODAL;
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_FULLSCREEN, 0, 0,
            &surface, NULL, NULL);
    TAP_EQ_INT(s_call_enact_client_toggle_fullscreen, 0,
            "a resizable but modal, non-fullscreen client still" \
            " guards KEYBIND_CLIENT_FULLSCREEN from reaching" \
            " enact_client_toggle_fullscreen");

    s_fixture_client.properties.flags = CLIENT_FLAG_RESIZABLE;
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_FULLSCREEN, 0, 0,
            &surface, NULL, NULL);
    TAP_EQ_INT(s_call_enact_client_toggle_fullscreen, 1,
            "a resizable, non-modal, non-fullscreen client lets" \
            " KEYBIND_CLIENT_FULLSCREEN reach" \
            " enact_client_toggle_fullscreen exactly once");
}


static void s_test_client_fullscreen_exit_ignores_resizable_flag(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_give_active_client();
    s_fixture_client.properties.flags = CLIENT_FLAG_MODAL;
    s_fixture_client.properties.state = CLIENT_STATE_FULLSCREEN;

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_FULLSCREEN, 0, 0,
            &surface, NULL, NULL);

    TAP_EQ_INT(s_call_enact_client_toggle_fullscreen, 1,
            "a client already fullscreen (via its own EWMH request)" \
            " still lets KEYBIND_CLIENT_FULLSCREEN reach" \
            " enact_client_toggle_fullscreen exactly once, even while" \
            " modal and not resizable, since exiting stays ungated" \
            " on purpose per execute.c's own comment");
}


static void s_test_client_toggle_decoration_unshades_first_if_shaded(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_give_active_client();
    s_fixture_client.properties.flags = 0;

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_TOGGLE_DECORATION, 0, 0,
            &surface, NULL, NULL);
    TAP_EQ_INT(s_call_ccmd_client_unshade, 0,
            "an unshaded client never reaches ccmd_client_unshade" \
            " through KEYBIND_CLIENT_TOGGLE_DECORATION");
    TAP_EQ_INT(s_call_enact_client_toggle_decorate, 1,
            "KEYBIND_CLIENT_TOGGLE_DECORATION always reaches" \
            " enact_client_toggle_decorate exactly once regardless" \
            " of shaded state");

    s_reset();
    s_give_active_client();
    s_fixture_client.properties.flags = CLIENT_FLAG_SHADED;
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_TOGGLE_DECORATION, 0, 0,
            &surface, NULL, NULL);
    TAP_EQ_INT(s_call_ccmd_client_unshade, 1,
            "a shaded client reaches ccmd_client_unshade exactly" \
            " once, before enact_client_toggle_decorate, through" \
            " KEYBIND_CLIENT_TOGGLE_DECORATION");
    TAP_EQ_INT(s_call_enact_client_toggle_decorate, 1,
            "KEYBIND_CLIENT_TOGGLE_DECORATION still reaches" \
            " enact_client_toggle_decorate exactly once after" \
            " unshading");
}


static void s_test_client_cycle_layer_blocks_fullscreen(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_give_active_client();
    s_fixture_client.properties.state = CLIENT_STATE_FULLSCREEN;

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_CYCLE_LAYER, 0, 0,
            &surface, NULL, NULL);
    TAP_EQ_INT(s_call_enact_client_cycle_layer, 0,
            "a fullscreen client guards KEYBIND_CLIENT_CYCLE_LAYER" \
            " from reaching enact_client_cycle_layer");

    s_fixture_client.properties.state = 0;
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_CYCLE_LAYER, 0, 0,
            &surface, NULL, NULL);
    TAP_EQ_INT(s_call_enact_client_cycle_layer, 1,
            "a non-fullscreen client lets KEYBIND_CLIENT_CYCLE_LAYER" \
            " reach enact_client_cycle_layer exactly once");
}


static void s_test_client_info_and_inspect(void)
{
    surface_td surface;
    config_td cfg;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&cfg, 0, sizeof(cfg));
    s_give_active_client();
    xcb_connection_set(s_fake_connection);

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_INFO, 0, 0, &surface,
            NULL, &cfg);
    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_INSPECT, 0, 0, &surface,
            NULL, &cfg);

    TAP_EQ_INT(s_call_popup_show, 1,
            "KEYBIND_CLIENT_INFO reaches popup_show exactly once");
    TAP_EQ_INT(s_call_dialog_inspect_show, 1,
            "KEYBIND_CLIENT_INSPECT reaches dialog_inspect_show" \
            " exactly once");

    xcb_connection_set(NULL);
}


/* KEYBIND_LAUNCH_*: routed straight to ik_handle_launch */

static void s_test_launch_family_passes_correct_program(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    ik_execute_binding(s_fake_wm, KEYBIND_LAUNCH_TERMINAL, 0, 0, &surface,
            NULL, NULL);
    TAP_OK(s_last_launch_program == IK_LAUNCH_TERMINAL,
            "KEYBIND_LAUNCH_TERMINAL passes IK_LAUNCH_TERMINAL to" \
            " ik_handle_launch");

    ik_execute_binding(s_fake_wm, KEYBIND_LAUNCH_LAUNCHER, 0, 0, &surface,
            NULL, NULL);
    TAP_OK(s_last_launch_program == IK_LAUNCH_LAUNCHER,
            "KEYBIND_LAUNCH_LAUNCHER passes IK_LAUNCH_LAUNCHER to" \
            " ik_handle_launch");

    ik_execute_binding(s_fake_wm, KEYBIND_LAUNCH_FILE_MANAGER, 0, 0,
            &surface, NULL, NULL);
    TAP_OK(s_last_launch_program == IK_LAUNCH_FILE_MANAGER,
            "KEYBIND_LAUNCH_FILE_MANAGER passes" \
            " IK_LAUNCH_FILE_MANAGER to ik_handle_launch");

    ik_execute_binding(s_fake_wm, KEYBIND_LAUNCH_WEB_BROWSER, 0, 0,
            &surface, NULL, NULL);
    TAP_OK(s_last_launch_program == IK_LAUNCH_WEB_BROWSER,
            "KEYBIND_LAUNCH_WEB_BROWSER passes IK_LAUNCH_WEB_BROWSER" \
            " to ik_handle_launch");

    ik_execute_binding(s_fake_wm, KEYBIND_LAUNCH_EDITOR, 0, 0, &surface,
            NULL, NULL);
    TAP_OK(s_last_launch_program == IK_LAUNCH_EDITOR,
            "KEYBIND_LAUNCH_EDITOR passes IK_LAUNCH_EDITOR to" \
            " ik_handle_launch");

    TAP_EQ_INT(s_call_ik_handle_launch, 5,
            "all five KEYBIND_LAUNCH_* scenarios above each reached" \
            " ik_handle_launch exactly once");
}


/* KEYBIND_CLIENT_MOVE_*: routed straight to ik_handle_move */

static void s_test_move_family_passes_correct_direction(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MOVE_LEFT, 0, 0,
            &surface, NULL, NULL);
    TAP_OK(s_last_move_direction == IK_MOVE_LEFT,
            "KEYBIND_CLIENT_MOVE_LEFT passes IK_MOVE_LEFT to" \
            " ik_handle_move");

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MOVE_RIGHT, 0, 0,
            &surface, NULL, NULL);
    TAP_OK(s_last_move_direction == IK_MOVE_RIGHT,
            "KEYBIND_CLIENT_MOVE_RIGHT passes IK_MOVE_RIGHT to" \
            " ik_handle_move");

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MOVE_UP, 0, 0, &surface,
            NULL, NULL);
    TAP_OK(s_last_move_direction == IK_MOVE_UP,
            "KEYBIND_CLIENT_MOVE_UP passes IK_MOVE_UP to" \
            " ik_handle_move");

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MOVE_DOWN, 0, 0,
            &surface, NULL, NULL);
    TAP_OK(s_last_move_direction == IK_MOVE_DOWN,
            "KEYBIND_CLIENT_MOVE_DOWN passes IK_MOVE_DOWN to" \
            " ik_handle_move");

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MOVE_TOP_LEFT, 0, 0,
            &surface, NULL, NULL);
    TAP_OK(s_last_move_direction == IK_MOVE_TOP_LEFT,
            "KEYBIND_CLIENT_MOVE_TOP_LEFT passes IK_MOVE_TOP_LEFT to" \
            " ik_handle_move");

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MOVE_TOP_RIGHT, 0, 0,
            &surface, NULL, NULL);
    TAP_OK(s_last_move_direction == IK_MOVE_TOP_RIGHT,
            "KEYBIND_CLIENT_MOVE_TOP_RIGHT passes IK_MOVE_TOP_RIGHT" \
            " to ik_handle_move");

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MOVE_BOTTOM_LEFT, 0, 0,
            &surface, NULL, NULL);
    TAP_OK(s_last_move_direction == IK_MOVE_BOTTOM_LEFT,
            "KEYBIND_CLIENT_MOVE_BOTTOM_LEFT passes" \
            " IK_MOVE_BOTTOM_LEFT to ik_handle_move");

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT, 0, 0,
            &surface, NULL, NULL);
    TAP_OK(s_last_move_direction == IK_MOVE_BOTTOM_RIGHT,
            "KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT passes" \
            " IK_MOVE_BOTTOM_RIGHT to ik_handle_move");

    TAP_EQ_INT(s_call_ik_handle_move, 8,
            "all eight KEYBIND_CLIENT_MOVE_* scenarios above each" \
            " reached ik_handle_move exactly once");
}


/* KEYBIND_CLIENT_RESIZE_*: routed straight to ik_handle_resize */

static void s_test_resize_family_passes_correct_edge(void)
{
    surface_td surface;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_RESIZE_LEFT, 0, 0,
            &surface, NULL, NULL);
    TAP_OK(s_last_resize_edge == IK_RESIZE_LEFT,
            "KEYBIND_CLIENT_RESIZE_LEFT passes IK_RESIZE_LEFT to" \
            " ik_handle_resize");

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_RESIZE_RIGHT, 0, 0,
            &surface, NULL, NULL);
    TAP_OK(s_last_resize_edge == IK_RESIZE_RIGHT,
            "KEYBIND_CLIENT_RESIZE_RIGHT passes IK_RESIZE_RIGHT to" \
            " ik_handle_resize");

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_RESIZE_UP, 0, 0,
            &surface, NULL, NULL);
    TAP_OK(s_last_resize_edge == IK_RESIZE_UP,
            "KEYBIND_CLIENT_RESIZE_UP passes IK_RESIZE_UP to" \
            " ik_handle_resize");

    ik_execute_binding(s_fake_wm, KEYBIND_CLIENT_RESIZE_DOWN, 0, 0,
            &surface, NULL, NULL);
    TAP_OK(s_last_resize_edge == IK_RESIZE_DOWN,
            "KEYBIND_CLIENT_RESIZE_DOWN passes IK_RESIZE_DOWN to" \
            " ik_handle_resize");

    TAP_EQ_INT(s_call_ik_handle_resize, 4,
            "all four KEYBIND_CLIENT_RESIZE_* scenarios above each" \
            " reached ik_handle_resize exactly once");
}


/* KEYBIND_NONE: logged and ignored */

static void s_test_keybind_none_is_a_logged_noop(void)
{
    s_reset();

    ik_execute_binding(s_fake_wm, KEYBIND_NONE, 0, 0, NULL, NULL, NULL);

    TAP_OK(true,
            "KEYBIND_NONE reaches only its own LOGGER_TRACE call and" \
            " returns, exercising no stand-in at all, which this" \
            " assertion documents explicitly rather than leaving the" \
            " case entirely unexercised");
}


int main(void)
{
    TAP_PLAN(103);

    s_test_desktop_north_south_east_west();
    s_test_desktop_north_null_surface_is_noop();
    s_test_viewport_pan_north_south_east_west();
    s_test_viewport_pan_north_null_surface_is_noop();
    s_test_viewport_goto_n_passes_correct_page();
    s_test_viewport_goto_1_null_surface_is_noop();
    s_test_desktop_show_toggles_the_flag();
    s_test_scratchpad_toggle_needs_a_surface();
    s_test_desktop_clients_iconify_deiconify_rearrange();
    s_test_desktop_goto_n_passes_correct_index();
    s_test_desktop_add_remove_and_strutless();
    s_test_client_cycle_next_prev_and_icon_next_prev();
    s_test_cycle_next_needs_a_resolvable_desktop();
    s_test_emergency_exit_is_a_documented_noop();
    s_test_fortune_needs_flag_surface_and_connection();
    s_test_redraw_reload_quit_shortcuts();
    s_test_root_menu_center_position();
    s_test_windows_menu_needs_surface_and_connection();
    s_test_root_menu_under_mouse_falls_back_on_null_reply();
    s_test_search_windows_needs_surface_and_connection();
    s_test_client_window_menu_needs_active_client();

    s_test_client_family_no_surface_is_noop();
    s_test_client_family_no_desktop_is_noop();
    s_test_client_family_no_active_client_is_noop();
    s_test_client_iconify_hide_close_kill();
    s_test_client_maximize_needs_maximizable();
    s_test_client_center_and_move_monitor_family();
    s_test_client_send_to_desktop_family();
    s_test_client_shade_pin();
    s_test_client_fullscreen_blocks_non_resizable_or_modal();
    s_test_client_fullscreen_exit_ignores_resizable_flag();
    s_test_client_toggle_decoration_unshades_first_if_shaded();
    s_test_client_cycle_layer_blocks_fullscreen();
    s_test_client_info_and_inspect();

    s_test_launch_family_passes_correct_program();
    s_test_move_family_passes_correct_direction();
    s_test_resize_family_passes_correct_edge();

    s_test_keybind_none_is_a_logged_noop();

    return TAP_DONE();
}
