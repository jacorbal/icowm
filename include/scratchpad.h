/**
 * @file scratchpad.h
 *
 * @brief A single dedicated client, launched on demand from its own
 *        configured command, toggled visible/hidden by its own keybind
 *        or IPC command instead of iconified/restored
 *
 * The scratchpad client keeps running between one hide and the next
 * show: hiding it never terminates the underlying process, only maps it
 * off-screen the same way @a hide_client already does for any other
 * client, so its own state (a shell's scrollback, say) is exactly where
 * it was left.  Only the client actually exiting on its own (the shell
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
#include <client.h>
#include <desktop.h>
#include <surface.h>
#include <wm.h>


/**
 * @brief Launch the scratchpad, or toggle its visibility if one is
 *        already running
 *
 * Does nothing at all when @p config->base.scratchpad.is_enabled is
 * @c false.  With no scratchpad client currently alive, launches
 * @p config->base.scratchpad.command on @p desktop's own surface and
 * returns immediately; the launched client is recognized and claimed as
 * the scratchpad once it maps, not by this call itself.  With one
 * already alive, shows it (raised above whatever else is on that layer,
 * moved to @p desktop if it was on a different one) when currently
 * hidden, or hides it otherwise.
 *
 * @param wm      Window manager state, for its own configuration
 * @param desktop Desktop to show the scratchpad on, or to launch it
 *                onto the first time
 *
 * @note Complexity: @e O(1)
 *
 * @see @a scratchpad_notice_client_created
 */
void scratchpad_toggle(wm_td *wm, desktop_td *desktop);

/**
 * @brief Claim a newly created client as the scratchpad, if one was
 *        just launched and is still awaited
 *
 * Called from @a client_init itself, after every other field
 * (including @p info.class_name) is already filled in.
 *
 * A no-op unless @a scratchpad_toggle's own launch is still pending, in
 * which case @p client (regardless of its own real @c WM_CLASS, since
 * not every program a user might configure can be made to set one) is
 * forced to @c WM_SCRATCHPAD_WM_CLASS (@c defs/scratchpad.h)
 * via @a ccmd_client_reclass, marked to skip the taskbar and pager the
 * same way any client requesting that itself would be, stripped of
 * decoration, and has its own @p rule_position_locked set so the
 * ordinary map-time @a place_apply never touches its geometry.  That
 * geometry is applied separately, by @a scratchpad_position, once
 * @p client's own desktop and surface are known (client_init runs
 * before either is assigned).
 *
 * @param client Newly created client, not yet added to any desktop
 *
 * @note Complexity: @e O(1)
 */
void scratchpad_notice_client_created(client_td *client);

/**
 * @brief Position the current scratchpad client against its own
 *        configured edge, size, and desktop
 *
 * Meant to be called once, at map time, right after @p client's own
 * @p desktop_id and @p screen_id are assigned, since
 * @a scratchpad_notice_client_created runs too early for either to be
 * available yet.
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
void scratchpad_position(client_td *client, desktop_td *desktop,
        surface_td *surface);

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
 * @brief Query whether @p client is the current scratchpad client
 *
 * Meant for every listing of managed clients that should never include
 * it.  Command @c list_clients (IPC), the window cycle, and the
 * window-list menu, none of which otherwise filter on @p skip_taskbar
 * or @p skip_pager (those only ever reach external tools over EWMH,
 * never IcoWM's own internal listings).
 *
 * @param client Client to query; @c NULL is never the scratchpad
 *
 * @return @c true when @p client is the current scratchpad client
 *
 * @note Complexity: @e O(1)
 */
bool scratchpad_is_client(const client_td *client);


#endif  /* ! SCRATCHPAD_H */
