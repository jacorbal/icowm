/**
 * @file cmds/client/icon.c
 *
 * @brief Icon window creation, positioning, and slot-conflict detection
 *
 * One of the files @c cmds/client/ is made of.
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
#include <policy/stacking.h>
#include <ipc.h>
#include <lookup.h>
#include <render/outdate.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <cmds/client/icon.h>
#include <cmds/client/move.h>
#include <cmds/client/screen.h>
#include <cmds/client/visibility.h>
#include <utils/xcb/connection.h>


/**
 * @brief What @a s_icon_overlap_visit is testing, and what it found
 */
struct s_icon_overlap_ctx_s {
    const client_td *self;          /**< Client whose spot is tested */
    struct dimensions_s icon_dim;   /**< Size one icon occupies */
    bool is_taken;                  /**< Whether something sits there */
};


/**
 * @brief Note whether one client's own icon already sits on the spot
 *
 * @param client Client reached by the walk
 * @param data   Pointer to the @c s_icon_overlap_ctx_s being filled
 *
 * @note Once one overlap is found the rest are passed over: the answer
 *       cannot change, and the walk cannot be ended early
 * @note Complexity: @e O(1)
 */
static void s_icon_overlap_visit(client_td *client, void *data)
{
    struct s_icon_overlap_ctx_s *const overlap_ctx = data;

    if (client == NULL || overlap_ctx == NULL ||
            overlap_ctx->is_taken || client == overlap_ctx->self ||
            client->icon_window == 0u ||
            !client_is_iconified(client)) {
        return;
    }

    if (geom_intersection_area(
                overlap_ctx->self->icon_pos.x,
                overlap_ctx->self->icon_pos.y,
                overlap_ctx->icon_dim.w, overlap_ctx->icon_dim.h,
                client->icon_pos.x, client->icon_pos.y,
                overlap_ctx->icon_dim.w,
                overlap_ctx->icon_dim.h) > 0u) {
        overlap_ctx->is_taken = true;
    }
}


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
 * that is not the one currently shown, @a surface_clients_hide, in
 * @c surface/actions.c, clears it precisely for that reason), so
 * relying on it here would report every slot on a non-current desktop
 * as free regardless of how many icons already actually occupy it.
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
    const desktop_td *desktop;
    struct s_icon_overlap_ctx_s overlap_ctx;

    if (client == NULL || client->icon_pos.x < 0 ||
            client->icon_pos.y < 0) {
        return false;
    }

    desktop = wm_get_client_desktop(client);
    if (desktop == NULL) {
        return false;
    }

    overlap_ctx.self = client;
    overlap_ctx.icon_dim = icon_dim;
    overlap_ctx.is_taken = false;
    stacking_walk(desktop, s_icon_overlap_visit, &overlap_ctx);

    return overlap_ctx.is_taken;
}


/**
 * @brief Choose where one client's icon goes, from scratch
 *
 * Everything a fresh icon position needs, in one place: the monitor
 * @p client sits on rather than the whole combined screen, that
 * monitor's origin, @c desktops.margins and the system tray's reserved
 * strut shifting the origin inward and shrinking the room, the
 * configured placement policy, and a last check against the tray's
 * actual rectangle.
 *
 * Shared by @a ccmd_client_ensure_icon_window and
 * @a ccmd_client_relocate_icon_if_taken so the two can never disagree
 * about where an icon belongs.  They used to compute it separately,
 * and the shorter of the two answered in whole-screen coordinates
 * with no origin, no margins and no strut, which put a relocated icon
 * inside a margin, under the tray, or on the wrong monitor entirely.
 *
 * @param client   Client whose icon is being placed
 * @param icon_dim Icon width/height, in pixels
 * @param out_pos  Receives the position, in root coordinates
 *
 * @return @c true when a position was chosen
 *
 * @note Answers @c false only for a client with no configuration
 *       attached, which has no placement policy to apply
 * @note The result is not clamped to @c int16_t here; both callers
 *       store it into @c client_td's own 16-bit @p icon_pos, which is
 *       what the X protocol takes for a window position anyway
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop, from the overlap search this hands on to
 */
static bool s_icon_position_choose(client_td *client,
        struct dimensions_s icon_dim,
        struct position_s *restrict out_pos)
{
    int32_t mx = 0;
    int32_t my = 0;
    uint16_t screen_w = 1024u;
    uint16_t screen_h = 768u;
    monitor_td monitor;
    surface_td *surface = NULL;
    desktop_td *desktop;
    enum config_icon_placement_e policy;
    struct dimensions_s screen_dim;
    struct position_s anchor;
    struct position_s icon_pos;
    int16_t ix;
    int16_t iy;

    if (client == NULL || client->config == NULL || out_pos == NULL) {
        return false;
    }

    policy = client->config->base.icons.placement_policy;
    desktop = wm_get_client_desktop(client);

    if (ccmd_client_monitor(client, &surface, &monitor)) {
        mx = monitor.x;
        my = monitor.y;
        screen_w = geom_dim_clamp((int32_t) monitor.w);
        screen_h = geom_dim_clamp((int32_t) monitor.h);
    } else if (ccmd_screen_dim(client, &screen_w, &screen_h)) {
        /* dimensions updated */
    }

    /* 'monitor' above is deliberately raw (see comment of
     * 'ccmd_client_monitor'), the same as 'desktop_update_workarea'
     * (in 'desktop.c') starts from before folding in
     * 'desktops.margins' and the systray's reservation for windows;
     * applied here the same way, per monitor rather than once across
     * the whole surface: top/left shift this monitor's placement
     * origin inward, and right/bottom shrink the available area, so
     * the icon grid never lands within a margin a window's maximize
     * and placement already stay clear of, nor under the systray's
     * dock window (which would otherwise sit right on top of
     * a restored icon left behind there, blocking that dock window's
     * repaint). */
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

    /* Where the window itself sits, moved out of root coordinates and
     * into the (0, 0)-relative space 'place_icon_apply' works in, by
     * taking off the same 'mx'/'my' its answer is shifted back by just
     * below.  Taken after the margins have been folded into 'mx'/'my'
     * rather than before: an anchor measured from the raw monitor
     * origin would sit one margin further right and down than the
     * window it is supposed to name. */
    anchor.x = client->layout.geometry.cur.pos.x - mx;
    anchor.y = client->layout.geometry.cur.pos.y - my;

    screen_dim.w = screen_w;
    screen_dim.h = screen_h;
    place_icon_apply(client, desktop, policy, icon_dim, screen_dim,
            &anchor, &icon_pos);

    /* 'place_icon_apply' works in a (0,0)-relative coordinate space
     * bounded by 'screen_w'/'screen_h' alone; offset by 'mx'/'my', the
     * target monitor's origin plus its top/left margin, so the icon
     * lands on that monitor within the combined surface, past whatever
     * margin is configured, rather than always in the raw top-left
     * corner of the whole screen. */
    ix = (int16_t) (icon_pos.x + mx);
    iy = (int16_t) (icon_pos.y + my);

    out_pos->x = ix;
    out_pos->y = iy;

    return true;
}


/**
 * @brief Push an icon position clear of the system tray, wherever it
 *        came from
 *
 * Applies to a freshly chosen position and to one remembered from an
 * earlier iconify alike: an icon dragged under the tray by hand, or
 * left where a tray that has since grown now reaches, has to be pushed
 * clear just the same as one being placed for the first time.
 *
 * The margin and strut shrink @a s_icon_position_choose applies only
 * ever accounts for the tray's *reserved* strut, which stays all-zero
 * whenever @c systray.reserve-space is left at its default of
 * @c false (see the comment by @p partial staying all-zero in
 * @a s_systray_strut_update, @c systray/layout.c).  The tray still
 * visually occupies real screen space either way, so this check
 * against its actual current rectangle catches what that coarser
 * shrink alone misses.  A tray docked in a corner, reaching only
 * partway along an edge, is the case that shrink cannot express at
 * all: it only ever knows the tray's side widths, nothing about how
 * far along that edge it actually reaches.
 *
 * @param client   Client whose icon is being positioned
 * @param icon_dim Icon width/height, in pixels
 * @param io_x     Icon X position; read but never adjusted
 * @param io_y     Icon Y position; overwritten when pushed
 *
 * @note A no-op when the client sits on no known surface, or when that
 *       surface has no tray mapped
 * @note Complexity: @e O(1)
 */
static void s_icon_tray_avoid(client_td *client,
        struct dimensions_s icon_dim,
        const int16_t *restrict io_x, int16_t *restrict io_y)
{
    const desktop_td *desktop;
    monitor_td monitor;
    surface_td *surface = NULL;
    struct geometry_s tray;

    if (client == NULL || io_x == NULL || io_y == NULL) {
        return;
    }

    (void) ccmd_client_monitor(client, &surface, &monitor);
    if (surface == NULL || !systray_get_geometry(surface, &tray)) {
        return;
    }

    desktop = wm_get_client_desktop(client);
    (void) place_icon_avoid_systray_overlap(io_x, io_y, icon_dim, tray,
            (desktop != NULL) ? &desktop->workarea : NULL);
}


/* Relocate an already-iconified client's own icon if its current spot
 * is now occupied by another one */
void ccmd_client_relocate_icon_if_taken(client_td *client)
{
    struct dimensions_s icon_dim;
    struct position_s icon_pos;
    int16_t ix;
    int16_t iy;

    if (client == NULL || client->config == NULL ||
            client->icon_window == 0u || !client_is_iconified(client)) {
        return;
    }

    icon_dim.w = (uint32_t) WM_ICON_SQUARE_SIZE;
    icon_dim.h = (uint32_t) (WM_ICON_SQUARE_SIZE +
            ((client->config->theme.icon.is_captioned)
             ? WM_ICON_CAPTION_HEIGHT : 0u));

    if (!s_icon_slot_is_taken(client, icon_dim)) {
        return;
    }

    if (!s_icon_position_choose(client, icon_dim, &icon_pos)) {
        return;
    }

    ix = (int16_t) icon_pos.x;
    iy = (int16_t) icon_pos.y;
    s_icon_tray_avoid(client, icon_dim, &ix, &iy);

    client->icon_pos.x = ix;
    client->icon_pos.y = iy;

    if (xcb_connection_get() != NULL) {
        ccmd_client_apply_geometry(client, client->icon_window,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y,
                ix, iy, 0u, 0u, 0u);
    }
}


/* Create the client's icon window if it does not exist yet, or
 * reposition the existing one at its saved coordinates; see
 * 'cmds/client/internal.h' for the full comment */
void ccmd_client_ensure_icon_window(client_td *client,
        uint16_t icon_h_out)
{
    if (client->icon_window == 0) {
        struct dimensions_s icon_dim;
        struct position_s icon_pos;
        int16_t ix;
        int16_t iy;
        uint32_t mask;
        uint32_t values[3];

        icon_dim.w = (uint32_t) WM_ICON_SQUARE_SIZE;
        icon_dim.h = icon_h_out;

        /* Re-use the saved position when the client was already
         * iconified once (and possibly manually repositioned by the
         * user), UNLESS another client's icon has since claimed that
         * exact spot (e.g., it was free when this client was last
         * iconified, but has since been taken by a window that got
         * iconified while this one was restored).  In that case pick
         * a fresh position just like a client with no remembered one
         * at all, so the two icons never overlap. */
        if (client->icon_pos.x >= 0 && client->icon_pos.y >= 0 &&
                !s_icon_slot_is_taken(client, icon_dim)) {
            ix = client->icon_pos.x;
            iy = client->icon_pos.y;
        } else if (s_icon_position_choose(client, icon_dim, &icon_pos)) {
            ix = (int16_t) icon_pos.x;
            iy = (int16_t) icon_pos.y;
        } else {
            ix = 0;
            iy = 0;
        }

        s_icon_tray_avoid(client, icon_dim, &ix, &iy);

        client->icon_pos.x = ix;
        client->icon_pos.y = iy;

        client->icon_window = xcb_generate_id(xcb_connection_get());
        mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
            XCB_CW_EVENT_MASK;

        values[0] = client->config->theme.icon.inactive.color.background;
        values[1] = client->config->theme.icon.inactive.border.color;
        values[2] = XCB_EVENT_MASK_EXPOSURE |
            XCB_EVENT_MASK_BUTTON_PRESS |
            XCB_EVENT_MASK_BUTTON_MOTION;

        xcb_create_window(xcb_connection_get(),
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
        ccmd_client_apply_geometry(client, client->icon_window,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y,
                client->icon_pos.x, client->icon_pos.y, 0u, 0u, 0u);
    }
}
