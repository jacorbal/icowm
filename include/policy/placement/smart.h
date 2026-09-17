/**
 * @file policy/placement/smart.h
 *
 * @brief Smart placement: the search for the least-covered spot
 *
 * Walks the positions a client could take and keeps the one overlapping
 * least of what is already on screen, which is what "smart" means here.
 * The arithmetic it walks with lives in @c policy/placement/rect.h, and
 * the area it walks over comes from @c policy/placement/monitor.h.
 *
 * @defgroup placement_smart Smart placement
 * @ingroup policy
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef POLICY_PLACEMENT_SMART_H
#define POLICY_PLACEMENT_SMART_H


/* System includes */
#include <stdint.h>

/* Utils includes */
#include <utils/geom.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>

/* Local includes */
#include <defs/placement.h>


/* Public interface */
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
 * largest.  The client lands centered inside that free rectangle.  The
 * breathing room around it comes from how much real free space exists
 * there, not from any fixed margin.  Falls back to whichever candidate
 * has the least overlap when the desktop is too full for any candidate
 * to fit the client at all.
 *
 * The systray, not a real client, is treated as one more obstacle
 * alongside every visible client above, and its edges are tested as
 * candidate anchors the same way every client's edges already are, when
 * @c systray.avoid-overlap is @c true and @c systray.reserve-space is
 * @c false (see either one's comment in @c config.h).  Both matter
 * equally, since testing the tray's edges as candidates without also
 * shrinking against the tray itself would let a candidate anchored
 * right at its corner overlap it outright, and shrinking against it
 * without testing its edges as candidates would leave real free space
 * sitting right next to the tray untested, unable to ever be found
 * (this second half is the easier one to overlook.
 *
 * A corner that is otherwise a genuinely productive candidate, workarea
 * (0, 0) with the tray docked there by default, collapses to zero free
 * area once the tray shrinks against it, and nothing replaced it as
 * a candidate anchored at the tray's edge instead, until this).
 * Fetched fresh from @a systray_get_geometry for this one placement
 * decision, then passed to every @a placement_free_rect_grow /
 * @a placement_score_window_pos call the same way @p desktop's clients
 * already are.  Affects placement scoring only, nothing about the tray
 * becoming movable, iconifiable, or otherwise actable on the way a real
 * window is.
 *
 * @param wm     Pointer to the window manager singleton
 * @param stage  Pointer to the stage where the client will appear
 * @param client Pointer to the client being placed
 * @param out_x  Output pointer for the selected X coordinate
 * @param out_y  Output pointer for the selected Y coordinate
 *
 * @return @c true if a free position was found
 *
 * @note Complexity: @e O(g * n), where @e g is the number of grid
 *       positions tested and @e n is the number of clients on the
 *       current desktop
 */
bool place_window_smart(const wm_td *wm,
        stage_td *stage, client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y);


#endif  /* ! POLICY_PLACEMENT_SMART_H */
