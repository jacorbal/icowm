/**
 * @file policy/focus.c
 *
 * @brief Client focus policy and application implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>

/* Local includes */
#include <cmds/ccmd.h>
#include <policy/focus.h>


bool focus_is_follow_mouse(const config_td *cfg)
{
    if (cfg == NULL) {
        return false;
    }

    return (cfg->base.windows.focus_policy ==
            CONFIG_FOCUS_POLICY_FOLLOW_MOUSE);
}


void focus_apply(list_td *surfaces,
        surface_td *surface, desktop_td *desktop,
        client_td *client, bool raise, const config_td *cfg)
{
    client_td *previous = NULL;
    bool should_raise;

    if (surface == NULL || desktop == NULL || client == NULL) {
        return;
    }

    /* Unfocus the previous active client, and focus this one,
     * synchronously and in that exact order, rather than through the
     * queued 'client_send_event_unfocus'/'client_send_event_focus'
     * pair this used to use: 'wcmd_client_unfocus' redirects the X
     * server's real input focus to 'XCB_INPUT_FOCUS_POINTER_ROOT' (see
     * its own doc comment in cmds/ccmd.c), on the assumption that a
     * caller unfocusing a client to immediately focus another
     * "harmlessly overrides this a moment later".  That assumption
     * only holds if the override actually runs before anything else
     * can observe or act on the intervening pointer-follows-focus
     * state; queued through the same event queue a caller reached
     * here from (e.g. 'wcmd_client_restore', which already
     * synchronously focuses 'client' once before this function even
     * runs, only for the queued unfocus below to then run afterward
     * and undo it), nothing guarantees that ordering.  Calling both
     * functions directly here removes the gap entirely: this
     * function already holds both 'previous' and 'client' with a
     * precise ordering requirement between them, so it needs neither
     * the queue's own decoupling nor its reentrancy guarantees to
     * begin with. */
    if (surfaces != NULL &&
            desktop->client_active_id != 0 &&
            desktop->client_active_id != client->id) {
        previous = lookup_find_client(surfaces,
                desktop->client_active_id, NULL, NULL);
        if (previous != NULL) {
            wcmd_client_unfocus(previous);
        }
    }

    desktop->client_active_id = client->id;
    desktop->focus_dirty = true;
    wcmd_client_focus(client);

    /* Mark outdated so the next update cycle repaints titlebars */
    desktop->is_outdated = true;
    surface->is_outdated = true;

    /* Always move the newly focused client to the tail of the stacking
     * list (MRU head) so that the cycle menu, the Z-order restoration
     * on desktop switch, and focus recovery all track the last focused
     * window, regardless of whether the window is being raised. */
    (void) desktop_action_client_send_front(desktop, client);

    should_raise = (raise ||
            (cfg != NULL &&
             cfg->base.windows.focus.is_raised_on_focus));
    if (should_raise) {
        (void) client_send_event_raise(client);
    }
}
