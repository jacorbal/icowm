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
#include <cmds/client/focus.h>
#include <cmds/client/layer.h>
#include <cmds/client/transient.h>
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

    /* Bring any transient descendant now sitting on a different
     * desktop back onto this client's own, before the redirect just
     * below ever runs: see 'ccmd_client_bring_family''s own doc
     * comment (cmds/client/transient.h) for the full reasoning,
     * matching Openbox's own 'client_bring_modal_windows'. */
    ccmd_client_bring_family(client);

    /* Redirect to whichever mapped transient descendant should
     * actually receive focus in this client's place (a "save
     * changes?" prompt still sitting open on top of it, say), before
     * anything else below (including the 'accepts_input_focus' check
     * immediately following, and every piece of this function's own
     * active-client bookkeeping past it) ever sees the original,
     * un-redirected 'client'.  Doing this only inside
     * 'ccmd_client_focus' itself, in 'cmds/client/focus.c', is not
     * enough on its own:
     * that would still correctly steer the raw X11 input focus to the
     * dialog, but this function's own 'desktop->client_active_id'
     * assignment below, and the stacking-order raise further down,
     * would still track the original client, since a callee
     * reassigning its own local copy of a pointer parameter can never
     * be observed by its caller.  See 'ccmd_client_focus_target''s own
     * doc comment (cmds/client/transient.h) for the full reasoning. */
    client = ccmd_client_focus_target(client);
    if (client == NULL) {
        return;
    }

    /* ICCCM §4.1.7: a client whose own declared input model can
     * never actually receive real keyboard focus (see
     * 'client_accepts_input_focus', client.h) must not be allowed to
     * take over 'client_active_id'/'_NET_WM_STATE_FOCUSED' anyway,
     * or the client already holding real focus would be unfocused
     * below in its favor, leaving keyboard input directed nowhere:
     * 'ccmd_client_focus' correctly withholds 'SetInputFocus' and
     * 'WM_TAKE_FOCUS' from such a client already, but everything
     * else this function does (unfocusing whichever client actually
     * had focus, marking this one active, publishing
     * '_NET_WM_STATE_FOCUSED') would still run unless refused here.
     * Openbox's own 'focus_valid_target' (focus.c) gates on exactly
     * this same 'can_focus || focus_notify' condition before
     * considering a client at all. */
    if (!client_accepts_input_focus(client)) {
        return;
    }

    /* Unfocus the previous active client, and focus this one,
     * synchronously and in that exact order, rather than through the
     * queued 'client_send_event_unfocus'/'client_send_event_focus' pair
     * this used to use: 'ccmd_client_unfocus' redirects the X server's
     * real input focus to 'XCB_INPUT_FOCUS_POINTER_ROOT' (see its own
     * doc comment in 'cmds/client/focus.c'), on the assumption that
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
    desktop->is_focus_dirty = true;
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
             cfg->base.windows.focus.raise));
    if (should_raise) {
        enact_client_raise(client);
    } else if (client_is_fullscreen(client) ||
            (previous != NULL && client_is_fullscreen(previous))) {
        /* 'enact_client_raise' above already re-enforces layer
         * stacking as a side effect of raising, which is what
         * actually forces a focused fullscreen client above
         * everything else (see 'ccmd_desktop_enforce_layers''s own
         * doc comment) and lets one that just lost focus fall back
         * into its own real layer.  Without 'should_raise', neither
         * of those would otherwise happen at all for this focus
         * change, and that guarantee has to hold regardless of
         * whether raise-on-focus itself is configured on. */
        ccmd_desktop_enforce_layers(desktop);
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
