/**
 * @file render/viewport/mesh.h
 *
 * @brief Dot mesh painted on the root window to make viewport panning
 *        visible
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef ICOWM_RENDER_VIEWPORT_MESH_H_
#define ICOWM_RENDER_VIEWPORT_MESH_H_


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <desktop.h>


/**
 * @brief Derive the mesh dot color from the desktop background color
 *
 * Applied per channel.  A background whose Rec. 601 luma reaches
 * @c 128 counts as light and its mesh is darkened,
 * @c C' @c = @c C @c * @c (100 @c - @c P) @c / @c 100 ; a darker one
 * gets a lightened mesh instead,
 * @c C' @c = @c C @c + @c (255 @c - @c C) @c * @c P @c / @c 100 .
 * Deriving rather than configuring a color keeps the contrast between
 * mesh and background about the same whichever theme or per-desktop
 * background is in use, which a fixed color taken from anywhere else
 * cannot do.
 *
 * The luma threshold is deliberately not a comparison of the packed
 * value against @c 0x808080 : a bright green such as @c 0x00FF00 is
 * numerically far below that while being a very light color, so such
 * a test would lighten an already light background and hide the mesh
 * completely.
 *
 * @param background Desktop background color the mesh is drawn over,
 *                   as @c 0x00RRGGBB
 * @param tone_shift How far to push each channel, as a percentage
 *
 * @return Mesh dot color, as @c 0x00RRGGBB
 *
 * @note Expects @p tone_shift already clamped by the configuration
 *       loader, and merely saturates rather than validating
 * @note Complexity: @e O(1)
 *
 * @note Implemented in @c render/viewport/mesh.c
 */
uint32_t viewport_mesh_color_from_background(uint32_t background,
        uint32_t tone_shift);

/**
 * @brief Where one axis' first dot sits inside the mesh tile for a
 *        given viewport origin
 *
 * The tile is @p spacing wide and the dot moves within it as the
 * viewport pans, which is what makes the mesh appear to travel with
 * the clients.  A negative @p origin is folded back into
 * @c [0, @p spacing) rather than left negative, since C's @c %
 * truncates toward zero and would otherwise place the dot outside the
 * tile.
 *
 * @param origin  Viewport origin along this axis, in pixels; may be
 *                negative
 * @param spacing Distance between neighboring dots along this axis,
 *                in pixels; never zero
 *
 * @return Dot offset within the tile, in @c [0, @p spacing)
 *
 * @note Takes a single axis so that either one can be exercised on its
 *       own; callers invoke it once per axis
 * @note Complexity: @e O(1)
 *
 * @note Implemented in @c render/viewport/mesh.c
 */
uint32_t viewport_mesh_tile_origin(int32_t origin, uint32_t spacing);

/**
 * @brief Whether a mesh should be painted for @p desktop at all
 *
 * Three conditions have to hold at once, and any of them can change
 * across a configuration reload, so this is asked on every background
 * render rather than resolved once.  The mesh has to be enabled in
 * @c viewport.mesh.is-enabled ; the surface's configured viewport has
 * to be able to pan, since a mesh that can never move is decoration
 * rather than an aid; and no external tool may own the root window's
 * pixels, because the mesh is applied as the root's background pixmap
 * and the two cannot both be there.
 *
 * @param desktop Desktop about to have its background rendered
 *
 * @retval  true if a mesh belongs on this desktop's root window
 * @retval false if the plain background color belongs there instead
 *
 * @note A null @p desktop, or one with no configuration, reports
 *       @c false
 * @note Complexity: @e O(1)
 *
 * @note Implemented in @c render/viewport/mesh.c
 */
bool viewport_mesh_is_visible(const desktop_td *desktop);

/**
 * @brief Build the mesh tile pixmap for @p desktop's current viewport
 *        origin
 *
 * The pixmap is exactly one tile, @c spacing.horizontal by
 * @c spacing.vertical pixels, holding the background color with a
 * single square dot at the offset
 * @a viewport_mesh_tile_origin reports.  The X server repeats it
 * across the whole root window on its own once it is installed as the
 * root's background pixmap, so no screen-sized drawable is ever
 * created and panning only ever costs one small tile.
 *
 * Any pixmap this screen already held is freed first, so callers never
 * have to pair this with @a viewport_mesh_cache_invalidate .
 *
 * @param connection Connection to draw the tile over
 * @param desktop    Desktop whose viewport origin and background the
 *                   tile is built from
 *
 * @return Tile pixmap, or @c XCB_NONE where one could not be built
 *
 * @note Complexity: @e O(1)
 *
 * @note Implemented in @c render/viewport/mesh.c
 */
xcb_pixmap_t viewport_mesh_tile_create(xcb_connection_t *connection,
        const desktop_td *desktop);

/**
 * @brief Paint @p desktop's mesh onto its root window
 *
 * Installs a freshly built tile as the root window's background pixmap
 * and clears the root so it shows.  Repeats that only when something
 * it depends on actually changed, since this runs on every full
 * desktop render and not just on an actual pan; the cached state is
 * kept per screen rather than per desktop, because every desktop on a
 * screen shares that screen's one root window.
 *
 * @param connection Connection to paint over
 * @param desktop    Desktop whose root window to paint
 *
 * @retval 0 on success, whether or not anything was actually repainted
 * @retval 1 where the mesh could not be painted
 *
 * @note Expects @a viewport_mesh_is_visible to have been consulted
 *       first, and does not check it again
 * @note Complexity: @e O(1)
 *
 * @note Implemented in @c render/viewport/mesh.c
 */
int viewport_mesh_render(xcb_connection_t *connection,
        const desktop_td *desktop);

/**
 * @brief Drop every screen's cached mesh tile and painted state
 *
 * Called when a configuration reload may have changed the mesh
 * settings, the desktop background colors, or whether a mesh belongs
 * on screen at all, so that the next render rebuilds from whatever
 * the reloaded configuration now says.  Also releases the tile
 * pixmaps themselves, so a mesh switched off in the reloaded
 * configuration stops costing anything.
 *
 * @note Complexity: @e O(n) on @c CONFIG_MAX_SCREENS
 *
 * @note Implemented in @c render/viewport/mesh.c
 */
void viewport_mesh_cache_invalidate(void);

/**
 * @brief Free every tile the root window has already been pointed
 *        away from
 *
 * @a viewport_mesh_cache_invalidate only sets a tile aside, because
 * the root's own @c XCB_CW_BACK_PIXMAP may still name it.  Whatever
 * points that attribute elsewhere, by installing a new tile or by
 * setting @c XCB_BACK_PIXMAP_NONE, calls this straight afterwards.
 *
 * @param connection Connection the pixmaps live on
 *
 * @note A no-op with nothing set aside, so it is safe to call after
 *       any background change rather than only after a mesh one
 * @note Complexity: @e O(n), where @e n is the number of screens
 */
void viewport_mesh_cache_release_retired(xcb_connection_t *connection);


#endif  /* ICOWM_RENDER_VIEWPORT_MESH_H_ */
