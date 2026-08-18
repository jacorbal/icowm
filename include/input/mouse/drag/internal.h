/**
 * @file input/mouse/drag/internal.h
 *
 * @brief Private declarations shared across the drag/ modules
 *
 * @c input/mouse/drag.c was split by sub-concern into
 * @c drag/overlay.c (the centered feedback window), @c drag/snap.c
 * (edge/peer snap math), @c drag/outline.c (the non-solid-drag
 * outline stand-in), and @c drag/warp.c (the edge-drag desktop
 * warp), leaving @c drag.c itself with only the public state-machine
 * API (@c drag_start and its own siblings, @c drag_update,
 * @c drag_end, @c drag_cancel, and the small query functions).
 *
 * Every function declared here is called from at least one of those
 * files other than the one that defines it; a helper only ever
 * called from within its own file (@c drag_abs_i32,
 * @c drag_closer_delta, and so on) stays @c static there instead and
 * has no business appearing in this header at all.
 *
 * @c s_drag itself, the module's own singleton drag state, is shared
 * the exact same way: every one of these files reads or writes some
 * part of it.  Storage for it lives in @c drag.c; every other file
 * only ever sees the @c extern declaration below.
 *
 * @note This header is private to @c input/mouse/drag/ and must not
 *       be included outside of it.
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

/* Project includes */
#include <client.h>
#include <desktop.h>


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


/** Singleton drag state; storage lives in drag.c */
extern drag_state_td s_drag;


/* drag/overlay.c */

/**
 * @brief Synchronize the active visual of the drag icon window
 *
 * @param connection XCB connection
 */
void drag_sync_icon_active_visual(xcb_connection_t *connection);

/**
 * @brief Saturating cast from @c uint32_t down to @c uint16_t
 *
 * @param value Value to clamp
 *
 * @return @p value, clamped to @c UINT16_MAX
 */
uint16_t drag_u16_sat(uint32_t value);

/**
 * @brief Full icon window height for @p client, caption band
 *        included if the theme captions icons
 *
 * @param client Client whose icon height to compute
 *
 * @return Icon height, in pixels
 */
uint16_t drag_icon_height(const client_td *client);

/**
 * @brief Hide the centered feedback overlay window, if currently
 *        shown
 *
 * @param connection XCB connection
 */
void drag_overlay_hide(xcb_connection_t *connection);

/**
 * @brief Show (creating it if needed) the centered feedback overlay
 *        window with the given geometry and text
 *
 * @param connection  XCB connection
 * @param is_icon     Whether this overlay belongs to an icon drag
 * @param target_x    Overlay target rectangle's own left edge
 * @param target_y    Overlay target rectangle's own top edge
 * @param target_w    Overlay target rectangle's own width
 * @param target_h    Overlay target rectangle's own height
 * @param text        Overlay text to display
 */
void drag_overlay_show(xcb_connection_t *connection, bool is_icon,
        int32_t target_x, int32_t target_y,
        uint16_t target_w, uint16_t target_h, const char *text);


/* drag/snap.c */

/**
 * @brief Snap a moving client's own candidate position against peer
 *        windows and screen edges
 *
 * @param x      Candidate X position, updated in place if snapped
 * @param y      Candidate Y position, updated in place if snapped
 * @param width  Client width
 * @param height Client height
 */
void drag_snap_move(int32_t *restrict x, int32_t *restrict y,
        uint32_t width, uint32_t height);

/**
 * @brief Snap a resized client's own candidate geometry against peer
 *        windows and screen edges
 *
 * @param x      Candidate X position, updated in place if snapped
 * @param y      Candidate Y position, updated in place if snapped
 * @param width  Candidate width, updated in place if snapped
 * @param height Candidate height, updated in place if snapped
 */
void drag_snap_resize(int32_t *restrict x, int32_t *restrict y,
        uint32_t *restrict width, uint32_t *restrict height);


/* drag/outline.c */

/**
 * @brief Begin an outline drag: create and place the 4 strip windows
 *
 * @param connection XCB connection
 * @param x          Initial outline X position
 * @param y          Initial outline Y position
 * @param w          Initial outline width
 * @param h          Initial outline height
 */
void drag_outline_start(xcb_connection_t *connection,
        int32_t x, int32_t y, uint32_t w, uint32_t h);

/**
 * @brief Move the outline drag's own 4 strip windows to a new
 *        geometry
 *
 * @param connection XCB connection
 * @param x          New outline X position
 * @param y          New outline Y position
 * @param w          New outline width
 * @param h          New outline height
 */
void drag_outline_move(xcb_connection_t *connection,
        int32_t x, int32_t y, uint32_t w, uint32_t h);

/**
 * @brief End an outline drag: destroy its own 4 strip windows
 *
 * @param connection XCB connection
 */
void drag_outline_end(xcb_connection_t *connection);

/**
 * @brief Move the real client window off-screen for the duration of
 *        an outline drag
 *
 * @param connection XCB connection
 * @param client     Client being outline-dragged
 */
void drag_move_client_offscreen(xcb_connection_t *connection,
        client_td *client);


/* drag/warp.c */

/**
 * @brief Check whether the pointer is currently held against a
 *        warp-eligible screen edge, arming or continuing the warp
 *        countdown accordingly
 *
 * @param root_x Root-relative pointer X position
 */
void drag_check_warp_edge(int16_t root_x);


#endif /* ! INPUT_MOUSE_DRAG_INTERNAL_H */
