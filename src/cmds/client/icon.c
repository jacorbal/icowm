/**
 * @file cmds/client/icon.c
 *
 * @brief Icon window creation, positioning, and slot-conflict detection
 *
 * Split out of what used to be a single, flat @c cmds/client/basic.c.
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
#include <stdlib.h>     /* NULL, free */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Default initial values */
#include <defs/desktop.h>
#include <defs/icon.h>

/* Windows & icons policy includes */
#include <policy/placement/icon.h>

/* Utils includes */
#include <utils/geom.h>

/* Types includes */
#include <types/pair.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <ipc.h>
#include <lookup.h>
#include <render/outdate.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <cmds/client/basic.h>
#include <cmds/client/internal.h>


/**
 * @brief Whether a remembered icon position is already occupied
 *
 * Checks @p client's saved @p icon_x / @p icon_y against every other
 * iconified client on the same desktop, so @c ccmd_client_iconify can
 * tell a genuinely free remembered spot from one that another window's
 * icon has since claimed (e.g., because that other window was iconified
 * while @p client was still restored, and happened to land where
 * @p client's own icon last was).
 *
 * Checked against @a client_is_iconified rather than @c is_icon_mapped.
 * The latter only reflects whether a desktop's own icons are currently
 * mapped on screen right now (@c false for every client on a desktop
 * that is not the one currently shown, @a surface_clients_hide,
 * surface/actions.c, clears it precisely for that reason), so relying
 * on it here would report every slot on a non-current desktop as free
 * regardless of how many icons already actually occupy it.
 *
 * @param client   Client about to be iconified; its own @p icon_window
 *                 may still be non-zero from a previous iconify, in
 *                 which case it is skipped so it never collides with
 *                 itself
 * @param icon_dim Icon width/height, in pixels
 *
 * @return @c true if another icon already overlaps that position
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
static bool s_icon_slot_is_taken(const client_td *client,
        struct dimensions_s icon_dim)
{
    desktop_td *desktop;
    cdlist_item_td *node;
    const cdlist_item_td *initial;

    if (client == NULL || client->icon_pos.x < 0 ||
            client->icon_pos.y < 0) {
        return false;
    }

    desktop = wm_get_client_desktop(client);
    if (desktop == NULL || desktop->stacking == NULL) {
        return false;
    }

    node = cdlist_head(desktop->stacking);
    initial = node;
    if (node == NULL) {
        return false;
    }

    do {
        const client_td *other = (const client_td *) cdlist_data(node);

        if (other != NULL && other != client &&
                other->icon_window != 0u && client_is_iconified(other) &&
                geom_intersection_area(
                    client->icon_pos.x, client->icon_pos.y,
                    icon_dim.w, icon_dim.h,
                    other->icon_pos.x, other->icon_pos.y,
                    icon_dim.w, icon_dim.h) > 0u) {
            return true;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    return false;
}


/* Relocate an already-iconified client's own icon if its current spot
 * is now occupied by another one */
void ccmd_client_relocate_icon_if_taken(client_td *client)
{
    uint16_t icon_h;
    desktop_td *desktop;
    enum config_icon_placement_e policy = CONFIG_ICON_PLACEMENT_BOTTOM;
    struct dimensions_s screen_dim = { 1024u, 768u };
    struct position_s icon_pos;
    uint16_t screen_w;
    uint16_t screen_h;

    if (client == NULL || client->config == NULL ||
            client->icon_window == 0u || !client_is_iconified(client)) {
        return;
    }

    icon_h = (uint16_t) (WM_ICON_SQUARE_SIZE +
            ((client->config->theme.icon.is_captioned)
             ? WM_ICON_CAPTION_HEIGHT : 0u));

    if (!s_icon_slot_is_taken(client,
                (struct dimensions_s) { WM_ICON_SQUARE_SIZE, icon_h })) {
        return;
    }

    desktop = wm_get_client_desktop(client);
    if (client->config != NULL) {
        policy = client->config->base.icons.placement_policy;
    }
    screen_w = (uint16_t) screen_dim.w;
    screen_h = (uint16_t) screen_dim.h;
    (void) ccmd_screen_dim(client, &screen_w, &screen_h);
    screen_dim.w = screen_w;
    screen_dim.h = screen_h;

    place_icon_apply(client, desktop, policy,
            (struct dimensions_s) { WM_ICON_SQUARE_SIZE, icon_h },
            screen_dim, &icon_pos);

    client->icon_pos.x = (int16_t) icon_pos.x;
    client->icon_pos.y = (int16_t) icon_pos.y;

    if (client->connection != NULL) {
        const uint32_t vals[2] = {
            (uint32_t) icon_pos.x, (uint32_t) icon_pos.y };

        xcb_configure_window(client->connection, client->icon_window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals);
    }
}


/* Create the client's icon window if it does not exist yet, or
 * reposition the existing one at its saved coordinates; see
 * cmds/client/internal.h for the full doc comment */
void ccmd_client_ensure_icon_window(client_td *client,
        uint16_t icon_h_out)
{
    if (client->icon_window == 0) {
        uint16_t screen_w;
        uint16_t screen_h;
        int16_t ix;
        int16_t iy;
        int32_t mx = 0;
        int32_t my = 0;
        monitor_td monitor;
        surface_td *surface = NULL;
        desktop_td *desktop;
        enum config_icon_placement_e policy =
            CONFIG_ICON_PLACEMENT_BOTTOM;
        uint32_t mask;
        uint32_t values[3];

        screen_w = 1024u;
        screen_h = 768u;

        if (ccmd_client_monitor(client, &surface, &monitor)) {
            mx = monitor.x;
            my = monitor.y;
            screen_w = geom_dim_clamp((int32_t) monitor.w);
            screen_h = geom_dim_clamp((int32_t) monitor.h);
        } else if (ccmd_screen_dim(client, &screen_w, &screen_h)) {
            /* dimensions updated */
        }

        /* 'monitor' above is deliberately raw (see
         * 'ccmd_client_monitor''s comment), the same as
         * 'desktop_update_workarea' (in 'desktop.c') starts from before
         * folding in 'desktops.margins' and the systray's own
         * reservation for windows; applied here the same way, per
         * monitor rather than once across the whole surface: top/left
         * shift this monitor's own placement origin inward, and
         * right/bottom shrink the available area, so the icon grid
         * never lands within a margin a window's own maximize and
         * placement already stay clear of, nor under the systray's own
         * dock window (which would otherwise sit right on top of
         * a restored icon left behind there, blocking that dock
         * window's own repaint). */
        if (surface != NULL) {
            const struct strut_partial_s *tray_strut =
                systray_get_reserved_strut(surface);
            uint32_t margin_left = 0u;
            uint32_t margin_right = 0u;
            uint32_t margin_top = 0u;
            uint32_t margin_bottom = 0u;
            uint32_t horiz;
            uint32_t vert;

            if (surface->config != NULL) {
                const struct config_desktop_s *cd =
                    &surface->config->desktops;

                margin_left += cd->margins.left;
                margin_right += cd->margins.right;
                margin_top += cd->margins.top;
                margin_bottom += cd->margins.bottom;
            }
            if (tray_strut != NULL) {
                margin_left += (uint32_t) tray_strut->sides.left;
                margin_right += (uint32_t) tray_strut->sides.right;
                margin_top += (uint32_t) tray_strut->sides.top;
                margin_bottom += (uint32_t) tray_strut->sides.bottom;
            }

            horiz = margin_left + margin_right;
            vert = margin_top + margin_bottom;

            mx += (int32_t) margin_left;
            my += (int32_t) margin_top;
            screen_w = ((uint32_t) screen_w > horiz)
                ? (uint16_t) ((uint32_t) screen_w - horiz) : 0u;
            screen_h = ((uint32_t) screen_h > vert)
                ? (uint16_t) ((uint32_t) screen_h - vert) : 0u;
        }

        if (client->config != NULL) {
            policy = client->config->base.icons.placement_policy;
        }

        /* Re-use the saved position when the client was already
         * iconified once (and possibly manually repositioned by the
         * user), UNLESS another client's icon has since claimed that
         * exact spot (e.g., it was free when this client was last
         * iconified, but has since been taken by a window that got
         * iconified while this one was restored).  In that case fall
         * through to 'place_icon_apply' just like a client with no
         * remembered position at all, so the two icons never
         * overlap. */
        desktop = wm_get_client_desktop(client);
        if (client->icon_pos.x >= 0 && client->icon_pos.y >= 0 &&
                !s_icon_slot_is_taken(client,
                    (struct dimensions_s) {
                    WM_ICON_SQUARE_SIZE, icon_h_out })) {
            ix = client->icon_pos.x;
            iy = client->icon_pos.y;
        } else {
            struct dimensions_s screen_dim;
            struct position_s icon_pos;

            screen_dim.w = screen_w;
            screen_dim.h = screen_h;
            place_icon_apply(client, desktop, policy,
                    (struct dimensions_s) { WM_ICON_SQUARE_SIZE,
                        icon_h_out },
                    screen_dim, &icon_pos);
            ix = (int16_t) icon_pos.x;
            iy = (int16_t) icon_pos.y;
            /* 'place_icon_apply' works in a (0,0)-relative coordinate
             * space bounded by 'screen_w'/'screen_h' alone; offset by
             * 'mx'/'my', the target monitor's own origin plus its
             * top/left margin, so the icon lands on that monitor
             * within the combined surface, past whatever margin is
             * configured, rather than always in its raw top-left
             * corner. */
            ix = (int16_t) (ix + mx);
            iy = (int16_t) (iy + my);
        }

        /* The margin/strut-based shrink of 'screen_w'/'screen_h' above
         * only ever accounts for the tray's own *reserved* strut, which
         * stays all-zero whenever 'systray.reserve-space' is left at
         * its own default of 'false' (see the comment right by
         * 'partial' staying all-zero in 's_systray_strut_update',
         * systray/layout.c): the tray still visually occupies real
         * screen space either way, so a final check against its actual
         * current rectangle, the same one a drag or a config reload
         * already goes through (see
         * 'place_icon_avoid_systray_overlap''s comment), catches what
         * that coarser shrink alone still misses.  A tray docked in
         * a corner, reaching only partway
         * along an edge, being the case that shrink cannot express at
         * all: it only ever knows the tray's own side widths, nothing
         * about how far along that edge it actually reaches. */
        if (surface != NULL) {
            struct geometry_s tray;

            if (systray_get_geometry(surface, &tray)) {
                (void) place_icon_avoid_systray_overlap(&ix, &iy,
                        (struct dimensions_s) { WM_ICON_SQUARE_SIZE,
                            icon_h_out },
                        tray,
                        (desktop != NULL) ? &desktop->workarea : NULL);
            }
        }

        client->icon_pos.x = ix;
        client->icon_pos.y = iy;

        client->icon_window = xcb_generate_id(client->connection);
        mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
            XCB_CW_EVENT_MASK;

        values[0] = client->config->theme.icon.inactive.color.background;
        values[1] = client->config->theme.icon.inactive.border.color;
        values[2] = XCB_EVENT_MASK_EXPOSURE |
            XCB_EVENT_MASK_BUTTON_PRESS |
            XCB_EVENT_MASK_BUTTON_MOTION;

        xcb_create_window(client->connection,
                XCB_COPY_FROM_PARENT,
                client->icon_window,
                client->parent_id,
                ix, iy,
                (uint16_t) WM_ICON_SQUARE_SIZE, icon_h_out,
                (uint16_t) client->config->theme.icon.active.border.width,
                XCB_WINDOW_CLASS_INPUT_OUTPUT,
                XCB_COPY_FROM_PARENT,
                mask, values);
    } else {
        /* Re-map at the saved position (may have been dragged) */
        xcb_configure_window(client->connection, client->icon_window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                (const uint32_t[]) {
                (uint32_t) client->icon_pos.x,
                (uint32_t) client->icon_pos.y
                });
    }
}
