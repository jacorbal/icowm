/**
 * @file menu/context/submenu/desktop.c
 *
 * @brief Shared "Send to desktop" context menu submenu implementation
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
#include <stdint.h>
#include <stdio.h>      /* snprintf */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/uistr.h>
#include <i18n.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <surface.h>

/* Menu includes */
#include <menu/context/ctxmenu.h>

/* Local includes */
#include <menu/context/submenu/desktop.h>


/** Most desktops this submenu ever lists, tied to @c CONFIG_MAX_DESKTOPS
 *  itself, the one real source of truth for how many a surface can
 *  ever have, rather than an independent number of its own that could
 *  silently drift out of step with it */
#define S_MAX_DESKTOPS CONFIG_MAX_DESKTOPS


/**
 * @brief Userdata structure passed to the "Send to desktop" callback
 */
typedef struct {
    client_td *client;      /**< Target client */
    desktop_td *src;        /**< Source desktop */
    desktop_td *dst;        /**< Destination desktop */
} s_send_data_td;


/** Entries for the "Send to desktop" submenu */
static ctxmenu_entry_td s_desk_entries[S_MAX_DESKTOPS + 2];

/** State for the "Send to desktop" child menu */
static ctxmenu_state_td s_desk_state;

/** Per-desktop userdata pool for "Send to desktop" callbacks */
static s_send_data_td s_send_data[S_MAX_DESKTOPS + 1];


/**
 * @brief Context threaded through @a s_desktop_entry_visit
 */
struct s_desk_entry_ctx_s {
    client_td *client;              /**< Client the entries send */
    desktop_td *current;            /**< Desktop it is on already */
    const surface_td *surface;      /**< Surface being offered */
    uint32_t count;                 /**< Entries built so far */
    uint32_t index;                 /**< Desktop index reached */
    bool is_pinned;                 /**< Whether it is on all already */
};


/**
 * @brief Callback: send client to a specific desktop
 *
 * @param connection XCB connection (unused)
 * @param userdata   Pointer to this entry's own @c s_send_data_td
 *
 * @note Complexity: @e O(1)
 */
static void s_cb_send_to_desktop(xcb_connection_t *connection,
        void *userdata)
{
    s_send_data_td *d;

    (void) connection;

    if (userdata == NULL) {
        return;
    }
    d = (s_send_data_td *) userdata;
    if (d->client == NULL || d->src == NULL || d->dst == NULL) {
        return;
    }

    enact_desktop_client_send(d->src, d->client, d->dst);
}


/**
 * @brief Callback: toggle whether the target client is pinned to
 *        every desktop
 *
 * @param connection XCB connection (unused)
 * @param userdata   Target @c client_td
 *
 * @note Complexity: @e O(1)
 */
static void s_cb_toggle_pin(xcb_connection_t *connection, void *userdata)
{
    (void) connection;

    if (userdata == NULL) {
        return;
    }

    enact_client_toggle_pin((client_td *) userdata);
}


/**
 * @brief Build one desktop's "send there" entry
 *
 * @param desktop Desktop reached by the walk
 * @param data    The @c s_desk_entry_ctx_s being built
 *
 * @note Stops building once the menu is full, the whole walk still
 *       running: a visitor has no way to end one
 * @note Complexity: @e O(1)
 */
static void s_desktop_entry_visit(desktop_td *desktop, void *data)
{
    struct s_desk_entry_ctx_s *const ctx = data;
    char label[WM_DESKTOP_MAX_LENGTH_NAME + 64];
    uint32_t n;

    if (ctx == NULL || ctx->count >= S_MAX_DESKTOPS) {
        return;
    }

    n = ctx->count;
    surface_desktop_label(ctx->surface, ctx->index, desktop->name,
            false, true, label, sizeof(label));
    (void) snprintf(s_desk_entries[n].label,
            sizeof(s_desk_entries[n].label), "%s%s%s",
            MENU_CONTEXT_CTXMENU_LABEL_PREFIX, label,
            MENU_CONTEXT_CTXMENU_LABEL_SUFFIX);

    s_desk_entries[n].type = CTXMENU_COMMAND;
    /* A pinned client is already on every desktop, so there is
     * nowhere left to send it: every row is refused and only the
     * unpin entry below the separator stays live.  Unpinned, only the
     * desktop it already sits on is refused */
    s_desk_entries[n].is_disabled = ctx->is_pinned ||
        (desktop->id == ctx->current->id);
    s_send_data[n].client = ctx->client;
    s_send_data[n].src = ctx->current;
    s_send_data[n].dst = desktop;
    s_desk_entries[n].on_activate = s_cb_send_to_desktop;
    s_desk_entries[n].userdata = &s_send_data[n];
    ctx->count++;
    ctx->index++;
}


int ctxmenu_submenu_desktop_build(surface_td *surface,
        desktop_td *desktop, client_td *client,
        ctxmenu_entry_td **out_entries, ctxmenu_state_td **out_state)
{
    struct s_desk_entry_ctx_s desk_ctx;
    int n;
    bool is_pinned;

    if (surface == NULL || desktop == NULL || client == NULL ||
            out_entries == NULL || out_state == NULL ||
            surface->desktop_count <= 1u) {
        return 0;
    }

    is_pinned = (client->properties.flags & CLIENT_FLAG_PIN) != 0u;

    memset(s_desk_entries, 0, sizeof(s_desk_entries));

    desk_ctx.client = client;
    desk_ctx.current = desktop;
    desk_ctx.surface = surface;
    desk_ctx.count = 0u;
    desk_ctx.index = 0u;
    desk_ctx.is_pinned = is_pinned;
    surface_desktops_walk(surface, s_desktop_entry_visit, &desk_ctx);
    n = (int) desk_ctx.count;

    /* Separates the numbered-desktop entries above from the pin/unpin
     * one below, only when there actually are any.  With none (an empty
     * or single-surface edge case), a bare separator would lead
     * nowhere. */
    if (n > 0) {
        s_desk_entries[n].type = CTXMENU_SEPARATOR;
        ++n;
    }

    /* "All desktops" entry for pin support.  When the client is
     * already pinned, relabel it as an active un-pin action instead of
     * disabling it, since toggling the pin flag on this entry already
     * works both ways and there is otherwise no menu entry to remove
     * a pin once set */
    if (is_pinned) {
        safe_strncpy(s_desk_entries[n].label,
                _(STR_WINCMENU_THIS_DESKTOP_UNPIN),
                sizeof(s_desk_entries[n].label) - 1u);
    } else {
        safe_strncpy(s_desk_entries[n].label,
                _(STR_WINCMENU_ALL_DESKTOPS_PIN),
                sizeof(s_desk_entries[n].label) - 1u);
    }

    s_desk_entries[n].type = CTXMENU_COMMAND;
    s_desk_entries[n].is_disabled = false;
    s_desk_entries[n].on_activate = s_cb_toggle_pin;
    s_desk_entries[n].userdata = client;
    ++n;

    memset(&s_desk_state, 0, sizeof(s_desk_state));
    s_desk_state.window = XCB_WINDOW_NONE;
    s_desk_state.entries = s_desk_entries;
    s_desk_state.entry_count = n;

    *out_entries = s_desk_entries;
    *out_state = &s_desk_state;
    return n;
}
