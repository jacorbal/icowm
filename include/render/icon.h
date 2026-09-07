/**
 * @file render/icon.h
 *
 * @brief Rendering an iconified client's icon window, and the
 *        state-hint indicators drawn on it
 *
 * @a ri_icon_hints_draw is called from both @c render/icon.c (the
 * normal desktop repaint path) and @c handler/expose.c (redrawing
 * a single icon window after it is exposed), so the two stay visually
 * consistent without duplicating the hint-drawing logic itself between
 * them.
 *
 * @a ri_render_client_icon is called from @c render/desktop.c's own
 * normal repaint path and from @c menu/cycle.c (repainting an icon's
 * real desktop window the moment its cycle-selection state changes,
 * rather than leaving it visually stuck until some unrelated full
 * desktop repaint happens to run).  Both are public rather than
 * declared in @c render/internal.h because each has at least one caller
 * outside the render subsystem that header is restricted to.
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

/* Type includes */
#include <types/handles.h>

/* Project includes */
#include <config.h>


/**
 * @brief Render the icon window for a hidden (iconified) client
 *
 * Applies icon window attributes (background, border color and width,
 * stacking) and optionally draws a caption label.  Called from
 * @a desktop_render_clients for clients with @c CLIENT_FLAG_HIDDEN set,
 * and from @c menu/cycle.c whenever the cycle menu's selection moves on
 * to or off of @p client, so its real desktop icon (border color, and
 * the hint indicators @a ri_icon_hints_draw below draws) reflects that
 * immediately rather than staying stuck at whichever it was the last
 * time an unrelated full desktop repaint happened to run.
 *
 * The icon draws in its selected styling, meaning active colors, the
 * active font and no pixmap, whenever the cycle menu has picked this
 * client or a drag has hold of it.  Both are asked here rather than
 * painted by whoever started them, so that every repaint of a picked-up
 * icon agrees whatever triggered it; painting it from outside did not
 * hold, an ordinary render pass arriving afterwards having drawn it
 * plain again.
 *
 * The theme and the connection come from @p client itself, which is why
 * no desktop is passed: the desktop was only ever a route to those two,
 * and needing one shut this function out of callers that hold a client
 * and nothing else.
 *
 * @param client     The iconified client to render
 * @param is_current @c true when the client's desktop is the one
 *                   currently visible
 * @param force      Render even when nothing about the icon changed
 *                   since its last one, for a caller that needs the
 *                   window repainted now rather than on the next pass
 *
 * @note No-op when @p client has no icon window or is not icon-mapped
 * @note Implemented in @c render/icon.c
 * @note Complexity: @e O(1)
 */
void ri_render_client_icon(client_td *client, bool is_current,
        bool force);

/**
 * @brief Draw the state-hint indicators in an iconified client's top
 *        corners
 *
 * A filled square in the top-left corner when @p client is
 * pinned (@c CLIENT_FLAG_PIN), drawn in the same foreground the
 * state letter opposite it uses (@c icon.active.color.foreground or
 * @c icon.inactive.color.foreground, whichever the icon is currently
 * wearing): it is a state hint like the letters, only shaped rather
 * than lettered, and reads as one of them rather than as a piece of
 * titlebar borrowed onto the icon.  Sized from @c WM_ICON_SQUARE_SIZE
 * and @c WM_ICON_PIXMAP_SCALE_PERCENT rather than reusing the
 * titlebar's @c WM_DECOR_BTN_SIZE_MIN, too large here relative to a 48px
 * icon.
 *
 * A single letter in the top-right corner for whichever maximize or
 * fullscreen state @p client was in right before it was last iconified:
 *
 * - @c CLIENT_STATE_NORMAL draws nil;
 * - @c CLIENT_STATE_FULLSCREEN draws 'f';
 * - @c CLIENT_STATE_MAXIMIZED draws 'm';
 * - @c CLIENT_STATE_MAXIMIZED_HORZ draws 'h'; and
 * - @c CLIENT_STATE_MAXIMIZED_VERT draws 'v',
 *
 * all in the same foreground/background colors already used for the
 * icon's caption text below it, so both pieces of text read as one
 * consistent style.
 *
 * @param connection   Active XCB connection
 * @param client       The iconified client these hints belong to
 * @param target       Drawable the hints are actually drawn onto:
 *                     @p client's real icon window, or an off-screen
 *                     buffer standing in for it while the caller
 *                     assembles a full repaint before copying it over
 * @param is_cycle_sel Whether @p client is the currently
 *                     cycle-menu-selected icon (selects active vs.
 *                     inactive icon colors, matching whichever the
 *                     caller already drew the rest of the icon in)
 * @param theme        Active theme
 *
 * @see @c client_properties_s.state in @c client.h, for the state
 *      bits each letter comes from
 * @see This function's implementation comment in @c render/icon.c,
 *      for the derivation of the pin square's size
 *
 * @note No-op when @p client has no icon window, @p target is
 *       @c XCB_NONE, @p theme is @c NULL, or @p theme->icon.show_hints
 *       is @c false
 * @note Complexity: @e O(1)
 */
void ri_icon_hints_draw(xcb_connection_t *connection, client_td *client,
        xcb_drawable_t target, bool is_cycle_sel,
        const struct config_theme_s *theme);


#endif  /* ! RENDER_ICON_H */
