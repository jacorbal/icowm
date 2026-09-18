/**
 * @file client/layout.h
 *
 * @brief Client geometry and decoration layout
 *
 * Where a client's frame is, how large it is, what its size hints
 * allow, and how much of the screen edge it reserves.
 *
 * @ingroup client
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CLIENT_LAYOUT_H
#define CLIENT_LAYOUT_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* Default initial values */
#include <defs/client.h>
#include <defs/config.h>   /* CONFIG_MAX_LENGTH_NAME, ..._FONTNAME */

/**
 * @brief Window layout, position, dimensions and strut
 */
struct client_layout_s {
    /**
     * @brief Position and dimensions of the client
     *
     * @note @p old holds what @p cur is to be restored to later, as
     *       when the geometry is put aside before maximizing and
     *       given back on unmaximizing
     */
    struct {
        struct geometry_s cur;
        struct geometry_s old;
    } geometry;

    /**
     * @brief Position most recently asked of the X server for this
     *        client's own target window
     *
     * A @c ConfigureNotify is believed only when it confirms this.
     * The server echoes one back for every configure the manager
     * issues, and during a burst, a viewport pan drag being the
     * clearest case, an echo for an earlier request routinely arrives
     * after a later one has already been sent.  Writing that stale
     * position into @p geometry.cur would leave the manager's own
     * idea of where the client sits one step behind, and since a pan
     * adds its delta to whatever is stored, the error is kept for
     * good rather than corrected by the next step.
     *
     * @p has_requested_pos starts false, so a client the manager has
     * not placed yet still takes the position the server reports.
     */
    struct position_s requested_pos;
    bool has_requested_pos;

    /**
     * @brief Inner-window geometry the render pass last placed
     *
     * The pass ends by clearing the content window with exposures
     * turned on, so a toolkit that redraws on @c Expose alone follows
     * the frame when the content is moved or resized under it.  The
     * pass itself runs for any reason at all, a changed title among
     * them, and the titlebar is a window of its own: redrawing it
     * never needs the content blanked.  Kept here so the clear can be
     * left to the case it was written for.
     *
     * @note @p has_placed_inner starts false, so a client's first
     *       pass always clears
     */
    struct geometry_s placed_inner;
    bool has_placed_inner;

    /**
     * @brief Frame background color the decoration repaint last set
     *
     * The repaint sets the frame's background and clears it so the
     * new color shows.  The frame is the content window's parent, so
     * that clear paints over the area the content occupies until the
     * client draws itself again, which is seen on any application
     * that is not immediate about it.  The color only ever changes
     * on a focus change, while the repaint runs for any reason at
     * all, a changed title among them, so the two are kept apart
     * here.
     *
     * @note @p has_frame_bg starts false, so a client's first repaint
     *       always clears
     */
    uint32_t frame_bg;
    bool has_frame_bg;

    /**
     * @brief Everything @a render_client_titlebar_repaint_content
     *        actually draws from, snapshotted right after its last
     *        real repaint
     *
     * Compared fresh against the client's current state at the top
     * of that function, on every single field, deciding whether a
     * pass with nothing left to actually show differently can skip
     * repainting the same content all over again.  A change in any
     * one of these, name included, still forces the full repaint;
     * @c bg_color and @c fg_color are the theme's resolved colors,
     * not just the focus state that picked between the two, so a
     * live theme reload that only changes a color or a font, with
     * every dimension and every button's own state unchanged, is
     * still caught.
     *
     * @note @p has_titlebar_paint starts false, so a client's first
     *       repaint always paints
     */
    struct {
        uint32_t bg_color;
        uint32_t fg_color;
        char name[CONFIG_MAX_LENGTH_NAME];
        char font[CONFIG_MAX_LENGTH_FONTNAME];
        uint16_t inner_w;
        uint16_t title_h;
        bool can_maximize;
        bool is_pinned;
        bool is_sticky;
        bool is_marked_layer;
        bool hide_pin;
        bool hide_sticky;
        bool has_titlebar_paint;
    } titlebar_paint;

    /**
     * @brief Area where the client exist on the screen, plus the area
     *        are marked off-bounds for client placement
     *
     * @note Traditional strut property will be @p strut_partial.sides
     *        when @p .start and @p .end are zero
     */
    struct strut_partial_s strut_partial;

    /**
     * @brief Window gravity
     *
     * Set once, at @a client_init (@c client.c), from this client's
     * @c WM_NORMAL_HINTS if it already declares @c win_gravity there,
     * or from @c windows.gravity in @c config.json otherwise (see
     * @c config.md §2.4).  That config field is only ever a fallback
     * for a client that never states its gravity, at any point in its
     * life, not a way to override one that does.
     *
     * A later @c WM_NORMAL_HINTS update, in
     * @a client_props_refresh_normal_hints (@c client/props.c), keeps
     * this field in sync with whatever @c win_gravity that update
     * itself carries, per ICCCM's "MUST honor" mandate; several common
     * toolkits (XTerm's Xt shell, LibreOffice's VCL) only send their
     * real hints a moment after their first map, once fonts and chrome
     * are ready, which is when most real clients' gravity actually
     * takes hold over the config default. */
    uint16_t gravity;
    struct sides_s frame_extents;   /**< [left, top, right, bottom] */
};


#endif  /* ! CLIENT_LAYOUT_H */
