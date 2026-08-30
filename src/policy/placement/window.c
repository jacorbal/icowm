/**
 * @file policy/placement/window.c
 *
 * @brief Window placement policy implementation
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

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/ohtbl.h>

/* Utils includes */
#include <utils/geom.h>

/* Type includes */
#include <types/pair.h>

/* Command includes */
#include <cmds/client/transient.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <defs/placement.h>
#include <policy/placement/manual.h>
#include <policy/placement/monitor.h>
#include <policy/placement/rect.h>
#include <policy/placement/score.h>
#include <policy/placement/smart.h>
#include <policy/placement/window.h>
#include <utils/xcb/window.h>


/* Where the cascade put the last window it placed, as an offset from
 * the workarea origin rather than an absolute position, so the run
 * carries on sensibly across monitors of different sizes and origins.
 * The position itself is the state, and not a count of how many
 * windows have been placed.  A cascade means each window sits one step
 * on from the one before it, whereas a count would have to be turned
 * back into a position by a modulus whose divisor depends on the size
 * of whichever window is being placed (used in
 * 'place_window_apply_cascade') */
static struct position_s s_cascade_last = { 0, 0 };

/* Whether 's_cascade_last' holds a position yet, false only until the
 * first window this policy ever places, which goes at the workarea
 * origin rather than one step past it */
static bool s_cascade_has_last = false;


/**
 * @brief What one placement step did about the window
 */
enum s_place_result_e {
    /** Nothing was decided; the next step, or the fallback, answers */
    S_PLACE_RESULT_DECLINED = 0,
    /** A position was written, still to go through gravity and the
     *  final clamp */
    S_PLACE_RESULT_POSITION,
    /** The window is already where it belongs, and nothing further
     *  is to be done to it */
    S_PLACE_RESULT_DONE
};


/**
 * @brief Everything a placement step is allowed to look at
 *
 * Gathered once by @a place_window_apply and handed to each step in
 * turn, so no step resolves a monitor, reads configuration or works
 * out a workarea for itself, and so two of them can never disagree
 * about what any of those are.
 *
 * @note Every pointer comes first and the 32-bit window id last, so
 *       the layout carries no hole between fields
 */
struct s_place_ctx_s {
    const wm_td *wm;            /**< Window manager instance */
    surface_td *surface;        /**< Surface being placed on */
    client_td *client;          /**< Window being placed */
    config_td *config;          /**< Configuration in force */
    xcb_connection_t *connection;   /**< XCB connection */
    desktop_td *desktop;        /**< Current desktop, or null */
    struct geometry_s wa;       /**< Workarea, whole surface */
    struct geometry_s mon_wa;   /**< Workarea, reference monitor */
    struct dimensions_s screen; /**< Screen size, whole surface */
    struct dimensions_s mon_sz; /**< Screen size, reference monitor */
    struct dimensions_s frame;  /**< Size the window occupies */
    xcb_window_t leader;        /**< Its application group, or none */
};


/**
 * @brief What every entry of the two placement tables looks like
 *
 * @param ctx     Everything the step may look at
 * @param out_pos Where a chosen position is written, for a step
 *                answering @c S_PLACE_RESULT_POSITION
 *
 * @return What the step did
 */
typedef enum s_place_result_e (*s_place_step_fn)(
        const struct s_place_ctx_s *ctx,
        struct position_s *restrict out_pos);


/**
 * @brief Offset placement coordinates according to client gravity and
 *        clamp
 *
 * Adjusts the requested top-left placement coordinates so the client's
 * frame is positioned relative to its configured gravity point (e.g.,
 * centering or anchoring the frame by its edge or corner instead of its
 * top-left corner).  After applying the gravity offset, the resulting
 * position is clamped so the frame stays fully within the surface
 * bounds, preferring to keep it at the near edge when it does not fit.
 *
 * @param surface Pointer to the surface providing the placement bounds
 * @param client  Pointer to the client whose gravity and frame size are
 *                used
 * @param x       Pointer to the X coordinate to adjust and clamp in
 *                place
 * @param y       Pointer to the Y coordinate to adjust and clamp in
 *                place
 *
 * @note Complexity: @e O(1)
 */
static void s_place_window_apply_gravity(const surface_td *surface,
        const client_td *client,
        int32_t *restrict x, int32_t *restrict y)
{
    int32_t nx;
    int32_t ny;
    uint32_t sw;
    uint32_t sh;
    uint32_t fw;
    uint32_t fh;

    if (surface == NULL || client == NULL || x == NULL || y == NULL) {
        return;
    }

    nx = *x;
    ny = *y;
    sw = surface->properties.dim.w;
    sh = surface->properties.dim.h;
    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;

    if (client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_NORTH_EAST ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_EAST ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH_EAST) {
        nx -= (int32_t) fw;
    } else if (client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_NORTH ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_CENTER ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH) {
        nx -= (int32_t) (fw / 2u);
    }

    if (client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH_EAST ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH_WEST) {
        ny -= (int32_t) fh;
    } else if (client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_EAST ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_CENTER ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_WEST) {
        ny -= (int32_t) (fh / 2u);
    }

    if (nx < 0) {
        nx = 0;
    } else if ((uint32_t) nx + fw > sw) {
        nx = (sw > fw) ? (int32_t) (sw - fw) : 0;
    }
    if (ny < 0) {
        ny = 0;
    } else if ((uint32_t) ny + fh > sh) {
        ny = (sh > fh) ? (int32_t) (sh - fh) : 0;
    }

    *x = nx;
    *y = ny;
}


/**
 * @brief Center a client over its ICCCM §4.1.2.6 @c WM_TRANSIENT_FOR
 *        parent, clamped to that parent's monitor, and configure its
 *        window
 *
 * On success, this fully places the client (configures its window and
 * updates its stored geometry) and returns @c true, so
 * @a place_window_apply has nothing further to do.
 *
 * @param wm      Window manager instance
 * @param surface Surface @p client is on
 * @param client  Client being placed
 * @param wa      Surface-wide workarea, for the fallback monitor clip
 *                below
 *
 * @return @c true if @p client was transient and got placed here
 *
 * @note A no-op answering @c false when @p client is transient for
 *       nothing, or when its declared parent's geometry could be
 *       resolved neither from an already-managed client entry nor from
 *       a raw @a xcb_get_geometry reply
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients (see @a lookup_find_client)
 */
static bool s_place_window_transient_centered(const wm_td *wm,
        surface_td *surface, client_td *client,
        struct geometry_s wa)
{
    uint32_t fw;
    uint32_t fh;
    int32_t new_x = 0;
    int32_t new_y = 0;
    bool placed_as_transient = false;
    xcb_window_t target;
    client_td *parent;
    struct geometry_s t_wa;
    struct dimensions_s screen;
    struct dimensions_s t_sz;
    xcb_connection_t *connection = wm_connection(wm);

    if (client->transient_for == XCB_WINDOW_NONE) {
        return false;
    }

    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;
    parent = NULL;

    /* ICCCM: 'WM_TRANSIENT_FOR' set to the root window means this
     * dialog is transient for its whole application group, not one
     * specific window ("Window Managers should decide" how to handle
     * this, per the spec's wording); prefer
     * centering over whichever currently-mapped sibling shares the
     * same group leader as this client, falling through to the
     * ordinary geometry-based fallback further below (which, for the
     * root window specifically, ends up centering on screen) when no
     * such sibling is currently mapped. */
    if (client->transient_for == surface->screen->root) {
        xcb_window_t leader = client_group_leader(client);

        if (leader != XCB_WINDOW_NONE) {
            desktop_td *desktop =
                surface_desktop_get(surface, surface->desktop_cur);

            if (desktop != NULL && desktop->clients != NULL) {
                void *elem;

                ohtbl_foreach(desktop->clients, elem) {
                    client_td *const sibling = (client_td *) elem;

                    if (sibling != client &&
                            client_group_leader(sibling) == leader &&
                            !client_is_iconified(sibling)) {
                        parent = sibling;
                        break;
                    }
                }
            }
        } /* ! if (leader) */
    }

    /* Prefer the WM's stored frame geometry over
     * 'xcb_get_geometry': after reparenting the parent's inner
     * window lives inside the frame, so 'xcb_get_geometry' would
     * return its position relative to the frame (left, top); not
     * the frame's root-relative screen position.  Using the stored
     * geometry correctly centers the dialog wherever the parent
     * window is on screen. */
    if (parent == NULL) {
        parent = lookup_find_client(wm_surfaces(wm),
                client->transient_for, NULL, NULL);
    }
    if (parent != NULL) {
        int32_t px = parent->layout.geometry.cur.pos.x;
        int32_t py = parent->layout.geometry.cur.pos.y;
        uint32_t pw = parent->layout.geometry.cur.dim.w;
        uint32_t ph = parent->layout.geometry.cur.dim.h;

        new_x = px + ((int32_t) pw - (int32_t) fw) / 2;
        new_y = py + ((int32_t) ph - (int32_t) fh) / 2;
        placed_as_transient = true;
    } else {
        /* Parent not yet managed (or unmanaged window).  Fall back
         * to 'xcb_get_geometry' on the declared transient-for
         * window */
        xcb_get_geometry_cookie_t pgc;
        xcb_get_geometry_reply_t *pgr;
        pgc = xcb_get_geometry(connection, client->transient_for);
        pgr = xcb_get_geometry_reply(connection, pgc, NULL);
        if (pgr != NULL) {
            new_x = (int32_t) pgr->x +
                    ((int32_t) pgr->width - (int32_t) fw) / 2;
            new_y = (int32_t) pgr->y +
                    ((int32_t) pgr->height - (int32_t) fh) / 2;
            free(pgr);
            placed_as_transient = true;
        }
    }

    if (!placed_as_transient) {
        return false;
    }

    /* Resolved from the dialog's proposed center, not the
     * pointer: it is meant to sit with its parent, wherever
     * that is, regardless of where the pointer happens to be
     * right now. */
    screen.w = surface->properties.dim.w;
    screen.h = surface->properties.dim.h;
    placement_clip_to_monitor(surface, &wa, &screen,
            surface_monitor_for_point(surface,
                    (struct position_s) {
                        new_x + (int32_t) (fw / 2u),
                        new_y + (int32_t) (fh / 2u) }),
            &t_wa, &t_sz);

    if (new_x < t_wa.pos.x) { new_x = t_wa.pos.x; }
    if (new_y < t_wa.pos.y) { new_y = t_wa.pos.y; }
    if ((uint32_t) new_x + fw > t_sz.w) {
        new_x = (t_sz.w > fw) ? (int32_t) (t_sz.w - fw) : t_wa.pos.x;
    }
    if ((uint32_t) new_y + fh > t_sz.h) {
        new_y = (t_sz.h > fh) ? (int32_t) (t_sz.h - fh) : t_wa.pos.y;
    }

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame : client->window;
    xcb_window_move(target, new_x, new_y);
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
    return true;
}


/**
 * @brief Apply gravity, clamp to the workarea, and move the client to
 *        its final resolved position
 *
 * Shared final step of every placement policy.  Adjusts for window
 * gravity, clamps so the title bar never ends up above the workarea or
 * the physical screen edge, then issues the @c ConfigureWindow that
 * moves the window and records the position it was moved to.
 *
 * @param surface Surface the client lives on
 * @param client  Client being placed
 * @param wa_pos  Workarea origin, unclipped to any single monitor
 * @param new_pos Policy-resolved position, before gravity/clamping
 *
 * @note Complexity: @e O(1)
 */
static void s_place_window_finalize(const surface_td *surface,
        client_td *client, struct position_s wa_pos,
        struct position_s new_pos)
{
    xcb_window_t target;

    s_place_window_apply_gravity(surface, client,
            &new_pos.x, &new_pos.y);

    /* Final safety: gravity adjustments must not push the title bar
     * above the workarea top or above the physical screen edge */
    if (new_pos.y < wa_pos.y) { new_pos.y = wa_pos.y; }
    if (new_pos.x < wa_pos.x) { new_pos.x = wa_pos.x; }

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame
        : client->window;

    xcb_window_move(target, new_pos.x, new_pos.y);
    client->layout.geometry.cur.pos = new_pos;
}


/* Place the client following the cascade policy, unconditionally */
void place_window_apply_cascade(const wm_td *wm,
        surface_td *surface, client_td *client)
{
    const int32_t step = (int32_t) WM_PLACE_CASCADE_STEP;
    uint32_t fw;
    uint32_t fh;
    struct geometry_s wa;
    struct geometry_s mon_wa;
    struct dimensions_s mon_sz;
    struct position_s off;

    if (wm == NULL || wm_config(wm) == NULL ||
            surface == NULL || client == NULL) {
        return;
    }

    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;
    placement_workarea(wm, surface, client, &wa, &mon_wa, &mon_sz);

    off.x = (s_cascade_has_last) ? s_cascade_last.x + step : 0;
    off.y = (s_cascade_has_last) ? s_cascade_last.y + step : 0;

    /* Each axis wraps when that axis stops fitting, separately, and
     * against the size of this window rather than of the run as
     * a whole.  One step count shared between the two would let the
     * shorter axis decide for both, giving up on a wide screen with
     * most of the width unused and returning to the position the run
     * began at.  Wrapped separately, the vertical axis coming back to
     * the top while the
     * horizontal one keeps going is what opens the next column. */
    if ((uint32_t) off.x + fw > mon_sz.w) {
        off.x = 0;
    }
    if ((uint32_t) off.y + fh > mon_sz.h) {
        off.y = 0;
    }

    s_cascade_last = off;
    s_cascade_has_last = true;

    /* Offset from the workarea origin, not from (0, 0), so the title
     * bar is never hidden behind a panel or dock */
    s_place_window_finalize(surface, client, wa.pos,
            (struct position_s) { mon_wa.pos.x + off.x,
                mon_wa.pos.y + off.y });
}


/**
 * @brief Center a splash screen on the workarea
 *
 * EWMH does not require this, saying only what the type means, but it
 * is what every toolkit offering a splash does and what the user
 * expects to see.  A start-up screen cascaded into a corner alongside
 * ordinary windows looks like a mistake.
 *
 * First of the overrides, ahead of the honored-position step and not
 * merely ahead of the transient centering.  A splash routinely works
 * out a centre for itself and asks for it through @c PPosition, which
 * that step obeys, so a splash never reached this at all while it came
 * later.  What it asks for is a guess at where the middle is, made
 * without knowing the workarea or which monitor it will land on, and
 * this knows both.  Ahead of the transient centering too, since
 * a splash may be transient for something and its screen is the frame
 * that matters for it rather than whatever window spawned it.
 *
 * @param ctx     Everything the step may look at
 * @param out_pos Unused; this step places the window itself
 *
 * @return @c S_PLACE_RESULT_DONE for a splash,
 *         @c S_PLACE_RESULT_DECLINED for anything else
 *
 * @note Places without going through @a s_place_window_finalize, which
 *       applies the window's gravity to whatever position it is handed
 * @note That is right for a position the client asked for in terms of
 *       its gravity, and wrong for this one, the middle worked out
 *       here being already where the window goes; a splash declaring
 *       center gravity would otherwise have half its width taken off
 *       again and land left of centre
 * @note Complexity: @e O(1)
 */
static enum s_place_result_e s_place_step_splash(
        const struct s_place_ctx_s *ctx,
        struct position_s *restrict out_pos)
{
    struct position_s centered;

    (void) out_pos;

    if (ctx->client->properties.type != (uint16_t) CLIENT_TYPE_SPLASH) {
        return S_PLACE_RESULT_DECLINED;
    }

    centered.x = ctx->wa.pos.x + ((int32_t) ctx->wa.dim.w -
            (int32_t) ctx->frame.w) / 2;
    centered.y = ctx->wa.pos.y + ((int32_t) ctx->wa.dim.h -
            (int32_t) ctx->frame.h) / 2;

    xcb_configure_window(ctx->connection,
            (client_is_decorated(ctx->client) && ctx->client->frame != 0)
                ? ctx->client->frame : ctx->client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) { (uint32_t) centered.x,
                (uint32_t) centered.y });
    ctx->client->layout.geometry.cur.pos = centered;

    return S_PLACE_RESULT_DONE;
}


/**
 * @brief Honor a position the client asked for itself
 *
 * ICCCM §4.1.2.3: a client-requested position takes priority over
 * every policy, including the transient-centering convenience of the
 * step after this one.  An explicit request is the client's most
 * specific, deliberate statement of where it wants to appear, ahead of
 * any convenience default this window manager would otherwise pick on
 * its behalf.
 *
 * One exception: a transient window asking for exactly (0, 0).  In
 * practice that combination is never a deliberate placement choice on
 * a dialog's part; it is toolkit boilerplate left over from a default
 * @c PPosition or @c USPosition hint nobody meant to set to a specific
 * value, and honoring it verbatim pins every such dialog to the
 * screen's top-left corner instead of the transient-centered position
 * ICCCM §4.1.2.6 recommends.  A window genuinely wanting (0, 0) is
 * vanishingly rare among transients, so the exception costs nothing
 * for any other client while fixing that one common, confusing case,
 * a "save changes?" prompt landing at the screen corner instead of
 * over its parent.
 *
 * @param ctx     Everything the step may look at
 * @param out_pos Receives the requested position
 *
 * @return @c S_PLACE_RESULT_POSITION when a position was asked for and
 *         honored, @c S_PLACE_RESULT_DECLINED otherwise
 *
 * @note Complexity: @e O(1)
 */
static enum s_place_result_e s_place_step_requested(
        const struct s_place_ctx_s *ctx,
        struct position_s *restrict out_pos)
{
    const bool is_junk_origin =
        ctx->client->transient_for != XCB_WINDOW_NONE &&
        ctx->client->hints_icccm.size.req_pos.x == 0 &&
        ctx->client->hints_icccm.size.req_pos.y == 0;

    if (!ctx->client->hints_icccm.size.has_position || is_junk_origin) {
        return S_PLACE_RESULT_DECLINED;
    }

    *out_pos = ctx->client->hints_icccm.size.req_pos;

    return S_PLACE_RESULT_POSITION;
}


/**
 * @brief Center a transient dialog over its parent
 *
 * ICCCM §4.1.2.6, carried out by @a s_place_window_transient_centered,
 * which places the window itself when it finds a parent to center on.
 *
 * @param ctx     Everything the step may look at
 * @param out_pos Unused; the helper places the window itself
 *
 * @return @c S_PLACE_RESULT_DONE when the window was centered,
 *         @c S_PLACE_RESULT_DECLINED when no parent was found
 *
 * @note Complexity: @e O(1)
 */
static enum s_place_result_e s_place_step_transient(
        const struct s_place_ctx_s *ctx,
        struct position_s *restrict out_pos)
{
    (void) out_pos;

    if (!s_place_window_transient_centered(ctx->wm, ctx->surface,
                ctx->client, ctx->wa)) {
        return S_PLACE_RESULT_DECLINED;
    }

    return S_PLACE_RESULT_DONE;
}


/**
 * @brief Put a window next to another of the same application
 *
 * When another currently mapped client on this desktop shares this
 * one's @c WM_CLIENT_LEADER or @c WM_HINTS group, the new window goes
 * offset from the group rather than wherever the configured policy
 * would send it, so related windows stay visually together.  Gated by
 * @c windows.placement.group-related, since not everyone wants it.
 *
 * The offset scales with how many siblings already exist, not just
 * whichever one the walk happens to visit first, so a third, fourth
 * and later window of the same group each land at a further, distinct
 * position instead of every one after the second piling up on exactly
 * the spot the second took.
 *
 * @param ctx     Everything the step may look at
 * @param out_pos Receives the offset position, clamped
 *
 * @return @c S_PLACE_RESULT_POSITION when a sibling was found,
 *         @c S_PLACE_RESULT_DECLINED otherwise
 *
 * @note Resolved against the monitor the sibling sits on rather than
 *       the one the pointer is over, a related window being meant to
 *       stay with its group wherever that is
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
static enum s_place_result_e s_place_step_sibling(
        const struct s_place_ctx_s *ctx,
        struct position_s *restrict out_pos)
{
    struct geometry_s sibling_wa;
    struct dimensions_s sibling_sz;
    void *elem;
    client_td *anchor = NULL;
    uint32_t sibling_count = 0u;

    if (ctx->leader == XCB_WINDOW_NONE || ctx->desktop == NULL ||
            ctx->desktop->clients == NULL ||
            !ctx->config->base.windows.group_related) {
        return S_PLACE_RESULT_DECLINED;
    }

    ohtbl_foreach(ctx->desktop->clients, elem) {
        client_td *const sibling = (client_td *) elem;

        if (sibling == ctx->client ||
                client_group_leader(sibling) != ctx->leader ||
                client_is_iconified(sibling)) {
            continue;
        }

        sibling_count++;
        anchor = sibling;
    }

    if (anchor == NULL) {
        return S_PLACE_RESULT_DECLINED;
    }

    out_pos->x = anchor->layout.geometry.cur.pos.x +
        (int32_t) ((uint32_t) WM_PLACE_CASCADE_STEP * sibling_count);
    out_pos->y = anchor->layout.geometry.cur.pos.y +
        (int32_t) ((uint32_t) WM_PLACE_CASCADE_STEP * sibling_count);

    placement_clip_to_monitor(ctx->surface, &ctx->wa, &ctx->screen,
            surface_monitor_for_point(ctx->surface,
                    (struct position_s) {
                        out_pos->x + (int32_t) (ctx->frame.w / 2u),
                        out_pos->y + (int32_t) (ctx->frame.h / 2u) }),
            &sibling_wa, &sibling_sz);

    /* Clamped the same way the cascade policy is, so a sibling near
     * the edge does not push the new window off screen */
    if (out_pos->x < sibling_wa.pos.x) {
        out_pos->x = sibling_wa.pos.x;
    }
    if (out_pos->y < sibling_wa.pos.y) {
        out_pos->y = sibling_wa.pos.y;
    }
    if ((uint32_t) out_pos->x + ctx->frame.w > sibling_sz.w) {
        out_pos->x = (sibling_sz.w > ctx->frame.w)
            ? (int32_t) (sibling_sz.w - ctx->frame.w)
            : sibling_wa.pos.x;
    }
    if ((uint32_t) out_pos->y + ctx->frame.h > sibling_sz.h) {
        out_pos->y = (sibling_sz.h > ctx->frame.h)
            ? (int32_t) (sibling_sz.h - ctx->frame.h)
            : sibling_wa.pos.y;
    }

    return S_PLACE_RESULT_POSITION;
}


/**
 * @brief Look for the spot overlapping least of what is already shown
 *
 * @param ctx     Everything the step may look at
 * @param out_pos Receives the spot the scan settled on
 *
 * @return @c S_PLACE_RESULT_POSITION when the scan found one,
 *         @c S_PLACE_RESULT_DECLINED when nothing is free
 *
 * @note Complexity: @e O(g * n), where @e g is the number of grid
 *       positions tested and @e n is the number of clients on
 *       the desktop
 */
static enum s_place_result_e s_place_step_smart(
        const struct s_place_ctx_s *ctx,
        struct position_s *restrict out_pos)
{
    if (!place_window_smart(ctx->wm, ctx->surface, ctx->client,
                &out_pos->x, &out_pos->y)) {
        return S_PLACE_RESULT_DECLINED;
    }

    return S_PLACE_RESULT_POSITION;
}


/**
 * @brief Step each window down and to the right of the last
 *
 * @param ctx     Everything the step may look at
 * @param out_pos Unused; the cascade places the window itself
 *
 * @return @c S_PLACE_RESULT_DONE, always
 *
 * @note Complexity: @e O(1)
 */
static enum s_place_result_e s_place_step_cascade(
        const struct s_place_ctx_s *ctx,
        struct position_s *restrict out_pos)
{
    (void) out_pos;

    place_window_apply_cascade(ctx->wm, ctx->surface, ctx->client);

    return S_PLACE_RESULT_DONE;
}


/**
 * @brief Center the window on the workarea
 *
 * On the workarea of the reference monitor, not on the full screen, so
 * the window is centered on what can actually be used rather than on
 * whatever a panel or a second monitor leaves it sitting across.
 *
 * @param ctx     Everything the step may look at
 * @param out_pos Receives the centered position
 *
 * @return @c S_PLACE_RESULT_POSITION, always
 *
 * @note Complexity: @e O(1)
 */
static enum s_place_result_e s_place_step_centered(
        const struct s_place_ctx_s *ctx,
        struct position_s *restrict out_pos)
{
    out_pos->x = ctx->mon_wa.pos.x +
        ((int32_t) ctx->mon_wa.dim.w - (int32_t) ctx->frame.w) / 2;
    out_pos->y = ctx->mon_wa.pos.y +
        ((int32_t) ctx->mon_wa.dim.h - (int32_t) ctx->frame.h) / 2;

    if (out_pos->x < ctx->mon_wa.pos.x) {
        out_pos->x = ctx->mon_wa.pos.x;
    }
    if (out_pos->y < ctx->mon_wa.pos.y) {
        out_pos->y = ctx->mon_wa.pos.y;
    }

    return S_PLACE_RESULT_POSITION;
}


/**
 * @brief Ask the user where the window goes, and sit meanwhile where
 *        the smart scan chose
 *
 * @param ctx     Everything the step may look at
 * @param out_pos Receives the position the outline starts from
 *
 * @return @c S_PLACE_RESULT_POSITION when the scan found one,
 *         @c S_PLACE_RESULT_DECLINED when nothing is free
 *
 * @note Declining still leaves the window marked as one to ask about,
 *       so the question is put either way and only the position it
 *       starts from differs
 * @note Complexity: @e O(g * n), where @e g is the number of grid
 *       positions tested and @e n is the number of clients on
 *       the desktop
 */
static enum s_place_result_e s_place_step_manual(
        const struct s_place_ctx_s *ctx,
        struct position_s *restrict out_pos)
{
    if (!place_window_manual(ctx->wm, ctx->surface, ctx->client,
                &out_pos->x, &out_pos->y)) {
        return S_PLACE_RESULT_DECLINED;
    }

    return S_PLACE_RESULT_POSITION;
}


/**
 * @brief Put the window where the pointer is
 *
 * @param ctx     Everything the step may look at
 * @param out_pos Receives the position, centered on the pointer and
 *                clamped to the reference monitor
 *
 * @return @c S_PLACE_RESULT_POSITION when the pointer was found,
 *         @c S_PLACE_RESULT_DONE when it was not
 *
 * @note A failed pointer query answers @c S_PLACE_RESULT_DONE rather
 *       than declining, since falling back to the cascade would move
 *       the window for no reason; the position the X server already
 *       gave it is left alone instead
 * @note Complexity: @e O(1)
 */
static enum s_place_result_e s_place_step_under_mouse(
        const struct s_place_ctx_s *ctx,
        struct position_s *restrict out_pos)
{
    xcb_query_pointer_reply_t *pointer_reply;

    pointer_reply = xcb_query_pointer_reply(ctx->connection,
            xcb_query_pointer(ctx->connection, ctx->surface->screen->root),
            NULL);
    if (pointer_reply == NULL) {
        LOGGER_WARNING("Failed to query pointer for 'under-mouse'" \
                " placement; keeping X-server-assigned position",
                L_NARG);
        return S_PLACE_RESULT_DONE;
    }

    out_pos->x = (int32_t) pointer_reply->root_x -
        (int32_t) (ctx->frame.w / 2u);
    out_pos->y = (int32_t) pointer_reply->root_y -
        (int32_t) (ctx->frame.h / 2u);
    free(pointer_reply);

    if (out_pos->x < ctx->mon_wa.pos.x) {
        out_pos->x = ctx->mon_wa.pos.x;
    } else if ((uint32_t) out_pos->x + ctx->frame.w > ctx->mon_sz.w) {
        out_pos->x = (ctx->mon_sz.w > ctx->frame.w)
            ? (int32_t) (ctx->mon_sz.w - ctx->frame.w)
            : ctx->mon_wa.pos.x;
    }
    if (out_pos->y < ctx->mon_wa.pos.y) {
        out_pos->y = ctx->mon_wa.pos.y;
    } else if ((uint32_t) out_pos->y + ctx->frame.h > ctx->mon_sz.h) {
        out_pos->y = (ctx->mon_sz.h > ctx->frame.h)
            ? (int32_t) (ctx->mon_sz.h - ctx->frame.h)
            : ctx->mon_wa.pos.y;
    }

    return S_PLACE_RESULT_POSITION;
}


/**
 * @brief Leave the window where the X server put it
 *
 * What a policy this file does not recognize falls back on, and the
 * only step that can answer that nothing at all needs doing.  The
 * position is kept as it stands unless the title bar would be hidden
 * above the workarea top, behind a panel, or above the screen edge.
 *
 * @param ctx     Everything the step may look at
 * @param out_pos Receives the position, once corrected
 *
 * @return @c S_PLACE_RESULT_DONE when the position was already good,
 *         @c S_PLACE_RESULT_POSITION when it had to be corrected
 *
 * @note Never declines, being what declining falls back to
 * @note Complexity: @e O(1)
 */
static enum s_place_result_e s_place_step_keep(
        const struct s_place_ctx_s *ctx,
        struct position_s *restrict out_pos)
{
    *out_pos = ctx->client->layout.geometry.cur.pos;

    if (out_pos->x < 0) {
        out_pos->x = 0;
    }
    if (out_pos->y < ctx->wa.pos.y) {
        out_pos->y = ctx->wa.pos.y;
    }

    if (out_pos->x == ctx->client->layout.geometry.cur.pos.x &&
            out_pos->y == ctx->client->layout.geometry.cur.pos.y) {
        return S_PLACE_RESULT_DONE;
    }

    return S_PLACE_RESULT_POSITION;
}


/* Apply the configured placement policy to a newly mapped client */
void place_window_apply(const wm_td *wm,
        surface_td *surface, client_td *client)
{
    /* Tried in this order, and the order is the precedence.  What used
     * to be several paragraphs explaining why a splash has to be
     * settled before an honored position, and that before the
     * transient centering, is now the order they are written in */
    const s_place_step_fn overrides[] = {
        s_place_step_splash,
        s_place_step_requested,
        s_place_step_transient
    };
    /* Indexed by 'config_placement_policy_e' itself, so a member added
     * to that enumeration and not wired here shows up as a null entry
     * and falls back, rather than quietly taking some neighbour's
     * behavior */
    const s_place_step_fn policies[CONFIG_PLACEMENT_POLICY_MANUAL + 1] = {
        [CONFIG_PLACEMENT_POLICY_SMART] = s_place_step_smart,
        [CONFIG_PLACEMENT_POLICY_CASCADE] = s_place_step_cascade,
        [CONFIG_PLACEMENT_POLICY_CENTERED] = s_place_step_centered,
        [CONFIG_PLACEMENT_POLICY_UNDER_MOUSE] = s_place_step_under_mouse,
        [CONFIG_PLACEMENT_POLICY_MANUAL] = s_place_step_manual
    };
    struct s_place_ctx_s ctx;
    struct position_s chosen = { 0, 0 };
    enum s_place_result_e result;
    config_td *config = wm_config(wm);

    if (wm == NULL || config == NULL ||
            surface == NULL || client == NULL) {
        return;
    }

    ctx.wm = wm;
    ctx.surface = surface;
    ctx.client = client;
    ctx.config = config;
    ctx.connection = wm_connection(wm);
    ctx.screen = surface->properties.dim;
    ctx.frame = client->layout.geometry.cur.dim;
    ctx.leader = client_group_leader(client);

    /* The usable workarea, which respects panel struts, falling back
     * to the whole screen when no workarea is set */
    ctx.desktop = surface_desktop_get(surface, surface->desktop_cur);
    if (ctx.desktop != NULL && ctx.desktop->workarea.dim.w > 0u &&
            ctx.desktop->workarea.dim.h > 0u) {
        ctx.wa = ctx.desktop->workarea;
    } else {
        ctx.wa.pos.x = 0;
        ctx.wa.pos.y = 0;
        ctx.wa.dim = ctx.screen;
    }

    /* Neither the monitor-clipped workarea nor its screen bound mean
     * anything to the overrides below, which all work off the whole
     * surface, so they are left unset until after those have had their
     * turn (see the clip further down) */
    ctx.mon_wa = ctx.wa;
    ctx.mon_sz = ctx.screen;

    for (size_t i = 0u; i < sizeof(overrides) / sizeof(overrides[0]);
            ++i) {
        result = overrides[i](&ctx, &chosen);

        if (result == S_PLACE_RESULT_DONE) {
            return;
        }
        if (result == S_PLACE_RESULT_POSITION) {
            s_place_window_finalize(surface, client, ctx.wa.pos, chosen);
            return;
        }
    }

    /* Clipped down to whichever physical monitor
     * 'windows.placement.monitor' resolves to, on a surface made of
     * more than one: the steps below score and clamp against these two,
     * and without this they would do so against the whole combined area
     * instead of one monitor.  Falls back to the unclipped values when
     * there is only one monitor, or when clipping would leave nothing
     * to place into. */
    placement_clip_to_monitor(surface, &ctx.wa, &ctx.screen,
            placement_reference_monitor(wm, surface, client,
                    config->base.windows.monitor_policy),
            &ctx.mon_wa, &ctx.mon_sz);

    /* Ahead of the configured policy rather than one of it.  A window
     * joining a group it belongs to is a stronger statement about
     * where it goes than any of them */
    result = s_place_step_sibling(&ctx, &chosen);

    if (result == S_PLACE_RESULT_DECLINED) {
        const enum config_placement_policy_e policy =
            config->base.windows.placement_policy;
        const s_place_step_fn step =
            ((unsigned int) policy <
                sizeof(policies) / sizeof(policies[0]))
            ? policies[policy] : NULL;

        /* A policy this file does not know leaves the window where the
         * X server put it; one it does know, that found nowhere to put
         * it, cascades.  One fallback in one place, rather than the
         * copy the smart policy and the manual policy each kept. */
        result = (step == NULL)
            ? s_place_step_keep(&ctx, &chosen)
            : step(&ctx, &chosen);

        if (result == S_PLACE_RESULT_DECLINED) {
            result = s_place_step_cascade(&ctx, &chosen);
        }
    }

    if (result == S_PLACE_RESULT_POSITION) {
        s_place_window_finalize(surface, client, ctx.wa.pos, chosen);
    }
}
