/**
 * @file input/mouse/bounds.h
 *
 * @brief Shared resize-border and adaptive grab-margin computation
 *
 * Declares @a im_resize_bounds, used by both
 * @c input/mouse/event/press.c
 * (border-hit detection and the resize-cursor display) and
 * @c input/mouse/drag.c (picking which corner a resize drag anchors
 * to), so the three stay in agreement about exactly where a client's
 * resize border is instead of drifting apart as three independent
 * computations.
 *
 * @note This header is private to the mouse subsystem and must not be
 *       included outside of @c src/input/mouse/
 *
 * @ingroup input_mouse
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_BOUNDS_H
#define INPUT_MOUSE_BOUNDS_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Project includes */
#include <client.h>


/**
 * @brief A client's resize border, in root coordinates, together with
 *        the adaptive per-edge grab margin around it
 *
 * Members @p left/top/right/bottom are the client's own current
 * bounding box (its frame's, if decorated; its own window's,
 * otherwise).  Members @p margin_left/top/right/bottom are how many
 * pixels beyond (outside @p left / @p top, inside @p right / @p bottom)
 * still count as "on that edge" for resize purposes; see
 * @a im_resize_bounds and @c WM_RESIZE_GRAB_THRESHOLD in
 * @c defs/input.h for how each is derived from that edge's own actual
 * border width.
 *
 * Member @p has_titlebar_row, when true, means @p titlebar_row_top and
 * @p titlebar_row_bottom bound a row (in the same root-coordinate
 * space) that the left/right margins do not apply within.  The titlebar
 * is a move region, not a resize one, so a point inside it should never
 * register as near the left or right edge merely because it is
 * horizontally close to one, the way a titlebar button such as close,
 * typically placed near the frame's own right edge, otherwise would.
 */
typedef struct {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
    int32_t margin_left;
    int32_t margin_top;
    int32_t margin_right;
    int32_t margin_bottom;
    bool has_titlebar_row;
    int32_t titlebar_row_top;
    int32_t titlebar_row_bottom;
} im_resize_bounds_td;


/**
 * @brief Compute a client's resize border and adaptive grab margins
 *
 * Each edge's margin is @a max(that edge's own actual border width,
 * @c WM_RESIZE_GRAB_THRESHOLD).  A border already at least the
 * threshold wide needs no help, so the margin is exactly that border
 * width, while a thinner one is padded out to the full threshold
 * instead, so every border thinner than the threshold feels the same to
 * grab regardless of how thin it visually is.  The top edge uses
 * only the border strip above the titlebar (see @c has_titlebar_row
 * above), never the titlebar's own height, as its border width for this
 * purpose.
 *
 * @param client Client to compute bounds for (must not be null)
 *
 * @return The computed bounds; every field is @c 0 if @p client is
 *         @c NULL
 *
 * @note Complexity: @e O(1)
 */
im_resize_bounds_td im_resize_bounds(const client_td *client);


#endif  /* ! INPUT_MOUSE_BOUNDS_H */
