/**
 * @file tests/wm/test_ewmh.c
 *
 * @brief Test battery for root-level EWMH initialization and
 *        synchronization (wm/ewmh.c)
 *
 * Both public entry points, wm_ewmh_init and wm_ewmh_sync, are
 * exercised through the real source file, linked directly, together
 * with the real src/wm/instance.c narrow accessors it calls through
 * (wm_connection, wm_ewmh, wm_stages, wm_ewmh_support_win,
 * wm_sync_available) and the real src/adt/list.c the stages list
 * itself is built from.  Every genuine libxcb/libxcb-ewmh call
 * reached from wm/ewmh.c (xcb_change_property, xcb_flush,
 * xcb_ewmh_set_wm_name, xcb_ewmh_set_supported,
 * xcb_ewmh_set_supporting_wm_check, xcb_ewmh_set_number_of_desktops,
 * xcb_ewmh_set_current_desktop, xcb_ewmh_set_desktop_geometry,
 * xcb_ewmh_set_desktop_viewport, xcb_ewmh_set_active_window,
 * xcb_ewmh_set_showing_desktop, xcb_ewmh_set_desktop_names,
 * xcb_ewmh_set_workarea, xcb_ewmh_set_client_list,
 * xcb_ewmh_set_client_list_stacking) is a recording stand-in rather
 * than the real library function, the same reasoning
 * tests/cmds/client/test_ewmh.c already gives for doing the same:
 * this project never opens a real X connection in a unit test, so
 * every one of these would otherwise either hang or fail outright.
 * 'xcb_ewmh_connection_get' and 'xcb_connection_get' are test-
 * controlled stand-ins answering the address of a real, fully-
 * initialized 'xcb_ewmh_connection_t' (a plain, non-opaque struct)
 * and a fixed non-null, never-dereferenced connection handle,
 * respectively.  'atom_intern' is a recording stand-in answering a
 * fixed, recognizable, distinct atom per name so a test can tell
 * exactly which of the several one-off atoms wm_ewmh_init interns
 * ended up in the supported-atoms list.  'stage_desktop_walk_all',
 * 'stacking_walk', and 'stacking_count' are test-controlled stand-ins
 * that actually walk a plain array of desktop_td/client_td pointers a
 * test registers beforehand (rather than the real cdlist/ohtbl
 * machinery those real implementations use), since what is under
 * test here is wm/ewmh.c's own per-desktop and per-client visitor
 * logic and its consumption of what the walk it are given, not the
 * container walk itself, already covered elsewhere (e.g.,
 * tests/wm/test_clients.c for wm_for_each_client's own cdlist/ohtbl
 * walk).  'stage_desktop_get' is a test-controlled stand-in
 * answering whichever desktop_td a test registers for a given id.
 * 'lookup_find_client' is a test-controlled stand-in answering
 * whichever client_td a test registers, mirroring the same pattern
 * tests/wm/test_lifecycle.c already uses for it.
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
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <defs/desktop.h>
#include <defs/ewmh.h>
#include <desktop.h>
#include <lookup.h>
#include <policy/stacking.h>
#include <stage.h>
#include <wm.h>
#include <wm/ewmh.h>
#include <wm/internal.h>

/* Local includes */
#include <harness/tap.h>


/** Real, fully-initialized EWMH connection this file's own
 *  @a xcb_ewmh_connection_get stand-in answers, filled with distinct,
 *  recognizable atom numbers so a test can tell exactly which one a
 *  call published */
static xcb_ewmh_connection_t s_ewmh;
static bool s_ewmh_present;


/**
 * @brief Test-controlled stand-in for @a xcb_ewmh_connection_get
 * @note Complexity: @e O(1)
 */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return (s_ewmh_present) ? &s_ewmh : NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_connection_get
 *
 * Answers a fixed non-null, never-dereferenced value: nothing in
 * this file ever reads through the pointer itself, since every
 * genuine libxcb call reached through it is itself a stand-in
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return (xcb_connection_t *) (uintptr_t) 1;
}


/** Recorded name/count of every @a atom_intern call */
static char s_last_interned_name[64];
static int s_intern_count;


/**
 * @brief Test-controlled, recording stand-in for @a atom_intern
 *
 * Answers a fixed, distinct, recognizable atom per name, none of
 * which collides with any field @a s_ewmh itself carries
 *
 * @note Complexity: @e O(1)
 */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    (void) connection;
    (void) only_if_exists;

    strncpy(s_last_interned_name, name, sizeof(s_last_interned_name) - 1);
    s_last_interned_name[sizeof(s_last_interned_name) - 1] = '\0';
    s_intern_count++;

    if (strcmp(name, "_NET_WM_STATE_FOCUSED") == 0) {
        return (xcb_atom_t) 9001u;
    }
    if (strcmp(name, "_NET_WM_WINDOW_TYPE_NOTIFICATION") == 0) {
        return (xcb_atom_t) 9002u;
    }
    if (strcmp(name, "_NET_WM_ICON_GEOMETRY") == 0) {
        return (xcb_atom_t) 9003u;
    }
    if (strcmp(name, "WM_ICON_SIZE") == 0) {
        return (xcb_atom_t) 9004u;
    }
    if (strcmp(name, "_NET_RESTACK_WINDOW") == 0) {
        return (xcb_atom_t) 9005u;
    }
    if (strcmp(name, "_NET_WM_FULLSCREEN_MONITORS") == 0) {
        return (xcb_atom_t) 9006u;
    }
    if (strcmp(name, "_NET_WM_MOVERESIZE") == 0) {
        return (xcb_atom_t) 9007u;
    }

    return XCB_ATOM_NONE;
}


/** Recorded arguments from the last @a xcb_change_property call, and
 *  a running count of how many happened in total */
static xcb_window_t s_cp_window;
static xcb_atom_t s_cp_property;
static xcb_atom_t s_cp_type;
static uint32_t s_cp_data_len;
static uint32_t s_cp_data[8];
static int s_cp_count;


/**
 * @brief Recording stand-in for @a xcb_change_property
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_change_property(xcb_connection_t *connection,
        uint8_t mode, xcb_window_t window, xcb_atom_t property,
        xcb_atom_t type, uint8_t format, uint32_t data_len,
        const void *data)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};
    uint32_t i;

    (void) connection;
    (void) mode;
    (void) format;

    s_cp_window = window;
    s_cp_property = property;
    s_cp_type = type;
    s_cp_data_len = data_len;
    memset(s_cp_data, 0, sizeof(s_cp_data));
    for (i = 0u; i < data_len && i < 8u; i++) {
        s_cp_data[i] = ((const uint32_t *) data)[i];
    }
    s_cp_count++;

    return cookie;
}


/** How many times @a xcb_flush was called */
static int s_flush_count;


/**
 * @brief Recording stand-in for @a xcb_flush
 * @note Complexity: @e O(1)
 */
int xcb_flush(xcb_connection_t *c)
{
    (void) c;
    s_flush_count++;
    return 1;
}


/** Recorded arguments from the last @a xcb_ewmh_set_wm_name call */
static xcb_window_t s_wmname_window;
static char s_wmname_value[64];
static int s_wmname_count;


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_wm_name
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_wm_name(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, uint32_t strings_len, const char *strings)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};
    uint32_t copy_len = strings_len;

    (void) ewmh;

    s_wmname_window = window;
    if (copy_len >= sizeof(s_wmname_value)) {
        copy_len = sizeof(s_wmname_value) - 1u;
    }
    memcpy(s_wmname_value, strings, copy_len);
    s_wmname_value[copy_len] = '\0';
    s_wmname_count++;

    return cookie;
}


/** Recorded arguments from the last @a xcb_ewmh_set_supported call */
static uint32_t s_supported_len;
static xcb_atom_t s_supported_list[WM_EWMH_SUPPORTED_COUNT];
static int s_supported_count;


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_supported
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_supported(xcb_ewmh_connection_t *ewmh,
        int screen_nbr, uint32_t list_len, xcb_atom_t *list)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};
    uint32_t i;

    (void) ewmh;
    (void) screen_nbr;

    s_supported_len = list_len;
    memset(s_supported_list, 0, sizeof(s_supported_list));
    for (i = 0u; i < list_len && i < WM_EWMH_SUPPORTED_COUNT; i++) {
        s_supported_list[i] = list[i];
    }
    s_supported_count++;

    return cookie;
}


static bool s_supported_has(xcb_atom_t atom)
{
    uint32_t i;

    for (i = 0u; i < s_supported_len; i++) {
        if (s_supported_list[i] == atom) {
            return true;
        }
    }
    return false;
}


/** Recorded arguments from the last @a xcb_ewmh_set_supporting_wm_check
 *  call */
static xcb_window_t s_swc_parent;
static xcb_window_t s_swc_child;
static int s_swc_count;


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_supporting_wm_check
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_supporting_wm_check(
        xcb_ewmh_connection_t *ewmh, xcb_window_t parent_window,
        xcb_window_t child_window)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};

    (void) ewmh;

    s_swc_parent = parent_window;
    s_swc_child = child_window;
    s_swc_count++;

    return cookie;
}


/** Recorded arguments from the last @a xcb_ewmh_set_number_of_desktops
 *  call */
static uint32_t s_nod_value;
static int s_nod_count;


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_number_of_desktops
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_number_of_desktops(
        xcb_ewmh_connection_t *ewmh, int screen_nbr,
        uint32_t number_of_desktops)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};

    (void) ewmh;
    (void) screen_nbr;

    s_nod_value = number_of_desktops;
    s_nod_count++;

    return cookie;
}


/** Recorded arguments from the last @a xcb_ewmh_set_current_desktop
 *  call */
static uint32_t s_cd_value;
static int s_cd_count;


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_current_desktop
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_current_desktop(xcb_ewmh_connection_t *ewmh,
        int screen_nbr, uint32_t new_current_desktop)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};

    (void) ewmh;
    (void) screen_nbr;

    s_cd_value = new_current_desktop;
    s_cd_count++;

    return cookie;
}


/** Recorded arguments from the last @a xcb_ewmh_set_desktop_geometry
 *  call */
static uint32_t s_dg_width;
static uint32_t s_dg_height;
static int s_dg_count;


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_desktop_geometry
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_desktop_geometry(xcb_ewmh_connection_t *ewmh,
        int screen_nbr, uint32_t new_width, uint32_t new_height)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};

    (void) ewmh;
    (void) screen_nbr;

    s_dg_width = new_width;
    s_dg_height = new_height;
    s_dg_count++;

    return cookie;
}


/** How many times @a xcb_ewmh_set_desktop_viewport was called, and the
 *  list length it was last given */
static uint32_t s_dv_len;
static int s_dv_count;

/** A copy of the per-desktop coordinates last handed to
 *  @a xcb_ewmh_set_desktop_viewport, for tests that verify real
 *  viewport origins get published rather than an all-zero list */
static xcb_ewmh_coordinates_t s_dv_last[8];


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_desktop_viewport
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_desktop_viewport(xcb_ewmh_connection_t *ewmh,
        int screen_nbr, uint32_t list_len, xcb_ewmh_coordinates_t *list)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};
    uint32_t i;

    (void) ewmh;
    (void) screen_nbr;

    s_dv_len = list_len;
    s_dv_count++;
    for (i = 0; i < list_len && i < 8u && list != NULL; i++) {
        s_dv_last[i] = list[i];
    }

    return cookie;
}


/** Recorded arguments from the last @a xcb_ewmh_set_active_window
 *  call */
static xcb_window_t s_aw_value;
static int s_aw_count;


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_active_window
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_active_window(xcb_ewmh_connection_t *ewmh,
        int screen_nbr, xcb_window_t new_active_window)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};

    (void) ewmh;
    (void) screen_nbr;

    s_aw_value = new_active_window;
    s_aw_count++;

    return cookie;
}


/** Recorded arguments from the last @a xcb_ewmh_set_showing_desktop
 *  call */
static uint32_t s_sd_value;
static int s_sd_count;


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_showing_desktop
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_showing_desktop(xcb_ewmh_connection_t *ewmh,
        int screen_nbr, uint32_t desktop)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};

    (void) ewmh;
    (void) screen_nbr;

    s_sd_value = desktop;
    s_sd_count++;

    return cookie;
}


/** Recorded arguments from the last @a xcb_ewmh_set_desktop_names
 *  call */
static uint32_t s_dn_len;
static char s_dn_value[128];
static int s_dn_count;


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_desktop_names
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_desktop_names(xcb_ewmh_connection_t *ewmh,
        int screen_nbr, uint32_t strings_len, const char *strings)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};
    uint32_t copy_len = strings_len;

    (void) ewmh;
    (void) screen_nbr;

    s_dn_len = strings_len;
    if (copy_len >= sizeof(s_dn_value)) {
        copy_len = sizeof(s_dn_value) - 1u;
    }
    memcpy(s_dn_value, strings, copy_len);
    s_dn_value[copy_len] = '\0';
    s_dn_count++;

    return cookie;
}


/** Recorded arguments from the last @a xcb_ewmh_set_workarea call */
static uint32_t s_wa_len;
static xcb_ewmh_geometry_t s_wa_list[8];
static int s_wa_count;


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_workarea
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_workarea(xcb_ewmh_connection_t *ewmh,
        int screen_nbr, uint32_t list_len, xcb_ewmh_geometry_t *list)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};
    uint32_t i;

    (void) ewmh;
    (void) screen_nbr;

    s_wa_len = list_len;
    memset(s_wa_list, 0, sizeof(s_wa_list));
    for (i = 0u; i < list_len && i < 8u; i++) {
        s_wa_list[i] = list[i];
    }
    s_wa_count++;

    return cookie;
}


/** Recorded arguments from the last @a xcb_ewmh_set_client_list call */
static uint32_t s_cl_len;
static xcb_window_t s_cl_list[16];
static int s_cl_count;


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_client_list
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_client_list(xcb_ewmh_connection_t *ewmh,
        int screen_nbr, uint32_t list_len, xcb_window_t *list)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};
    uint32_t i;

    (void) ewmh;
    (void) screen_nbr;

    s_cl_len = list_len;
    memset(s_cl_list, 0, sizeof(s_cl_list));
    for (i = 0u; i < list_len && i < 16u; i++) {
        s_cl_list[i] = list[i];
    }
    s_cl_count++;

    return cookie;
}


/** Recorded arguments from the last
 *  @a xcb_ewmh_set_client_list_stacking call */
static uint32_t s_cls_len;
static xcb_window_t s_cls_list[16];
static int s_cls_count;


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_client_list_stacking
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_client_list_stacking(
        xcb_ewmh_connection_t *ewmh, int screen_nbr, uint32_t list_len,
        xcb_window_t *list)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};
    uint32_t i;

    (void) ewmh;
    (void) screen_nbr;

    s_cls_len = list_len;
    memset(s_cls_list, 0, sizeof(s_cls_list));
    for (i = 0u; i < list_len && i < 16u; i++) {
        s_cls_list[i] = list[i];
    }
    s_cls_count++;

    return cookie;
}


/** Fixed array of desktops a test registers, and how many are valid,
 *  for @a stage_desktop_walk_all and @a stage_desktop_get to walk
 *  or search through */
static desktop_td *s_walk_desktops[8];
static int s_walk_desktop_count;


/**
 * @brief Test-controlled stand-in for @a stage_desktop_walk_all
 *
 * Actually invokes @p visit once per desktop a test registered
 * through @a s_set_walk_desktops, exactly as the real
 * stage_desktop_walk_all (stage.c) does over its own cdlist, so that
 * wm/ewmh.c's own per-desktop visitor functions are genuinely
 * exercised rather than merely proven reachable
 *
 * @note Complexity: @e O(n), where @e n is the number of registered
 *       desktops
 */
void stage_desktop_walk_all(const stage_td *stage,
        void (*visit)(desktop_td *desktop, void *data), void *data)
{
    int i;

    (void) stage;

    if (visit == NULL) {
        return;
    }

    for (i = 0; i < s_walk_desktop_count; i++) {
        visit(s_walk_desktops[i], data);
    }
}


static void s_set_walk_desktops(desktop_td **desktops, int count)
{
    int i;

    s_walk_desktop_count = count;
    for (i = 0; i < count && i < 8; i++) {
        s_walk_desktops[i] = desktops[i];
    }
}


/**
 * @brief Test-controlled stand-in for @a stage_desktop_get
 *
 * Answers whichever registered desktop's own id matches @p desktop_id,
 * or NULL if none does
 *
 * @note Complexity: @e O(n), where @e n is the number of registered
 *       desktops
 */
desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    int i;

    (void) stage;

    for (i = 0; i < s_walk_desktop_count; i++) {
        if (s_walk_desktops[i] != NULL &&
                s_walk_desktops[i]->id == desktop_id) {
            return s_walk_desktops[i];
        }
    }
    return NULL;
}


/** Fixed array of clients a test registers, one desktop's worth at a
 *  time, for @a stacking_walk and @a stacking_count to walk or count */
static client_td *s_stack_clients[8];
static int s_stack_client_count;


/**
 * @brief Test-controlled stand-in for @a stacking_walk
 *
 * Ignores @p desktop entirely and walks whichever fixed client array
 * a test last registered through @a s_set_stack_clients, since this
 * file's own desktop stand-in objects carry no real per-desktop
 * client storage of their own
 *
 * @note Complexity: @e O(n), where @e n is the number of registered
 *       clients
 */
void stacking_walk(const desktop_td *desktop,
        void (*visit)(client_td *client, void *data), void *data)
{
    int i;

    (void) desktop;

    if (visit == NULL) {
        return;
    }

    for (i = 0; i < s_stack_client_count; i++) {
        visit(s_stack_clients[i], data);
    }
}


/**
 * @brief Test-controlled stand-in for @a stacking_count
 * @note Complexity: @e O(1)
 */
uint32_t stacking_count(const desktop_td *desktop)
{
    (void) desktop;
    return (uint32_t) s_stack_client_count;
}


static void s_set_stack_clients(client_td **clients, int count)
{
    int i;

    s_stack_client_count = count;
    for (i = 0; i < count && i < 8; i++) {
        s_stack_clients[i] = clients[i];
    }
}


/** The client @a lookup_find_client answers, registered through
 *  @a s_set_lookup_client */
static client_td *s_lookup_client;


/**
 * @brief Test-controlled stand-in for @a lookup_find_client
 * @note Complexity: @e O(1)
 */
client_td *lookup_find_client(list_td *stages, xcb_window_t window,
        stage_td **out_stage, desktop_td **out_desktop)
{
    (void) stages;
    (void) window;
    (void) out_stage;
    (void) out_desktop;

    return s_lookup_client;
}


static void s_reset(void)
{
    memset(&s_ewmh, 0, sizeof(s_ewmh));
    s_ewmh_present = false;
    memset(s_last_interned_name, 0, sizeof(s_last_interned_name));
    s_intern_count = 0;
    s_cp_window = XCB_WINDOW_NONE;
    s_cp_property = XCB_ATOM_NONE;
    s_cp_type = XCB_ATOM_NONE;
    s_cp_data_len = 0u;
    memset(s_cp_data, 0, sizeof(s_cp_data));
    s_cp_count = 0;
    s_flush_count = 0;
    s_wmname_window = XCB_WINDOW_NONE;
    memset(s_wmname_value, 0, sizeof(s_wmname_value));
    s_wmname_count = 0;
    s_supported_len = 0u;
    memset(s_supported_list, 0, sizeof(s_supported_list));
    s_supported_count = 0;
    s_swc_parent = XCB_WINDOW_NONE;
    s_swc_child = XCB_WINDOW_NONE;
    s_swc_count = 0;
    s_nod_value = 0u;
    s_nod_count = 0;
    s_cd_value = 0u;
    s_cd_count = 0;
    s_dg_width = 0u;
    s_dg_height = 0u;
    s_dg_count = 0;
    s_dv_len = 0u;
    s_dv_count = 0;
    s_aw_value = XCB_WINDOW_NONE;
    s_aw_count = 0;
    s_sd_value = 0u;
    s_sd_count = 0;
    s_dn_len = 0u;
    memset(s_dn_value, 0, sizeof(s_dn_value));
    s_dn_count = 0;
    s_wa_len = 0u;
    memset(s_wa_list, 0, sizeof(s_wa_list));
    s_wa_count = 0;
    s_cl_len = 0u;
    memset(s_cl_list, 0, sizeof(s_cl_list));
    s_cl_count = 0;
    s_cls_len = 0u;
    memset(s_cls_list, 0, sizeof(s_cls_list));
    s_cls_count = 0;
    memset(s_walk_desktops, 0, sizeof(s_walk_desktops));
    s_walk_desktop_count = 0;
    memset(s_stack_clients, 0, sizeof(s_stack_clients));
    s_stack_client_count = 0;
    s_lookup_client = NULL;
}


static void s_set_ewmh_present(bool present)
{
    xcb_atom_t atom;

    memset(&s_ewmh, 0, sizeof(s_ewmh));

    /* Every field wm/ewmh.c itself reads off 'ewmh' gets a distinct
     * atom number, ten apart, so a wrong field showing up in a
     * recorded call is never mistaken for the right one */
    atom = 100u;
    s_ewmh._NET_SUPPORTED = atom;
    atom += 10u;
    s_ewmh._NET_SUPPORTING_WM_CHECK = atom;
    atom += 10u;
    s_ewmh._NET_CLIENT_LIST = atom;
    atom += 10u;
    s_ewmh._NET_CLIENT_LIST_STACKING = atom;
    atom += 10u;
    s_ewmh._NET_NUMBER_OF_DESKTOPS = atom;
    atom += 10u;
    s_ewmh._NET_CURRENT_DESKTOP = atom;
    atom += 10u;
    s_ewmh._NET_DESKTOP_GEOMETRY = atom;
    atom += 10u;
    s_ewmh._NET_DESKTOP_VIEWPORT = atom;
    atom += 10u;
    s_ewmh._NET_WORKAREA = atom;
    atom += 10u;
    s_ewmh._NET_DESKTOP_NAMES = atom;
    atom += 10u;
    s_ewmh._NET_WM_STRUT_PARTIAL = atom;
    atom += 10u;
    s_ewmh._NET_WM_STRUT = atom;
    atom += 10u;
    s_ewmh._NET_ACTIVE_WINDOW = atom;
    atom += 10u;
    s_ewmh._NET_WM_NAME = atom;
    atom += 10u;
    s_ewmh._NET_WM_ICON_NAME = atom;
    atom += 10u;
    s_ewmh._NET_WM_DESKTOP = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_HIDDEN = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_FULLSCREEN = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_MAXIMIZED_VERT = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_MAXIMIZED_HORZ = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_ABOVE = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_BELOW = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_STICKY = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_SHADED = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_DEMANDS_ATTENTION = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_SKIP_TASKBAR = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_SKIP_PAGER = atom;
    atom += 10u;
    s_ewmh._NET_CLOSE_WINDOW = atom;
    atom += 10u;
    s_ewmh._NET_WM_WINDOW_TYPE = atom;
    atom += 10u;
    s_ewmh._NET_WM_WINDOW_TYPE_DOCK = atom;
    atom += 10u;
    s_ewmh._NET_WM_WINDOW_TYPE_NORMAL = atom;
    atom += 10u;
    s_ewmh._NET_WM_WINDOW_TYPE_DIALOG = atom;
    atom += 10u;
    s_ewmh._NET_MOVERESIZE_WINDOW = atom;
    atom += 10u;
    s_ewmh._NET_FRAME_EXTENTS = atom;
    atom += 10u;
    s_ewmh._NET_REQUEST_FRAME_EXTENTS = atom;
    atom += 10u;
    s_ewmh._NET_DESKTOP_LAYOUT = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_MODAL = atom;
    atom += 10u;
    s_ewmh._NET_WM_ALLOWED_ACTIONS = atom;
    atom += 10u;
    s_ewmh._NET_WM_ACTION_MOVE = atom;
    atom += 10u;
    s_ewmh._NET_WM_ACTION_RESIZE = atom;
    atom += 10u;
    s_ewmh._NET_WM_ACTION_MINIMIZE = atom;
    atom += 10u;
    s_ewmh._NET_WM_ACTION_SHADE = atom;
    atom += 10u;
    s_ewmh._NET_WM_ACTION_STICK = atom;
    atom += 10u;
    s_ewmh._NET_WM_ACTION_MAXIMIZE_HORZ = atom;
    atom += 10u;
    s_ewmh._NET_WM_ACTION_MAXIMIZE_VERT = atom;
    atom += 10u;
    s_ewmh._NET_WM_ACTION_FULLSCREEN = atom;
    atom += 10u;
    s_ewmh._NET_WM_ACTION_CHANGE_DESKTOP = atom;
    atom += 10u;
    s_ewmh._NET_WM_ACTION_CLOSE = atom;
    atom += 10u;
    s_ewmh._NET_WM_ACTION_ABOVE = atom;
    atom += 10u;
    s_ewmh._NET_WM_ACTION_BELOW = atom;
    atom += 10u;
    s_ewmh._NET_WM_PING = atom;
    atom += 10u;
    s_ewmh._NET_WM_USER_TIME = atom;
    atom += 10u;
    s_ewmh._NET_WM_SYNC_REQUEST = atom;
    atom += 10u;
    s_ewmh._NET_WM_SYNC_REQUEST_COUNTER = atom;
    atom += 10u;
    s_ewmh._NET_SHOWING_DESKTOP = atom;
    atom += 10u;
    s_ewmh._NET_WM_WINDOW_TYPE_DESKTOP = atom;
    atom += 10u;
    s_ewmh._NET_WM_WINDOW_TYPE_TOOLBAR = atom;
    atom += 10u;
    s_ewmh._NET_WM_WINDOW_TYPE_MENU = atom;
    atom += 10u;
    s_ewmh._NET_WM_WINDOW_TYPE_UTILITY = atom;
    atom += 10u;
    s_ewmh._NET_WM_WINDOW_TYPE_SPLASH = atom;
    atom += 10u;
    s_ewmh._NET_WM_WINDOW_TYPE_TOOLTIP = atom;
    atom += 10u;
    s_ewmh._NET_WM_VISIBLE_NAME = atom;
    atom += 10u;
    s_ewmh._NET_WM_VISIBLE_ICON_NAME = atom;
    atom += 10u;
    s_ewmh._NET_WM_PID = atom;
    atom += 10u;
    s_ewmh._NET_WM_USER_TIME_WINDOW = atom;

    s_ewmh_present = present;
}


/* wm_ewmh_init on a null wm, a null connection, or a null EWMH
 * connection is a pure guard clause returning failure without
 * touching a single stand-in */
static void s_test_init_null_guards(void)
{
    wm_td wm_instance;
    int result_null_wm;
    int result_null_ewmh;

    s_reset();
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = NULL;

    result_null_wm = wm_ewmh_init(NULL);
    result_null_ewmh = wm_ewmh_init(&wm_instance);

    TAP_EQ_INT(result_null_wm, 1, "a null wm makes wm_ewmh_init fail");
    TAP_EQ_INT(result_null_ewmh, 1,
            "a null ewmh connection makes wm_ewmh_init fail too");
    TAP_EQ_INT(s_cp_count, 0,
            "neither guard failure writes a single property");
}


/* With no managed stages at all, wm_ewmh_init still publishes the
 * WM name and interns every one-off atom, but never touches a
 * per-stage property */
static void s_test_init_no_stages_still_publishes_wm_name(void)
{
    wm_td wm_instance;
    list_td *stages;
    int result;

    s_reset();
    s_set_ewmh_present(true);
    stages = list_init(NULL);
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = &s_ewmh;
    wm_instance.stages = stages;
    wm_instance.ewmh_support_win = (xcb_window_t) 77u;

    result = wm_ewmh_init(&wm_instance);

    TAP_EQ_INT(result, 0, "wm_ewmh_init succeeds with zero stages");
    TAP_EQ_INT(s_wmname_count, 1,
            "the WM name is published exactly once regardless");
    TAP_EQ_STR(s_wmname_value, "IcoWM",
            "and it is this project's own EWMH name");
    TAP_EQ_INT((long) s_wmname_window, 77,
            "published on the supporting-check window itself");
    TAP_EQ_INT(s_supported_count, 0,
            "with no stages, _NET_SUPPORTED is never published");
    TAP_EQ_INT(s_flush_count, 1,
            "the connection is flushed exactly once on the way out");

    list_destroy(stages);
}


/* One managed stage receives both _NET_SUPPORTING_WM_CHECK and a
 * full _NET_SUPPORTED atom list, the latter including every one-off
 * atom wm_ewmh_init interns itself rather than reading off the
 * EWMH connection struct */
static void s_test_init_one_stage_publishes_supported(void)
{
    wm_td wm_instance;
    list_td *stages;
    stage_td stage;
    xcb_screen_t screen;
    int result;

    s_reset();
    s_set_ewmh_present(true);
    stages = list_init(NULL);
    memset(&screen, 0, sizeof(screen));
    screen.root = (xcb_window_t) 500u;
    memset(&stage, 0, sizeof(stage));
    stage.screen = &screen;
    stage.id = 0u;
    list_ins_next(stages, list_tail(stages), &stage);
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = &s_ewmh;
    wm_instance.stages = stages;
    wm_instance.ewmh_support_win = (xcb_window_t) 77u;
    wm_instance.is_sync_available = false;

    result = wm_ewmh_init(&wm_instance);

    TAP_EQ_INT(result, 0, "wm_ewmh_init succeeds with one stage");
    TAP_EQ_INT(s_swc_count, 1,
            "_NET_SUPPORTING_WM_CHECK is published exactly once");
    TAP_EQ_INT((long) s_swc_parent, 500,
            "targeting the stage's own root window");
    TAP_EQ_INT((long) s_swc_child, 77,
            "and naming the support window itself");
    TAP_EQ_INT(s_supported_count, 1,
            "_NET_SUPPORTED is published exactly once");
    TAP_OK(s_supported_has(s_ewmh._NET_SUPPORTED),
            "the list includes a field read straight off the EWMH"
            " connection struct");
    TAP_OK(s_supported_has((xcb_atom_t) 9001u),
            "and also the one-off _NET_WM_STATE_FOCUSED atom this"
            " file's own atom_intern stand-in resolves");
    TAP_OK(s_supported_has((xcb_atom_t) 9002u),
            "along with _NET_WM_WINDOW_TYPE_NOTIFICATION");
    TAP_OK(s_supported_has((xcb_atom_t) 9003u),
            "and _NET_WM_ICON_GEOMETRY");
    TAP_OK(!s_supported_has(s_ewmh._NET_WM_SYNC_REQUEST),
            "with XSync unavailable, _NET_WM_SYNC_REQUEST is left out"
            " of the list entirely");
    list_destroy(stages);
}


/* When the singleton reports XSync as available, wm_ewmh_init adds
 * both XSync-related atoms to the supported list; when it does not,
 * both are left out, the exact difference in what gets published */
static void s_test_init_sync_available_adds_two_atoms(void)
{
    wm_td wm_instance;
    list_td *stages;
    stage_td stage;
    xcb_screen_t screen;
    uint32_t len_without_sync;

    s_reset();
    s_set_ewmh_present(true);
    stages = list_init(NULL);
    memset(&screen, 0, sizeof(screen));
    screen.root = (xcb_window_t) 500u;
    memset(&stage, 0, sizeof(stage));
    stage.screen = &screen;
    stage.id = 0u;
    list_ins_next(stages, list_tail(stages), &stage);
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = &s_ewmh;
    wm_instance.stages = stages;
    wm_instance.ewmh_support_win = (xcb_window_t) 77u;
    wm_instance.is_sync_available = false;
    wm_ewmh_init(&wm_instance);
    len_without_sync = s_supported_len;

    s_reset();
    s_set_ewmh_present(true);
    wm_instance.ewmh = &s_ewmh;
    wm_instance.is_sync_available = true;
    wm_ewmh_init(&wm_instance);

    TAP_EQ_INT((long) s_supported_len, (long) len_without_sync + 2,
            "XSync available adds exactly two more atoms to the"
            " supported list than XSync unavailable");
    TAP_OK(s_supported_has(s_ewmh._NET_WM_SYNC_REQUEST),
            "_NET_WM_SYNC_REQUEST itself is one of the two");
    TAP_OK(s_supported_has(s_ewmh._NET_WM_SYNC_REQUEST_COUNTER),
            "and _NET_WM_SYNC_REQUEST_COUNTER is the other");
    list_destroy(stages);
}


/* A stage with a null screen pointer is skipped entirely: neither
 * _NET_SUPPORTING_WM_CHECK nor _NET_SUPPORTED is published for it,
 * and it does not stop a later, valid stage from receiving both */
static void s_test_init_skips_stage_with_null_screen(void)
{
    wm_td wm_instance;
    list_td *stages;
    stage_td bad_stage;
    stage_td good_stage;
    xcb_screen_t screen;

    s_reset();
    s_set_ewmh_present(true);
    stages = list_init(NULL);
    memset(&bad_stage, 0, sizeof(bad_stage));
    bad_stage.screen = NULL;
    bad_stage.id = 0u;
    memset(&screen, 0, sizeof(screen));
    screen.root = (xcb_window_t) 900u;
    memset(&good_stage, 0, sizeof(good_stage));
    good_stage.screen = &screen;
    good_stage.id = 1u;
    list_ins_next(stages, list_tail(stages), &bad_stage);
    list_ins_next(stages, list_tail(stages), &good_stage);
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = &s_ewmh;
    wm_instance.stages = stages;
    wm_instance.ewmh_support_win = (xcb_window_t) 77u;

    wm_ewmh_init(&wm_instance);

    TAP_EQ_INT(s_swc_count, 1,
            "only the good stage, not the null-screen one, ever"
            " receives _NET_SUPPORTING_WM_CHECK");
    TAP_EQ_INT((long) s_swc_parent, 900,
            "specifically the good stage's own root window");
    TAP_EQ_INT(s_supported_count, 1,
            "and only one _NET_SUPPORTED publication happens in"
            " total, for the stage the null-screen one did not"
            " prevent from being reached");
    list_destroy(stages);
}


/* wm_ewmh_sync on a null wm, or a singleton with a null stages list
 * or a null EWMH connection, is a pure guard clause: not one
 * per-stage stand-in is ever touched */
static void s_test_sync_null_guards(void)
{
    wm_td wm_null_stages;
    wm_td wm_null_ewmh;
    list_td *stages;

    s_reset();
    s_set_ewmh_present(true);
    memset(&wm_null_stages, 0, sizeof(wm_null_stages));
    wm_null_stages.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_null_stages.ewmh = &s_ewmh;
    wm_null_stages.stages = NULL;

    stages = list_init(NULL);
    memset(&wm_null_ewmh, 0, sizeof(wm_null_ewmh));
    wm_null_ewmh.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_null_ewmh.ewmh = NULL;
    wm_null_ewmh.stages = stages;

    wm_ewmh_sync(NULL);
    wm_ewmh_sync(&wm_null_stages);
    wm_ewmh_sync(&wm_null_ewmh);

    TAP_EQ_INT(s_nod_count, 0,
            "none of the three null-guard cases publishes"
            " _NET_NUMBER_OF_DESKTOPS");
    TAP_EQ_INT(s_flush_count, 0,
            "and none of them flushes the connection either, since"
            " every one returns before the loop that would");

    list_destroy(stages);
}


/* A plain, single-desktop stage with no active client publishes
 * its own desktop count, current desktop, and geometry, an
 * XCB_NONE active window, and a false showing-desktop flag */
static void s_test_sync_plain_stage_publishes_geometry(void)
{
    wm_td wm_instance;
    list_td *stages;
    stage_td stage;
    desktop_td desktop;
    desktop_td *desktops[1];

    s_reset();
    s_set_ewmh_present(true);
    stages = list_init(NULL);
    memset(&desktop, 0, sizeof(desktop));
    desktop.id = 0u;
    desktop.client_active_id = XCB_NONE;
    memset(&stage, 0, sizeof(stage));
    stage.id = 3u;
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;
    stage.properties.dim.w = 1920u;
    stage.properties.dim.h = 1080u;
    stage.is_showing_desktop = false;
    list_ins_next(stages, list_tail(stages), &stage);
    desktops[0] = &desktop;
    s_set_walk_desktops(desktops, 1);
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = &s_ewmh;
    wm_instance.stages = stages;

    wm_ewmh_sync(&wm_instance);

    TAP_EQ_INT((long) s_nod_value, 1,
            "the stage's own desktop count is published verbatim");
    TAP_EQ_INT((long) s_cd_value, 0,
            "as is its current desktop index");
    TAP_EQ_INT((long) s_dg_width, 1920,
            "the desktop geometry's width comes from the stage's"
            " own dimensions");
    TAP_EQ_INT((long) s_dg_height, 1080, "and so does its height");
    TAP_EQ_INT((long) s_aw_value, (long) XCB_NONE,
            "with no active client on the current desktop, the"
            " active window published is XCB_NONE");
    TAP_EQ_INT((long) s_sd_value, 0,
            "a stage not showing the desktop publishes a false"
            " showing-desktop flag");
    TAP_EQ_INT(s_flush_count, 1,
            "the connection is flushed exactly once at the end");

    list_destroy(stages);
}


/* When the current desktop's active client resolves through
 * lookup_find_client, its own window, not the client id, is what
 * gets published as the active window */
static void s_test_sync_active_client_resolves_window(void)
{
    wm_td wm_instance;
    list_td *stages;
    stage_td stage;
    desktop_td desktop;
    desktop_td *desktops[1];
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    stages = list_init(NULL);
    memset(&client, 0, sizeof(client));
    client.window = (xcb_window_t) 4242u;
    s_lookup_client = &client;
    memset(&desktop, 0, sizeof(desktop));
    desktop.id = 0u;
    desktop.client_active_id = (xcb_window_t) 99u;
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;
    list_ins_next(stages, list_tail(stages), &stage);
    desktops[0] = &desktop;
    s_set_walk_desktops(desktops, 1);
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = &s_ewmh;
    wm_instance.stages = stages;

    wm_ewmh_sync(&wm_instance);

    TAP_EQ_INT((long) s_aw_value, 4242,
            "the active window published is the resolved client's"
            " own X window, not its internal id");

    list_destroy(stages);
}


/* When lookup_find_client fails to resolve the active client id at
 * all, the active window falls back to XCB_NONE rather than
 * publishing a stale or garbage value */
static void s_test_sync_active_client_missing_falls_back(void)
{
    wm_td wm_instance;
    list_td *stages;
    stage_td stage;
    desktop_td desktop;
    desktop_td *desktops[1];

    s_reset();
    s_set_ewmh_present(true);
    stages = list_init(NULL);
    s_lookup_client = NULL;
    memset(&desktop, 0, sizeof(desktop));
    desktop.id = 0u;
    desktop.client_active_id = (xcb_window_t) 99u;
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;
    list_ins_next(stages, list_tail(stages), &stage);
    desktops[0] = &desktop;
    s_set_walk_desktops(desktops, 1);
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = &s_ewmh;
    wm_instance.stages = stages;

    wm_ewmh_sync(&wm_instance);

    TAP_EQ_INT((long) s_aw_value, (long) XCB_NONE,
            "an active client id that fails to resolve at all falls"
            " back to publishing XCB_NONE, never a stale window");

    list_destroy(stages);
}


/* Desktop names, one per desktop, either the real name or a
 * generated "Desktop N" fallback for one with none of its own, all
 * end up in a single null-separated _NET_DESKTOP_NAMES buffer */
static void s_test_sync_publishes_desktop_names_with_fallback(void)
{
    wm_td wm_instance;
    list_td *stages;
    stage_td stage;
    desktop_td named;
    desktop_td unnamed;
    desktop_td *desktops[2];

    s_reset();
    s_set_ewmh_present(true);
    stages = list_init(NULL);
    memset(&named, 0, sizeof(named));
    named.id = 0u;
    strncpy(named.name, "Work", sizeof(named.name) - 1);
    memset(&unnamed, 0, sizeof(unnamed));
    unnamed.id = 1u;
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_count = 2u;
    stage.desktop_cur = 0u;
    list_ins_next(stages, list_tail(stages), &stage);
    desktops[0] = &named;
    desktops[1] = &unnamed;
    s_set_walk_desktops(desktops, 2);
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = &s_ewmh;
    wm_instance.stages = stages;

    wm_ewmh_sync(&wm_instance);

    TAP_EQ_INT(s_dn_count, 1,
            "_NET_DESKTOP_NAMES is published exactly once");
    TAP_OK(memcmp(s_dn_value, "Work\0Desktop 2\0", 15) == 0,
            "and the buffer holds the real name first, followed by"
            " the generated fallback for the unnamed second desktop");

    list_destroy(stages);
}


/* One workarea rectangle per desktop, in desktop order, published
 * through a single _NET_WORKAREA call */
static void s_test_sync_publishes_one_workarea_per_desktop(void)
{
    wm_td wm_instance;
    list_td *stages;
    stage_td stage;
    desktop_td first;
    desktop_td second;
    desktop_td *desktops[2];

    s_reset();
    s_set_ewmh_present(true);
    stages = list_init(NULL);
    memset(&first, 0, sizeof(first));
    first.id = 0u;
    first.workarea.pos.x = 0;
    first.workarea.pos.y = 0;
    first.workarea.dim.w = 800u;
    first.workarea.dim.h = 600u;
    memset(&second, 0, sizeof(second));
    second.id = 1u;
    second.workarea.pos.x = 10;
    second.workarea.pos.y = 20;
    second.workarea.dim.w = 400u;
    second.workarea.dim.h = 300u;
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_count = 2u;
    stage.desktop_cur = 0u;
    list_ins_next(stages, list_tail(stages), &stage);
    desktops[0] = &first;
    desktops[1] = &second;
    s_set_walk_desktops(desktops, 2);
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = &s_ewmh;
    wm_instance.stages = stages;

    wm_ewmh_sync(&wm_instance);

    TAP_EQ_INT((long) s_wa_len, 2,
            "exactly one workarea rectangle per desktop is published");
    TAP_EQ_INT((long) s_wa_list[0].width, 800,
            "the first desktop's own width comes through");
    TAP_EQ_INT((long) s_wa_list[0].height, 600,
            "and its own height");
    TAP_EQ_INT((long) s_wa_list[1].x, 10,
            "the second desktop's x offset comes through too");
    TAP_EQ_INT((long) s_wa_list[1].y, 20, "and its y offset");

    list_destroy(stages);
}


/* A negative workarea position clamps to zero rather than
 * publishing a value libxcb would read back as an enormous unsigned
 * offset */
static void s_test_sync_workarea_clamps_negative_position(void)
{
    wm_td wm_instance;
    list_td *stages;
    stage_td stage;
    desktop_td desktop;
    desktop_td *desktops[1];

    s_reset();
    s_set_ewmh_present(true);
    stages = list_init(NULL);
    memset(&desktop, 0, sizeof(desktop));
    desktop.id = 0u;
    desktop.workarea.pos.x = -5;
    desktop.workarea.pos.y = -5;
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;
    list_ins_next(stages, list_tail(stages), &stage);
    desktops[0] = &desktop;
    s_set_walk_desktops(desktops, 1);
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = &s_ewmh;
    wm_instance.stages = stages;

    wm_ewmh_sync(&wm_instance);

    TAP_EQ_INT((long) s_wa_list[0].x, 0,
            "a negative x position clamps to zero rather than"
            " publishing it as-is");
    TAP_EQ_INT((long) s_wa_list[0].y, 0,
            "and so does a negative y position");

    list_destroy(stages);
}


/* With no managed clients at all, both _NET_CLIENT_LIST and its
 * stacking variant are still published, each as an explicit empty
 * list rather than being skipped entirely */
static void s_test_sync_no_clients_publishes_empty_lists(void)
{
    wm_td wm_instance;
    list_td *stages;
    stage_td stage;
    desktop_td desktop;
    desktop_td *desktops[1];

    s_reset();
    s_set_ewmh_present(true);
    stages = list_init(NULL);
    memset(&desktop, 0, sizeof(desktop));
    desktop.id = 0u;
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;
    list_ins_next(stages, list_tail(stages), &stage);
    desktops[0] = &desktop;
    s_set_walk_desktops(desktops, 1);
    s_set_stack_clients(NULL, 0);
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = &s_ewmh;
    wm_instance.stages = stages;

    wm_ewmh_sync(&wm_instance);

    TAP_EQ_INT(s_cl_count, 1,
            "_NET_CLIENT_LIST is published exactly once even with"
            " nothing to put in it");
    TAP_EQ_INT((long) s_cl_len, 0, "as an explicit empty list");
    TAP_EQ_INT(s_cls_count, 1,
            "and so is _NET_CLIENT_LIST_STACKING");
    TAP_EQ_INT((long) s_cls_len, 0, "also empty");

    list_destroy(stages);
}


/* Every managed client on a desktop ends up in both _NET_CLIENT_LIST
 * and its stacking variant, and each has its own _NET_WM_DESKTOP
 * property published alongside naming the desktop it is actually on */
static void s_test_sync_publishes_client_list_and_desktop_property(void)
{
    wm_td wm_instance;
    list_td *stages;
    stage_td stage;
    desktop_td desktop;
    desktop_td *desktops[1];
    client_td client_a;
    client_td client_b;
    client_td *clients[2];

    s_reset();
    s_set_ewmh_present(true);
    stages = list_init(NULL);
    memset(&desktop, 0, sizeof(desktop));
    desktop.id = 7u;
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;
    list_ins_next(stages, list_tail(stages), &stage);
    desktops[0] = &desktop;
    s_set_walk_desktops(desktops, 1);
    memset(&client_a, 0, sizeof(client_a));
    client_a.window = (xcb_window_t) 11u;
    memset(&client_b, 0, sizeof(client_b));
    client_b.window = (xcb_window_t) 22u;
    clients[0] = &client_a;
    clients[1] = &client_b;
    s_set_stack_clients(clients, 2);
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = &s_ewmh;
    wm_instance.stages = stages;

    wm_ewmh_sync(&wm_instance);

    TAP_EQ_INT((long) s_cl_len, 2,
            "both managed clients end up in _NET_CLIENT_LIST");
    TAP_EQ_INT((long) s_cl_list[0], 11,
            "the first client's own window comes first");
    TAP_EQ_INT((long) s_cl_list[1], 22,
            "and the second client's window comes second");
    TAP_EQ_INT((long) s_cls_len, 2,
            "the stacking variant carries the same two windows too");
    TAP_EQ_INT((long) s_cp_property, (long) s_ewmh._NET_WM_DESKTOP,
            "the last _NET_WM_DESKTOP property write targets that"
            " very property");
    TAP_EQ_INT((long) s_cp_data[0], 7,
            "and names the desktop the client actually walked off of");

    list_destroy(stages);
}


/* A pinned client always has WM_DESKTOP_ID_ALL published as its own
 * _NET_WM_DESKTOP value, regardless of which desktop it is walked
 * from */
static void s_test_sync_pinned_client_publishes_all_desktops(void)
{
    wm_td wm_instance;
    list_td *stages;
    stage_td stage;
    desktop_td desktop;
    desktop_td *desktops[1];
    client_td client;
    client_td *clients[1];

    s_reset();
    s_set_ewmh_present(true);
    stages = list_init(NULL);
    memset(&desktop, 0, sizeof(desktop));
    desktop.id = 3u;
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;
    list_ins_next(stages, list_tail(stages), &stage);
    desktops[0] = &desktop;
    s_set_walk_desktops(desktops, 1);
    memset(&client, 0, sizeof(client));
    client.window = (xcb_window_t) 55u;
    client.properties.flags = (uint32_t) CLIENT_FLAG_PIN;
    clients[0] = &client;
    s_set_stack_clients(clients, 1);
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = &s_ewmh;
    wm_instance.stages = stages;

    wm_ewmh_sync(&wm_instance);

    TAP_EQ_INT((long) s_cp_data[0], (long) WM_DESKTOP_ID_ALL,
            "a pinned client always publishes the EWMH \"all"
            " desktops\" sentinel for its own _NET_WM_DESKTOP,"
            " ignoring which desktop the walk actually found it on");

    list_destroy(stages);
}


/* wm_ewmh_sync publishes each desktop's own real viewport origin,
 * rather than leaving the whole coordinates list at zero */
static void s_test_sync_publishes_real_viewport_origins(void)
{
    wm_td wm_instance;
    list_td *stages;
    stage_td stage;
    desktop_td first;
    desktop_td second;
    desktop_td *desktops[2];

    s_reset();
    s_set_ewmh_present(true);
    stages = list_init(NULL);
    memset(&first, 0, sizeof(first));
    first.id = 0u;
    first.viewport_origin.x = 1024;
    first.viewport_origin.y = 0;
    memset(&second, 0, sizeof(second));
    second.id = 1u;
    second.viewport_origin.x = 0;
    second.viewport_origin.y = 768;
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_count = 2u;
    stage.desktop_cur = 0u;
    list_ins_next(stages, list_tail(stages), &stage);
    desktops[0] = &first;
    desktops[1] = &second;
    s_set_walk_desktops(desktops, 2);
    memset(&wm_instance, 0, sizeof(wm_instance));
    wm_instance.connection = (xcb_connection_t *) (uintptr_t) 1;
    wm_instance.ewmh = &s_ewmh;
    wm_instance.stages = stages;

    wm_ewmh_sync(&wm_instance);

    TAP_EQ_INT((long) s_dv_len, 2,
            "wm_ewmh_sync publishes one viewport coordinate pair per" \
            " desktop");
    TAP_EQ_INT((long) s_dv_last[0].x, 1024,
            "the first desktop's real viewport X origin is" \
            " published rather than left at zero");
    TAP_EQ_INT((long) s_dv_last[1].y, 768,
            "the second desktop's real viewport Y origin is" \
            " published rather than left at zero");

    list_destroy(stages);
}


int main(void)
{
    TAP_PLAN(59);

    s_test_init_null_guards();
    s_test_init_no_stages_still_publishes_wm_name();
    s_test_init_one_stage_publishes_supported();
    s_test_init_sync_available_adds_two_atoms();
    s_test_init_skips_stage_with_null_screen();
    s_test_sync_null_guards();
    s_test_sync_plain_stage_publishes_geometry();
    s_test_sync_active_client_resolves_window();
    s_test_sync_active_client_missing_falls_back();
    s_test_sync_publishes_desktop_names_with_fallback();
    s_test_sync_publishes_one_workarea_per_desktop();
    s_test_sync_workarea_clamps_negative_position();
    s_test_sync_no_clients_publishes_empty_lists();
    s_test_sync_publishes_client_list_and_desktop_property();
    s_test_sync_pinned_client_publishes_all_desktops();
    s_test_sync_publishes_real_viewport_origins();

    return TAP_DONE();
}
