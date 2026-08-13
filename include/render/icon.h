/**
 * @file render/icon.h
 *
 * @brief Rendering an iconified client's own icon window, and the
 *        state-hint indicators drawn on it
 *
 * @a ri_draw_icon_hints is called from both @c render/icon.c (the
 * normal desktop repaint path) and @c handler/expose.c (redrawing a
 * single icon window after it is exposed), so the two stay visually
 * consistent without duplicating the hint-drawing logic itself
 * between them.  @a ri_render_client_icon is called from @c render/
 * desktop.c's own normal repaint path and from @c menu/cycle.c
 * (repainting an icon's real desktop window the moment its cycle-
 * selection state changes, rather than leaving it visually stuck
 * until some unrelated full desktop repaint happens to run).  Both
 * are public rather than declared in @c render/internal.h because
 * each has at least one caller outside the render subsystem that
 * header is restricted to.
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

#ifndef RENDER_ICON_H
#define RENDER_ICON_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>


/**
 * @brief Render the icon window for a hidden (iconified) client
 *
 * Applies icon window attributes (background, border color and width,
 * stacking) and optionally draws a caption label.  Called from
 * @a desktop_render_clients for clients with @c CLIENT_FLAG_HIDDEN
 * set, and from @c menu/cycle.c whenever the cycle menu's own
 * selection moves on to or off of @p client, so its real desktop icon
 * (border color, and the hint indicators @a ri_draw_icon_hints below
 * draws) reflects that immediately rather than staying stuck at
 * whichever it was the last time an unrelated full desktop repaint
 * happened to run.
 *
 * @param desktop    Desktop whose rendering context and theme are used
 * @param client     The iconified client to render
 * @param is_current @c true when @p desktop is the currently visible one
 *
 * @note No-op when @p client has no icon window or is not icon-mapped
 * @note Implemented in @c render/icon.c
 * @note Complexity: @e O(1)
 */
void ri_render_client_icon(desktop_td *desktop, client_td *client,
        bool is_current);

/**
 * @brief Render an iconified client's icon window in its "currently
 *        selected" state.  Active colors, its own caption, and its
 *        hint indicators stay, but not the pixmap
 *
 * Clears the icon window, redraws only its caption (when @c
 * theme.icon.is-captioned is set) and its hint indicators (@a
 * ri_draw_icon_hints below), all in the client's own active colors.
 * It deliberately omits only the pixmap a full render (@a ri_render_
 * client_icon above) would otherwise draw.  Originally mirrored the
 * exact same simplified look an icon gets the moment it starts being
 * dragged, a separate, deliberately independent implementation of the
 * same idea rather than a shared call, so a change meant for one
 * never risks the other, which hides its caption too and stays that
 * way for the rest of the drag.  This one differs on purpose, keeping
 * caption and hints visible, since a busy icon being cycled through
 * only needs its pixmap out of the way to read clearly, not its name
 * or state hints as well.
 *
 * @param connection XCB connection
 * @param client     The iconified client to render
 *
 * @see @a s_drag_sync_icon_active_visual in input/mouse/drag.c
 *
 * @note No-op when @p client has no icon window, is not icon-mapped,
 *       or its own @c theme is unset
 * @note Complexity: @e O(1)
 */
void ri_render_client_icon_selected(xcb_connection_t *connection,
        client_td *client);

/**
 * @brief Draw the state-hint indicators in an iconified client's own
 *        top corners
 *
 * A filled square in the top-left corner when @p client is
 * sticky/pinned (@c CLIENT_FLAG_PIN), colored the same as the
 * titlebar's own pin button (@c window.titlebar.buttons.color.on)
 * and sized from @c WM_ICON_SQUARE_SIZE and
 * @c WM_ICON_PIXMAP_SCALE_PERCENT rather than reusing the titlebar's
 * own @c WM_DECOR_BTN_SIZE, too large here relative to a 48px icon.
 * A single letter in the top-right corner for whichever
 * maximize/fullscreen state @p client was in right before it was
 * last iconified.  @c CLIENT_STATE_NORMAL draws none,
 * @c CLIENT_STATE_FULLSCREEN draws @c 'f', @c CLIENT_STATE_MAXIMIZED
 * draws @c 'm', @c CLIENT_STATE_MAXIMIZED_HORZ draws @c 'h', and
 * @c CLIENT_STATE_MAXIMIZED_VERT draws @c 'v', all in the same
 * foreground/background colors already used for the icon's own
 * caption text below it, so both pieces of text read as one
 * consistent style.
 *
 * @param connection   Active XCB connection
 * @param client       The iconified client whose icon window to draw on
 * @param is_cycle_sel Whether @p client is the currently
 *                     cycle-menu-selected icon (selects active vs.
 *                     inactive icon colors, matching whichever the
 *                     caller already drew the rest of the icon in)
 * @param theme        Active theme
 *
 * @see @c client_properties_s.pre_iconify_state in client.h, for the
 *      maximize/fullscreen state each letter comes from
 * @see This function's own implementation comment in render/icon.c,
 *      for the derivation of the pin square's size
 *
 * @note No-op when @p client has no icon window, @p theme is @c NULL,
 *       or @c theme->icon.show_hints is @c false
 * @note Complexity: @e O(1)
 */
void ri_draw_icon_hints(xcb_connection_t *connection, client_td *client,
        bool is_cycle_sel, const struct config_theme_s *theme);


#endif  /* ! RENDER_ICON_H */
