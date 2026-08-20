/**
 * @file scratchpad.c
 *
 * @brief A single dedicated client, launched on demand, toggled
 *        visible/hidden instead of iconified/restored
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */


/* System includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>  /* pid_t */
#include <time.h>       /* clock_gettime, struct timespec */

/* Project includes */
#include <client.h>
#include <cmds/client/basic.h>
#include <cmds/client/layer.h>
#include <cmds/client/meta.h>
#include <cmds/client/state.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <policy/focus.h>
#include <surface.h>
#include <wm.h>

/* Default initial values */
#include <defs/scratchpad.h>

/* Local includes */
#include <scratchpad.h>


/** The current scratchpad client, or @c NULL when none is alive */
static client_td *s_scratchpad_client = NULL;

/**
 * @brief Set right after launching the configured command, until the
 *        next client to be created is claimed as the scratchpad
 *
 * Guards against a second launch stacking up while the first one is
 * still starting.
 */
static bool s_awaiting_scratchpad = false;

/**
 * @brief PID of the process launched by the @a s_awaiting_scratchpad
 *        currently in progress, or @c -1 when none is
 *
 * Checked in @a scratchpad_notice_client_created before a client is
 * claimed, so a second, unrelated window finishing @c client_init
 * while this launch is still starting is never mistaken for it:
 * without this, whichever client happened to finish next, regardless
 * of what it actually was, got claimed instead, which a burst of
 * several windows opening at once made easy to trigger in practice.
 */
static pid_t s_awaiting_scratchpad_pid = (pid_t) -1;

/**
 * @brief When the currently in-progress @a s_awaiting_scratchpad
 *        launch was started
 *
 * Only meaningful while @a s_awaiting_scratchpad itself is @c true;
 * checked in @a scratchpad_toggle to give up on a launch that never
 * produced a matching client (the process died before creating any
 * window, or never managed to connect at all, both realistic outcomes
 * under the exact kind of X-server resource pressure the scratchpad
 * is often reached for in the first place), rather than leaving
 * every later toggle silently refusing to try again for the rest of
 * the session.
 */
static struct timespec s_awaiting_scratchpad_since;


/**
 * @brief Resolve one dimension against the given available extent
 *
 * @param size      Configured dimension, fixed or @c "max"
 * @param available Extent of the axis it is measured against
 *
 * @return @p size.pixels under @c CONFIG_SCRATCHPAD_SIZE_FIXED,
 *         @p available under @c CONFIG_SCRATCHPAD_SIZE_MAX
 *
 * @note Complexity: @e O(1)
 */
static uint32_t s_resolve_size(struct config_scratchpad_size_s size,
        uint32_t available)
{
    return (size.mode == CONFIG_SCRATCHPAD_SIZE_MAX)
        ? available : size.pixels;
}


/* Launch the scratchpad, or toggle its visibility if one is already
 * running */
void scratchpad_toggle(const wm_td *wm, desktop_td *desktop)
{
    const config_td *config = wm_config(wm);

    if (wm == NULL || desktop == NULL || config == NULL ||
            !config->base.scratchpad.is_enabled) {
        return;
    }

    if (s_scratchpad_client == NULL) {
        pid_t launched_pid = (pid_t) -1;
        struct timespec now;
        bool awaiting_expired = false;

        if (s_awaiting_scratchpad) {
            if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
                long elapsed_seconds =
                    now.tv_sec - s_awaiting_scratchpad_since.tv_sec;
                awaiting_expired = (elapsed_seconds >=
                        WM_SCRATCHPAD_AWAIT_TIMEOUT_SECONDS);
            }
            if (!awaiting_expired) {
                return;
            }
            LOGGER_WARNING("Scratchpad launch never produced a" \
                    " matching client within %ld second(s);" \
                    " allowing a fresh attempt",
                    WM_SCRATCHPAD_AWAIT_TIMEOUT_SECONDS);
        }

        s_awaiting_scratchpad =
            (desktop_action_process_launch_with_class(desktop,
                    config->base.scratchpad.command,
                    WM_SCRATCHPAD_WM_CLASS, &launched_pid) == 0);
        s_awaiting_scratchpad_pid = s_awaiting_scratchpad
            ? launched_pid : (pid_t) -1;
        if (s_awaiting_scratchpad) {
            (void) clock_gettime(CLOCK_MONOTONIC,
                    &s_awaiting_scratchpad_since);
        }
        return;
    }

    if (client_is_hidden(s_scratchpad_client)) {
        surface_td *surface;

        if (s_scratchpad_client->desktop_id != desktop->id) {
            desktop_td *const source = wm_get_client_desktop(
                    s_scratchpad_client);

            if (source != NULL) {
                enact_desktop_client_send(source, s_scratchpad_client,
                        desktop);
            }
        }
        enact_client_unhide(s_scratchpad_client);

        /* Actually focuses the scratchpad, not merely raising it above
         * whatever else was on screen: a dropdown terminal is expected
         * to be ready to type into the moment it appears, with no
         * separate click first needed to focus it.
         *
         * Routed through 'focus_apply' ('policy/focus.h') rather than
         * 'enact_client_raise' + some 'ccmd_client_focus' call of its
         * own, since 'focus_apply' is the one place that already
         * unfocuses whatever was previously active first (border and
         * titlebar repainted back to its own inactive style there,
         * 'ccmd_client_unfocus', in 'cmds/client/focus.c') before
         * focusing this one.  Skipping it left whatever was focused
         * a moment ago with no real unfocus ever applied to it at all,
         * this client's own raise just visually covering it instead. */
        surface = wm_get_surface_by_id(desktop->screen_id);

        focus_apply(wm_surfaces(wm), surface, desktop,
                s_scratchpad_client, true, config);
    } else {
        enact_client_hide(s_scratchpad_client);
    }
}


/* Claim a newly created client as the scratchpad, if one was just
 * launched and is still awaited */
void scratchpad_notice_client_created(client_td *client)
{
    if (client == NULL || !s_awaiting_scratchpad) {
        return;
    }

    /* A definite mismatch, both PIDs known and different, means this
     * is provably some other, unrelated window finishing client_init
     * while the real launch is still starting; left alone, so a
     * later client that does match still gets the chance to claim
     * this instead.  A client whose own '_NET_WM_PID' the launched
     * application never set (client->process.pid still client_init's
     * own -1 default) cannot be disproven this way, so it still gets
     * claimed here rather than left waiting forever: for an
     * application that never reports its PID, this is the same
     * "claim whoever's next" behavior scratchpad support already had,
     * not a new risk introduced here, and the alternative (refusing
     * to ever claim such an application at all) would break the
     * scratchpad outright for it. */
    if (client->process.pid != (pid_t) -1 &&
            s_awaiting_scratchpad_pid != (pid_t) -1 &&
            client->process.pid != s_awaiting_scratchpad_pid) {
        return;
    }

    s_awaiting_scratchpad = false;
    s_awaiting_scratchpad_pid = (pid_t) -1;
    s_scratchpad_client = client;

    ccmd_client_reclass(client, WM_SCRATCHPAD_WM_CLASS,
            WM_SCRATCHPAD_WM_CLASS);
    client_skip_taskbar(client);
    client_skip_pager(client);
    ccmd_client_pin(client);
    client->properties.layer = CLIENT_LAYER_ABOVE;
    client->rule_position_locked = true;

    /* Removed here, before 'client_lock' below, rather than left for
     * whatever theme default 'ci_create_decorations' (vid. 'client.c')
     * already applied moments earlier in this same 'client_init' call.
     * Never decorated is a hard guarantee of this feature, not
     * something any theme's own 'window.is-decorated' gets a say in.
     *
     * 'ccmd_client_toggle_decorate' ('cmds/client/state.c') is safe to
     * call this early, with 'client' not yet on any desktop: its own
     * 'wm_get_client_desktop' lookup already handles a NULL result,
     * skipping only the focus/redraw bookkeeping a client with no
     * desktop yet has no use for regardless.
     *
     * Ordered before 'client_lock': that flag is what makes
     * 'ccmd_client_toggle_decorate' itself refuse to run at all past
     * this point, so undoing decoration has to happen first, while it
     * can still act. */
    if (client_is_decorated(client)) {
        ccmd_client_toggle_decorate(client);
    }

    /* Set here rather than left to whatever generic window border
     * 'ccmd_client_toggle_decorate' above just configured
     * (client->theme->window.active/inactive.border, meant for an
     * ordinary decorated client, not this always-undecorated one):
     * the scratchpad themes its own border independently
     * ('config_theme_s::scratchpad.border', in 'config.h').
     *
     * Setting 'border_override' rather than applying the border
     * directly here is what makes it survive every later focus change
     * too: 'ccmd_client_focus'/'ccmd_client_unfocus' (both in
     * 'cmds/client/focus.c') already re-apply a client's own border on
     * every single one via 'client_border_apply' ('client.h'), which
     * already prefers this field over the theme's own default whenever
     * it is set, without needing to know anything about the scratchpad
     * specifically.
     *
     * Applying it once more here regardless, rather than only ever
     * relying on the next focus change to do it, covers
     * 'focus.focus-new: false', where a freshly mapped client is
     * not focused at all and so would otherwise show no border until
     * the first time it is.  A theme reload only ever affects the next
     * scratchpad launched, not this one still running (see
     * 'config.md''s note on this), since this whole block runs once,
     * right here. */
    if (client->theme != NULL) {
        client->border_override.is_set = true;
        client->border_override.color =
            client->theme->scratchpad.border.color;
        client->border_override.width =
            client->theme->scratchpad.border.width;
        client_border_apply(client, true);
    }

    client_lock(client);

    /* Never a valid fallback focus target on its own, e.g., after some
     * other client on the same desktop shades or hides.  Its own
     * visibility is managed entirely by 'scratchpad_toggle', not by
     * anything that picks a next client to focus generically; see
     * 'CLIENT_FLAG_NO_FOCUS_FALLBACK' (client.h) */
    client_set_no_focus_fallback(client);

    LOGGER_TRACE("Claimed client %#x as the scratchpad",
            client->window);
}


/* Position the current scratchpad client against its own configured
 * edge, size, and desktop */
void scratchpad_position(client_td *client, desktop_td *desktop,
        surface_td *surface)
{
    const config_td *config;
    int32_t area_x;
    int32_t area_y;
    uint32_t area_w;
    uint32_t area_h;
    uint32_t border;
    uint32_t avail_w;
    uint32_t avail_h;
    uint32_t width;
    uint32_t height;
    int32_t x = 0;
    int32_t y = 0;

    if (!scratchpad_is_client(client) || desktop == NULL ||
            surface == NULL || surface->config == NULL) {
        return;
    }

    config = surface->config;

    if (config->base.scratchpad.ignore_margins ||
            desktop->workarea.dim.w == 0u ||
            desktop->workarea.dim.h == 0u) {
        area_x = 0;
        area_y = 0;
        area_w = surface->properties.dim.w;
        area_h = surface->properties.dim.h;
    } else {
        area_x = desktop->workarea.pos.x;
        area_y = desktop->workarea.pos.y;
        area_w = desktop->workarea.dim.w;
        area_h = desktop->workarea.dim.h;
    }

    /* Per the X11 protocol ('ConfigureWindow'), 'x'/'y' name a window's
     * own top-left corner including its border, if any, which is then
     * drawn growing rightward/downward from there, meaning the full
     * on-screen footprint of a client with a border reaches all the way
     * to 'x + 2 * border + width' (equivalently for height),
     * '2 * border' wider/taller than 'width'/'height' alone.  Reserving
     * that much here, before 'width'/'height' are ever resolved against
     * 'area_w'/'area_h' below (rather than only afterward, e.g., by
     * shrinking a "max" result in place), keeps that full footprint
     * within the configured edge's own area on every side, not just
     * flush against whichever edge 'x'/'y' themselves already sit on.
     *
     * An unadjusted "max" width, say, already flush with the left edge
     * at 'x == area_x', would otherwise still run its own right-hand
     * border '2 * border' past 'area_x + area_w' on the right, off
     * whatever the configured edge's own area was ever meant to stay
     * within. */
    /* 'client_border_width' ('client.h') already reflects both
     * 'border_override' set just above in 'scratchpad_notice_
     * client_created' and any 'a11y.focus-indicator.min-border-width'
     * floor over it, the exact width 'client_border_apply' itself will
     * actually draw, rather than 'border_override.width' alone, which
     * could be narrower than what a11y ends up enforcing and so reserve
     * too little room here for it. */
    border = client_border_width(client, true, false);
    avail_w = (area_w > 2u * border) ? area_w - 2u * border : 0u;
    avail_h = (area_h > 2u * border) ? area_h - 2u * border : 0u;

    width = s_resolve_size(config->base.scratchpad.width, avail_w);
    height = s_resolve_size(config->base.scratchpad.height, avail_h);
    if (width > avail_w) {
        width = avail_w;
    }
    if (height > avail_h) {
        height = avail_h;
    }

    switch (config->base.scratchpad.edge) {
    case CONFIG_SCRATCHPAD_EDGE_BOTTOM:
        x = area_x + (int32_t) ((avail_w - width) / 2u);
        y = area_y + (int32_t) (avail_h - height);
        break;
    case CONFIG_SCRATCHPAD_EDGE_LEFT:
        x = area_x;
        y = area_y + (int32_t) ((avail_h - height) / 2u);
        break;
    case CONFIG_SCRATCHPAD_EDGE_RIGHT:
        x = area_x + (int32_t) (avail_w - width);
        y = area_y + (int32_t) ((avail_h - height) / 2u);
        break;
    case CONFIG_SCRATCHPAD_EDGE_TOP:
        x = area_x + (int32_t) ((avail_w - width) / 2u);
        y = area_y;
        break;
    }

    enact_client_resize_force(client, x, y, width, height);
}


/* Release the scratchpad client reference, if the given client was it */
void scratchpad_notice_client_destroyed(const client_td *client)
{
    if (client != NULL && client == s_scratchpad_client) {
        s_scratchpad_client = NULL;
    }
}


/* Query whether the given client is the current scratchpad client */
bool scratchpad_is_client(const client_td *client)
{
    return client != NULL && client == s_scratchpad_client;
}
