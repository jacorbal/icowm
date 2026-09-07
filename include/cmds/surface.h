/**
 * @file cmds/surface.h
 *
 * @brief Declaration of actions related to screen surface management
 *
 * @ingroup cmds
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_SCMD_H
#define CMDS_SCMD_H


/* Project includes */
#include <surface.h>


/* Public interface */
/**
 * @brief Switch the current view to a specified desktop
 *
 * @param surface    Pointer to the surface
 * @param desktop_id Target desktop index
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch(surface_td *surface,
        uint32_t desktop_id);

/**
 * @brief Switch the current view to the desktop north of the current
 *        one
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch_north(surface_td *surface);

/**
 * @brief Switch the current view to the desktop south of the current
 *        one
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch_south(surface_td *surface);

/**
 * @brief Switch the current view to the desktop east of the current
 *        one
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch_east(surface_td *surface);

/**
 * @brief Switch the current view to the desktop west of the current
 *        one
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch_west(surface_td *surface);

/**
 * @brief Whether the current desktop's viewport still has room to pan
 *        one more screen toward @p direction
 *
 * Shared by @c input/mouse/drag/warp.h (to defer an edge-triggered
 * desktop warp while a pan is still possible instead) and
 * @c input/mouse/drag/pan.h (to decide whether an edge-triggered pan
 * itself is), so neither has to duplicate the clamp math
 * @a scmd_surface_viewport_pan_north and its three siblings already
 * apply.
 *
 * @param surface   Pointer to the surface
 * @param direction Compass direction to check
 *
 * @return Whether panning one more screen toward @p direction would
 *         actually move the viewport
 *
 * @note Complexity: @e O(1)
 */
bool scmd_surface_viewport_pan_available(surface_td *surface,
        enum compass_direction_e direction);

/**
 * @brief Set or clear the one client a viewport pan's per-client
 *        translate walk should leave untouched
 *
 * A drag in progress already repositions @p client (or leaves it
 * deliberately parked off screen, for an outline drag) through its
 * own logic in @c input/mouse/drag/pan.c, so folding it into the
 * ordinary translate walk too (@c s_viewport_translate_visit, this
 * file) would fight that logic instead of cooperating with it: an
 * outline drag in particular parks the real window off screen for
 * the whole drag and must never have that parking spot silently
 * nudged back toward the visible screen by an unrelated pan.
 *
 * @param client Client to exclude from every future pan's translate
 *               walk until cleared, or @c NULL to clear it
 *
 * @note Passing @c NULL clears the exclusion once the drag ends or is
 *       cancelled
 * @note Complexity: @e O(1)
 */
void scmd_surface_viewport_drag_exclude(client_td *client);

/**
 * @brief Pan the current desktop's viewport one screen north, clamped
 *        at the top of the pannable area
 *
 * A no-op whenever the current desktop's configured 'viewport' is
 * only one screen tall, or the viewport already sits at its northmost
 * origin: unlike @a scmd_surface_desktop_switch_north, this never
 * wraps around and never changes which desktop is current, only where
 * within it the screen is looking.
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void scmd_surface_viewport_pan_north(surface_td *surface);

/**
 * @brief Pan the current desktop's viewport one screen south, clamped
 *        at the bottom of the pannable area
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void scmd_surface_viewport_pan_south(surface_td *surface);

/**
 * @brief Pan the current desktop's viewport one screen east, clamped
 *        at the right of the pannable area
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void scmd_surface_viewport_pan_east(surface_td *surface);

/**
 * @brief Pan the current desktop's viewport one screen west, clamped
 *        at the left of the pannable area
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void scmd_surface_viewport_pan_west(surface_td *surface);

/**
 * @brief Pan the current desktop's viewport by @c viewport.pan-step
 *        pixels toward @p direction, clamped at the edges of the
 *        pannable area
 *
 * Unlike @a scmd_surface_viewport_pan_north and its three siblings,
 * which always move by a full screen and are shared with the
 * edge-triggered drag/hover pan mechanisms, this is the keyboard-only
 * counterpart: one press moves the viewport by a configurable pixel
 * amount instead of jumping a whole screen, the same way
 * @c windows.move-step already steps a selected client.
 *
 * @param surface   Pointer to the surface
 * @param direction Compass direction to pan toward
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void scmd_surface_viewport_pan_step(surface_td *surface,
        enum compass_direction_e direction);

/**
 * @brief Move the current desktop's viewport straight to an absolute
 *        origin, clamped to the pannable area
 *
 * The @c _NET_DESKTOP_VIEWPORT counterpart to
 * @a scmd_surface_viewport_pan_north and its three siblings: those
 * move by exactly one screen in a compass direction, while this jumps
 * straight to whatever @p x, @p y a pager or other external EWMH
 * client asked for, still translating every non-sticky client by the
 * resulting delta the same way.
 *
 * @param surface Pointer to the surface
 * @param x       Requested viewport origin's X coordinate, in pixels
 * @param y       Requested viewport origin's Y coordinate, in pixels
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void scmd_surface_viewport_set(surface_td *surface,
        int32_t x, int32_t y);

/**
 * @brief Jump the current desktop's viewport straight to one of its
 *        configured pages, addressed by a single linear index rather
 *        than an X/Y origin
 *
 * A page's own origin is derived the same way @a scmd_surface_
 * viewport_pan_east and its siblings already derive each one-screen
 * step: @p page's row is its index divided by the configured column
 * count, its column the remainder, each then multiplied by the
 * desktop's own screen-sized dimensions to land on that page's top-left
 * pixel, handed to @a scmd_surface_viewport_set exactly as a pager's
 * absolute request would be.  Out of the configured
 * @c (columns * @c rows) range, @p page is refused outright rather than
 * clamped into range: unlike a pan or a pager's arbitrary pixel origin,
 * a page index has no meaningful nearest neighbor to fall back to once
 * it no longer names any real page at all.
 *
 * @param surface Pointer to the surface
 * @param page    Zero-based page index, in row-major order across the
 *                configured viewport grid (§2.2's @c columns first,
 *                then @c rows)
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void scmd_surface_viewport_goto(surface_td *surface, uint32_t page);

/**
 * @brief Move @p client to a given page of the current desktop's
 *        viewport
 *
 * The client's offset within whichever page it sits on now is kept, so
 * a window near one page's top-left corner lands near the new page's
 * top-left corner rather than being re-placed by the placement policy.
 *
 * @param surface Surface whose current desktop owns @p client
 * @param client  Client to move
 * @param col     Zero-based destination column
 * @param row     Zero-based destination row
 *
 * @note A no-op on a sticky client, which is on screen from every
 *       origin and so belongs to no one page
 * @note Marks @p surface outdated, leaving the repaint to the caller's
 *       own render pass
 * @note Complexity: @e O(1)
 */
void scmd_surface_viewport_client_send_to_page(surface_td *surface,
        client_td *client, uint32_t col, uint32_t row);

/**
 * @brief Report the viewport page a client currently sits on
 *
 * A window belongs to the page its own top-left corner falls in, and
 * to that one only, whatever it overlaps.  A window straddling a
 * boundary, or one larger than a page, would otherwise belong to
 * several at once, which the "Send to page" submenu, the per-page
 * rearrange and @a scmd_surface_viewport_center_on_client all need a
 * single answer from.
 *
 * @param surface Surface owning @p desktop
 * @param desktop Desktop @p client sits on
 * @param client  Client to locate
 * @param col_out Where the zero-based column is written
 * @param row_out Where the zero-based row is written
 *
 * @retval  true when the viewport has pages and @p client belongs to
 *               one of them
 * @retval false on a single-page viewport, and for a sticky client,
 *               which is on screen from every origin and so belongs
 *               to no one page; both outputs are left untouched
 *
 * @note Complexity: @e O(1)
 */
bool scmd_surface_viewport_client_page(const surface_td *surface,
        const desktop_td *desktop, const client_td *client,
        uint32_t *col_out, uint32_t *row_out);

/**
 * @brief Report which configured viewport page @p desktop's viewport
 *        is currently panned to
 *
 * @param surface   Surface @p desktop belongs to, whose configured
 *                  viewport size the page is computed against
 * @param desktop   Desktop to report the currently shown page of
 * @param col_out   Resulting zero-based column, updated in place only
 *                  on a @c true return
 * @param row_out   Resulting zero-based row, updated in place only on
 *                  a @c true return
 *
 * @return @c false if @p surface or @p desktop is @c NULL, or if the
 *         configured viewport is a plain @c 1x1 (panning disabled,
 *         nothing meaningful to report)
 *
 * @note Complexity: @e O(1)
 */
bool scmd_surface_viewport_desktop_page(const surface_td *surface,
        const desktop_td *desktop,
        uint32_t *col_out, uint32_t *row_out);

/**
 * @brief Pan the current desktop's viewport, if needed, so a client
 *        not currently visible ends up centered on screen
 *
 * A no-op, leaving the viewport exactly where it already was, when
 * @p client is already on the page being shown: this only ever moves
 * the viewport to bring an otherwise-invisible client into view,
 * never nudges one already on screen just to perfect its centering.
 *
 * @param surface Surface to pan
 * @param client  Client to center the viewport on if not visible
 *
 * @note A no-op on a sticky client, which is on screen from every
 *       origin
 * @note A no-op on a client belonging to any desktop other than the
 *       one being shown, whose position is expressed against that
 *       desktop's own viewport origin and means nothing against this
 *       one's
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void scmd_surface_viewport_center_on_client(surface_td *surface,
        client_td *client);


#endif  /* ! CMDS_SCMD_H */
