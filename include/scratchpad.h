/**
 * @file scratchpad.h
 *
 * @brief A single dedicated client, launched on demand from its
 *        configured command, toggled visible/hidden by its keybind or
 *        IPC command instead of iconified/restored
 *
 * The scratchpad client keeps running between one hide and the next
 * show: hiding it never terminates the underlying process, only maps it
 * off-screen the same way @a hide_client already does for any other
 * client, so its state (a shell's scrollback, say) is exactly where it
 * was left.  Only the client actually exiting on its own (the shell
 * inside it receiving @a exit, for instance) ever clears the current
 * scratchpad, at which point the next toggle launches a fresh one from
 * @p config->base.scratchpad.command.
 *
 * @ingroup scratchpad
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SCRATCHPAD_H
#define SCRATCHPAD_H


/* System includes */
#include <stdbool.h>

/* Project includes */
#include <types/handles.h>


/**
 * @brief Launch the scratchpad, or toggle its visibility if one is
 *        already running
 *
 * Does nothing at all when @p config->base.scratchpad.is_enabled is
 * @c false.  With no scratchpad client currently alive, launches
 * @p config->base.scratchpad.command on @p desktop's surface and
 * returns immediately; the launched client is recognized and claimed as
 * the scratchpad once it maps, not by this call itself.  With one
 * already alive, shows it (raised above whatever else is on that layer,
 * moved to @p desktop if it was on a different one, repositioned fresh
 * against its configured edge every single time, since any viewport
 * pan since it was last shown may have shifted it away from that edge)
 * when currently hidden, or hides it otherwise.
 *
 * @param wm      Window manager state, for its configuration
 * @param desktop Desktop to show the scratchpad on, or to launch it
 *                onto the first time
 *
 * @note Complexity: @e O(1)
 *
 * @see @a scratchpad_notice_client_created
 * @see @a scratchpad_position
 */
void scratchpad_toggle(const wm_td *wm, desktop_td *desktop);

/**
 * @brief Claim a newly created client as the scratchpad, if one was
 *        just launched and is still awaited
 *
 * Called from @a client_init itself, after every other field (including
 * @p info.class_name) is already filled in.
 *
 * A no-op unless @a scratchpad_toggle's launch is still pending, in
 * which case @p client (regardless of its own real @c WM_CLASS, since
 * not every program a user might configure can be made to set one) is
 * forced to @c WM_SCRATCHPAD_WM_CLASS (@c defs/scratchpad.h)
 * via @a ccmd_client_reclass, marked to skip the taskbar and pager the
 * same way any client requesting that itself would be, stripped of
 * decoration, and has its @p rule_position_locked set so the ordinary
 * map-time @a place_window_apply never touches its geometry.  That
 * geometry is applied separately, by @a scratchpad_position, once
 * @p client's desktop and surface are known (client_init runs before
 * either is assigned).
 *
 * @param client Newly created client, not yet added to any desktop
 *
 * @note Complexity: @e O(1)
 */
void scratchpad_notice_client_created(client_td *client);

/**
 * @brief Position the current scratchpad client against its configured
 *        edge, size, and desktop
 *
 * Called once, at map time, right after @p client's @p desktop_id and
 * @p screen_id are assigned, since @a scratchpad_notice_client_created
 * runs too early for either to be available yet; called again later by
 * @a scratchpad_reposition, whenever @p surface's work areas are
 * recomputed for any other reason.
 *
 * An axis configured @c "max" is maximized, not merely resized to the
 * workarea's edge: it sets @c CLIENT_STATE_MAXIMIZED_HORZ /
 * @c CLIENT_STATE_MAXIMIZED_VERT (both, under
 * @c CLIENT_STATE_MAXIMIZED, when both axes are @c "max") and reaches
 * that edge exactly, ignoring @c WM_NORMAL_HINTS resize increments the
 * way @a ccmd_client_maximize_horz / @a _vert already do.  Everything
 * else about the scratchpad (its fixed edge anchoring, never being
 * draggable, hiding through @a enact_client_hide) is unaffected.
 *
 * @param client  Client to position
 * @param desktop Desktop @p client was just added to
 * @param surface Surface @p desktop belongs to
 *
 * @note A no-op for any client other than the current scratchpad one
 * @note Complexity: @e O(1)
 *
 * @see @a scratchpad_is_client
 */
void scratchpad_position(client_td *client,
        const desktop_td *desktop, surface_td *surface);

/**
 * @brief Reposition the current scratchpad client, if its desktop
 *        belongs to the given surface
 *
 * Called from @a surface_refresh_workareas (@c surface/workareas.c)
 * itself, right after that function recomputes every desktop's work
 * area on @p surface, so every path already reaching that function (an
 * XRandR resolution change, a dock or panel appearing or disappearing,
 * and every other one) reaches this too, without each needing its
 * separate call.  Without this, a scratchpad already positioned against
 * one work area (@c "max" width against the bottom edge, say) stayed at
 * that exact size and position no matter how the work area it was
 * computed against later changed, until the underlying process happened
 * to exit on its own and a fresh one was relaunched against the work
 * area current then.
 *
 * @param surface Surface whose work areas were just recomputed
 *
 * @note A no-op if there is no current scratchpad client, or if its own
 *       desktop does not belong to @p surface
 * @note Complexity: @e O(1)
 *
 * @see @a scratchpad_position, which this calls
 */
void scratchpad_reposition(surface_td *surface);

/**
 * @brief Release the scratchpad client reference, if @p client was it
 *
 * Called whenever a client is destroyed; a no-op for any client other
 * than the current scratchpad one.  Leaves the next
 * @a scratchpad_toggle call free to launch a new one, since nothing
 * else ever clears this reference.
 *
 * @param client Client that was just destroyed
 *
 * @note Complexity: @e O(1)
 */
void scratchpad_notice_client_destroyed(const client_td *client);

/**
 * @brief Hide the current scratchpad client if @p desktop, its own,
 *        just had its viewport panned
 *
 * Called from @a s_viewport_apply_origin (@c cmds/surface.c) right
 * after it actually shifts @p desktop's viewport origin, i.e., only
 * when a real pan happened, never on a no-op request that resolved
 * back to the same origin already in place.  A visible scratchpad is
 * just another non-sticky client that walk already shifts along with
 * everything else on @p desktop (see @c s_viewport_translate_visit),
 * so left alone it would keep following the pan away from its own
 * configured edge instead of staying anchored there; hiding it here
 * instead avoids ever showing it drifted out of its own zone, and the
 * next @a scratchpad_toggle repositions it fresh before showing it
 * again.
 *
 * @param desktop Desktop whose viewport just panned
 *
 * @note A no-op with no current scratchpad client, one already hidden,
 *       or one that belongs to a different desktop than @p desktop
 * @note Complexity: @e O(1)
 *
 * @see @a scratchpad_toggle, which repositions before showing again
 */
void scratchpad_notice_viewport_panned(const desktop_td *desktop);

/**
 * @brief Query whether @p client is the current scratchpad client
 *
 * Meant for every listing of managed clients that should never include
 * it.  Command @c list_clients (IPC), the window cycle, and the
 * window-list menu, none of which otherwise filter on @p skip_taskbar
 * or @p skip_pager (those only ever reach external tools over EWMH,
 * never IcoWM's internal listings).
 *
 * @param client Client to query; @c NULL is never the scratchpad
 *
 * @return @c true when @p client is the current scratchpad client
 *
 * @note Complexity: @e O(1)
 */
bool scratchpad_is_client(const client_td *client);


#endif  /* ! SCRATCHPAD_H */
