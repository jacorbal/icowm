/**
 * @file systray.c
 *
 * @brief Built-in systray dock implementation
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
#include <stdlib.h>     /* free */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils include */
#include <utils/safe/safestr.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <systray.h>


/** Side length in pixels of each docked icon's embed window */
#define SYSTRAY_ICON_SIZE (24u)

/** Padding in pixels around and between icons */
#define SYSTRAY_ICON_PAD (4u)

/**
 * Upper bound on simultaneously docked icons; a plain fixed array is
 * enough for a systray and keeps this module allocation-free
 */
#define SYSTRAY_MAX_ICONS (32u)

/**
 * XEMBED opcode sent to a newly docked icon (@c XEMBED_EMBEDDED_NOTIFY)
 */
#define SYSTRAY_XEMBED_EMBEDDED_NOTIFY (0u)

/**
 * @c _NET_SYSTEM_TRAY_OPCODE opcode requesting an icon be docked
 */
#define SYSTRAY_OPCODE_REQUEST_DOCK (0u)


/**
 * @brief One docked icon window
 */
typedef struct {
    xcb_window_t window;   /**< Icon's (reparented) top-level window */
    char sort_key[64];     /**< Best-effort @c WM_CLASS instance name,
                                used only for the alphabetical order
                                policies; empty when unavailable, which
                                sorts before any named icon */
} systray_icon_td;


/**
 * @brief Module-level built-in systray state
 *
 * A single tray instance for the whole window manager, matching
 * @c config.systray being a single global (not per-surface) setting.
 */
static struct {
    bool window_ready;              /**< Window created, atoms interned;
                                         persists across is-enabled
                                         toggles so docked icons are
                                         never evicted just because the
                                         tray was disabled */
    bool selection_owned;           /**< Currently owns the
                                         @c _NET_SYSTEM_TRAY_Sn
                                         selection; gates accepting new
                                         dock requests and showing the
                                         window at all */
    xcb_connection_t *connection;
    surface_td *surface;            /**< Surface the tray is docked on */
    xcb_window_t window;            /**< Tray dock window */
    xcb_atom_t selection_atom;      /**< @c _NET_SYSTEM_TRAY_Sn */
    xcb_atom_t manager_atom;        /**< @c MANAGER */
    xcb_atom_t opcode_atom;         /**< @c _NET_SYSTEM_TRAY_OPCODE */
    xcb_atom_t orientation_atom;    /**< @c _NET_SYSTEM_TRAY_ORIENTATION */
    xcb_atom_t visual_atom;         /**< @c _NET_SYSTEM_TRAY_VISUAL */
    xcb_atom_t xembed_atom;         /**< @c _XEMBED */
    enum config_systray_position_e position;
    enum config_systray_order_e order;
    systray_icon_td icons[SYSTRAY_MAX_ICONS];
    uint16_t icon_count;
} s_tray;


/**
 * @brief Intern an atom by name and return it, or @c XCB_ATOM_NONE
 *
 * @param connection X connection
 * @param name       Atom name (need not be null-terminated beyond @p len)
 * @param len        Length of @p name in bytes
 *
 * @return The interned atom, or @c XCB_ATOM_NONE on failure
 */
static xcb_atom_t s_systray_intern(xcb_connection_t *connection,
        const char *name, uint16_t len)
{
    xcb_intern_atom_reply_t *reply;
    xcb_atom_t atom;

    reply = xcb_intern_atom_reply(connection,
            xcb_intern_atom(connection, 0, len, name), NULL);
    atom = (reply != NULL) ? reply->atom : XCB_ATOM_NONE;
    free(reply);

    return atom;
}


/**
 * @brief Pixel width the tray window needs for the current icon count
 *
 * @return 0 when no icons are docked (the tray window stays unmapped in
 *         that case), otherwise enough to fit every icon with padding
 *         around and between each
 */
static uint16_t s_systray_content_width(void)
{
    if (s_tray.icon_count == 0u) {
        return 0u;
    }

    return (uint16_t) (SYSTRAY_ICON_PAD +
            s_tray.icon_count * (SYSTRAY_ICON_SIZE + SYSTRAY_ICON_PAD));
}


/**
 * @brief Reposition the tray window and lay out its docked icons
 *
 * Unmaps the tray window while empty or while the selection is not
 * currently owned (e.g., disabled by configuration, or another tray
 * manager is active), so it never shows on screen in either case;
 * otherwise sizes and moves it to the configured corner of
 * @c s_tray.surface and arranges icons in a single horizontal row
 * inside it, in @c s_tray.icons order (see @c s_systray_dock for how
 * that order is maintained per the @c order policy).
 */
static void s_systray_reflow(void)
{
    uint16_t w;
    uint16_t h;
    int16_t x = 0;
    int16_t y = 0;
    uint32_t geom_values[4];

    if (!s_tray.window_ready || s_tray.surface == NULL) {
        return;
    }

    if (!s_tray.selection_owned || s_tray.icon_count == 0u) {
        xcb_unmap_window(s_tray.connection, s_tray.window);
        xcb_flush(s_tray.connection);
        return;
    }

    h = (uint16_t) (SYSTRAY_ICON_SIZE + 2u * SYSTRAY_ICON_PAD);
    w = s_systray_content_width();

    switch (s_tray.position) {
        case CONFIG_SYSTRAY_POSITION_TOP_LEFT:
            x = 0;
            y = 0;
            break;

        case CONFIG_SYSTRAY_POSITION_BOTTOM_LEFT:
            x = 0;
            y = (int16_t) ((int32_t) s_tray.surface->properties.dim.h -
                    (int32_t) h);
            break;

        case CONFIG_SYSTRAY_POSITION_BOTTOM_RIGHT:
            x = (int16_t) ((int32_t) s_tray.surface->properties.dim.w -
                    (int32_t) w);
            y = (int16_t) ((int32_t) s_tray.surface->properties.dim.h -
                    (int32_t) h);
            break;

        case CONFIG_SYSTRAY_POSITION_TOP_RIGHT:
            x = (int16_t) ((int32_t) s_tray.surface->properties.dim.w -
                    (int32_t) w);
            y = 0;
            break;
    }

    geom_values[0] = (uint32_t) x;
    geom_values[1] = (uint32_t) y;
    geom_values[2] = w;
    geom_values[3] = h;
    xcb_configure_window(s_tray.connection, s_tray.window,
            XCB_CONFIG_WINDOW_X     |
            XCB_CONFIG_WINDOW_Y     |
            XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT,
            geom_values);

    for (uint16_t i = 0u; i < s_tray.icon_count; ++i) {
        uint32_t icon_pos[2];

        icon_pos[0] = SYSTRAY_ICON_PAD +
            i * (SYSTRAY_ICON_SIZE + SYSTRAY_ICON_PAD);
        icon_pos[1] = SYSTRAY_ICON_PAD;
        xcb_configure_window(s_tray.connection, s_tray.icons[i].window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, icon_pos);
    }

    xcb_map_window(s_tray.connection, s_tray.window);
    geom_values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(s_tray.connection, s_tray.window,
            XCB_CONFIG_WINDOW_STACK_MODE, geom_values);

    xcb_flush(s_tray.connection);
}


/**
 * @brief Best-effort sort key for an icon window: its @c WM_CLASS
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
static void s_systray_fetch_sort_key(xcb_window_t icon,
        char *out, size_t out_size)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    const char *value;
    int len;
    size_t copy_len;

    out[0] = '\0';
    if (out_size == 0u) {
        return;
    }

    cookie = xcb_get_property(s_tray.connection, 0, icon,
            XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 64);
    reply = xcb_get_property_reply(s_tray.connection, cookie, NULL);
    if (reply == NULL) {
        return;
    }

    value = (const char *) xcb_get_property_value(reply);
    len = xcb_get_property_value_length(reply);
    if (value != NULL && len > 0) {
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
 * Implements the @c systray.order configuration policy: appends,
 * prepends, or finds the correct position to keep @c s_tray.icons
 * sorted by @p sort_key.
 *
 * @param sort_key Candidate icon's sort key (see
 *                  @c s_systray_fetch_sort_key)
 *
 * @return Index in @c [0, s_tray.icon_count] at which to insert
 *
 * @note Complexity: @e O(n) for the alphabetical policies, @e O(1)
 *       otherwise, where @e n is the current icon count
 */
static uint16_t s_systray_insert_index(const char *sort_key)
{
    uint16_t i;

    switch (s_tray.order) {
        case CONFIG_SYSTRAY_ORDER_RIGHT_TO_LEFT:
            return 0u;

        case CONFIG_SYSTRAY_ORDER_ASCENDING:
            for (i = 0u; i < s_tray.icon_count; ++i) {
                if (safe_strcmp(sort_key,
                            s_tray.icons[i].sort_key) < 0) {
                    return i;
                }
            }
            return s_tray.icon_count;

        case CONFIG_SYSTRAY_ORDER_DESCENDING:
            for (i = 0u; i < s_tray.icon_count; ++i) {
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


/**
 * @brief Dock an icon window: reparent it in, embed it, and reflow
 *
 * @param icon Icon window named by a @c SYSTEM_TRAY_REQUEST_DOCK
 *             request
 */
static void s_systray_dock(xcb_window_t icon)
{
    uint32_t attr_values[1];
    uint32_t size_values[2];
    xcb_client_message_event_t ev;
    char sort_key[64];
    uint16_t insert_at;

    if (!s_tray.selection_owned || icon == XCB_WINDOW_NONE) {
        return;
    }

    if (s_tray.icon_count >= SYSTRAY_MAX_ICONS) {
        LOGGER_NOTICE("Systray is full; ignoring dock request for" \
                " window 0x%x", icon);
        return;
    }

    /* Track 'StructureNotify' so 'systray_handle_destroy' learns when
     * the icon's application exits or otherwise destroys the window */
    attr_values[0] = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    xcb_change_window_attributes(s_tray.connection, icon,
            XCB_CW_EVENT_MASK, attr_values);

    xcb_reparent_window(s_tray.connection, icon, s_tray.window, 0, 0);

    size_values[0] = SYSTRAY_ICON_SIZE;
    size_values[1] = SYSTRAY_ICON_SIZE;
    xcb_configure_window(s_tray.connection, icon,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            size_values);

    xcb_map_window(s_tray.connection, icon);

    /* XEMBED handshake: tell the icon it is now embedded, and by whom */
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
    xcb_send_event(s_tray.connection, 0, icon, XCB_EVENT_MASK_NO_EVENT,
            (const char *) &ev);

    s_systray_fetch_sort_key(icon, sort_key, sizeof(sort_key));
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

    s_systray_reflow();
}


/**
 * @brief Create the tray window and intern its atoms, once
 *
 * Idempotent: does nothing (beyond returning success) if
 * @c s_tray.window_ready is already true.  Does not acquire the
 * selection; see @c s_systray_acquire_selection.
 *
 * @param wm Window manager state
 *
 * @return @c true on success (or if already ready), @c false if it
 *         could not be created
 */
static bool s_systray_ensure_window(wm_td *wm)
{
    surface_td *surface;
    char selection_name[32];
    int selection_name_len;
    uint32_t mask;
    uint32_t values[3];

    if (s_tray.window_ready) {
        return true;
    }

    if (wm == NULL || wm->connection == NULL || wm->config == NULL ||
            wm->surfaces == NULL) {
        return false;
    }

    surface = (surface_td *) list_data(list_head(wm->surfaces));
    if (surface == NULL || surface->screen == NULL) {
        return false;
    }

    s_tray.connection = wm->connection;
    s_tray.surface = surface;

    selection_name_len = snprintf(selection_name, sizeof(selection_name),
            "_NET_SYSTEM_TRAY_S%u", (unsigned int) surface->id);
    s_tray.selection_atom = s_systray_intern(wm->connection,
            selection_name,
            (selection_name_len > 0) ? (uint16_t) selection_name_len
                                      : 0u);
    s_tray.manager_atom = s_systray_intern(wm->connection, "MANAGER", 7u);
    s_tray.opcode_atom = s_systray_intern(wm->connection,
            "_NET_SYSTEM_TRAY_OPCODE", 23u);
    s_tray.orientation_atom = s_systray_intern(wm->connection,
            "_NET_SYSTEM_TRAY_ORIENTATION", 28u);
    s_tray.visual_atom = s_systray_intern(wm->connection,
            "_NET_SYSTEM_TRAY_VISUAL", 23u);
    s_tray.xembed_atom = s_systray_intern(wm->connection, "_XEMBED", 7u);

    if (s_tray.selection_atom == XCB_ATOM_NONE ||
            s_tray.opcode_atom == XCB_ATOM_NONE ||
            s_tray.xembed_atom == XCB_ATOM_NONE) {
        LOGGER_WARNING("Failed to intern systray atoms;" \
                " built-in systray disabled", L_NARG);
        return false;
    }

    s_tray.window = xcb_generate_id(wm->connection);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    values[0] = wm->config->theme.window.inactive.background_color;
    values[1] = 1;   /* override_redirect: never managed as a client */
    values[2] = XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    xcb_create_window(wm->connection, XCB_COPY_FROM_PARENT,
            s_tray.window, surface->screen->root,
            0, 0, 1,
            (uint16_t) (SYSTRAY_ICON_SIZE + 2u * SYSTRAY_ICON_PAD),
            0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
            mask, values);
    xcb_flush(wm->connection);

    s_tray.window_ready = true;
    return true;
}


/**
 * @brief Acquire the tray selection on the already-created window
 *
 * @return @c true if ownership was acquired (or already held),
 *         @c false if another tray manager already owns the selection
 *         or the window is not ready yet
 */
static bool s_systray_acquire_selection(void)
{
    xcb_get_selection_owner_cookie_t owner_cookie;
    xcb_get_selection_owner_reply_t *owner_reply;
    xcb_client_message_event_t manager_ev;
    uint32_t orientation;

    if (s_tray.selection_owned) {
        return true;
    }
    if (!s_tray.window_ready) {
        return false;
    }

    xcb_set_selection_owner(s_tray.connection, s_tray.window,
            s_tray.selection_atom, XCB_CURRENT_TIME);

    owner_cookie = xcb_get_selection_owner(s_tray.connection,
            s_tray.selection_atom);
    owner_reply = xcb_get_selection_owner_reply(s_tray.connection,
            owner_cookie, NULL);
    if (owner_reply == NULL || owner_reply->owner != s_tray.window) {
        LOGGER_NOTICE("Another systray manager already owns the tray" \
                " selection; built-in systray stays disabled", L_NARG);
        free(owner_reply);
        return false;
    }
    free(owner_reply);

    /* ICCCM manager-selection convention: announce ownership on the
     * root window so pagers and other tools notice a tray appeared */
    memset(&manager_ev, 0, sizeof(manager_ev));
    manager_ev.response_type = XCB_CLIENT_MESSAGE;
    manager_ev.format = 32;
    manager_ev.window = s_tray.surface->screen->root;
    manager_ev.type = s_tray.manager_atom;
    manager_ev.data.data32[0] = XCB_CURRENT_TIME;
    manager_ev.data.data32[1] = s_tray.selection_atom;
    manager_ev.data.data32[2] = s_tray.window;
    xcb_send_event(s_tray.connection, 0, s_tray.surface->screen->root,
            XCB_EVENT_MASK_STRUCTURE_NOTIFY, (const char *) &manager_ev);

    orientation = 0u;   /* _NET_SYSTEM_TRAY_ORIENTATION_HORZ */
    xcb_change_property(s_tray.connection, XCB_PROP_MODE_REPLACE,
            s_tray.window, s_tray.orientation_atom, XCB_ATOM_CARDINAL,
            32, 1, &orientation);
    xcb_change_property(s_tray.connection, XCB_PROP_MODE_REPLACE,
            s_tray.window, s_tray.visual_atom, XCB_ATOM_VISUALID,
            32, 1, &s_tray.surface->screen->root_visual);

    s_tray.selection_owned = true;
    xcb_flush(s_tray.connection);

    LOGGER_INFO("Systray dock active on surface %u (selection atom" \
            " 0x%x, %u icon(s) already docked)", s_tray.surface->id,
            (unsigned int) s_tray.selection_atom,
            (unsigned int) s_tray.icon_count);

    return true;
}


/**
 * @brief Release the tray selection, keeping the window and icons
 *
 * The tray window and any currently docked icons are left exactly as
 * they are, just hidden (see the @c s_tray struct comment on
 * @c window_ready).  Safe to call when the selection is not currently
 * owned.
 */
static void s_systray_release_selection(void)
{
    if (!s_tray.selection_owned) {
        return;
    }

    xcb_set_selection_owner(s_tray.connection, XCB_NONE,
            s_tray.selection_atom, XCB_CURRENT_TIME);
    s_tray.selection_owned = false;
    s_systray_reflow();   /* unmaps: 'selection_owned' is now false */

    LOGGER_INFO("Systray selection released (%u icon(s) kept docked" \
            " in the background)", (unsigned int) s_tray.icon_count);
}


/* Acquire the tray selection and create the dock window */
void systray_init(wm_td *wm)
{
    if (wm == NULL || wm->config == NULL ||
            !wm->config->base.systray.is_enabled) {
        return;
    }

    s_tray.position = wm->config->base.systray.position;
    s_tray.order = wm->config->base.systray.order;

    if (!s_systray_ensure_window(wm)) {
        return;
    }

    (void) s_systray_acquire_selection();
}


/* Release the tray selection and destroy the dock window */
void systray_shutdown(wm_td *wm)
{
    (void) wm;

    s_systray_release_selection();

    if (s_tray.window_ready && s_tray.connection != NULL &&
            s_tray.window != XCB_WINDOW_NONE) {
        /* Destroying the tray window implicitly reparents any still-
         * docked icons back to the root window; each icon's own
         * application is responsible for re-docking if a tray reappears
         * later, exactly as with every other systray.  This full
         * teardown is only for the window manager itself exiting;
         * toggling 'is-enabled' off goes through 'systray_reload',
         * which keeps the window and icons alive via
         * 's_systray_release_selection' instead. */
        xcb_destroy_window(s_tray.connection, s_tray.window);
        xcb_flush(s_tray.connection);
    }

    memset(&s_tray, 0, sizeof(s_tray));
}


/* Query whether 'window' is the tray dock window itself */
bool systray_owns_window(xcb_window_t window)
{
    return s_tray.window_ready && window != XCB_WINDOW_NONE &&
        window == s_tray.window;
}


/* Handle a 'ClientMessage' addressed to the tray window */
void systray_handle_client_message(wm_td *wm,
        const xcb_client_message_event_t *event)
{
    (void) wm;

    if (event == NULL || !s_tray.selection_owned) {
        return;
    }

    if (event->window != s_tray.window ||
            event->type != s_tray.opcode_atom) {
        return;
    }

    /* 'data32[1]' is the opcode; only 'SYSTEM_TRAY_REQUEST_DOCK' is
     * implemented; 'BEGIN_MESSAGE'/'CANCEL_MESSAGE' (balloon-style
     * messages) are silently acknowledged as ignored */
    if (event->data.data32[1] == SYSTRAY_OPCODE_REQUEST_DOCK) {
        s_systray_dock((xcb_window_t) event->data.data32[2]);
    }
}


/* Handle a docked icon window being destroyed */
void systray_handle_destroy(wm_td *wm, xcb_window_t window)
{
    (void) wm;

    if (!s_tray.window_ready) {
        return;
    }

    for (uint16_t i = 0u; i < s_tray.icon_count; ++i) {
        if (s_tray.icons[i].window != window) {
            continue;
        }

        for (uint16_t j = i; j + 1u < s_tray.icon_count; ++j) {
            s_tray.icons[j] = s_tray.icons[j + 1u];
        }
        s_tray.icon_count--;

        LOGGER_DEBUG("Systray icon 0x%x undocked (%u remaining)",
                window, (unsigned int) s_tray.icon_count);

        s_systray_reflow();
        return;
    }
}


/* Reposition the tray dock window for its surface's current size */
void systray_handle_surface_resize(wm_td *wm)
{
    (void) wm;

    s_systray_reflow();
}


/* React to a configuration reload */
void systray_reload(wm_td *wm)
{
    bool should_be_enabled;

    if (wm == NULL || wm->config == NULL) {
        return;
    }

    should_be_enabled = wm->config->base.systray.is_enabled;
    s_tray.position = wm->config->base.systray.position;
    s_tray.order = wm->config->base.systray.order;

    if (s_tray.selection_owned && !should_be_enabled) {
        LOGGER_INFO("Systray disabled by configuration reload;" \
                " releasing the selection (icons stay docked" \
                " hidden in the background)", L_NARG);
        s_systray_release_selection();
        return;
    }

    if (!s_tray.selection_owned && should_be_enabled) {
        LOGGER_INFO("Systray enabled by configuration reload", L_NARG);
        if (s_systray_ensure_window(wm) &&
                s_systray_acquire_selection()) {
            s_systray_reflow();
        }
        return;
    }

    if (s_tray.selection_owned) {
        /* Still enabled: pick up a possible 'position' change without
         * disturbing already-docked icons ('order' only affects where
         * a newly docked icon is inserted, not icons already placed) */
        s_systray_reflow();
    }
}
