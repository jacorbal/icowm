/**
 * @file render/client/titlebar.h
 *
 * @brief Client titlebar rendering functions
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

#ifndef RENDER_CLIENT_TITLEBAR_H
#define RENDER_CLIENT_TITLEBAR_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>

/* Project includes */
#include <config.h>


/**
 * @brief Repaint a titlebar's background, text, and buttons
 *
 * The single place that does this: called from every titlebar repaint
 * path in the codebase (the render pass's "geometry changed" and "only
 * focus changed" branches, and the @c Expose-event handler), so none of
 * them can ever end up drawing the title or buttons differently from
 * one another.  Computes the button layout itself via
 * @a client_titlebar_layout and draws the title text through
 * @a s_titlebar_draw_title (alignment-aware & width-aware, so a title
 * too long for the space the buttons leave is truncated rather than
 * drawn underneath them) before calling
 * @a s_desktop_titlebar_buttons_draw.
 *
 * Background, title and buttons are all drawn into an off-screen pixmap
 * first and copied onto the titlebar with a single request only once
 * every one of them is on it, rather than drawn straight onto the
 * titlebar itself across several separate requests; without that, the
 * gap between the first of those requests and the last was wide enough
 * for the X server to show the titlebar with its background alone,
 * neither title nor buttons yet drawn, an intermittent flicker on any
 * window regardless of whether that particular window was the one whose
 * focus, urgency, or geometry actually changed.
 *
 * @param connection Active XCB connection
 * @param client     Client whose titlebar is to be repainted
 * @param is_focused Whether to paint with the active or inactive font
 *                   and colors.  Ordinarily reflects whether @p client
 *                   actually holds input focus, except for an urgent
 *                   client mid-blink (see @c policy/urgency.h), where
 *                   it is deliberately the opposite of the real focus
 *                   state for one half of the blink cycle: font and
 *                   colors swap together, so the titlebar reads as
 *                   clearly attention-grabbing rather than merely
 *                   looking like focus flickered
 * @param inner_w    Width available for the titlebar (the frame's
 *                   width minus its left/right decoration extents)
 * @param title_h    Titlebar height in pixels
 * @param theme      Theme providing colors, font, and titlebar layout
 *
 * @note A no-op if @p client has no titlebar window
 * @note Complexity: @e O(n), where @e n is the number of configured
 *       titlebar buttons plus the length of the client's title
 */
void render_client_titlebar_repaint_content(xcb_connection_t *connection,
        client_td *client, bool is_focused, uint16_t inner_w,
        uint16_t title_h, const struct config_theme_s *theme);


#endif  /* ! RENDER_CLIENT_TITLEBAR_H */
