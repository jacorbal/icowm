/**
 * @file client/geom.c
 *
 * @brief Client geometry and decoration layout helpers
 *
 * Covers three closely related concerns that all operate on the
 * @c layout sub-struct of a @c client_td:
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Utils includes */
#include <utils/safe/safemem.h>

/* Default initial values */
#include <defs/config.h>
#include <defs/wm.h>

/* Project includes */
#include <client.h>
#include <config.h>

/* Local includes */
#include <client/internal.h>


/* Allocate and zero all heap string buffers for a client */
int ci_alloc_strings(client_td *client)
{
    client->info.name = malloc(CONFIG_MAX_LENGTH_NAME);
    client->info.visible_name = malloc(CONFIG_MAX_LENGTH_NAME);
    client->info.role_name = malloc(CONFIG_MAX_LENGTH_NAME);
    client->info.class_name[0] = malloc(CONFIG_MAX_LENGTH_NAME);
    client->info.class_name[1] = malloc(CONFIG_MAX_LENGTH_NAME);
    client->icon_info.icon_name = malloc(CONFIG_MAX_LENGTH_NAME);
    client->icon_info.visible_icon_name = malloc(CONFIG_MAX_LENGTH_NAME);

    if (client->info.name == NULL ||
            client->info.visible_name == NULL ||
            client->info.role_name == NULL ||
            client->info.class_name[0] == NULL ||
            client->info.class_name[1] == NULL ||
            client->icon_info.icon_name == NULL ||
            client->icon_info.visible_icon_name == NULL) {
        safe_free((void **) &client->info.name);
        safe_free((void **) &client->info.visible_name);
        safe_free((void **) &client->info.role_name);
        safe_free((void **) &client->info.class_name[0]);
        safe_free((void **) &client->info.class_name[1]);
        safe_free((void **) &client->icon_info.icon_name);
        safe_free((void **) &client->icon_info.visible_icon_name);
        return -1;
    }

    client->info.name[0] = '\0';
    client->info.visible_name[0] = '\0';
    client->info.role_name[0] = '\0';
    client->info.class_name[0][0] = '\0';
    client->info.class_name[1][0] = '\0';
    client->icon_info.icon_name[0] = '\0';
    client->icon_info.visible_icon_name[0] = '\0';
    client->icon_info.icons = NULL;

    return 0;
}


/* Apply decoration defaults from the loaded theme */
void ci_set_decoration_defaults(client_td *client,
        struct config_theme_s *theme)
{
    uint16_t border_width = 0;

    if (client == NULL) {
        return;
    }

    client->title_height = WM_TITLEBAR_DEFAULT_HEIGHT;
    if (theme != NULL) {
        border_width = (uint16_t) theme->window.general.border_width;
    }

    if (theme != NULL && theme->window.general.is_decorated) {
        client_set_decoration(client);
        client->layout.frame_extents.left = border_width;
        client->layout.frame_extents.right = border_width;
        client->layout.frame_extents.top =
            (uint16_t) (border_width + client->title_height);
        client->layout.frame_extents.bottom = border_width;
    } else {
        client_unset_decoration(client);
        client->layout.frame_extents = (struct sides_s) {0, 0, 0, 0};
    }
}


/* Synchronize the inner and titlebar geometry with the current frame
 * extents */
void client_sync_decoration_layout(client_td *client)
{
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;
    uint16_t title_h;
    uint16_t inner_w;
    uint16_t inner_h;
    uint16_t title_y;

    if (client == NULL || client->frame == 0 ||
            !client_is_decorated(client)) {
        return;
    }

    left = (uint16_t) client->layout.frame_extents.left;
    right = (uint16_t) client->layout.frame_extents.right;
    top = (uint16_t) client->layout.frame_extents.top;
    bottom = (uint16_t) client->layout.frame_extents.bottom;
    title_h = client->title_height;
    title_y = (top > title_h) ? (uint16_t) (top - title_h) : 0u;
    inner_w = (client->layout.geometry.cur.dim.w > left + right)
        ? (uint16_t) (client->layout.geometry.cur.dim.w - left - right)
        : WM_MIN_WINDOW_DIMENSION;
    inner_h = (client->layout.geometry.cur.dim.h > top + bottom)
        ? (uint16_t) (client->layout.geometry.cur.dim.h - top - bottom)
        : WM_MIN_WINDOW_DIMENSION;

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                left, top, inner_w, inner_h
            });

    if (client->titlebar != 0) {
        xcb_configure_window(client->connection, client->titlebar,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                (const uint32_t[]) {
                    left, title_y, inner_w, title_h
                });
    }

    /* Force the reparented client area to repaint immediately after the
     * frame/title layout changes.  Using 'exposures=1' causes the
     * X server to generate an Expose event so applications that do not
     * repaint on 'ConfigureNotify' alone redraw the newly exposed lower
     * area without requiring an additional user-triggered action. */
    xcb_clear_area(client->connection, 1, client->window, 0, 0, 0, 0);
}


/* Apply ICCCM size-hint constraints to a requested width and height */
void client_constrain_size(const client_td *client,
        uint32_t *width, uint32_t *height)
{
    uint32_t req_w;
    uint32_t req_h;

    if (client == NULL || width == NULL || height == NULL) {
        return;
    }

    req_w = *width;
    req_h = *height;

    if (client->size_hints.valid) {
        if (client->size_hints.min_w > 0 &&
                req_w < (uint32_t) client->size_hints.min_w) {
            req_w = (uint32_t) client->size_hints.min_w;
        }

        if (client->size_hints.max_w > 0 &&
                req_w > (uint32_t) client->size_hints.max_w) {
            req_w = (uint32_t) client->size_hints.max_w;
        }

        if (client->size_hints.min_h > 0 &&
                req_h < (uint32_t) client->size_hints.min_h) {
            req_h = (uint32_t) client->size_hints.min_h;
        }

        if (client->size_hints.max_h > 0 &&
                req_h > (uint32_t) client->size_hints.max_h) {
            req_h = (uint32_t) client->size_hints.max_h;
        }

        if (client->size_hints.inc_w > 1) {
            uint32_t base;
            uint32_t inc;
            uint32_t over;
            /* ICCCM §4.1.2.3: when 'BASE_SIZE' is absent, 'MIN_SIZE'
             * serves as the base for the increment grid */
            base = (client->size_hints.base_w > 0)
                ? (uint32_t) client->size_hints.base_w
                : (client->size_hints.min_w > 0
                        ? (uint32_t) client->size_hints.min_w
                        : 0u);
            inc = (uint32_t) client->size_hints.inc_w;
            over = (req_w > base) ? (req_w - base) : 0u;
            req_w = base + (over / inc) * inc;
        }

        if (client->size_hints.inc_h > 1) {
            uint32_t base;
            uint32_t inc;
            uint32_t over;
            /* ICCCM §4.1.2.3: when 'BASE_SIZE' is absent, 'MIN_SIZE'
             * serves as the base for the increment grid */
            base = (client->size_hints.base_h > 0)
                ? (uint32_t) client->size_hints.base_h
                : (client->size_hints.min_h > 0
                        ? (uint32_t) client->size_hints.min_h
                        : 0u);
            inc = (uint32_t) client->size_hints.inc_h;
            over = (req_h > base) ? (req_h - base) : 0u;
            req_h = base + (over / inc) * inc;
        }

        if (client->size_hints.min_w > 0 &&
                req_w < (uint32_t) client->size_hints.min_w) {
            req_w = (uint32_t) client->size_hints.min_w;
        }

        if (client->size_hints.min_h > 0 &&
                req_h < (uint32_t) client->size_hints.min_h) {
            req_h = (uint32_t) client->size_hints.min_h;
        }
    }

    *width = req_w;
    *height = req_h;
}


/* Create frame and titlebar windows for a decorated client */
int ci_create_decorations(client_td *client)
{
    uint32_t mask;
    uint32_t values[3];
    uint16_t frame_w;
    uint16_t frame_h;
    int16_t frame_x;
    int16_t frame_y;
    int32_t frame_x32;
    int32_t frame_y32;
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;
    uint16_t inner_w;
    uint16_t title_h;
    uint16_t title_y;

    if (client == NULL || !client_is_decorated(client) ||
            client->theme == NULL || client->parent_id == 0) {
        return 0;
    }

    left = (uint16_t) client->layout.frame_extents.left;
    right = (uint16_t) client->layout.frame_extents.right;
    top = (uint16_t) client->layout.frame_extents.top;
    bottom = (uint16_t) client->layout.frame_extents.bottom;
    inner_w = (uint16_t) client->layout.geometry.cur.dim.w;
    title_h = client->title_height;
    title_y = (top > title_h) ? (uint16_t) (top - title_h) : 0u;
    frame_x32 = client->layout.geometry.cur.pos.x - (int32_t) left;
    frame_y32 = client->layout.geometry.cur.pos.y - (int32_t) top;

    if (frame_x32 < INT16_MIN) {
        frame_x = INT16_MIN;
    } else if (frame_x32 > INT16_MAX) {
        frame_x = INT16_MAX;
    } else {
        frame_x = (int16_t) frame_x32;
    }

    if (frame_y32 < INT16_MIN) {
        frame_y = INT16_MIN;
    } else if (frame_y32 > INT16_MAX) {
        frame_y = INT16_MAX;
    } else {
        frame_y = (int16_t) frame_y32;
    }

    frame_w =
        (uint16_t) (client->layout.geometry.cur.dim.w + left + right);
    frame_h =
        (uint16_t) (client->layout.geometry.cur.dim.h + top + bottom);

    client->frame = xcb_generate_id(client->connection);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = client->theme->window.inactive.border_color;
    values[1] = client->theme->window.inactive.border_color;
    /* 'XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT' is essential here, not
     * optional: once the client's own top-level window is reparented
     * into this frame, its *parent* for X11 purposes becomes the frame
     * instead of the root.
     *
     * A client's own attempt to reconfigure itself is delivered as
     * a 'ConfigureRequest' to whichever client selected
     * substructure-redirect on its *parent*; if that is only ever
     * selected on the root window (needed for top-level 'MapRequest's)
     * and not on every frame the window manager itself
     * creates, the server has nothing to redirect a reparented client's
     * own resize to, and simply performs it directly with no
     * 'ConfigureRequest' ever generated at all. */
    values[2] = XCB_EVENT_MASK_EXPOSURE             |
                XCB_EVENT_MASK_BUTTON_PRESS         |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY     |
                XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY  |
                XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                XCB_EVENT_MASK_POINTER_MOTION;
    xcb_create_window(client->connection,
            XCB_COPY_FROM_PARENT,
            client->frame,
            client->parent_id,
            frame_x, frame_y,
            frame_w, frame_h,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    client->titlebar = xcb_generate_id(client->connection);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = client->theme->window.inactive.background_color;
    values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS;
    xcb_create_window(client->connection,
            XCB_COPY_FROM_PARENT,
            client->titlebar,
            client->frame,
            (int16_t) left, (int16_t) title_y,
            inner_w, title_h,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    xcb_reparent_window(client->connection,
            client->window,
            client->frame,
            (int16_t) left, (int16_t) top);

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH, (const uint32_t[]) {0});

    /* Passive grab: selected button, any modifier, SYNC pointer mode.
     * With 'owner_events=0' button presses on the frame or any of its
     * children (titlebar, client window) are delivered to the window
     * manager through this grab rather than via SelectInput.  SYNC mode
     * freezes pointer events until the window manager calls
     * 'xcb_allow_events', which lets the manager focus the window
     * before deciding whether to replay the click to the application or
     * consume it silently.  'XCB_MOD_MASK_ANY' already covers all
     * lock-modifier combinations, so no lock-modifier loop is required.
     *
     * Scroll-wheel buttons 4 and 5 are intentionally excluded.  A sync
     * passive grab on those buttons intercepts scroll events before the
     * application can receive them; even though 'ReplayPointer' is
     * issued, some applications (e.g. Chromium, pcmanfm) use their own
     * grabs internally and the event is not re-delivered correctly.
     * Leaving 4 and 5 ungrabbed here allows the X server to deliver
     * scroll events directly to the focused application window. */
    /* Root-level 'MOD1+button' grabs are more specific (specific
     * modifier beats 'XCB_MOD_MASK_ANY') and therefore still take
     * priority for move/resize interactions. */
    {
        static const xcb_button_index_t s_grab_buttons[] = {
            XCB_BUTTON_INDEX_1,
            XCB_BUTTON_INDEX_2,
            XCB_BUTTON_INDEX_3,
            6,   /* extra side buttons */
            7
        };
        size_t nb = sizeof(s_grab_buttons) / sizeof(s_grab_buttons[0]);
        for (size_t bi = 0; bi < nb; ++bi) {
            xcb_grab_button(client->connection,
                    0,                              /* owner_events */
                    client->frame,
                    XCB_EVENT_MASK_BUTTON_PRESS |
                    XCB_EVENT_MASK_BUTTON_RELEASE,
                    XCB_GRAB_MODE_SYNC,             /* freeze until allow_events */
                    XCB_GRAB_MODE_ASYNC,
                    XCB_NONE,
                    XCB_NONE,
                    (uint8_t) s_grab_buttons[bi],
                    XCB_MOD_MASK_ANY);
        }
    }

    client->layout.geometry.cur.pos.x = frame_x;
    client->layout.geometry.cur.pos.y = frame_y;
    client->layout.geometry.cur.dim.w = frame_w;
    client->layout.geometry.cur.dim.h = frame_h;
    client->layout.geometry.old = client->layout.geometry.cur;
    client_sync_decoration_layout(client);

    /* Publish '_NET_FRAME_EXTENTS' so clients and taskbars know the
     * size of the WM-added decoration around the content window */
    if (client->ewmh != NULL) {
        uint32_t extents[4];
        extents[0] = (uint32_t) client->layout.frame_extents.left;
        extents[1] = (uint32_t) client->layout.frame_extents.right;
        extents[2] = (uint32_t) client->layout.frame_extents.top;
        extents[3] = (uint32_t) client->layout.frame_extents.bottom;
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->window, client->ewmh->_NET_FRAME_EXTENTS,
                XCB_ATOM_CARDINAL, 32, 4, extents);
    }


    return 0;
}


/* Send a synthetic 'ConfigureNotify' to inform the client of its
 * screen-relative geometry (ICCCM §4.2.3) */
void client_send_synthetic_configure_notify(xcb_connection_t *connection,
        const client_td *client)
{
    xcb_configure_notify_event_t notify;
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;

    if (connection == NULL || client == NULL || client->window == 0) {
        return;
    }

    left = (uint16_t) client->layout.frame_extents.left;
    right = (uint16_t) client->layout.frame_extents.right;
    top = (uint16_t) client->layout.frame_extents.top;
    bottom = (uint16_t) client->layout.frame_extents.bottom;

    memset(&notify, 0, sizeof(notify));
    notify.response_type = XCB_CONFIGURE_NOTIFY;
    notify.event = client->window;
    notify.window = client->window;
    notify.above_sibling = XCB_NONE;
    notify.x = (int16_t) (client->layout.geometry.cur.pos.x +
            (int32_t) left);
    notify.y = (int16_t) (client->layout.geometry.cur.pos.y +
            (int32_t) top);
    notify.width =
        (uint16_t) ((client->layout.geometry.cur.dim.w > left + right)
                ? (client->layout.geometry.cur.dim.w - left - right)
                : WM_MIN_WINDOW_DIMENSION);
    notify.height =
        (uint16_t) ((client->layout.geometry.cur.dim.h > top + bottom)
                ? (client->layout.geometry.cur.dim.h - top - bottom)
                : WM_MIN_WINDOW_DIMENSION);
    notify.border_width = 0;
    notify.override_redirect = 0;

    xcb_send_event(connection, 0, client->window,
            XCB_EVENT_MASK_STRUCTURE_NOTIFY, (const char *) &notify);
}
