/**
 * @file input/mouse/drag/internal.h
 *
 * @brief The one piece of state every drag module shares
 *
 * @c input/mouse/drag.c was split by sub-concern into @c drag/overlay.c
 * (the centered feedback window), @c drag/snap.c (edge/peer snap math),
 * @c drag/outline.c (the non-solid-drag outline stand-in), and
 * @c drag/warp.c (the edge-drag desktop warp), leaving @c drag.c itself
 * with only the public state-machine API (@c drag_start and its
 * siblings, @c drag_update, @c drag_end, @c drag_cancel, and the small
 * query functions, declared in the public @c input/mouse/drag.h instead
 * of here).
 *
 * @c s_drag, the module's singleton drag state, is the one thing
 * genuinely shared across all of them this way rather than through
 * a function call: every file reads or writes some part of it directly.
 * Storage for it lives in @c drag.c; every other file only ever sees
 * the @c extern declaration below.  Each drag/ file's functions that
 * its siblings call directly are declared in that file's header instead
 * (@c drag/overlay.h, @c drag/snap.h, @c drag/outline.h, and
 * @c drag/warp.h), not duplicated here.
 *
 * @note This header is private to @c input/mouse/drag/ (and
 *       @c input/mouse/drag.c, which orchestrates every drag/ file) and
 *       must not be included outside of them
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_DRAG_INTERNAL_H
#define INPUT_MOUSE_DRAG_INTERNAL_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/direction.h>
#include <types/pair.h>

/* Project includes */
#include <client.h>
#include <types/handles.h>


/**
 * @brief Mouse drag state
 */
typedef struct {
    bool is_active;
    enum window_operation_e operation;
    client_td *client;
    desktop_td *desktop;
    xcb_window_t drag_window;       /**< Icon window being moved,
                                         @c XCB_WINDOW_NONE for a normal
                                         drag */
    int16_t pointer_start_x;
    int16_t pointer_start_y;
    struct geometry_s client_start;
    uint32_t screen_w;              /**< Screen width for edge snap */
    uint32_t screen_h;              /**< Screen height for edge snap */
    uint32_t snap_window;           /**< Snap distance in pixels toward
                                         another window's edge */
    uint32_t snap_screen;           /**< Snap distance in pixels toward
                                         the screen's edge */
    struct geometry_s client_cur;   /**< Current geometry during drag
                                         (updated each motion notify
                                         event; only @c pos is
                                         meaningful during an icon drag,
                                         which never resizes) */
    /** While resizing, the right edge is fixed and the drag pulls
     *  from the left */
    bool is_anchor_right;
    /** While resizing, the bottom edge is fixed and the drag pulls
     *  from the top */
    bool is_anchor_bottom;
    bool is_resize_w;               /**< Resize: width is actively being
                                         changed in this drag */
    bool is_resize_h;               /**< Resize: height is actively being
                                         changed in this drag */
    bool is_resist_axis_w;          /**< Width started this drag
                                         maximize-locked (see
                                         'drag_start_resize_axis_locked'),
                                         making 'is_resize_w' above no
                                         longer fixed for the whole drag
                                         the way it is for every other
                                         client: 'drag_update'
                                         recomputes it every call
                                         instead, false below the
                                         configured resistance
                                         threshold, true past it,
                                         reversibly for the whole drag,
                                         matching Openbox's identical
                                         behavior ('moveresize.c') */
    bool is_resist_axis_h;          /**< Height's analogous case */
    bool is_move_x_locked;          /**< Move: X position pinned to its
                                         starting value for the whole
                                         drag (a horizontally-maximized
                                         client's width already fills
                                         its workarea, leaving no valid
                                         X but the one it started at) */
    bool is_move_y_locked;          /**< Move: Y position pinned to its
                                         starting value for the whole
                                         drag (a vertically-maximized
                                         client's analogous case) */
    xcb_window_t overlay_window;    /**< Centered feedback overlay window */
    bool is_overlay_icon;           /**< Overlay belongs to icon drag */
    char overlay_text[32];          /**< Current overlay text */
    bool was_icon_mapped;           /**< Original icon mapped state
                                         before drag */
    int16_t last_root_x;            /**< Root-relative pointer position
                                         'drag_update' last actually
                                         acted on, so a duplicate
                                         'MotionNotify' reporting the
                                         same position (the X server can
                                         deliver one right after a grab
                                         starts under an already-resting
                                         pointer) is skipped rather than
                                         repeating the same
                                         'xcb_configure_window' and
                                         'xcb_flush' for no visible
                                         change; meaningless until
                                         'has_last_pos' */
    int16_t last_root_y;            /**< See 'last_root_x' */
    bool has_last_pos;              /**< Whether 'last_root_x'/'last_root_y'
                                     hold a real prior position yet;
                                     false right after 'drag_start' so
                                     its first 'drag_update' always runs
                                     regardless of position */

    /**
     * @brief Whether the pointer is held against a warp-eligible screen
     *        edge, counting down to a desktop switch
     *
     *  @note Governed by @c desktops.warp_on_edge_drag in
     *        @c config.json
     */
    bool is_warp_pending;
    enum compass_direction_e warp_direction;/**< Which edge,
                                         meaningful only while
                                         @a is_warp_pending */
    struct timespec warp_due;       /**< When the held edge becomes due
                                         to warp, only meaningful when
                                         @a is_warp_pending */
    xcb_window_t root;              /**< Root window, saved at
                                         @a drag_start so @a drag_update /
                                         @a drag_end can draw an outline
                                         onto it without needing it
                                         added to their public signature */
    bool is_solid_drag;             /**< Snapshot of
                                         @c config->windows.solid_drag
                                         taken at @a drag_start , so a
                                         config reload mid-drag cannot
                                         switch behavior out from under
                                         an already-active one */
    bool is_outline_offscreened;    /**< Whether the real window has
                                         already been moved off-screen
                                         for the current outline drag;
                                         'drag_start' itself fires on
                                         every plain click, with no way
                                         yet to tell it apart from
                                         a genuine drag, so this only
                                         happens once the first real
                                         @a drag_update confirms actual
                                         movement, tracked here so it
                                        only ever happens once */
    xcb_window_t outline_windows[4];/**< The outline stand-in used in
                                         place of moving the real window
                                         live, when '!is_solid_drag':
                                         4 separate, opaque,
                                         override-redirect strip
                                         windows, one per side (top,
                                         bottom, left, right, in that
                                         fixed order), rather than
                                         a single filled rectangle, so
                                         the middle stays uncovered and
                                         whatever is genuinely
                                         underneath keeps showing
                                         through without needing
                                         a compositor at all.  Each
                                         entry is 'XCB_WINDOW_NONE'
                                         whenever no outline drag is in
                                         progress.  Real X windows the
                                         server itself manages the
                                         exposure/repaint of, so unlike
                                         the XOR rubber-band this
                                         replaced, nothing else
                                         redrawing underneath or around
                                         them (another window repainting
                                         itself, or an edge-warp desktop
                                         switch) can ever leave a stray
                                         artifact behind */
} drag_state_td;


/** Singleton drag state; storage lives in drag.c */
extern drag_state_td s_drag;


#endif  /* ! INPUT_MOUSE_DRAG_INTERNAL_H */
