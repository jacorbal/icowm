/**
 * @file policy/placement/score.c
 *
 * @brief Shared overlap-scoring core for SMART placement implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Utils includes */
#include <utils/geom.h>

/* Type includes */
#include <types/pair.h>

/* Default initial values */
#include <defs/placement.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <policy/stacking.h>

/* Local includes */
#include <policy/placement/score.h>


/**
 * @brief What @a s_score_window_visit sums up across the windows it
 *        meets
 */
struct s_score_ctx_s {
    struct geometry_s candidate;    /**< Rectangle being scored */
    const client_td *skip_client;   /**< Client left out of the sum */
    uint64_t win_pixel_cost;    /**< Cost per covered window pixel */
    uint64_t icon_pixel_cost;   /**< Cost per covered icon pixel */
    uint64_t cost;                  /**< Running total */
};


/**
 * @brief Add what one window would cost the candidate rectangle
 *
 * @param client Client reached by the walk
 * @param data   Pointer to the @c s_score_ctx_s this walk carries
 *
 * @note An iconified client costs by its icon rather than its window,
 *       that being all of it the user can see
 * @note Complexity: @e O(1)
 */
static void s_score_window_visit(client_td *client, void *data)
{
    struct s_score_ctx_s *const score_ctx = data;
    uint32_t area;

    if (client == NULL || score_ctx == NULL ||
            client == score_ctx->skip_client ||
            (client->properties.flags & CLIENT_FLAG_HIDDEN) ||
            client_is_locked(client)) {
        return;
    }

    if (!client_is_iconified(client)) {
        /* Visible window */
        area = geom_intersection_area(
                score_ctx->candidate.pos.x, score_ctx->candidate.pos.y,
                score_ctx->candidate.dim.w, score_ctx->candidate.dim.h,
                client->layout.geometry.cur.pos.x,
                client->layout.geometry.cur.pos.y,
                client->layout.geometry.cur.dim.w,
                client->layout.geometry.cur.dim.h);
        score_ctx->cost += score_ctx->win_pixel_cost * (uint64_t) area;
    } else if (client->icon_window != 0u && client->is_icon_mapped &&
            client->icon_pos.x >= 0 && client->icon_pos.y >= 0) {
        /* Visible icon */
        area = geom_intersection_area(
                score_ctx->candidate.pos.x, score_ctx->candidate.pos.y,
                score_ctx->candidate.dim.w, score_ctx->candidate.dim.h,
                client->icon_pos.x, client->icon_pos.y,
                (uint32_t) PLACE_SMART_WIN_ICON_SIZE,
                (uint32_t) PLACE_SMART_WIN_ICON_SIZE);
        score_ctx->cost += score_ctx->icon_pixel_cost * (uint64_t) area;
    }
}


/* Accumulate overlap-penalty cost for a candidate rectangle against
 * every visible, unlocked client on a desktop */
uint64_t place_overlap_score(const desktop_td *desktop,
        const client_td *skip_client, struct geometry_s candidate,
        uint64_t win_pixel_cost, uint64_t icon_pixel_cost)
{
    struct s_score_ctx_s score_ctx;

    score_ctx.candidate = candidate;
    score_ctx.skip_client = skip_client;
    score_ctx.win_pixel_cost = win_pixel_cost;
    score_ctx.icon_pixel_cost = icon_pixel_cost;
    score_ctx.cost = 0u;
    stacking_walk(desktop, s_score_window_visit, &score_ctx);

    return score_ctx.cost;
}
