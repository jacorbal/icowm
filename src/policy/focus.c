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

    /* Unfocus previous active client */
    if (surfaces != NULL &&
            desktop->client_active_id != 0 &&
            desktop->client_active_id != client->id) {
        previous = lookup_find_client(surfaces,
                desktop->client_active_id, NULL, NULL);
        if (previous != NULL) {
            (void) client_send_event_unfocus(previous);
        }
    }

    desktop->client_active_id = client->id;
    (void) client_send_event_focus(client);

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
