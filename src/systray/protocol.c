/**
 * @file systray/protocol.c
 *
 * @brief Systray protocol handling: selection ownership, window
 *        creation, and icon docking
 *
 * The low-level mechanics of being a system tray manager.  Interning
 * the atoms the protocol needs, creating the dock window, acquiring and
 * releasing the @c _NET_SYSTEM_TRAY_Sn selection per the ICCCM
 * manager-selection convention, and reparenting/embedding an icon
 * window that requests to dock.  Where the tray and its icons end up on
 * screen is a separate concern; see @c systray/layout.c.
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
#include <stdio.h>      /* snprintf, NULL */
#include <stdlib.h>     /* free */
#include <string.h>     /* memset, memcpy */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Utils include */
#include <utils/safe/safestr.h>
#include <utils/xcb/atom.h>
#include <utils/xcb/selection.h>

/* Project includes */
#include <logger.h>
#include <wm.h>

/* Local includes */
#include <systray/internal.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>


/**
 * @brief Best-effort sort key for an icon window.  Its @c WM_CLASS
 *        instance name
 *
 * Used only by the alphabetical @c order policies; left as an empty
 * string when the property is absent or unreadable, which sorts before
 * any named icon.
 *
 * @param icon     Icon window to query
 * @param out      Destination buffer
 * @param out_size Size of @p out in bytes
 */
static void s_systray_icon_sort_key_fetch(xcb_window_t icon,
        char *out, size_t out_size)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    const char *value;
    int len;

    out[0] = '\0';
    if (out_size == 0u) {
        return;
    }

    cookie = xcb_get_property(xcb_connection_get(), 0, icon,
            XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 64);
    reply = xcb_get_property_reply(xcb_connection_get(), cookie, NULL);
    if (reply == NULL) {
        return;
    }

    value = (const char *) xcb_get_property_value(reply);
    len = xcb_get_property_value_length(reply);
    if (value != NULL && len > 0) {
        size_t copy_len;

        /* 'WM_CLASS' is "instance\0class\0"; take the instance name up
         * to its terminating NUL, or the whole reply if it has none */
        for (copy_len = 0u;
                copy_len < (size_t) len && value[copy_len] != '\0';
                ++copy_len) {
            /* measuring only */
        }
        if (copy_len >= out_size) {
            copy_len = out_size - 1u;
        }
        memcpy(out, value, copy_len);
        out[copy_len] = '\0';
    }

    free(reply);
}


/**
 * @brief Index at which a newly docked icon should be inserted
 *
 * Implements the @p systray.order configuration policy.  Appends,
 * prepends, or finds the correct position to keep @p s_tray.icons
 * sorted by @p sort_key.
 *
 * @param sort_key Candidate icon's sort key
 *
 * @return Index in [0, @p s_tray.icon_count] at which to insert
 *
 * @note Complexity: @e O(n) for the alphabetical policies, @e O(1)
 *       otherwise, where @e n is the current icon count
 *
 * @see @a s_systray_icon_sort_key_fetch
 */
static uint16_t s_systray_insert_index(const char *sort_key)
{
    switch (s_tray.order) {
        case CONFIG_SYSTRAY_ORDER_RIGHT_TO_LEFT:
            return 0u;

        case CONFIG_SYSTRAY_ORDER_ASCENDING:
            for (uint16_t i = 0u; i < s_tray.icon_count; ++i) {
                if (safe_strcmp(sort_key,
                            s_tray.icons[i].sort_key) < 0) {
                    return i;
                }
            }
            return s_tray.icon_count;

        case CONFIG_SYSTRAY_ORDER_DESCENDING:
            for (uint16_t i = 0u; i < s_tray.icon_count; ++i) {
                if (safe_strcmp(sort_key,
                            s_tray.icons[i].sort_key) > 0) {
                    return i;
                }
            }
            return s_tray.icon_count;

        case CONFIG_SYSTRAY_ORDER_LEFT_TO_RIGHT:
            return s_tray.icon_count;
    }

    /* Just to avoid compiler warnings... */
    return s_tray.icon_count;
}


/* Re-sort every already-docked icon by the current 's_tray.order'
 * policy */
void systray_protocol_resort(void)
{
    if (s_tray.order != CONFIG_SYSTRAY_ORDER_ASCENDING &&
            s_tray.order != CONFIG_SYSTRAY_ORDER_DESCENDING) {
        return;
    }

    for (uint16_t i = 1u; i < s_tray.icon_count; ++i) {
        systray_icon_td key = s_tray.icons[i];
        uint16_t j = i;

        while (j > 0u &&
                ((s_tray.order == CONFIG_SYSTRAY_ORDER_ASCENDING)
                    ? (safe_strcmp(key.sort_key,
                            s_tray.icons[j - 1u].sort_key) < 0)
                    : (safe_strcmp(key.sort_key,
                            s_tray.icons[j - 1u].sort_key) > 0))) {
            s_tray.icons[j] = s_tray.icons[j - 1u];
            --j;
        }
        s_tray.icons[j] = key;
    }
}


/* Dock an icon window: reparent it in, embed it, and reflow */
void systray_protocol_dock(xcb_window_t icon)
{
    uint32_t attr_values[1];
    uint32_t size_values[2];
    xcb_client_message_event_t ev;
    char sort_key[64];
    uint16_t insert_at;

    if (!s_tray.is_selection_owned || icon == XCB_WINDOW_NONE) {
        return;
    }

    /* A dock request for a window already tracked is refused outright
     * rather than adding a second entry for it:
     * 'systray_handle_destroy' below only ever removes the first
     * matching entry it finds and returns immediately, so a second one
     * for the same window would be left dangling, still referencing the
     * window once it is actually destroyed, and 'systray_layout_reflow'
     * would keep trying to configure a window ID that either errors out
     * harmlessly or, worse, has since been reused by the X server for
     * something else entirely. */
    for (uint16_t i = 0u; i < s_tray.icon_count; ++i) {
        if (s_tray.icons[i].window == icon) {
            LOGGER_NOTICE("Window 0x%x is already docked in the" \
                    " systray; ignoring duplicate dock request",
                    icon);
            return;
        }
    }

    if (s_tray.icon_count >= WM_SYSTRAY_MAX_ICONS) {
        LOGGER_NOTICE("Systray is full; ignoring dock request for" \
                " window 0x%x", icon);
        return;
    }

    /* Track 'StructureNotify' so 'systray_handle_destroy' learns when
     * the icon's application exits or otherwise destroys the window */
    attr_values[0] = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    xcb_change_window_attributes(xcb_connection_get(), icon,
            XCB_CW_EVENT_MASK, attr_values);

    xcb_window_reparent(icon, s_tray.window, 0, 0);

    size_values[0] = s_tray.pixmap_size;
    size_values[1] = s_tray.pixmap_size;
    xcb_configure_window(xcb_connection_get(), icon,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            size_values);

    xcb_window_show(icon);

    /* The XEMBED handshake tells the icon it is now embedded, and by
     * whom */
    memset(&ev, 0, sizeof(ev));
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.format = 32;
    ev.window = icon;
    ev.type = s_tray.xembed_atom;
    ev.data.data32[0] = XCB_CURRENT_TIME;
    ev.data.data32[1] = SYSTRAY_XEMBED_EMBEDDED_NOTIFY;
    ev.data.data32[2] = 0u;
    ev.data.data32[3] = s_tray.window;
    ev.data.data32[4] = 0u;
    xcb_send_event(xcb_connection_get(), 0, icon,
            XCB_EVENT_MASK_NO_EVENT, (const char *) &ev);

    s_systray_icon_sort_key_fetch(icon, sort_key, sizeof(sort_key));
    insert_at = s_systray_insert_index(sort_key);

    for (uint16_t j = s_tray.icon_count; j > insert_at; --j) {
        s_tray.icons[j] = s_tray.icons[j - 1u];
    }
    s_tray.icons[insert_at].window = icon;
    memcpy(s_tray.icons[insert_at].sort_key, sort_key, sizeof(sort_key));
    s_tray.icon_count++;

    LOGGER_DEBUG("Docked systray icon 0x%x at position %u (%u total)",
            icon, (unsigned int) insert_at,
            (unsigned int) s_tray.icon_count);

    systray_layout_reflow();
}


/* Re-apply the theme's background color, border color, and border width
 * to the already-existing tray window */
void systray_protocol_apply_theme_style(void)
{
    if (!s_tray.is_window_ready || s_tray.theme == NULL) {
        return;
    }

    xcb_change_window_attributes(xcb_connection_get(), s_tray.window,
            XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
            (const uint32_t[]) {
                s_tray.theme->systray.style.color.background,
                s_tray.theme->systray.style.border.color
            });
    xcb_configure_window(xcb_connection_get(), s_tray.window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH,
            (const uint32_t[]) {
                s_tray.theme->systray.style.border.width
            });
}


/* Create the tray window and intern its atoms, once
 *
 * - Idempotent: does nothing (beyond returning success) if
 * 's_tray.is_window_ready' is already 'true'.
 * - Does not acquire the selection; see
 * 'systray_protocol_selection_acquire'. */
bool systray_protocol_window_ensure(const wm_td *wm)
{
    surface_td *surface;
    char selection_name[32];
    uint32_t mask;
    uint32_t values[4];
    xcb_connection_t *connection = wm_connection(wm);
    config_td *config = wm_config(wm);
    list_td *surfaces = wm_surfaces(wm);

    if (s_tray.is_window_ready) {
        return true;
    }

    if (wm == NULL || connection == NULL || config == NULL ||
            surfaces == NULL) {
        return false;
    }

    surface = (surface_td *) list_data(list_head(surfaces));
    if (surface == NULL || surface->screen == NULL) {
        return false;
    }

    s_tray.surface = surface;

    (void) snprintf(selection_name, sizeof(selection_name),
            "_NET_SYSTEM_TRAY_S%u", (unsigned int) surface->id);
    s_tray.selection_atom = atom_intern(connection, selection_name,
            false);
    s_tray.manager_atom = atom_intern(connection, "MANAGER", false);
    s_tray.opcode_atom = atom_intern(connection,
            "_NET_SYSTEM_TRAY_OPCODE", false);
    s_tray.orientation_atom = atom_intern(connection,
            "_NET_SYSTEM_TRAY_ORIENTATION", false);
    s_tray.visual_atom = atom_intern(connection,
            "_NET_SYSTEM_TRAY_VISUAL", false);
    s_tray.xembed_atom = atom_intern(connection, "_XEMBED", false);

    if (s_tray.selection_atom == XCB_ATOM_NONE ||
            s_tray.opcode_atom == XCB_ATOM_NONE ||
            s_tray.xembed_atom == XCB_ATOM_NONE) {
        LOGGER_WARNING("Failed to intern systray atoms;" \
                " built-in systray disabled", L_NARG);
        return false;
    }

    s_tray.window = xcb_generate_id(connection);
    mask = XCB_CW_BACK_PIXEL   |
        XCB_CW_BORDER_PIXEL    |
        XCB_CW_OVERRIDE_REDIRECT |
        XCB_CW_EVENT_MASK;
    values[0] = config->theme.systray.style.color.background;
    values[1] = config->theme.systray.style.border.color;
    values[2] = 1;   /* override_redirect: never managed as a client */
    values[3] = XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        /* Without this, a docked icon's resize attempt on itself
         * (many apps resize their tray icon for DPI or content reasons)
         * is applied by the server directly with no 'ConfigureRequest'
         * ever generated, silently undoing the configured
         * 's_tray.pixmap_size' this module forces on it at dock time;
         * see 'systray_icon_size_enforce' in 'systray.c'. */
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
        /* Without this, the server never generates an 'Expose' event
         * for this window at all, regardless of how correct
         * 'handler_expose''s systray check is.  A region covered and
         * then uncovered stays blank until 'systray_clock_tick' happens
         * to redraw it anyway on its own next per-second update, rather
         * than right away. */
        XCB_EVENT_MASK_EXPOSURE;

    xcb_create_window(connection, XCB_COPY_FROM_PARENT,
            s_tray.window, surface->screen->root,
            0, 0, 1, s_tray.height,
            (uint16_t) config->theme.systray.style.border.width,
            XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
            mask, values);
    atom_set_window_opacity(connection, s_tray.window,
            config_theme_opacity_to_raw(
                config->theme.systray.style.opacity));

    s_tray.is_window_ready = true;
    return true;
}


/* Acquire the tray selection on the already-created window */
bool systray_protocol_selection_acquire(void)
{
    uint32_t orientation;

    if (s_tray.is_selection_owned) {
        return true;
    }
    if (!s_tray.is_window_ready) {
        return false;
    }

    if (!util_xcb_acquire_manager_selection(xcb_connection_get(),
                s_tray.window, s_tray.selection_atom,
                s_tray.manager_atom, s_tray.surface->screen->root)) {
        LOGGER_NOTICE("Another systray manager already owns the tray" \
                " selection; built-in systray stays disabled", L_NARG);
        return false;
    }

    orientation = 0u;   /* '_NET_SYSTEM_TRAY_ORIENTATION_HORZ' */
    xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
            s_tray.window, s_tray.orientation_atom, XCB_ATOM_CARDINAL,
            32, 1, &orientation);
    xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
            s_tray.window, s_tray.visual_atom, XCB_ATOM_VISUALID,
            32, 1, &s_tray.surface->screen->root_visual);

    s_tray.is_selection_owned = true;

    LOGGER_INFO("Systray dock active on surface %u (selection atom" \
            " 0x%x, %u icon(s) already docked)", s_tray.surface->id,
            (unsigned int) s_tray.selection_atom,
            (unsigned int) s_tray.icon_count);

    return true;
}


/* Release the tray selection, keeping the window and icons */
void systray_protocol_selection_release(void)
{
    if (!s_tray.is_selection_owned) {
        return;
    }

    xcb_set_selection_owner(xcb_connection_get(), XCB_NONE,
            s_tray.selection_atom, XCB_CURRENT_TIME);
    s_tray.is_selection_owned = false;
    /* Actually unmaps only if 'is_active' is also already false by now:
     * systray_reload's 'disabled' path always sets that first,
     * right before calling this.  Still safe to call from
     * systray_shutdown instead, where 'is_active' may still be true
     * here, since that caller destroys the window outright right after
     * regardless of whether this unmapped it first. */
    systray_layout_reflow();

    LOGGER_INFO("Systray selection released (%u icon(s) kept docked" \
            " in the background)", (unsigned int) s_tray.icon_count);
}
