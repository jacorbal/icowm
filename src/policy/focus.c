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
#include <scratchpad.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* IPC includes */
#include <ipc.h>

/* Local includes */
#include <cmds/client/basic.h>
#include <policy/focus.h>


/* Determine whether the loaded focus policy follows the pointer */
bool focus_is_sloppy(const config_td *cfg)
{
    if (cfg == NULL) {
        return false;
    }

    return (cfg->base.windows.focus_policy ==
            CONFIG_FOCUS_POLICY_SLOPPY);
}


/* Focus a client and keep focus-related state in sync */
void focus_apply(list_td *surfaces, surface_td *surface,
        desktop_td *desktop, client_td *client,
        bool raise, const config_td *cfg)
{
    client_td *previous = NULL;
    bool should_raise;
    cJSON *fields;

    if (surface == NULL || desktop == NULL || client == NULL) {
        return;
    }

    /* Unfocus the previous active client, and focus this one,
     * synchronously and in that exact order, rather than through the
     * queued 'client_send_event_unfocus'/'client_send_event_focus' pair
     * this used to use: 'ccmd_client_unfocus' redirects the X server's
     * real input focus to 'XCB_INPUT_FOCUS_POINTER_ROOT' (see its own
     * doc comment in 'cmds/client/basic.c'), on the assumption that
     * a caller unfocusing a client to immediately focus another
     * "harmlessly overrides this a moment later".  That assumption only
     * holds if the override actually runs before anything else can
     * observe or act on the intervening pointer-follows-focus state;
     * called directly from the same synchronous path a caller reached
     * here from (e.g., 'ccmd_client_restore', which already
     * synchronously focuses 'client' once before this function even
     * runs, only for the unfocus below to then run afterward and undo
     * it), nothing guarantees that ordering.
     *
     * Calling both functions directly here removes the gap entirely:
     * this function already holds both 'previous' and 'client' with
     * a precise ordering requirement between them, so it needs neither
     * the queue's own decoupling nor its reentrancy guarantees to begin
     * with. */
    if (surfaces != NULL &&
            desktop->client_active_id != 0 &&
            desktop->client_active_id != client->id) {
        previous = lookup_find_client(surfaces,
                desktop->client_active_id, NULL, NULL);
        if (previous != NULL) {
            ccmd_client_unfocus(previous);
            if (scratchpad_is_client(previous) &&
                    !client_is_hidden(previous)) {
                enact_client_hide(previous);
            }
        }
    }

    desktop->client_active_id = client->id;
    desktop->focus_dirty = true;
    ccmd_client_focus(client);

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
        enact_client_raise(client);
    }

    fields = cJSON_CreateObject();
    if (fields != NULL) {
        cJSON_AddNumberToObject(fields, "surface_id",
                (double) surface->id);
        cJSON_AddNumberToObject(fields, "client_id",
                (double) client->id);
    }
    ipc_broadcast_event(IPC_EVENT_FOCUS_CHANGED, fields);
}
