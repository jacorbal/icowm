/**
 * @file wm/ewmh.c
 *
 * @brief Window manager EWMH initialization and synchronization
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdint.h>
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* NULL, calloc, free, malloc */
#include <string.h>     /* memcpy */
#include <time.h>       /* time */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Utils includes */
#include <utils/safe/safestr.h>
#include <utils/xcb/atom.h>
#include <utils/xcb/connection.h>

/* Default initial values */
#include <defs/ewmh.h>
#include <defs/desktop.h>
#include <defs/icon.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <policy/stacking.h>
#include <logger.h>
#include <lookup.h>
#include <stage.h>
#include <stage/desktop.h>

/* Local includes */
#include <wm.h>
#include <wm/ewmh.h>
#include <wm/ewmh.h>
#include <wm/internal.h>


/**
 * @brief What @a s_workarea_collect_visit is filling in
 */
struct s_workarea_ctx_s {
    xcb_ewmh_geometry_t *out;   /**< Array being filled, one each */
    uint32_t count;             /**< How many are written so far */
    uint32_t capacity;          /**< How many it has room for */
};


/**
 * @brief What @a s_window_list_visit is gathering into
 */
struct s_window_list_ctx_s {
    xcb_window_t *out;          /**< Array of window IDs being built */
    size_t *count;              /**< How many have been put in so far */
    size_t capacity;            /**< How many it holds */
};


/**
 * @brief What @a s_desktop_name_measure_visit is adding up
 */
struct s_name_measure_ctx_s {
    size_t *total;              /**< Running byte count, terminators
                                     included */
    uint32_t index;             /**< Which desktop this is, for
                                     a fallback */
};


/**
 * @brief What @a s_desktop_name_write_visit is filling in
 */
struct s_name_write_ctx_s {
    char *out;                  /**< Null-separated buffer being
                                     written */
    size_t *offset;             /**< How much of it is written so far */
    size_t capacity;            /**< Its size, in bytes */
    uint32_t index;             /**< Which desktop this is, for
                                     a fallback */
};


/**
 * @brief What @a s_client_list_visit is filling in
 */
struct s_client_list_ctx_s {
    xcb_window_t *out;          /**< Array of window IDs being built */
    size_t capacity;            /**< How many it holds */
    size_t count;               /**< How many have been put in so far */
    uint32_t desktop_id;        /**< Desktop the walk is now on */
};


/** Published for a client that is on every desktop */
static const uint32_t s_desktop_id_all = WM_DESKTOP_ID_ALL;


/**
 * @brief Note one client's window ID, bottom of the stack first
 *
 * @param client Client reached by the walk
 * @param data   Pointer to the @c s_window_list_ctx_s being filled
 *
 * @note Complexity: @e O(1)
 */
static void s_window_list_visit(client_td *client, void *data)
{
    struct s_window_list_ctx_s *const list_ctx = data;

    if (client == NULL || list_ctx == NULL ||
            client->window == XCB_NONE ||
            *list_ctx->count >= list_ctx->capacity) {
        return;
    }

    list_ctx->out[(*list_ctx->count)++] = client->window;
}


/**
 * @brief Add the room one desktop's name needs
 *
 * @param desktop Desktop reached by the walk
 * @param data    The @c s_name_measure_ctx_s being added to
 *
 * @note A desktop with no name of its own is counted as the fallback
 *       that will be published for it, so the two walks agree
 * @note Complexity: @e O(n), where @e n is the length of the name
 */
static void s_desktop_name_measure_visit(desktop_td *desktop, void *data)
{
    struct s_name_measure_ctx_s *const ctx = data;
    const uint32_t this_index = (ctx != NULL) ? ctx->index : 0u;

    if (ctx == NULL) {
        return;
    }

    ctx->index++;
    if (desktop->name[0] != '\0') {
        *ctx->total += safe_strlen(desktop->name) + 1u;
    } else {
        char fallback_name[32];
        const int written = snprintf(fallback_name,
                sizeof(fallback_name), "Desktop %u", this_index + 1u);

        if (written > 0) {
            *ctx->total += (size_t) written + 1u;
        }
    }
}


/**
 * @brief Write one desktop's name into the list
 *
 * @param desktop Desktop reached by the walk
 * @param data    The @c s_name_write_ctx_s being filled
 *
 * @note A name that would not fit is left out rather than truncated,
 *       and so is every one after it
 * @note Complexity: @e O(n), where @e n is the length of the name
 */
static void s_desktop_name_write_visit(desktop_td *desktop, void *data)
{
    struct s_name_write_ctx_s *const ctx = data;
    const uint32_t this_index = (ctx != NULL) ? ctx->index : 0u;
    char fallback_name[32];
    const char *name;
    size_t name_len;

    if (ctx == NULL) {
        return;
    }

    ctx->index++;
    if (desktop->name[0] != '\0') {
        name = desktop->name;
    } else {
        const int written = snprintf(fallback_name,
                sizeof(fallback_name), "Desktop %u", this_index + 1u);

        if (written <= 0) {
            return;
        }
        fallback_name[sizeof(fallback_name) - 1u] = '\0';
        name = fallback_name;
    }

    name_len = safe_strlen(name);
    if (*ctx->offset + name_len + 1u > ctx->capacity) {
        return;
    }

    memcpy(ctx->out + *ctx->offset, name, name_len + 1u);
    *ctx->offset += name_len + 1u;
}


/**
 * @brief Note one desktop's clients in stacking order
 *
 * @param desktop Desktop reached by the walk
 * @param data    The @c s_window_list_ctx_s being filled
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static void s_stacking_collect_visit(desktop_td *desktop, void *data)
{
    stacking_walk(desktop, s_window_list_visit, data);
}


/**
 * @brief Note one desktop's workarea
 *
 * @param desktop Desktop reached by the walk
 * @param data    The @c s_workarea_ctx_s being filled
 *
 * @note Never writes past @p data's capacity, the array being sized
 *       from @c stage->desktop_count while the walk that reaches here
 *       iterates the desktop list itself, which is a separate count
 *       that nothing here can prove equal
 * @note Complexity: @e O(1)
 */
static void s_workarea_collect_visit(desktop_td *desktop, void *data)
{
    struct s_workarea_ctx_s *const ctx = data;

    if (ctx == NULL || ctx->count >= ctx->capacity) {
        return;
    }

    ctx->out[ctx->count].x = (desktop->workarea.pos.x > 0)
        ? (uint32_t) desktop->workarea.pos.x
        : 0u;
    ctx->out[ctx->count].y = (desktop->workarea.pos.y > 0)
        ? (uint32_t) desktop->workarea.pos.y
        : 0u;
    ctx->out[ctx->count].width = desktop->workarea.dim.w;
    ctx->out[ctx->count].height = desktop->workarea.dim.h;
    ctx->count++;
}


/**
 * @brief Add one desktop's client count to a running total
 *
 * @param desktop Desktop reached by the walk
 * @param data    Pointer to the @c uint32_t total
 *
 * @note Complexity: @e O(1)
 */
static void s_client_count_visit(desktop_td *desktop, void *data)
{
    size_t *const total = data;

    if (total == NULL) {
        return;
    }

    *total += stacking_count(desktop);
}


/**
 * @brief Note one client, and tell it which desktop it is on
 *
 * @param client Client reached by the walk
 * @param data   The @c s_client_list_ctx_s being filled
 *
 * @note Complexity: @e O(1)
 */
static void s_client_list_one_visit(client_td *client, void *data)
{
    struct s_client_list_ctx_s *const ctx = data;

    if (ctx == NULL || client->window == XCB_NONE ||
            ctx->count >= ctx->capacity) {
        return;
    }

    ctx->out[ctx->count] = client->window;
    ctx->count++;

    /* Published here so taskbars and pagers can associate each window
     * with the desktop it is actually on */
    xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
            client->window,
            xcb_ewmh_connection_get()->_NET_WM_DESKTOP,
            XCB_ATOM_CARDINAL, 32, 1,
            (client->properties.flags & CLIENT_FLAG_PIN)
                ? &s_desktop_id_all : &ctx->desktop_id);
}


/**
 * @brief Note one desktop's clients, and tell each which it is on
 *
 * @param desktop Desktop reached by the walk
 * @param data    The @c s_client_list_ctx_s being filled
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static void s_client_list_visit(desktop_td *desktop, void *data)
{
    struct s_client_list_ctx_s *const ctx = data;

    if (ctx == NULL) {
        return;
    }

    ctx->desktop_id = desktop->id;
    stacking_walk(desktop, s_client_list_one_visit, ctx);
}


/**
 * @brief Compute and publish @c _NET_WORKAREA for one managed stage
 *
 * Builds an array of workarea rectangles, one per desktop on the given
 * stage, and writes it to the @c _NET_WORKAREA root property.
 *
 * @param stage Pointer to the target stage
 *
 * @note Desktops without a valid work area fall back to the full
 *       stage geometry
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
static void s_wm_sync_workarea(stage_td *stage)
{
    struct s_workarea_ctx_s workarea_ctx;
    xcb_ewmh_geometry_t *workareas;

    if (stage == NULL || xcb_ewmh_connection_get() == NULL ||
            stage->desktop_count == 0) {
        return;
    }

    /* Zeroed rather than merely allocated, so that a desktop the walk
     * never reaches leaves a defined rectangle behind instead of
     * whatever the heap held */
    workareas = calloc(stage->desktop_count,
            sizeof(xcb_ewmh_geometry_t));
    if (workareas == NULL) {
        return;
    }

    workarea_ctx.out = workareas;
    workarea_ctx.capacity = stage->desktop_count;
    workarea_ctx.count = 0u;
    stage_desktop_walk_all(stage, s_workarea_collect_visit,
            &workarea_ctx);

    /* One geometry per desktop, as many as '_NET_NUMBER_OF_DESKTOPS'
     * says there are and not merely as many as the walk reached:
     * a pager reads this array up to that count, so publishing fewer
     * would send it off the end of what it was given.  The 'calloc'
     * above is what makes that safe, a desktop the walk misses being
     * a zeroed rectangle rather than whatever the heap held. */
    xcb_ewmh_set_workarea(xcb_ewmh_connection_get(), (int) stage->id,
            stage->desktop_count, workareas);
    free(workareas);
}


/**
 * @brief Compute and publish @c _NET_DESKTOP_LAYOUT for one stage
 *
 * Builds a fixed four-element layout descriptor (orientation, columns,
 * rows, starting corner) describing the desktops as a single horizontal
 * row, and writes it to the @c _NET_DESKTOP_LAYOUT root property.
 *
 * @param stage Pointer to the target stage
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_sync_desktop_layout(stage_td *stage)
{
    uint32_t layout[4];

    if (stage == NULL || xcb_connection_get() == NULL ||
            stage->screen == NULL ||
            xcb_ewmh_connection_get() == NULL) {
        return;
    }

    /* Orientation=0(horizontal), cols=n, rows=1, corner=0(top-left) */
    layout[0] = 0u;
    layout[1] = (stage->desktop_count > 0u)
        ? stage->desktop_count : 1u;
    layout[2] = 1u;
    layout[3] = 0u;

    xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
            stage->screen->root,
            xcb_ewmh_connection_get()->_NET_DESKTOP_LAYOUT,
            XCB_ATOM_CARDINAL, 32, 4, layout);
}


/**
 * @brief Compute and publish @c _NET_CLIENT_LIST and its stacking
 *        variant
 *
 * Collects the windows of every managed client across all desktops of
 * the stage, publishing them via @c _NET_CLIENT_LIST in insertion
 * order and via @c _NET_CLIENT_LIST_STACKING in bottom-to-top stacking
 * order.  Also updates each client's @c _NET_WM_DESKTOP property, using
 * the special "all desktops" value for pinned clients.
 *
 * @param stage Pointer to the target stage
 *
 * @note Complexity: @e O(n), where @e n is the total number of managed
 *       clients across all desktops
 */
static void s_wm_sync_client_lists(stage_td *stage)
{
    struct s_client_list_ctx_s client_ctx;
    struct s_window_list_ctx_s stack_ctx;
    size_t total_clients = 0u;
    size_t idx = 0u;
    xcb_window_t *client_list;
    xcb_window_t *stacking_list;
    xcb_ewmh_connection_t *const ewmh =
        xcb_ewmh_connection_get();

    if (stage == NULL || ewmh == NULL) {
        return;
    }

    stage_desktop_walk_all(stage, s_client_count_visit,
            &total_clients);

    if (total_clients == 0u) {
        xcb_ewmh_set_client_list(ewmh,
                (int) stage->id, 0u, NULL);
        xcb_ewmh_set_client_list_stacking(ewmh,
                (int) stage->id, 0u, NULL);
        return;
    }

    client_list = malloc(sizeof(xcb_window_t) * total_clients);
    stacking_list = malloc(sizeof(xcb_window_t) * total_clients);
    if (client_list == NULL || stacking_list == NULL) {
        free(client_list);
        free(stacking_list);
        return;
    }

    client_ctx.out = client_list;
    client_ctx.capacity = total_clients;
    client_ctx.count = 0u;
    stage_desktop_walk_all(stage, s_client_list_visit, &client_ctx);
    idx = client_ctx.count;

    xcb_ewmh_set_client_list(ewmh,
            (int) stage->id,
            (uint32_t) idx, client_list);

    idx = 0u;
    stack_ctx.out = stacking_list;
    stack_ctx.capacity = total_clients;
    stack_ctx.count = &idx;
    stage_desktop_walk_all(stage,
            s_stacking_collect_visit, &stack_ctx);

    xcb_ewmh_set_client_list_stacking(ewmh,
            (int) stage->id, (uint32_t) idx, stacking_list);

    free(client_list);
    free(stacking_list);
}


/**
 * @brief Compute and publish @c _NET_DESKTOP_NAMES for one stage
 *
 * Builds a null-separated UTF-8 list with every desktop name of the
 * target stage and writes it to the root-window EWMH property.
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
static void s_wm_sync_desktop_names(stage_td *stage)
{
    size_t names_len;
    struct s_name_measure_ctx_s measure_ctx;
    struct s_name_write_ctx_s write_ctx;
    size_t offset;
    char *names;

    if (stage == NULL || xcb_ewmh_connection_get() == NULL ||
            stage->desktop_count == 0u) {
        return;
    }

    names_len = 0u;
    measure_ctx.total = &names_len;
    measure_ctx.index = 0u;
    stage_desktop_walk_all(stage, s_desktop_name_measure_visit,
            &measure_ctx);

    if (names_len == 0u || names_len > UINT32_MAX) {
        return;
    }

    names = calloc(names_len, sizeof(char));
    if (names == NULL) {
        return;
    }

    offset = 0u;
    write_ctx.out = names;
    write_ctx.capacity = names_len;
    write_ctx.offset = &offset;
    write_ctx.index = 0u;
    stage_desktop_walk_all(stage, s_desktop_name_write_visit,
            &write_ctx);

    if (offset > 0u) {
        xcb_ewmh_set_desktop_names(xcb_ewmh_connection_get(),
                (int) stage->id, (uint32_t) offset, names);
    }
    free(names);
}


/* Create and publish the root EWMH metadata clients require */
int wm_ewmh_init(const wm_td *wm)
{
    xcb_connection_t *connection = wm_connection(wm);
    xcb_ewmh_connection_t *ewmh = wm_ewmh(wm);
    list_td *stages = wm_stages(wm);
    xcb_atom_t supported_atoms[WM_EWMH_SUPPORTED_COUNT];
    uint32_t n_supported = 0u;
    xcb_window_t support = wm_ewmh_support_win(wm);
    xcb_atom_t net_wm_state_focused = XCB_ATOM_NONE;
    xcb_atom_t net_wm_win_type_notif = XCB_ATOM_NONE;
    xcb_atom_t net_wm_icon_geometry = XCB_ATOM_NONE;
    xcb_atom_t net_restack_window = XCB_ATOM_NONE;
    xcb_atom_t net_wm_fullscreen_monitors = XCB_ATOM_NONE;
    xcb_atom_t net_wm_moveresize = XCB_ATOM_NONE;
    xcb_atom_t wm_icon_size_atom = XCB_ATOM_NONE;
    uint32_t icon_size_hints[6];

    if (wm == NULL || connection == NULL || ewmh == NULL) {
        return 1;
    }

    /* Intern atoms not exposed directly by 'xcb_ewmh_connection_t' */
    net_wm_state_focused = atom_intern(connection,
            "_NET_WM_STATE_FOCUSED", false);
    net_wm_win_type_notif = atom_intern(connection,
            "_NET_WM_WINDOW_TYPE_NOTIFICATION", false);
    net_wm_icon_geometry = atom_intern(connection,
            "_NET_WM_ICON_GEOMETRY", false);
    wm_icon_size_atom = atom_intern(connection,
            "WM_ICON_SIZE", false);
    net_restack_window = atom_intern(connection,
            "_NET_RESTACK_WINDOW", false);
    net_wm_fullscreen_monitors = atom_intern(connection,
            "_NET_WM_FULLSCREEN_MONITORS", false);
    net_wm_moveresize = atom_intern(connection,
            "_NET_WM_MOVERESIZE", false);

    /* ICCCM §4.1.3: announce the fixed icon dimensions to clients */
    icon_size_hints[0] = WM_ICON_SQUARE_SIZE;   /* min_width */
    icon_size_hints[1] = WM_ICON_SQUARE_SIZE;   /* min_height */
    icon_size_hints[2] = WM_ICON_SQUARE_SIZE;   /* max_width */
    icon_size_hints[3] = WM_ICON_SQUARE_SIZE;   /* max_height */
    icon_size_hints[4] = 1u;                    /* width_inc */
    icon_size_hints[5] = 1u;                    /* height_inc */

    xcb_ewmh_set_wm_name(ewmh, support,
            sizeof(WM_EWMH_NAME) - 1u, WM_EWMH_NAME);
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
            support, ewmh->_NET_SUPPORTING_WM_CHECK,
            XCB_ATOM_WINDOW, 32, 1, &support);

    supported_atoms[n_supported++] = ewmh->_NET_SUPPORTED;
    supported_atoms[n_supported++] = ewmh->_NET_SUPPORTING_WM_CHECK;
    supported_atoms[n_supported++] = ewmh->_NET_CLIENT_LIST;
    supported_atoms[n_supported++] = ewmh->_NET_CLIENT_LIST_STACKING;
    supported_atoms[n_supported++] = ewmh->_NET_NUMBER_OF_DESKTOPS;
    supported_atoms[n_supported++] = ewmh->_NET_CURRENT_DESKTOP;
    supported_atoms[n_supported++] = ewmh->_NET_DESKTOP_GEOMETRY;
    supported_atoms[n_supported++] = ewmh->_NET_DESKTOP_VIEWPORT;
    supported_atoms[n_supported++] = ewmh->_NET_WORKAREA;
    supported_atoms[n_supported++] = ewmh->_NET_DESKTOP_NAMES;
    supported_atoms[n_supported++] = ewmh->_NET_WM_STRUT_PARTIAL;
    supported_atoms[n_supported++] = ewmh->_NET_WM_STRUT;
    supported_atoms[n_supported++] = ewmh->_NET_ACTIVE_WINDOW;
    supported_atoms[n_supported++] = ewmh->_NET_WM_NAME;
    supported_atoms[n_supported++] = ewmh->_NET_WM_ICON_NAME;
    supported_atoms[n_supported++] = ewmh->_NET_WM_DESKTOP;
    supported_atoms[n_supported++] = ewmh->_NET_WM_STATE;
    supported_atoms[n_supported++] = ewmh->_NET_WM_STATE_HIDDEN;
    supported_atoms[n_supported++] = ewmh->_NET_WM_STATE_FULLSCREEN;
    supported_atoms[n_supported++] = ewmh->_NET_WM_STATE_MAXIMIZED_VERT;
    supported_atoms[n_supported++] = ewmh->_NET_WM_STATE_MAXIMIZED_HORZ;
    supported_atoms[n_supported++] = ewmh->_NET_WM_STATE_ABOVE;
    supported_atoms[n_supported++] = ewmh->_NET_WM_STATE_BELOW;
    supported_atoms[n_supported++] = ewmh->_NET_WM_STATE_STICKY;
    supported_atoms[n_supported++] = ewmh->_NET_WM_STATE_SHADED;
    supported_atoms[n_supported++] =
        ewmh->_NET_WM_STATE_DEMANDS_ATTENTION;
    supported_atoms[n_supported++] = ewmh->_NET_WM_STATE_SKIP_TASKBAR;
    supported_atoms[n_supported++] = ewmh->_NET_WM_STATE_SKIP_PAGER;
    supported_atoms[n_supported++] = ewmh->_NET_CLOSE_WINDOW;
    supported_atoms[n_supported++] = ewmh->_NET_WM_WINDOW_TYPE;
    supported_atoms[n_supported++] = ewmh->_NET_WM_WINDOW_TYPE_DOCK;
    supported_atoms[n_supported++] = ewmh->_NET_WM_WINDOW_TYPE_NORMAL;
    supported_atoms[n_supported++] = ewmh->_NET_WM_WINDOW_TYPE_DIALOG;
    supported_atoms[n_supported++] = ewmh->_NET_MOVERESIZE_WINDOW;
    supported_atoms[n_supported++] = ewmh->_NET_FRAME_EXTENTS;
    supported_atoms[n_supported++] = ewmh->_NET_REQUEST_FRAME_EXTENTS;
    supported_atoms[n_supported++] = net_restack_window;
    supported_atoms[n_supported++] = net_wm_fullscreen_monitors;
    supported_atoms[n_supported++] = net_wm_moveresize;
    supported_atoms[n_supported++] = ewmh->_NET_DESKTOP_LAYOUT;
    supported_atoms[n_supported++] = ewmh->_NET_WM_STATE_MODAL;
    supported_atoms[n_supported++] = net_wm_state_focused;
    supported_atoms[n_supported++] = ewmh->_NET_WM_ALLOWED_ACTIONS;
    supported_atoms[n_supported++] = ewmh->_NET_WM_ACTION_MOVE;
    supported_atoms[n_supported++] = ewmh->_NET_WM_ACTION_RESIZE;
    supported_atoms[n_supported++] = ewmh->_NET_WM_ACTION_MINIMIZE;
    supported_atoms[n_supported++] = ewmh->_NET_WM_ACTION_SHADE;
    supported_atoms[n_supported++] = ewmh->_NET_WM_ACTION_STICK;
    supported_atoms[n_supported++] = ewmh->_NET_WM_ACTION_MAXIMIZE_HORZ;
    supported_atoms[n_supported++] = ewmh->_NET_WM_ACTION_MAXIMIZE_VERT;
    supported_atoms[n_supported++] = ewmh->_NET_WM_ACTION_FULLSCREEN;
    supported_atoms[n_supported++] = ewmh->_NET_WM_ACTION_CHANGE_DESKTOP;
    supported_atoms[n_supported++] = ewmh->_NET_WM_ACTION_CLOSE;
    supported_atoms[n_supported++] = ewmh->_NET_WM_ACTION_ABOVE;
    supported_atoms[n_supported++] = ewmh->_NET_WM_ACTION_BELOW;
    supported_atoms[n_supported++] = ewmh->_NET_WM_PING;
    supported_atoms[n_supported++] = ewmh->_NET_WM_USER_TIME;
    if (wm_sync_available(wm)) {
        supported_atoms[n_supported++] = ewmh->_NET_WM_SYNC_REQUEST;
        supported_atoms[n_supported++] =
            ewmh->_NET_WM_SYNC_REQUEST_COUNTER;
    }
    supported_atoms[n_supported++] = ewmh->_NET_SHOWING_DESKTOP;
    supported_atoms[n_supported++] = ewmh->_NET_WM_WINDOW_TYPE_DESKTOP;
    supported_atoms[n_supported++] = ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR;
    supported_atoms[n_supported++] = ewmh->_NET_WM_WINDOW_TYPE_MENU;
    supported_atoms[n_supported++] = ewmh->_NET_WM_WINDOW_TYPE_UTILITY;
    supported_atoms[n_supported++] = ewmh->_NET_WM_WINDOW_TYPE_SPLASH;
    supported_atoms[n_supported++] = ewmh->_NET_WM_WINDOW_TYPE_TOOLTIP;
    supported_atoms[n_supported++] = net_wm_win_type_notif;
    supported_atoms[n_supported++] = net_wm_icon_geometry;

    /* Implemented all along but never announced: a pager consulting
     * this list to learn whether the visible-name pair is worth reading
     * concluded it was not, and fell back to the untruncated title even
     * though the truncated form was there. */
    supported_atoms[n_supported++] = ewmh->_NET_WM_VISIBLE_NAME;
    supported_atoms[n_supported++] = ewmh->_NET_WM_VISIBLE_ICON_NAME;
    supported_atoms[n_supported++] = ewmh->_NET_WM_PID;
    supported_atoms[n_supported++] = ewmh->_NET_WM_USER_TIME_WINDOW;

    for (list_item_td *snode = list_head(stages);
            snode != NULL;
            snode = list_next(snode)) {
        stage_td *const stage = (stage_td *) list_data(snode);

        if (stage == NULL || stage->screen == NULL) {
            continue;
        }

        xcb_ewmh_set_supporting_wm_check(ewmh,
                stage->screen->root, support);
        xcb_ewmh_set_supported(ewmh, (int) stage->id,
                n_supported, supported_atoms);

        /* ICCCM §4.1.3: announce fixed icon dimensions on the root
         * window */
        if (wm_icon_size_atom != XCB_ATOM_NONE) {
            xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                    stage->screen->root, wm_icon_size_atom,
                    wm_icon_size_atom, 32, 6, icon_size_hints);
        }
    }

    xcb_flush(connection);
    return 0;
}


/* Synchronize EWMH root properties for all managed stages */
void wm_ewmh_sync(wm_td *wm)
{
    xcb_connection_t *connection = wm_connection(wm);
    list_td *stages = wm_stages(wm);
    xcb_ewmh_connection_t *const ewmh =
        xcb_ewmh_connection_get();

    if (wm == NULL || stages == NULL || wm_ewmh(wm) == NULL) {
        return;
    }

    for (list_item_td *snode = list_head(stages);
            snode != NULL;
            snode = list_next(snode)) {
        stage_td *const stage = (stage_td *) list_data(snode);
        desktop_td *current;
        xcb_window_t active = XCB_NONE;
        xcb_ewmh_coordinates_t *viewport;
        uint32_t viewport_columns = 1u;
        uint32_t viewport_rows = 1u;

        if (stage == NULL) {
            continue;
        }

        /* The pannable area can be wider and/or taller than the
         * physical screen by this many whole screens; a stage with no
         * 'config', or an 'id' past 'CONFIG_MAX_SCREENS', simply
         * reports the physical screen size back, the same 1x1
         * 'config_viewport_s' fallback every other reader of this field
         * already falls back to. */
        if (stage->config != NULL &&
                stage->id < (uint32_t) CONFIG_MAX_SCREENS) {
            viewport_columns = stage->config->base
                .screens[stage->id].viewport.columns;
            viewport_rows = stage->config->base
                .screens[stage->id].viewport.rows;
        }

        xcb_ewmh_set_number_of_desktops(ewmh,
                (int) stage->id, stage->desktop_count);
        xcb_ewmh_set_current_desktop(ewmh,
                (int) stage->id, stage->desktop_cur);
        xcb_ewmh_set_desktop_geometry(ewmh,
                (int) stage->id,
                stage->properties.dim.w * viewport_columns,
                stage->properties.dim.h * viewport_rows);
        viewport = calloc(stage->desktop_count,
                sizeof(xcb_ewmh_coordinates_t));
        if (viewport != NULL) {
            for (uint32_t i = 0; i < stage->desktop_count; i++) {
                const desktop_td *const d =
                    stage_desktop_get(stage, i);

                if (d != NULL) {
                    viewport[i].x = (uint32_t) d->viewport_origin.x;
                    viewport[i].y = (uint32_t) d->viewport_origin.y;
                }
            }
            xcb_ewmh_set_desktop_viewport(ewmh,
                    (int) stage->id, stage->desktop_count,
                    viewport);
            free(viewport);
        }

        current = stage_desktop_get(stage, stage->desktop_cur);
        if (current != NULL && current->client_active_id != XCB_NONE) {
            const client_td *active_client =
                lookup_find_client(stages,
                        current->client_active_id, NULL, NULL);
            if (active_client != NULL) {
                active = active_client->window;
            }
        }

        xcb_ewmh_set_active_window(ewmh,
                (int) stage->id, active);
        xcb_ewmh_set_showing_desktop(ewmh,
                (int) stage->id,
                (stage->is_showing_desktop) ? 1u : 0u);
        s_wm_sync_desktop_names(stage);
        s_wm_sync_desktop_layout(stage);
        s_wm_sync_workarea(stage);
        s_wm_sync_client_lists(stage);
    }

    xcb_flush(connection);
}
