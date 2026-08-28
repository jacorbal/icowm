/**
 * @file policy/placement/smart.c
 *
 * @brief Smart placement: the search for the least-overlapped spot
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
#include <policy/stacking.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <defs/placement.h>
#include <policy/placement/score.h>
#include <policy/placement/window.h>
#include <policy/placement/monitor.h>
#include <policy/placement/rect.h>
#include <policy/placement/smart.h>


/**
 * @brief Shared state one @a place_window_smart call threads through
 *        every candidate it tests
 */
struct s_place_window_smart_ctx_s {
    const desktop_td *desktop;    /**< Desktop @c client is placed on */
    /** Client being placed, excluded from the overlap checks */
    const client_td *skip_client;
    uint32_t fw;            /**< Client's frame width */
    uint32_t fh;            /**< Client's frame height */

    const struct geometry_s *tray_rect; /**< Systray rectangle to also
                                             avoid, or @c NULL */

    int32_t min_x;          /**< Leftmost a candidate corner may sit */
    int32_t min_y;          /**< Topmost a candidate corner may sit */
    int32_t max_x;          /**< Rightmost a candidate corner may sit */
    int32_t max_y;          /**< Bottommost a corner may sit */
    int32_t bound_right;    /**< Workarea's physical right edge */
    int32_t bound_bottom;   /**< Workarea's physical bottom edge */
    int32_t center_x;       /**< Workarea center X, tie-break ref */
    int32_t center_y;       /**< Workarea center Y, tie-break ref */
    int32_t best_x;         /**< Best candidate found so far, X */
    int32_t best_y;         /**< Best candidate found so far, Y */
    uint64_t best_cost;     /**< Lowest overlap cost found so far */
    uint64_t best_area;     /**< Largest free area found so far */
    bool has_free_rect;     /**< Whether any candidate so far actually
                                 fit @c client without overlapping
                                 anything */
};


/**
 * @brief Resolve the systray's current on-screen rectangle for one
 *        placement decision, when @c systray.avoid-overlap applies
 *
 * A live rectangle is fetched here, rather than trusting a stale one
 * cached earlier, since the tray can move, resize, or disappear between
 * one placement decision and the next.
 *
 * @param config   Window manager configuration
 * @param surface  Surface the placement decision is for
 * @param out_geom Receives the systray's rectangle when resolved;
 *                 untouched otherwise
 *
 * @return @p out_geom, or @c NULL when @c systray.avoid-overlap does
 *         not apply (see its doc comment, config.h), or the systray has
 *         no on-screen rectangle to report right now
 *
 * @note Complexity: @e O(1)
 */
static const struct geometry_s
    *s_place_window_smart_resolve_tray_rect(const config_td *config,
            const surface_td *surface, struct geometry_s *out_geom)
{
    if (config->base.systray.avoid_overlap &&
            !config->base.systray.reserve_space &&
            systray_get_geometry(surface, out_geom)) {
        return out_geom;
    }
    return NULL;
}


/**
 * @brief Test one candidate top-left corner, clamping it into the
 *        workarea first, keeping it if it beats whichever free
 *        rectangle or overlap cost @p ctx found so far
 *
 * @param ctx Placement context; @c best_x / @c best_y / @c best_cost /
 *            @c best_area / @c has_free_rect updated in place
 * @param x   Candidate left coordinate, not yet clamped
 * @param y   Candidate top coordinate, not yet clamped
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @c ctx->desktop (the cost of one @a placement_free_rect_grow /
 *       @a placement_score_window_pos pair)
 */
static void s_place_window_smart_test_candidate(
        struct s_place_window_smart_ctx_s *ctx, int32_t x, int32_t y)
{
    uint32_t free_w;
    uint32_t free_h;
    uint64_t cost;

    if (x < ctx->min_x) { x = ctx->min_x; }
    if (x > ctx->max_x) { x = ctx->max_x; }
    if (y < ctx->min_y) { y = ctx->min_y; }
    if (y > ctx->max_y) { y = ctx->max_y; }

    placement_free_rect_grow(ctx->desktop, ctx->skip_client, x, y,
            ctx->bound_right, ctx->bound_bottom, ctx->tray_rect,
            &free_w, &free_h);
    if (free_w >= ctx->fw && free_h >= ctx->fh) {
        uint64_t area = (uint64_t) free_w * (uint64_t) free_h;

        if (area > ctx->best_area) {
            ctx->best_area = area;
            ctx->best_x = x + (int32_t) ((free_w - ctx->fw) / 2u);
            ctx->best_y = y + (int32_t) ((free_h - ctx->fh) / 2u);
            ctx->has_free_rect = true;
        }
    }
    cost = placement_score_window_pos(ctx->desktop, ctx->skip_client,
            x, y,
            ctx->fw, ctx->fh, ctx->tray_rect,
            ctx->center_x, ctx->center_y);
    if (cost < ctx->best_cost) {
        ctx->best_cost = cost;
        if (!ctx->has_free_rect) {
            ctx->best_x = x;
            ctx->best_y = y;
        }
    }
}


/**
 * @brief What @a s_place_window_edge_visit needs beyond the client
 */
struct s_edge_ctx_s {
    /** The placement in progress, whose candidates this adds to */
    struct s_place_window_smart_ctx_s *place_ctx;
    /** Window being placed, which is not a candidate for itself */
    const client_td *skip_client;
};


/**
 * @brief Try the four spots flush against one window's own edges
 *
 * @param client Client reached by the walk
 * @param data   Pointer to the @c s_edge_ctx_s this walk carries
 *
 * @note Complexity: @e O(n), the cost of scoring four candidates
 */
static void s_place_window_edge_visit(client_td *client, void *data)
{
    struct s_edge_ctx_s *const edge_ctx = data;
    int32_t edge_x[4];
    int32_t edge_y[4];

    if (client == NULL || edge_ctx == NULL ||
            client == edge_ctx->skip_client ||
            (client->properties.flags & CLIENT_FLAG_HIDDEN) ||
            client_is_locked(client) || client_is_iconified(client)) {
        return;
    }

    edge_x[0] = client->layout.geometry.cur.pos.x +
        (int32_t) client->layout.geometry.cur.dim.w;
    edge_x[1] = client->layout.geometry.cur.pos.x -
        (int32_t) edge_ctx->place_ctx->fw;
    edge_x[2] = client->layout.geometry.cur.pos.x;
    edge_x[3] = client->layout.geometry.cur.pos.x;

    edge_y[0] = client->layout.geometry.cur.pos.y;
    edge_y[1] = client->layout.geometry.cur.pos.y;
    edge_y[2] = client->layout.geometry.cur.pos.y +
        (int32_t) client->layout.geometry.cur.dim.h;
    edge_y[3] = client->layout.geometry.cur.pos.y -
        (int32_t) edge_ctx->place_ctx->fh;

    for (int index = 0; index < 4; ++index) {
        s_place_window_smart_test_candidate(edge_ctx->place_ctx,
                edge_x[index], edge_y[index]);
    }
}


/**
 * @brief Find a non-overlapping smart position for a newly mapped
 *        client, centered inside the largest genuinely free area found
 *        on the current desktop
 *
 * Tests a bounded set of candidate top-left corners (the workarea
 * center, its four corners, every edge of every visible client already
 * on the desktop, and, when @c systray.avoid-overlap applies (see
 * below), every edge of the tray too), grows the real free rectangle
 * anchored at each one with @a placement_free_rect_grow, and keeps the
 * largest.  The client lands centered inside that free rectangle: the
 * breathing room around it comes from how much real free space exists
 * there, not from any fixed margin.  Falls back to whichever candidate
 * has the least overlap when the desktop is too full for any candidate
 * to fit the client at all.
 *
 * The systray, not a real client, is treated as one more obstacle
 * alongside every visible client above, and its edges are tested as
 * candidate anchors the same way every client's edges already are, when
 * @c systray.avoid-overlap is @c true and @c systray.reserve-space is
 * @c false (see either one's comment in @c config.h): both matter
 * equally, since testing the tray's edges as candidates without also
 * shrinking against the tray itself would let a candidate anchored
 * right at its corner overlap it outright, and shrinking against it
 * without testing its edges as candidates would leave real free space
 * sitting right next to the tray untested, unable to ever be found
 * (this second half is what actually went missing at first.  A corner
 * that used to be a genuinely productive candidate, workarea (0, 0)
 * with the tray docked there by default, collapses to zero free area
 * once the tray shrinks against it, and nothing replaced it as
 * a candidate anchored at the tray's edge instead, until this).
 * Fetched fresh from @a systray_get_geometry for this one placement
 * decision, then passed to every @a placement_free_rect_grow /
 * @a placement_score_window_pos call the same way @p desktop's own
 * clients already are.  Affects placement scoring only, nothing about
 * the tray becoming movable, iconifiable, or otherwise actable on the
 * way a real window is.
 *
 * @param wm      Pointer to the window manager singleton
 * @param surface Pointer to the surface where the client will appear
 * @param client  Pointer to the client being placed
 * @param out_x   Output pointer for the selected X coordinate
 * @param out_y   Output pointer for the selected Y coordinate
 *
 * @return @c true if a position was found, @c false otherwise
 *
 * @note Complexity: @e O(n^3) worst case, where @e n is the number of
 *       clients on the current desktop (@e n candidates, each scored by
 *       @a placement_free_rect_grow's @e O(n^2))
 */
bool place_window_smart(const wm_td *wm,
        surface_td *surface, client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y)
{
    desktop_td *desktop;
    struct geometry_s wa;
    struct geometry_s mon_wa;
    struct dimensions_s mon_sz;
    struct geometry_s tray_geom;
    struct s_place_window_smart_ctx_s ctx;
    struct s_edge_ctx_s edge_ctx;
    const xcb_connection_t *connection = wm_connection(wm);
    const config_td *config = wm_config(wm);

    if (surface == NULL || client == NULL ||
            out_x == NULL || out_y == NULL || wm == NULL ||
            connection == NULL || config == NULL) {
        return false;
    }

    desktop = surface_desktop_get(surface, surface->desktop_cur);
    if (desktop == NULL) {
        return false;
    }

    /* Resolves the workarea (falling back to the full surface
     * dimensions when none is set) and clips it to whichever physical
     * monitor 'windows.placement.monitor' resolves to, on a surface
     * made of more than one (the common case of several monitors
     * sharing one combined X screen): a new window should land within
     * one monitor, not be scored against the whole combined area, which
     * could place it straddling the seam between two of them.  'wa'
     * (unclipped) is only needed because 'placement_workarea' requires
     * somewhere to write it; every candidate below is tested against
     * 'mon_wa' instead. */
    placement_workarea(wm, surface, client, &wa, &mon_wa, &mon_sz);

    ctx.desktop = desktop;
    ctx.skip_client = client;
    ctx.fw = client->layout.geometry.cur.dim.w;
    ctx.fh = client->layout.geometry.cur.dim.h;
    ctx.tray_rect = s_place_window_smart_resolve_tray_rect(config,
            surface, &tray_geom);

    /* Candidate range keeps the top-left corner inside the workarea;
     * 'bound_right'/'bound_bottom' are the workarea's physical edges
     * instead, used by 'placement_free_rect_grow' to grow a free
     * rectangle as far as it genuinely goes, not just as far as
     * a top-left corner could still sit. */
    ctx.min_x = mon_wa.pos.x;
    ctx.min_y = mon_wa.pos.y;
    ctx.max_x = (mon_wa.dim.w > ctx.fw)
        ? mon_wa.pos.x + (int32_t) (mon_wa.dim.w - ctx.fw)
        : mon_wa.pos.x;
    ctx.max_y = (mon_wa.dim.h > ctx.fh)
        ? mon_wa.pos.y + (int32_t) (mon_wa.dim.h - ctx.fh)
        : mon_wa.pos.y;
    ctx.bound_right = mon_wa.pos.x + (int32_t) mon_wa.dim.w;
    ctx.bound_bottom = mon_wa.pos.y + (int32_t) mon_wa.dim.h;

    /* Workarea center used as the distance tie-breaker reference */
    ctx.center_x = mon_wa.pos.x + (int32_t) (mon_wa.dim.w / 2u);
    ctx.center_y = mon_wa.pos.y + (int32_t) (mon_wa.dim.h / 2u);

    ctx.best_x = mon_wa.pos.x;
    ctx.best_y = mon_wa.pos.y;
    ctx.best_cost = UINT64_MAX;
    ctx.best_area = 0u;
    ctx.has_free_rect = false;

    /* Try one candidate top-left corner at a time: the centered seed
     * first (so an empty desktop still lands the first window in the
     * middle of the screen), every corner of the workarea itself, the
     * systray's edges when 'systray.avoid-overlap' applies (see
     * 'ctx.tray_rect''s comment,
     * 's_place_window_smart_resolve_tray_rect'), then every edge of
     * every visible client already on this desktop.  A genuinely free
     * rectangle, whenever one exists, always has at least one edge
     * touching either another obstacle's edge or the workarea's
     * boundary, so these candidates are enough to find it without
     * testing a whole grid of positions in between two obstacles where
     * nothing changes.  Each candidate keeps whichever free rectangle
     * found so far is largest ('s_place_window_smart_test_candidate'):
     * the window ends up centered inside that real free space, not
     * pinned to whichever corner happened to be tried first, so the
     * breathing room around it comes from the free space itself rather
     * than from any fixed margin. */
    s_place_window_smart_test_candidate(&ctx,
            ctx.center_x - (int32_t) (ctx.fw / 2u),
            ctx.center_y - (int32_t) (ctx.fh / 2u));

    for (int ci = 0; ci < 2; ++ci) {
        const int32_t corner_x = (ci == 0) ? ctx.min_x : ctx.max_x;

        for (int cj = 0; cj < 2; ++cj) {
            const int32_t corner_y = (cj == 0) ? ctx.min_y : ctx.max_y;

            s_place_window_smart_test_candidate(&ctx,
                    corner_x, corner_y);
        } /* ! for (cj) */
    } /* ! for (ci) */

    /* The tray, not iterated per client since it is a single fixed
     * obstacle for this whole placement decision (unlike every client
     * edge below, tested once per visible client): without this, the
     * corner candidate that would otherwise land exactly on the tray's
     * corner collapses to zero free area (that same check inside
     * 's_free_rect_shrink_against' above), and no replacement candidate
     * anchored at the tray's edge ever gets tried in its place, so real
     * free space sitting right next to the tray goes completely
     * untested. */
    if (ctx.tray_rect != NULL) {
        const int32_t tx = ctx.tray_rect->pos.x;
        const int32_t ty = ctx.tray_rect->pos.y;
        const int32_t tw = (int32_t) ctx.tray_rect->dim.w;
        const int32_t th = (int32_t) ctx.tray_rect->dim.h;
        const int32_t edge_x[4] = {
            tx + tw, tx - (int32_t) ctx.fw, tx, tx
        };
        const int32_t edge_y[4] = {
            ty, ty, ty + th, ty - (int32_t) ctx.fh
        };

        for (int ei = 0; ei < 4; ++ei) {
            s_place_window_smart_test_candidate(&ctx,
                    edge_x[ei], edge_y[ei]);
        } /* ! for (ei) */
    }

    edge_ctx.place_ctx = &ctx;
    edge_ctx.skip_client = client;
    stacking_walk(desktop, s_place_window_edge_visit, &edge_ctx);

    LOGGER_DEBUG("Smart-placed window (pos=%+d%+d, size=%ux%u," \
            " free-rect=%s, free-area=%llu, tray=%s, wa-pos=%+d%+d," \
            " wa-size=%ux%u)",
            ctx.best_x, ctx.best_y, ctx.fw, ctx.fh,
            (ctx.has_free_rect) ? "yes" : "no",
            (unsigned long long) ctx.best_area,
            (ctx.tray_rect != NULL) ? "yes" : "no",
            mon_wa.pos.x, mon_wa.pos.y, mon_wa.dim.w, mon_wa.dim.h);

    *out_x = ctx.best_x;
    *out_y = ctx.best_y;
    return true;
}
