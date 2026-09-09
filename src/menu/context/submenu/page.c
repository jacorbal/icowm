/**
 * @file menu/context/submenu/page.c
 *
 * @brief Shared "Send to page" context menu submenu implementation
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

/* CMD includes */
#include <cmds/surface.h>

/* Menu includes */
#include <menu/context/ctxmenu.h>

/* Local includes */
#include <menu/context/submenu/page.h>


/** Most pages this submenu ever lists.  The configured viewport grid
 *  is capped per axis by @c CONFIG_VIEWPORT_MAX_PAGES
 *  (@c defs/config.h), so their product is the most a submenu could
 *  ever need. */
#define S_MAX_PAGES \
    (CONFIG_VIEWPORT_MAX_PAGES * CONFIG_VIEWPORT_MAX_PAGES)


/**
 * @brief Userdata structure passed to the "Send to page" callback
 */
typedef struct {
    surface_td *surface; /**< Surface owning the viewport */
    client_td *client;   /**< Target client */
    uint32_t col;        /**< Destination page column */
    uint32_t row;        /**< Destination page row */
} s_page_send_td;


/** Entries for the "Send to page" submenu */
static ctxmenu_entry_td s_page_entries[S_MAX_PAGES + 2];

/** State for the "Send to page" child menu */
static ctxmenu_state_td s_page_state;

/** Per-page userdata pool for "Send to page" callbacks */
static s_page_send_td s_page_send_data[S_MAX_PAGES];


/**
 * @brief Send the target client to the page one "Send to page" entry
 *        names
 *
 * @param connection Unused; the move needs no connection of its own
 * @param userdata   Pointer to this entry's own @c s_page_send_td
 *
 * @note Complexity: @e O(1)
 */
static void s_cb_send_to_page(xcb_connection_t *connection,
        void *userdata)
{
    const s_page_send_td *const send = userdata;

    (void) connection;

    if (send == NULL || send->client == NULL ||
            send->surface == NULL) {
        return;
    }

    enact_client_send_to_page(send->surface, send->client,
            send->col, send->row);
}


/**
 * @brief Callback: toggle whether the target client is stuck to
 *        every page
 *
 * @param connection XCB connection (unused)
 * @param userdata   Target @c client_td
 *
 * @note Complexity: @e O(1)
 */
static void s_cb_toggle_sticky(xcb_connection_t *connection,
        void *userdata)
{
    (void) connection;

    if (userdata == NULL) {
        return;
    }

    enact_client_toggle_stick((client_td *) userdata);
}


int ctxmenu_submenu_page_build(surface_td *surface, desktop_td *desktop,
        client_td *client, ctxmenu_entry_td **out_entries,
        ctxmenu_state_td **out_state)
{
    uint32_t columns;
    uint32_t rows;
    uint32_t cur_col = 0u;
    uint32_t cur_row = 0u;
    bool has_current;
    bool is_sticky;
    char label[64];
    int n = 0;

    if (surface == NULL || desktop == NULL || client == NULL ||
            out_entries == NULL || out_state == NULL ||
            !surface_viewport_has_room(surface)) {
        return 0;
    }

    surface_viewport_dims(surface, &columns, &rows);
    /* A grid of one page has nowhere to send anything, so it gets no
     * submenu even where the room check above somehow said otherwise */
    if (columns * rows <= 1u) {
        return 0;
    }

    is_sticky = client_is_sticky(client);
    has_current = scmd_surface_viewport_client_page(surface, desktop,
            client, &cur_col, &cur_row);

    memset(s_page_entries, 0, sizeof(s_page_entries));

    for (uint32_t row = 0u; row < rows && n < (int) S_MAX_PAGES;
            ++row) {
        for (uint32_t col = 0u; col < columns &&
                n < (int) S_MAX_PAGES; ++col) {
            (void) snprintf(label, sizeof(label),
                    _(STR_WINCMENU_PAGE), col, row);
            (void) snprintf(s_page_entries[n].label,
                    sizeof(s_page_entries[n].label), "%s%s%s",
                    MENU_CONTEXT_CTXMENU_LABEL_PREFIX, label,
                    MENU_CONTEXT_CTXMENU_LABEL_SUFFIX);
            s_page_entries[n].type = CTXMENU_COMMAND;
            /* A sticky client is already on every page, so there is
             * nowhere left to send it: every row is refused and only
             * the unsticky entry below the separator stays live.
             * Otherwise only the page it already sits on is refused */
            s_page_entries[n].is_disabled = is_sticky ||
                (has_current && col == cur_col && row == cur_row);
            s_page_send_data[n].surface = surface;
            s_page_send_data[n].client = client;
            s_page_send_data[n].col = col;
            s_page_send_data[n].row = row;
            s_page_entries[n].on_activate = s_cb_send_to_page;
            s_page_entries[n].userdata = &s_page_send_data[n];
            ++n;
        }
    }

    /* Separates the numbered-page entries above from the
     * sticky/unsticky one below, exactly as
     * 'ctxmenu_submenu_desktop_build' separates its own desktops from
     * its pin entry */
    if (n > 0) {
        s_page_entries[n].type = CTXMENU_SEPARATOR;
        ++n;
    }

    /* "All pages" entry for sticky support, the direct counterpart to
     * the pin entry in "Send to desktop": relabeled in place when the
     * client is already sticky rather than disabled, since toggling
     * works both ways and there would otherwise be no entry anywhere
     * to clear the flag once set */
    safe_strncpy(s_page_entries[n].label,
            (is_sticky) ? _(STR_WINCMENU_THIS_PAGE_UNSTICK)
                : _(STR_WINCMENU_ALL_PAGES_STICK),
            sizeof(s_page_entries[n].label) - 1u);
    s_page_entries[n].type = CTXMENU_COMMAND;
    s_page_entries[n].is_disabled = false;
    s_page_entries[n].on_activate = s_cb_toggle_sticky;
    s_page_entries[n].userdata = client;
    ++n;

    memset(&s_page_state, 0, sizeof(s_page_state));
    s_page_state.window = XCB_WINDOW_NONE;
    s_page_state.entries = s_page_entries;
    s_page_state.entry_count = n;

    *out_entries = s_page_entries;
    *out_state = &s_page_state;
    return n;
}
