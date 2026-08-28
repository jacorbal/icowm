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
#include <adt/cdlist.h>
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
/**
 * @brief Every managed client, most recently focused first
 *
 * See @c policy/focus.h for why there is one of these rather than one
 * per desktop.  The clients belong to their desktops; this list only
 * refers to them, so it is created without a destructor.
 */
static cdlist_td *s_focus_order = NULL;


/**
 * @brief Find the node holding a client, and the one before it
 *
 * @param client Client to look for
 * @param prev   Receives the node before the one found, which
 *               @a cdlist_rem_next needs, or @c NULL when the client
 *               sits at the head; may itself be @c NULL
 *
 * @return The node holding @p client, or @c NULL when it is absent
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients
 */
static cdlist_item_td *s_focus_order_find(const client_td *client,
        cdlist_item_td **prev)
{
    cdlist_item_td *node;
    cdlist_item_td *behind = NULL;
    const cdlist_item_td *initial;

    if (prev != NULL) {
        *prev = NULL;
    }

    if (s_focus_order == NULL || cdlist_size(s_focus_order) == 0u) {
        return NULL;
    }

    node = cdlist_head(s_focus_order);
    initial = node;
    if (node == NULL) {
        return NULL;
    }

    do {
        if ((const client_td *) cdlist_data(node) == client) {
            if (prev != NULL) {
                *prev = behind;
            }
            return node;
        }
        behind = node;
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    return NULL;
}


/**
 * @brief Create the focus order on first use
 *
 * @return @c true when the list is available
 *
 * @note Complexity: @e O(1)
 */
static bool s_focus_order_ensure(void)
{
    if (s_focus_order == NULL) {
        s_focus_order = cdlist_init(NULL);
    }

    return s_focus_order != NULL;
}


/* Record a newly managed client in the focus order */
void focus_order_add(client_td *client)
{
    if (client == NULL || !s_focus_order_ensure()) {
        return;
    }

    /* Left where it is when already recorded, so that a path running
     * twice cannot demote a client the person did use recently */
    if (s_focus_order_find(client, NULL) != NULL) {
        return;
    }

    (void) cdlist_ins_next(s_focus_order,
            cdlist_tail(s_focus_order), client);
}


/* Forget a client that is no longer managed */
void focus_order_remove(const client_td *client)
{
    const cdlist_item_td *node;
    cdlist_item_td *prev = NULL;
    void *removed = NULL;

    if (client == NULL || s_focus_order == NULL) {
        return;
    }

    node = s_focus_order_find(client, &prev);
    if (node == NULL) {
        return;
    }

    (void) cdlist_rem_next(s_focus_order, prev, &removed);
}


/* Move a client behind every other client of its own desktop */
void focus_order_to_bottom(const desktop_td *desktop, client_td *client)
{
    cdlist_item_td *node;
    cdlist_item_td *prev = NULL;
    cdlist_item_td *last_of_desktop = NULL;
    const cdlist_item_td *initial;

    if (desktop == NULL || client == NULL || !s_focus_order_ensure()) {
        return;
    }

    /* The last client of this same desktop other than this one, which
     * is where this goes: behind that one, and no further.  The order
     * spans every managed client, so its far end is behind the windows
     * of every other desktop too, and putting it there would rank it
     * against windows it shares no screen with. */
    node = cdlist_head(s_focus_order);
    initial = node;
    if (node != NULL) {
        do {
            const client_td *const c = cdlist_data(node);

            if (c != NULL && c != client &&
                    desktop_find_client_by_id(desktop, c->id) == c) {
                last_of_desktop = node;
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    /* Nothing else on this desktop to be behind, so nothing to do.
     * Moving it anyway would shift it among the clients of other
     * desktops, which is the opposite of what demoting it here
     * means. */
    if (last_of_desktop == NULL) {
        return;
    }

    node = s_focus_order_find(client, &prev);
    if (node != NULL) {
        void *removed = NULL;

        (void) cdlist_rem_next(s_focus_order, prev, &removed);
    }

    (void) cdlist_ins_next(s_focus_order, last_of_desktop, client);
}


/* Move a client to the front of the focus order */
void focus_order_to_top(client_td *client)
{
    const cdlist_item_td *node;
    cdlist_item_td *prev = NULL;

    if (client == NULL || !s_focus_order_ensure()) {
        return;
    }

    node = s_focus_order_find(client, &prev);
    if (node != NULL) {
        void *removed = NULL;

        /* Already at the front: removing and reinserting would churn
         * a node to no end */
        if (node == cdlist_head(s_focus_order)) {
            return;
        }
        (void) cdlist_rem_next(s_focus_order, prev, &removed);
    }

    /* Inserted after no node at all, which 'cdlist_ins_next' takes as
     * the head of an otherwise untouched list */
    (void) cdlist_ins_next(s_focus_order, NULL, client);
}


/* Most recently focused client on a desktop that may hold focus now */
client_td *focus_order_best(const desktop_td *desktop,
        bool (*is_valid)(const client_td *candidate, void *data),
        void *data)
{
    cdlist_item_td *node;
    const cdlist_item_td *initial;

    if (desktop == NULL || is_valid == NULL ||
            s_focus_order == NULL ||
            cdlist_size(s_focus_order) == 0u) {
        return NULL;
    }

    node = cdlist_head(s_focus_order);
    initial = node;
    if (node == NULL) {
        return NULL;
    }

    do {
        client_td *const candidate = (client_td *) cdlist_data(node);

        /* Filtered by desktop here rather than by holding a list per
         * desktop, which is what lets a client moving between them
         * keep its place in the order untouched */
        if (candidate != NULL &&
                desktop_find_client_by_id(desktop,
                    candidate->id) == candidate &&
                is_valid(candidate, data)) {
            return candidate;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    return NULL;
}


/* Visit every client of a desktop in focus order */
void focus_order_walk(const desktop_td *desktop,
        void (*visit)(client_td *client, void *data), void *data)
{
    cdlist_item_td *node;
    const cdlist_item_td *initial;

    if (desktop == NULL || visit == NULL || s_focus_order == NULL ||
            cdlist_size(s_focus_order) == 0u) {
        return;
    }

    node = cdlist_head(s_focus_order);
    initial = node;
    if (node == NULL) {
        return;
    }

    do {
        client_td *const c = (client_td *) cdlist_data(node);

        if (c != NULL &&
                desktop_find_client_by_id(desktop, c->id) == c) {
            visit(c, data);
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);
}


/* Release the focus order */
void focus_order_destroy(void)
{
    if (s_focus_order != NULL) {
        cdlist_destroy(s_focus_order);
        s_focus_order = NULL;
    }
}


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

    /* Recorded in the focus order, which is what the cycle menu and
     * focus recovery read, and never in the stacking list, which is
     * where the window sits on screen.  Moving it there instead, as
     * this once did, raised the window on the next desktop switch:
     * that switch replays the stacking list onto the X server, so a
     * window merely focused came back on top although
     * 'windows.focus.raise' had said not to raise it.  See
     * 'policy/focus.h'. */
    focus_order_to_top(client);

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
