/**
 * @file policy/placement/manual.h
 *
 * @brief Manual placement: the position the person picks themselves
 *
 * The oldest placement policy there is, and the one every other exists
 * to avoid: rather than the window manager choosing a spot, the person
 * is shown an outline and puts the window where they want it.
 *
 * Nothing here ever waits in place.  Asking holds the pointer and the
 * keyboard, draws an outline where the window would go, and returns,
 * so every event still arrives through the main loop as usual and
 * every other client keeps running while the question is open.  The
 * caller that asked hands over what finishes the map, to be called
 * once the position is settled, and gets told that it must not finish
 * the map itself.
 *
 * A window whose turn has not come yet is held unmapped, with the wait
 * for it starting only when it becomes the one being asked about.
 *
 * @defgroup placementmanual Manual placement
 * @ingroup policy
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef POLICY_PLACEMENT_MANUAL_H
#define POLICY_PLACEMENT_MANUAL_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>


/**
 * @brief What a caller hands over to finish taking a window under
 *        management, once its position is settled
 *
 * Called exactly once for a window that reaches a position, whether it
 * was pointed at, given up on with @c Escape, or decided by the wait
 * elapsing, and never at all for one that disappears before then.
 */
typedef void (*place_manual_done_fn)(const wm_td *wm,
        surface_td *surface, desktop_td *desktop, client_td *client);


/* Public interface */
/**
 * @brief Pick where a window goes, asking the person to point at it
 *
 * Marks @p client as the one window @a place_manual_enqueue may accept
 * next, and answers with where it sits meanwhile: whatever the smart
 * policy would have chosen, which is both where the outline starts and
 * where the window stays if the person gives up or never answers.
 *
 * @param wm      Window manager instance
 * @param surface Surface the window will appear on
 * @param client  Client being placed
 * @param out_x   Where the chosen X coordinate is written
 * @param out_y   Where the chosen Y coordinate is written
 *
 * @return @c true when a position was chosen
 *
 * @note Answers @c false when the smart search finds nothing free,
 *       exactly as @a place_window_smart itself does, leaving the
 *       caller on its cascade fallback
 * @note The mark is left set either way, so a window that fell back to
 *       the cascade is still asked about
 * @note Complexity: @e O(g * n), where @e g is the number of grid
 *       positions tested and @e n is the number of clients on the
 *       desktop
 */
bool place_window_manual(const wm_td *wm, surface_td *surface,
        client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y);

/**
 * @brief Take a window that must be placed by hand, and hold it
 *
 * Accepts @p client only when @a place_window_manual marked it during
 * the placement pass that just ran, so a window the policy never
 * reached (one that asked for a position itself, a transient centered
 * over its parent, a dock) is never asked about.  The window is left
 * unmapped; @p done is what maps it, later.
 *
 * @param connection XCB connection
 * @param wm         Window manager instance
 * @param surface    Surface the window will appear on
 * @param desktop    Desktop it belongs to
 * @param client     Client being placed
 * @param cursor     Shape the pointer takes while the question is
 *                   open, or @c XCB_NONE to leave it as it is
 * @param done       What finishes the map once the position settles
 *
 * @return @c true when the window was taken, meaning the caller must
 *         not finish the map itself
 *
 * @note Answers @c false for a window the policy did not mark, and for
 *       one arriving with the queue already full
 * @note The cursor is asked for rather than chosen here so that this
 *       whole policy stays clear of the mouse subsystem, which is the
 *       caller's concern and not a placement decision
 * @note Complexity: @e O(1)
 */
bool place_manual_enqueue(xcb_connection_t *connection, const wm_td *wm,
        surface_td *surface, desktop_td *desktop, client_td *client,
        xcb_cursor_t cursor, place_manual_done_fn done);

/**
 * @brief Whether a window is currently being pointed at
 *
 * True from the moment the outline appears until that one window is
 * settled, which is exactly while the pointer and the keyboard are
 * held; whoever else would act on either has to stand aside meanwhile.
 *
 * @return @c true while a window is being placed by hand
 *
 * @note Complexity: @e O(1)
 */
bool place_manual_is_active(void);

/**
 * @brief Follow the pointer with the outline of the window being
 *        placed
 *
 * @param connection XCB connection
 * @param root_pos   Current root-relative position of the pointer
 *
 * @note A no-op when no window is currently being pointed at, and when
 *       the pointer has not actually moved since the last call
 * @note Complexity: @e O(1)
 */
void place_manual_handle_motion(xcb_connection_t *connection,
        struct position_s root_pos);

/**
 * @brief Settle the window being placed where the outline stands
 *
 * @param connection XCB connection
 *
 * @note A no-op when no window is currently being pointed at
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop, from the map this hands on to
 */
void place_manual_handle_press(xcb_connection_t *connection);

/**
 * @brief Let the window being placed answer a key press
 *
 * @c Escape gives up, leaving the window where the smart policy had
 * already put it.  Every other key is swallowed: the keyboard is held
 * for as long as the question is open, and a key meant for whatever
 * had it before would have nowhere to go.
 *
 * @param connection XCB connection
 * @param keysym     Keysym of the pressed key
 *
 * @note A no-op when no window is currently being pointed at
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop, from the map an @c Escape hands on to
 */
void place_manual_handle_keypress(xcb_connection_t *connection,
        xcb_keysym_t keysym);

/**
 * @brief Milliseconds left before the window being placed is given up
 *        on and placed anyway
 *
 * For the main loop to fold into its @c poll timeout computation,
 * the same way @a mouse_hover_poll_ms_remaining already is, so the
 * wait ends on time rather than whenever the next unrelated event
 * happens to wake the loop up.
 *
 * @return Milliseconds remaining (never negative), or @c -1 when no
 *         window is currently being pointed at
 *
 * @note Complexity: @e O(1)
 */
int place_manual_ms_remaining(void);

/**
 * @brief Give up on the window being placed, if its wait has elapsed
 *
 * @param connection XCB connection
 *
 * @note A no-op when no window is being pointed at, or when one is but
 *       its wait has not elapsed yet
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop, from the map this hands on to
 */
void place_manual_tick(xcb_connection_t *connection);

/**
 * @brief Drop a window that disappeared before it could be placed
 *
 * Releases the pointer and the keyboard and takes the outline down
 * when @p client is the one currently being pointed at, and simply
 * forgets it when it was still waiting its turn.  What would have
 * finished the map is deliberately never called: there is no longer a
 * window to map.
 *
 * @param connection XCB connection
 * @param client     Client that disappeared
 *
 * @note A no-op for a client that was neither being placed nor waiting
 * @note Complexity: @e O(q), where @e q is the number of windows
 *       waiting their turn
 */
void place_manual_cancel_client(xcb_connection_t *connection,
        const client_td *client);


#endif  /* ! POLICY_PLACEMENT_MANUAL_H */
