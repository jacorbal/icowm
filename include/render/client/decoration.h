/**
 * @file render/client/decoration.h
 *
 * @brief Client frame decoration (border) rendering functions
 *
 * @ingroup render
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RENDER_CLIENT_DECORATION_H
#define RENDER_CLIENT_DECORATION_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>

/* Project includes */
#include <config.h>


/**
 * @brief Repaint the frame window decoration for a client
 *
 * Refreshes the frame background/border and redraws the corner resize
 * grips when they are supposed to be visible for the current client
 * state.
 *
 * The frame is only cleared when the color just set is not the one
 * already showing, which is on a focus change.  Clearing it paints
 * over the content window's own area, that window being its child,
 * until the client draws itself again, and this repaint runs for any
 * reason at all.
 *
 * @param connection       Active XCB connection
 * @param client           Client whose frame decoration will be
 *                         repainted
 * @param use_active_style Whether to use the active theme colors
 * @param theme            Theme providing frame and grip colors
 *
 * @note Complexity: @e O(1)
 */
void render_client_decoration_repaint_frame(xcb_connection_t *connection,
        client_td *client, bool use_active_style,
        const struct config_theme_s *theme);

/**
 * @brief Repaint a client's frame decoration, unless it is currently
 *        forced hidden
 *
 * Shared by @c desktop_render_one_client's full-repaint and
 * focus-only-repaint branches, which otherwise each repeat the exact
 * same @c hide_decoration guard around the same call (see that
 * function's @c hide_decoration for what forces this.  Currently only
 * a fullscreen client that was decorated before going fullscreen).
 *
 * @param connection      XCB connection
 * @param client          Client whose frame decoration to repaint
 * @param is_focused      Whether to use the active or inactive color
 *                        set
 * @param hide_decoration Whether decoration is currently suppressed
 *                        entirely; a no-op when @c true
 * @param theme           Active theme
 *
 * @note Complexity: @e O(1)
 */
void render_client_decoration_repaint_frame_unless_hidden(
        xcb_connection_t *connection, client_td *client,
        bool is_focused, bool hide_decoration,
        const struct config_theme_s *theme);


#endif  /* ! RENDER_CLIENT_DECORATION_H */
