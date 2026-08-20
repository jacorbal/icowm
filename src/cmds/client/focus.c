/**
 * @file cmds/client/focus.c
 *
 * @brief Focus-granting, focus-fallback, close, kill, and restore
 *        actions over clients
 *
 * Split out of what used to be a single, flat @c cmds/client/basic.c;
 * kept as one contiguous block (matching the order these already had
 * in that file) rather than separated further, since @c
 * ccmd_client_close/kill/restore all lead into the same focus-
 * fallback mechanism @c ccmd_client_focus_fallback/client_focus_
 * fallback provide right above them.
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
#include <stdint.h>
#include <stdlib.h>     /* NULL, free */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Default initial values */
#include <defs/desktop.h>
#include <defs/icon.h>

/* Windows policy includes */
#include <policy/placement/window.h>

/* Utils includes */
#include <utils/geom.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <ipc.h>
#include <lookup.h>
#include <render/outdate.h>
#include <systray.h>
#include <wm.h>
#include <cctl/kill.h>

/* Local includes */
#include <cmds/client/basic.h>
#include <cmds/client/geom.h>
#include <cmds/client/internal.h>


/**
 * @brief Whether @p candidate qualifies as a focus-fallback target
 *
 * Shared by both passes @a client_focus_fallback itself makes over
 * @p desktop's own stacking list: mapped and visible (not hidden,
 * shaded, or iconified), able to take real focus by window type,
 * not explicitly opted out via @c client_has_no_focus_fallback, and
 * not skipping the taskbar unless it is modal, urgent, or a dialog
 * (which need the person's attention regardless of that flag).
 *
 * @param candidate Client being considered as a fallback target
 * @param exclude   Client that must never be chosen (the one
 *                  leaving the current visible focus chain), or
 *                  @c NULL when nothing is excluded
 *
 * @return @c true if @p candidate is a valid fallback target
 *
 * @note Complexity: @e O(1)
 */
static bool s_client_focus_fallback_valid(const client_td *candidate,
        const client_td *exclude)
{
    return candidate != NULL && candidate != exclude &&
        !(candidate->properties.flags & CLIENT_FLAG_HIDDEN) &&
        !client_is_shaded(candidate) &&
        candidate->properties.state !=
            (uint16_t) CLIENT_STATE_ICONIFIED &&
        (candidate->properties.flags & CLIENT_FLAG_FOCUSABLE) &&
        !client_has_no_focus_fallback(candidate) &&
        (!(candidate->properties.flags & CLIENT_FLAG_SKIP_TASKBAR) ||
         client_is_modal(candidate) ||
         client_is_urgent(candidate) ||
         candidate->properties.type == (uint16_t) CLIENT_TYPE_DIALOG);
}


/* Transfer focus away from a client that is leaving the current
 * visible focus chain; see cmds/client/internal.h for the full
 * doc comment */
void ccmd_client_focus_fallback(const client_td *client)
{
    surface_td *surface;
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    surface = wm_get_surface_by_id(client->screen_id);
    if (surface == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL || desktop->client_active_id != client->id) {
        return;
    }

    client_focus_fallback(desktop, surface, client);
}


/* Transfer input focus away from a client leaving the current
 * visible focus chain to the most recently used other visible,
 * focusable client on the same desktop, or to 'PointerRoot' if
 * none qualifies; see the full criteria in the header */
void client_focus_fallback(desktop_td *desktop, surface_td *surface,
        const client_td *exclude)
{
    cdlist_item_td *node;
    client_td *next_focus = NULL;
    xcb_window_t exclude_leader;

    if (desktop == NULL) {
        return;
    }

    desktop->client_active_id = 0;
    desktop->focus_dirty = true;

    /* A window left behind by 'exclude' from the same application
     * (sharing its own 'WM_CLIENT_LEADER', ICCCM 4.1.2.5) is a more
     * natural fallback than an unrelated one equally close in MRU
     * order, the same reasoning 'place_apply' (policy/placement.c)
     * already applies when placing a new sibling window near its
     * own group; mirrors how Openbox's own 'focus_valid_target'
     * (focus.c) weighs group membership when picking a focus
     * target.  Tried first and only as a preference, not a
     * requirement: falls through to the plain MRU search below,
     * unchanged from before, whenever no such sibling qualifies. */
    exclude_leader = (exclude != NULL)
        ? client_group_leader(exclude) : XCB_WINDOW_NONE;

    if (exclude_leader != XCB_WINDOW_NONE &&
            desktop->stacking != NULL &&
            cdlist_size(desktop->stacking) > 0) {
        const cdlist_item_td *initial;

        node = cdlist_tail(desktop->stacking);
        initial = node;
        if (node != NULL) {
            do {
                client_td *candidate = (client_td *) cdlist_data(node);
                if (s_client_focus_fallback_valid(candidate, exclude) &&
                        client_group_leader(candidate) == exclude_leader) {
                    next_focus = candidate;
                    break;
                }
                node = cdlist_prev(node);
            } while (node != NULL && node != initial);
        }
    }

    if (next_focus == NULL &&
            desktop->stacking != NULL && cdlist_size(desktop->stacking) > 0) {
        const cdlist_item_td *initial;

        node = cdlist_tail(desktop->stacking);
        initial = node;
        if (node != NULL) {
            do {
                client_td *candidate = (client_td *) cdlist_data(node);
                if (s_client_focus_fallback_valid(candidate, exclude)) {
                    next_focus = candidate;
                    break;
                }
                node = cdlist_prev(node);
            } while (node != NULL && node != initial);
        } /* ! if (node) */
    }

    if (next_focus != NULL) {
        desktop->client_active_id = next_focus->id;
        desktop->focus_dirty = true;
        (void) desktop_action_client_send_front(desktop, next_focus);
        ccmd_client_focus(next_focus);
    } else if (desktop->connection != NULL) {
        xcb_set_input_focus(desktop->connection,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_CURRENT_TIME);
    }

    wm_outdate_desktop(desktop);
    wm_outdate_surface(surface);
}


/* Perform the action to close the client */
void ccmd_client_close(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* ICCCM §4.2.8: send a 'WM_DELETE_WINDOW' 'ClientMessage' when the
     * client advertises support in 'WM_PROTOCOLS'; fall back to
     * 'xcb_destroy_window' only when it does not */
    if (client->has_wm_delete_window && client->ewmh != NULL) {
        xcb_client_message_event_t ev;

        memset(&ev, 0, sizeof(ev));
        ev.response_type = XCB_CLIENT_MESSAGE;
        ev.format = 32;
        ev.window = client->window;
        ev.type = client->ewmh->WM_PROTOCOLS;
        ev.data.data32[0] = client->wm_delete_atom;
        ev.data.data32[1] = XCB_CURRENT_TIME;
        xcb_send_event(client->connection, 0, client->window,
                XCB_EVENT_MASK_NO_EVENT, (const char *) &ev);
    } else {
        /* Client does not support 'WM_DELETE_WINDOW'; destroy directly */
        xcb_destroy_window(client->connection, client->window);
    }
}


/* Forcibly kill the client's X connection */
void ccmd_client_kill(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* Unlike 'ccmd_client_close' (a request to destroy a single window
     * resource), 'xcb_kill_client' terminates the owning client's
     * ENTIRE connection to the X server.  Meant as a last resort for
     * unresponsive clients that ignore a normal close request. */
    xcb_kill_client(client->connection, client->window);

    /* Enough for the common case: losing its own X connection is
     * normally fatal to whatever toolkit the client is built on, so
     * the process exits on its own shortly after.  A genuinely
     * unresponsive client, stuck in some loop that never processes
     * its own X connection at all, never notices that loss and keeps
     * running regardless; 'cctl_kill_register' watches for exactly
     * that and sends a real 'SIGKILL' if it is still alive once its
     * own bounded window elapses.  See cctl/kill.h's own doc comment
     * for the full reasoning. */
    cctl_kill_register(client->process.pid);
}


/* Restore a client to its normal state */
void ccmd_client_restore(client_td *client)
{
    xcb_window_t target;
    xcb_atom_t icon_geom_atom;
    bool was_iconified;

    if (client == NULL) {
        return;
    }

    /* Fullscreen clients must be un-fullscreened first so the
     * decoration and EWMH atom are cleaned up properly */
    if (client_is_fullscreen(client)) {
        ccmd_client_unfullscreen(client);
        return;
    }

    /* Remember whether we are restoring from an iconified state so the
     * window can be raised and focused afterwards */
    was_iconified = client_is_iconified(client);

    target = ccmd_target_win(client);
    client_geometry_restore(client);

    if (client->icon_window != 0) {
        xcb_destroy_window(client->connection, client->icon_window);
        client->icon_window = 0;
        client->is_icon_mapped = false;
    }
    if (client->titlebar != 0) {
        xcb_map_window(client->connection, client->titlebar);
    }
    xcb_map_window(client->connection, target);
    if (target != client->window) {
        xcb_map_window(client->connection, client->window);
    }

    client_unhide(client);
    client->properties.state = CLIENT_STATE_NORMAL;

    ccmd_set_wm_state(client, CCMD_WM_STATE_NORMAL, XCB_NONE);

    /* EWMH §5.9: remove icon geometry hint when restoring to normal */
    icon_geom_atom = ccmd_intern_atom(client->connection,
            "_NET_WM_ICON_GEOMETRY");
    xcb_delete_property(client->connection, client->window,
            icon_geom_atom);

    ccmd_rem_states(client, 3,
            "_NET_WM_STATE_HIDDEN",
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");

    /* Re-enter whichever state this client was in right before it was
     * iconified (see 'pre_iconify_state''s own comment in client.h and
     * where it is captured in 'ccmd_client_iconify'), rather than
     * always settling for plain normal.  Each of these re-computes
     * its own geometry fresh against the current workarea/monitor
     * rather than replaying a stale saved one, since the screen
     * layout may have changed while this client sat iconified. */
    if (was_iconified) {
        uint16_t pre_iconify_state = client->properties.pre_iconify_state;

        client->properties.pre_iconify_state =
            (uint16_t) CLIENT_STATE_NORMAL;

        switch (pre_iconify_state) {
            case CLIENT_STATE_MAXIMIZED:
                ccmd_client_maximize(client);
                break;
            case CLIENT_STATE_MAXIMIZED_HORZ:
                ccmd_client_maximize_horz(client);
                break;
            case CLIENT_STATE_MAXIMIZED_VERT:
                ccmd_client_maximize_vert(client);
                break;
            case CLIENT_STATE_FULLSCREEN:
                ccmd_client_fullscreen(client);
                break;
            default:
                break;
        }
    }

    /* When restoring from an icon, raise the client to the top of the
     * desktop stacking order and give it real input focus so that
     * keyboard shortcuts and other window manager operations target
     * this window immediately, rather than whichever window was
     * previously active */
    if (was_iconified && client_is_focusable(client)) {
        desktop_td *const desktop = wm_get_client_desktop(client);
        if (desktop != NULL) {
            desktop->client_active_id = client->id;
            desktop->focus_dirty = true;
            (void) desktop_action_client_send_front(desktop, client);
            desktop->is_outdated = true;
        }
        ccmd_client_focus(client);
    }

    wm_request_client_redraw(client);
}


/* Focus a client */
void ccmd_client_focus(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* A client receiving real input focus has, by definition, gotten
     * the user's attention it was asking for: clear any pending
     * urgency hint here, at the one place every real focus-granting
     * path (a plain click via 'focus_apply', restoring an iconified
     * client, focus recovery when the previously active client
     * closes, and the rest) already converges on, rather than at
     * each of those call sites individually. */
    if (client_is_urgent(client)) {
        ccmd_client_unurge(client);
    }

    /* ICCCM §4.2.7: only call 'SetInputFocus' when the client's input
     * model accepts it ('WM_HINTS' input field, default 'true').
     * Clients that set 'input=false' rely solely on the 'WM_TAKE_FOCUS'
     * message to direct keyboard focus to themselves. */
    if (client->wm_input_hint) {
        xcb_set_input_focus(client->connection, XCB_INPUT_FOCUS_PARENT,
                            client->window, XCB_CURRENT_TIME);
    }

    /* ICCCM §4.2.7: send 'WM_TAKE_FOCUS' 'ClientMessage' when the
     * client has registered that protocol.  This covers both the
     * Locally Active and Globally Active input models. */
    if (client->has_wm_take_focus && client->ewmh != NULL) {
        xcb_client_message_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.response_type = XCB_CLIENT_MESSAGE;
        ev.format = 32;
        ev.window = client->window;
        ev.type = client->ewmh->WM_PROTOCOLS;
        ev.data.data32[0] = client->wm_take_focus_atom;
        ev.data.data32[1] = XCB_CURRENT_TIME;
        xcb_send_event(client->connection, 0, client->window,
                XCB_EVENT_MASK_NO_EVENT, (const char *) &ev);
    }

    /* EWMH: advertise keyboard focus via '_NET_WM_STATE_FOCUSED' */
    ccmd_add_states(client, 1, "_NET_WM_STATE_FOCUSED");

    xcb_map_window(client->connection, client->window);
    /* 'client_border_apply' ('client.h') preserves this same condition
     * (undecorated-or-frameless, never fullscreen) internally, and
     * additionally honors 'border_override' for a client that themes
     * its own border independently of 'theme->window.active/inactive'
     * (the scratchpad, 'scratchpad.c', is the only one that does so
     * today) unconditionally applying the theme's own real border width
     * here on every single focus change (this function runs on every
     * click, via 'focus_apply') used to undo the zero width
     * 'ccmd_client_fullscreen' ('cmds/state.c') had already set,
     * putting a real, visible border back on an undecorated fullscreen
     * client's own window; confirmed directly from runtime diagnostics.
     *
     * An undecorated client (e.g., 'mpv', which requests no decoration
     * of its own from the very start, so 'client_is_decorated' is
     * already false before it ever goes fullscreen, unlike a client
     * that only loses decoration because it went fullscreen) has no
     * separate frame at all ('client->frame' stays 0 throughout, this
     * branch's own 'hide_decoration' equivalent everywhere else in the
     * project never even applies to it), so this call is the only place
     * actually restoring its border on focus. */
    if ((!client_is_decorated(client) || client->frame == 0) &&
            client->theme != NULL && !client_is_fullscreen(client)) {
        client_border_apply(client, true);
    } else {
        client_theme_layout_resync(client, true);
    }

    if (client->ewmh != NULL) {
        xcb_ewmh_set_active_window(client->ewmh,
                (int) client->screen_id,
                client->window);
    }
}


/* Unfocus the client */
void ccmd_client_unfocus(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* EWMH: clear '_NET_WM_STATE_FOCUSED' when the window loses focus */
    ccmd_rem_states(client, 1, "_NET_WM_STATE_FOCUSED");

    client_unfocus(client);

    /* Actually redirect the X server's real input focus away from this
     * client, not just the window manager's own bookkeeping of which
     * client looks focused.  Without this, a client that keeps
     * 'WM_HINTS.input=true' (the default) still receives every
     * 'KeyPress'/'KeyRelease' after being visually unfocused (e.g., by
     * clicking the empty desktop), since nothing ever told the X
     * server to stop delivering keyboard events to its window.
     * A caller that is unfocusing this client only to immediately
     * focus another one (see 'focus_apply') harmlessly overrides this
     * a moment later via that client's own 'SetInputFocus' call, same
     * as the existing pattern in 'ccmd_client_close' below. */
    if (client->connection != NULL) {
        xcb_set_input_focus(client->connection,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_CURRENT_TIME);
    }

    if ((!client_is_decorated(client) || client->frame == 0) &&
            client->theme != NULL && !client_is_fullscreen(client)) {
        /* Same reasoning as the matching block in 'ccmd_client_focus'
         * just above: skipped for a fullscreen client so a losing-
         * focus repaint cannot put a real border back on an
         * undecorated fullscreen client's own window either;
         * 'client_border_apply' (client.h) additionally honors
         * 'border_override' the same way that one does. */
        client_border_apply(client, false);
    } else {
        client_theme_layout_resync(client, false);
    }

    if (client->ewmh != NULL) {
        xcb_ewmh_set_active_window(client->ewmh,
                (int) client->screen_id,
                XCB_NONE);
    }
}
