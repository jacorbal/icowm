/**
 * @file input/mouse/internal.h
 *
 * @brief Private cross-file declarations shared across the mouse
 *        input subsystem
 *
 * Splitting the mouse input subsystem into per-topic files (resize
 * cursors, hover polling, titlebar interaction, and the press/
 * release/enter dispatch, itself further split into
 * @c input/mouse/event/press.c, @c event/release.c, and
 * @c event/enter.c) still leaves three functions each
 * topic's own file exposes for at least one of the others to call
 * directly, since the underlying resize-cursor and hover state is
 * genuinely shared, not duplicated per file the way, say,
 * @c menu/dialog/confirm.c and @c menu/dialog/fortune.c each own
 * their own separate state.
 *
 * @note This header is private to the mouse input subsystem and must
 *       not be included outside of @c src/input/mouse/, for it is
 *       NOT part of the public API in @c input/mouse.h
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_INTERNAL_H
#define INPUT_MOUSE_INTERNAL_H


/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>


/**
 * @brief Recompute and apply the resize-border cursor for a client
 *        window at a given pointer position
 *
 * Shared by @a mouse_handle_motion_hover and @a mouse_hover_poll_tick
 * (in @c hover.c) and @a mouse_handle_enter (in @c event/enter.c),
 * since any one kind of event or poll can be the only signal a given
 * transition actually produces
 *
 * @param connection XCB connection
 * @param surfaces   Every managed surface, to look up the client
 *                   @p window belongs to
 * @param window     Window the crossing, motion, or poll was
 *                   evaluated for
 * @param root_x     Pointer X position in root-window coordinates
 * @param root_y     Pointer Y position in root-window coordinates
 *
 * @return The resolved client @p window belongs to, or @c NULL if it
 *         does not belong to a resizable client
 *
 * @note Complexity: @e O(1)
 */
client_td *im_update_resize_cursor(xcb_connection_t *connection,
        list_td *surfaces, xcb_window_t window, int16_t root_x,
        int16_t root_y);

/**
 * @brief Start (or clear) hover-poll tracking of a window's resize
 *        cursor
 *
 * Called from @a mouse_handle_enter (in @c event/enter.c) whenever
 * the
 * pointer crosses into a window: an undecorated client has no separate
 * frame to fall back on, so moving from its border to its interior
 * happens entirely within one window, with no further @c EnterNotify
 * for that transition to catch; periodic polling is the only way to
 * still re-evaluate the cursor there.
 *
 * @param window Window to track, or @c XCB_WINDOW_NONE to stop
 *               tracking (the common case: most entered windows do
 *               not need this fallback at all)
 *
 * @note Complexity: @e O(1)
 *
 * @see @a mouse_hover_poll_tick in @c hover.c
 */
void im_hover_track(xcb_window_t window);


/* ==========================================================
 * Shared across every drag/ .c file
 *
 * src/input/mouse/drag.c was split by sub-concern into
 * drag/overlay.c, drag/snap.c, drag/outline.c, and drag/warp.c,
 * leaving drag.c itself with just the public API's own state
 * machine (drag_start*, drag_update, drag_end, drag_cancel, and the
 * query functions).  All five files still need the exact same
 * module-private state (s_drag) and call freely into each other's
 * own helpers, since none of this was ever independent, separable
 * state to begin with: a single in-progress drag's own bookkeeping,
 * merely organized here by which sub-concern each helper serves.
 * ========================================================== */

/* Time includes */
#include <time.h>

/* Project includes */
#include <desktop.h>
#include <policy/internal.h>


/**
 * @brief Module-private state for whichever drag is currently in
 *        progress, if any
 *
 * One instance (@c s_drag), defined in @c drag.c itself, shared by
 * every other file under @c drag/ via this @c extern declaration.
 */
typedef struct {
    bool active;
    enum window_operation_e operation;
    client_td *client;
    desktop_td *desktop;
    xcb_window_t drag_window;   /**< Icon window moved, or
                                 *   'XCB_WINDOW_NONE' for normal drag */
    int16_t pointer_start_x;
    int16_t pointer_start_y;
    int32_t client_start_x;
    int32_t client_start_y;
    uint16_t client_start_w;
    uint16_t client_start_h;
    uint32_t screen_w;          /**< Screen width for edge snap */
    uint32_t screen_h;          /**< Screen height for edge snap */
    uint32_t snap;              /**< Snap distance in pixels */
    int32_t client_cur_x;       /**< Current X during drag (updated each
                                     motion notify event) */
    int32_t client_cur_y;       /**< Current Y during drag */
    bool anchor_right;          /**< Resize: right edge is fixed (resize
                                     from left) */
    bool anchor_bottom;         /**< Resize: bottom edge is fixed (resize
                                     from top) */
    bool resize_w;              /**< Resize: width is actively being
                                     changed in this drag */
    bool resize_h;              /**< Resize: height is actively being
                                     changed in this drag */
    xcb_window_t overlay_window;/**< Centered feedback overlay window */
    bool overlay_is_icon;       /**< Overlay belongs to icon drag */
    char overlay_text[32];      /**< Current overlay text */
    bool icon_was_mapped;       /**< Original icon mapped state before
                                     drag */
    int16_t last_root_x;        /**< Root-relative pointer position
                                     'drag_update' last actually acted
                                     on, so a duplicate 'MotionNotify'
                                     reporting the same position (the X
                                     server can deliver one right after
                                     a grab starts under an
                                     already-resting pointer) is
                                     skipped rather than repeating the
                                     same 'xcb_configure_window' and
                                     'xcb_flush' for no visible change;
                                     meaningless until 'has_last_pos' */
    int16_t last_root_y;        /**< See 'last_root_x' */
    bool has_last_pos;          /**< Whether 'last_root_x'/'last_root_y'
                                     hold a real prior position yet;
                                     false right after 'drag_start' so
                                     its first 'drag_update' always
                                     runs regardless of position */
    bool warp_pending;          /**< Whether the pointer is currently
                                     held against a warp-eligible
                                     screen edge, counting down to a
                                     desktop switch (see 'desktops.warp_on_edge_drag'
                                     in config.json, config_desktop_s) */
    bool warp_is_left;          /**< Which edge, only meaningful when
                                     'warp_pending' */
    struct timespec warp_due;   /**< When the held edge becomes due to
                                     warp, only meaningful when
                                     'warp_pending' */
    xcb_window_t root;          /**< Root window, saved at 'drag_start'
                                     so 'drag_update'/'drag_end' can
                                     draw an outline onto it without
                                     needing it added to their own
                                     public signature */
    bool solid_drag;            /**< Snapshot of
                                     'config->windows.solid_drag' taken
                                     at 'drag_start', so a config
                                     reload mid-drag cannot switch
                                     behavior out from under an
                                     already-active one */
    bool outline_offscreened;   /**< Whether the real window has
                                     already been moved off-screen for
                                     the current outline drag;
                                     'drag_start' itself fires on
                                     every plain click, with no way
                                     yet to tell it apart from
                                     a genuine drag, so this only
                                     happens once the first real
                                     'drag_update' confirms actual
                                     movement, tracked here so it only
                                     ever happens once */
    xcb_window_t outline_windows[4]; /**< The outline stand-in used in
                                           place of moving the real
                                           window live, when
                                           '!solid_drag': 4 separate,
                                           opaque, override-redirect
                                           strip windows, one per side
                                           (top, bottom, left, right,
                                           in that fixed order),
                                           rather than a single filled
                                           rectangle, so the middle
                                           stays uncovered and whatever
                                           is genuinely underneath
                                           keeps showing through
                                           without needing a
                                           compositor at all.  Each
                                           entry is 'XCB_WINDOW_NONE'
                                           whenever no outline drag is
                                           in progress.  Real X windows
                                           the server itself manages
                                           the exposure/repaint of, so
                                           unlike the XOR rubber-band
                                           this replaced, nothing else
                                           redrawing underneath or
                                           around them (another window
                                           repainting itself, or an
                                           edge-warp desktop switch)
                                           can ever leave a stray
                                           artifact behind */
    int32_t client_cur_w;       /**< Current width during a resize
                                     drag (mirrors 'client_cur_x'/'_y'
                                     above); needed because an outline
                                     drag never touches the real client
                                     until 'drag_end', so
                                     'layout.geometry.cur' cannot be
                                     relied on to hold it meanwhile */
    int32_t client_cur_h;       /**< See 'client_cur_w' */
} drag_state_td;

extern drag_state_td s_drag;


/* drag/overlay.c */

void drag_sync_icon_active_visual(xcb_connection_t *connection);
uint16_t drag_u16_sat(uint32_t value);
uint16_t drag_icon_height(const client_td *client);
void drag_overlay_rect(int32_t target_x, int32_t target_y,
        uint16_t target_w, uint16_t target_h,
        uint16_t overlay_w, uint16_t overlay_h,
        int16_t *out_x, int16_t *out_y);
void drag_overlay_hide(xcb_connection_t *connection);
void drag_overlay_show(xcb_connection_t *connection,
        bool is_icon,
        int32_t target_x, int32_t target_y,
        uint16_t target_w, uint16_t target_h,
        const char *text);

/* drag/snap.c */

int32_t drag_abs_i32(int32_t value);
int32_t drag_closer_delta(int32_t current, int32_t candidate);
bool drag_ranges_close(int32_t start_a, int32_t end_a,
        int32_t start_b, int32_t end_b, int32_t snap);
void drag_snap_move(int32_t *restrict x, int32_t *restrict y,
        uint32_t width, uint32_t height);
void drag_snap_resize(int32_t *restrict x, int32_t *restrict y,
        uint32_t *restrict width, uint32_t *restrict height);

/* drag/outline.c */

void drag_outline_place(xcb_connection_t *connection,
        int32_t x, int32_t y, uint32_t w, uint32_t h, bool create);
void drag_outline_start(xcb_connection_t *connection,
        int32_t x, int32_t y, uint32_t w, uint32_t h);
void drag_outline_move(xcb_connection_t *connection,
        int32_t x, int32_t y, uint32_t w, uint32_t h);
void drag_outline_end(xcb_connection_t *connection);
void drag_move_client_offscreen(xcb_connection_t *connection,
        client_td *client);

/* drag/warp.c */

void drag_check_warp_edge(int16_t root_x);


#endif  /* ! INPUT_MOUSE_INTERNAL_H */
