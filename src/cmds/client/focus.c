/**
 * @file cmds/client/focus.c
 *
 * @brief Focus-granting, focus-fallback, close, kill, and restore
 *        actions over clients
 *
 * One of the files @c cmds/client/ is made of; kept as one contiguous
 * block (matching the order these already had in that file) rather than
 * separated further, since @a ccmd_client_close, @a ccmd_client_kill
 * and @a ccmd_client_restore all lead into the same focus-fallback
 * mechanism that @a ccmd_client_focus_fallback and
 * @a client_focus_fallback provide right above them.
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
#include <stdlib.h>     /* NULL, free, malloc */
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
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>

/* Render includes */
#include <render/outdate.h>

/* Policy includes */
#include <policy/focus.h>

/* Control includes */
#include <cctl/kill.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <ipc.h>
#include <lookup.h>
#include <systray.h>
#include <wm.h>

/* Commands includes */
#include <cmds/client/ewmh.h>
#include <cmds/client/flags.h>
#include <cmds/client/maximize.h>
#include <cmds/client/screen.h>
#include <cmds/client/state.h>
#include <cmds/client/transient.h>
#include <cmds/client/visibility.h>

/* Local includes */
#include <cmds/client/focus.h>


/**
 * @brief What a fallback search needs beyond the candidate itself
 *
 * Handed through @a focus_order_best's opaque pointer rather than
 * kept at file scope, so that a search carries its state and two
 * of them could never read each other's.
 */
struct s_fallback_ctx_s {
    /** Client that must never be chosen, or @c NULL for none */
    const client_td *exclude;
    /** Group leader the same-application pass insists on */
    xcb_window_t leader;
};


/**
 * @brief Whether @p candidate qualifies as a focus-fallback target
 *
 * Shared by both passes @a client_focus_fallback itself makes over
 * @p desktop's stacking list: mapped and visible (not hidden or
 * iconified; shaded is fine, @a ccmd_client_focus below already targets
 * a shaded client's frame instead of its unmapped content), able to
 * take real focus by window type and by its declared ICCCM input model,
 * not explicitly opted out via @a client_has_no_focus_fallback, and not
 * skipping the taskbar unless it is modal, urgent, or a dialog (which
 * need the user's attention regardless of that flag).
 *
 * @param candidate Client being considered as a fallback target
 * @param data      Pointer to the @c s_fallback_ctx_s this search
 *                  carries, naming the client that must never be chosen
 *
 * @return @c true if @p candidate is a valid fallback target
 *
 * @note Complexity: @e O(1)
 */
static bool s_client_focus_fallback_valid(const client_td *candidate,
        void *data)
{
    const struct s_fallback_ctx_s *const ctx = data;
    const client_td *const exclude =
        (ctx != NULL) ? ctx->exclude : NULL;

    return candidate != NULL && candidate != exclude &&
        !(candidate->properties.flags & CLIENT_FLAG_HIDDEN) &&
        !client_is_iconified(candidate) &&
        (candidate->properties.flags & CLIENT_FLAG_FOCUSABLE) &&
        client_accepts_input_focus(candidate) &&
        !client_has_no_focus_fallback(candidate) &&
        (!(candidate->properties.flags & CLIENT_FLAG_SKIP_TASKBAR) ||
         client_is_modal(candidate) ||
         client_is_urgent(candidate) ||
         candidate->properties.type == (uint16_t) CLIENT_TYPE_DIALOG);
}


/**
 * @brief Whether a candidate is a valid fallback of the same
 *        application
 *
 * @param candidate Client being considered
 * @param data      Pointer to the @c s_fallback_ctx_s this search
 *                  carries
 *
 * @return @c true if @p candidate is valid and shares the leader that
 *         context names
 *
 * @note Complexity: @e O(1)
 */
static bool s_focus_fallback_valid_same_group(const client_td *candidate,
        void *data)
{
    const struct s_fallback_ctx_s *const ctx = data;

    return ctx != NULL &&
        s_client_focus_fallback_valid(candidate, data) &&
        client_group_leader(candidate) == ctx->leader;
}


/**
 * @brief Restore exactly this one client to its normal state, ignoring
 *        any transient family it may belong to
 *
 * Holds the single-client half of @a ccmd_client_restore, so that
 * function can redirect to, and cascade across, a transient family (see
 * its comment) while still sharing this single client's worth of
 * state-restoration logic with the top-level, family-unaware call sites
 * that only ever operate on one already- resolved client and have no
 * family to cascade to in the first place.
 *
 * @param client Client to restore; must be non-null
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_restore_one(client_td *client)
{
    xcb_window_t target;
    xcb_atom_t icon_geom_atom;
    bool is_on_screen;

    /* Restoring means two different things depending on where the
     * client is, and the iconified case has to be settled first.
     * A client sitting as an icon is restored by bringing it back,
     * whatever geometry state it may also hold; only a client already
     * on the desktop is restored by leaving that geometry state.
     *
     * Testing full screen ahead of this, as this function once did,
     * left an iconified full screen client merely losing its full
     * screen bit and staying an icon, since that bit now survives being
     * iconified where before it did not. */
    if (!client_is_iconified(client)) {
        /* Outermost state first, so one restore undoes one thing: full
         * screen over a maximized window comes back maximized, and
         * a second restore takes that away in turn. */
        if (client_is_fullscreen(client)) {
            ccmd_client_unfullscreen(client);
        } else if (client_is_maximized(client)) {
            ccmd_client_maximize(client);
        } else if (client_is_maximized_horz(client)) {
            ccmd_client_maximize_horz(client);
        } else if (client_is_maximized_vert(client)) {
            ccmd_client_maximize_vert(client);
        }
        return;
    }

    target = ccmd_target_win(client);
    client_geometry_restore(client);

    if (client->icon_window != 0) {
        xcb_window_destroy(client->icon_window);
        client->icon_window = 0;
        client->is_icon_mapped = false;
    }
    client_unhide(client);

    /* Only the iconified bit is cleared: whatever maximization or full
     * screen the window held before being iconified it holds still,
     * never having asked for that to be forgotten. */
    client->properties.state &= (uint16_t) ~CLIENT_STATE_ICONIFIED;

    ccmd_set_wm_state(client, CCMD_WM_STATE_NORMAL, XCB_NONE);

    /* EWMH §5.9: remove icon geometry hint when restoring to normal */
    icon_geom_atom = ccmd_intern_atom(xcb_connection_get(),
            "_NET_WM_ICON_GEOMETRY");
    xcb_delete_property(xcb_connection_get(), client->window,
            icon_geom_atom);

    ccmd_client_sync_states(client);

    /* Mapped only if its desktop is the one shown (or it is pinned):
     * a client of another desktop, restored from a pager or over IPC,
     * leaves the iconified state but stays unmapped until its desktop
     * is shown, instead of appearing on this one */
    is_on_screen = ccmd_client_is_on_screen(client);
    if (is_on_screen) {
        if (client->titlebar != 0) {
            xcb_window_show(client->titlebar);
        }
        xcb_window_show(target);
        if (target != client->window) {
            xcb_window_show(client->window);
        }
    }

    /* Put the geometry every state bit still standing calls for back on
     * the window.  The bits themselves survived being iconified, so
     * nothing had to be remembered anywhere; what is left to do is
     * re-apply the geometry each one implies, computed fresh against
     * the workarea as it is now, the screen layout having possibly
     * changed while the client sat as an icon.
     *
     * Maximization is re-applied through 'ccmd_client_refill_maximized'
     * rather than through the maximize command, since that one toggles
     * and would have to have its bits cleared first, which would in
     * turn let its 'client_geometry_save' fire on a window whose
     * current geometry is the maximized one, burying the true original.
     *
     * Full screen does go through its command, the window having
     * genuinely left that state when it was iconified (see
     * 'ccmd_client_iconify'), so there is real work to redo.  Only its
     * bit is cleared first, so that the command re-enters rather than
     * toggling out; the maximize bits stay standing throughout, which
     * is exactly what makes that command's 'client_geometry_save' skip
     * a maximized window and leave 'layout.geometry.old' alone.
     *
     * Maximization is re-applied first so that a client holding both
     * ends up full screen over a maximized window, as it went away. */
    if (client_is_maximized_any(client)) {
        ccmd_client_refill_maximized(client);
    }
    if (client_is_fullscreen(client)) {
        client->properties.state &=
            (uint16_t) ~CLIENT_STATE_FULLSCREEN;
        ccmd_client_fullscreen(client);
    }

    /* A window arriving back from an icon becomes the one in use, so
     * that keyboard shortcuts and other window manager operations
     * target it immediately rather than whichever window was active
     * while it was away; one left unmapped on a desktop not shown has
     * nothing on screen to take the keyboard */
    if (is_on_screen) {
        ccmd_client_make_active(client);
    }

    wm_request_client_redraw(client);
}


/* Transfer focus away from a client that is leaving the current visible
 * focus chain; see cmds/client/internal.h for the full comment */
void ccmd_client_focus_fallback(client_td *client)
{
    stage_td *stage;
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    /* Resolved to the desktop this client actually lives on, not to
     * whichever one happens to be showing.  A client can lose focus
     * while the user is looking elsewhere, the scratchpad being hidden
     * from another desktop is the ordinary case, and asking the current
     * desktop then compares this client's id against a different
     * desktop's active client, finds no match, and hands focus to
     * nobody.  The desktop it left is then holding an active id that
     * names a client no longer eligible, which is only noticed on
     * switching back to it.
     *
     * This is what the destroy path in 'handler/map.c' already does,
     * and for the same reason. */
    stage = wm_get_stage_by_id(client->screen_id);
    desktop = wm_get_client_desktop(client);
    if (stage == NULL || desktop == NULL ||
            desktop->client_active_id != client->id) {
        return;
    }

    client_focus_fallback(desktop, stage, client);
}


/* Transfer input focus away from a client leaving the current visible
 * focus chain to the most recently used other visible, focusable client
 * on the same desktop, or to 'PointerRoot' if none qualifies; see the
 * full criteria in the header */
void client_focus_fallback(desktop_td *desktop, stage_td *stage,
        client_td *exclude)
{
    client_td *next_focus = NULL;
    xcb_window_t exclude_leader;
    struct s_fallback_ctx_s ctx;
    const config_td *const config = wm_get_config();

    if (desktop == NULL) {
        return;
    }

    desktop->client_active_id = 0;
    desktop->is_focus_dirty = true;

    /* A window left behind by 'exclude' from the same application
     * (sharing its 'WM_CLIENT_LEADER', ICCCM 4.1.2.5, or failing that
     * its 'WM_HINTS' window group) is a more natural fallback than an
     * unrelated one equally close in MRU order, the same reasoning
     * 'place_window_apply' (policy/placement/window.c) already applies
     * when placing a new sibling window near its group;
     * mirrors how Openbox's 'focus_valid_target' (focus.c) weighs group
     * membership when picking a focus target.  Tried first and only as
     * a preference, not a requirement: falls through to the plain MRU
     * search below, unchanged from before, whenever no such sibling
     * qualifies.  Only when the user asked for it with
     * 'windows.focus.group-fallback': an application such as a file
     * manager keeps all its windows in one group, and focus jumping to
     * another of them, rather than to the window last used, is not
     * what anyone expects without knowing why. */
    exclude_leader = (exclude != NULL && config != NULL &&
            config->base.windows.focus.use_group_fallback)
        ? client_group_leader(exclude) : XCB_WINDOW_NONE;

    ctx.exclude = exclude;
    ctx.leader = exclude_leader;

    if (exclude_leader != XCB_WINDOW_NONE) {
        next_focus = focus_order_best(desktop,
                s_focus_fallback_valid_same_group, &ctx);
    }

    if (next_focus == NULL) {
        next_focus = focus_order_best(desktop,
                s_client_focus_fallback_valid, &ctx);
    }

    /* A desktop not on screen has no window that can take the focus, so
     * nothing below that gives or takes it may run for it: a focus
     * request for an unmapped window only fails, and relinquishing to
     * 'PointerRoot' takes the keyboard from the window the user is
     * typing into on the desktop that is shown.  Only the record is
     * kept, and 'stage_client_show_all' gives real focus again from the
     * focus order once this desktop is shown.  A pinned 'exclude' is on
     * screen whatever desktop holds it, so it takes the path below */
    if (stage != NULL && stage->desktop_cur != desktop->id &&
            !(exclude != NULL && client_is_pinned(exclude))) {
        if (next_focus != NULL) {
            desktop->client_active_id = next_focus->id;
        }
        if (exclude != NULL) {
            ccmd_client_unfocus_publish(exclude);
        }
        wm_outdate_desktop(desktop);
        wm_outdate_stage(stage);
        return;
    }

    if (next_focus != NULL) {
        /* Delegates to the same central path every other real
         * focus-granting call site already uses ('focus_apply',
         * policy/focus.c), rather than reimplementing its raise,
         * viewport-centering, and family-bringing steps by hand:
         * a client taking over after the one that held focus closed,
         * hid, or otherwise stopped qualifying is exactly the "now
         * being made the active one" case @a ccmd_client_make_active
         * already raises unconditionally for, and the same reasoning
         * applies here.  Raised unconditionally for that same reason,
         * regardless of the user's own raise-on-focus setting. */
        focus_apply(wm_get_stages(), stage, desktop, next_focus,
                true, wm_get_config());
        return;
    } else if (exclude != NULL) {
        /* No replacement candidate qualifies, so 'exclude' itself is
         * what actually still looks focused to the outside world:
         * 'ccmd_client_unfocus' clears its own '_NET_WM_STATE_FOCUSED'
         * mark and the root's '_NET_ACTIVE_WINDOW', on top of
         * redirecting real input focus to 'PointerRoot', rather than
         * leaving both stale the way relinquishing only the X input
         * focus below would. */
        ccmd_client_unfocus(exclude);
    } else if (xcb_connection_get() != NULL) {
        /* Same timestamp every other focus call in this file uses, so
         * that relinquishing here cannot record a last focus change
         * later than a subsequent request carries and have the server
         * discard that request */
        const uint32_t relinquish_time =
            (client_last_user_time() != 0u)
                ? client_last_user_time() : (uint32_t) XCB_CURRENT_TIME;

        xcb_set_input_focus(xcb_connection_get(),
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                relinquish_time);
    }

    wm_outdate_desktop(desktop);
    wm_outdate_stage(stage);
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
    if (client->hints_icccm.protocols.has_delete &&
            xcb_ewmh_connection_get() != NULL) {
        xcb_client_message_event_t ev;

        memset(&ev, 0, sizeof(ev));
        ev.response_type = XCB_CLIENT_MESSAGE;
        ev.format = 32;
        ev.window = client->window;
        ev.type = xcb_ewmh_connection_get()->WM_PROTOCOLS;
        ev.data.data32[0] = client->hints_icccm.protocols.delete_atom;
        ev.data.data32[1] = XCB_CURRENT_TIME;
        xcb_send_event(xcb_connection_get(), 0, client->window,
                XCB_EVENT_MASK_NO_EVENT, (const char *) &ev);
    } else {
        /* The client does not support 'WM_DELETE_WINDOW', so destroy it
         * directly */
        xcb_window_destroy(client->window);
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
    xcb_kill_client(xcb_connection_get(), client->window);

    /* Enough for the common case: losing its X connection is normally
     * fatal to whatever toolkit the client is built on, so the process
     * exits on its own shortly after.  A genuinely unresponsive client,
     * stuck in some loop that never processes its X connection at all,
     * never notices that loss and keeps running regardless;
     * 'cctl_kill_register' watches for exactly that and sends a real
     * 'SIGKILL' if it is still alive once its own bounded window
     * elapses.  See cctl/kill.h's comment for the full reasoning.
     *
     * Skipped for a client whose 'WM_CLIENT_MACHINE' names another
     * host, or names none at all: its 'process.pid' is meaningful only
     * on the host that set it, and this window manager has no business
     * sending a real signal to whatever local process happens to reuse
     * that same number.  'xcb_kill_client' above, being a request the
     * X server itself routes to the right place,
     * is already the whole answer for a remote client. */
    if (client->process.pid_is_local) {
        cctl_kill_register(client->process.pid);
    }
}


/* Restore the client to its normal state, bringing its whole transient
 * family back with it */
void ccmd_client_restore(client_td *client)
{
    client_td *top;
    size_t count;
    client_td **siblings;

    if (client == NULL) {
        return;
    }

    top = ccmd_client_transient_top_parent(client);
    if (top == NULL) {
        return;
    }

    /* Every other family member still iconified is restored before the
     * top parent's restore below, not after.  That restore's own
     * focus-granting step (inside 's_ccmd_client_restore_one', gated on
     * 'client_is_focusable') redirects through
     * 'ccmd_client_focus_target' to whichever transient dialog should
     * actually end up focused, as 'ccmd_client_focus''s comment
     * describes, which only finds that dialog if it is already mapped
     * by the time this reaches that step. */
    siblings = ccmd_client_transient_family_snapshot_anywhere(top,
            &count);
    if (siblings != NULL) {
        for (size_t i = 0; i < count; i++) {
            if (client_is_locked(siblings[i])) {
                continue;
            }

            if (client_is_iconified(siblings[i])) {
                s_ccmd_client_restore_one(siblings[i]);
            } else if (client_is_transient(siblings[i]) &&
                    client_is_hidden(siblings[i])) {
                /* A transient sibling is hidden, not iconified, when
                 * the family goes down together (see
                 * 's_ccmd_client_iconify_visit''s own comment,
                 * cmds/client/visibility.c), so restoring the family
                 * has to look for this too, not only for a sibling
                 * genuinely iconified: left unhidden, a dialog that was
                 * open when its application was minimized stayed
                 * invisible, unreachable, yet still holding on to
                 * whatever it held before, once the application it
                 * belonged to was restored. */
                ccmd_client_unhide(siblings[i]);
            }
        }

        free(siblings);
    }

    s_ccmd_client_restore_one(top);
}


/* Show, everywhere outside the X server's own focus, that a client
 * now holds input focus */
void ccmd_client_focus_publish(client_td *client)
{
    xcb_ewmh_connection_t *const ewmh = xcb_ewmh_connection_get();

    if (client == NULL) {
        return;
    }

    /* A client receiving real input focus has, by definition, gotten
     * the user's attention it was asking for.  Clear any pending
     * urgency hint here, at the one place every real focus-granting
     * path (a plain click via 'focus_apply', restoring an iconified
     * client, focus recovery when the previously active client closes,
     * and the rest) already converges on, rather than at each of those
     * call sites individually; a client that took focus on its own
     * ('focus_adopt', policy/focus.c) got that attention just the
     * same. */
    if (client_is_urgent(client)) {
        ccmd_client_unurge(client);
    }

    /* ICCCM §4.1.8/§2.8: colormap focus follows input focus here, the
     * common policy most window managers implement.  Only the explicit
     * 'WM_COLORMAP_WINDOWS' case is covered, the one
     * 'client_props_refresh_colormap_windows' in 'client/props.c'
     * populates this from, caching each window's colormap attribute
     * there already, so nothing here needs a round trip of its own);
     * a client that omits it but still uses a non-default colormap on
     * its top-level window falls back to whatever is already
     * installed. */
    for (uint32_t i = 0u; i < client->colormap_windows.count; ++i) {
        if (client->colormap_windows.colormap_ids[i] !=
                (xcb_colormap_t) XCB_NONE) {
            xcb_install_colormap(xcb_connection_get(),
                    client->colormap_windows.colormap_ids[i]);
        }
    }

    /* EWMH: advertise keyboard focus via '_NET_WM_STATE_FOCUSED' */
    client_focus_mark(client);
    ccmd_client_sync_states(client);

    /* An undecorated client's border is no longer painted from here:
     * the render pass writes it from 'is_focused', the same state the
     * frame of a decorated one is repainted from, so a focus change is
     * something to record rather than something to draw. */
    if (client_is_decorated(client) && client->frame != 0) {
        client_theme_layout_resync(client, true);
    }

    if (ewmh != NULL) {
        xcb_ewmh_set_active_window(ewmh,
                (int) client->screen_id,
                client->window);
    }
}


/* Focus a client */
void ccmd_client_focus(client_td *client)
{
    const uint32_t focus_time = (client_last_user_time() != 0u)
        ? client_last_user_time() : (uint32_t) XCB_CURRENT_TIME;
    xcb_ewmh_connection_t *const ewmh = xcb_ewmh_connection_get();

    if (client == NULL) {
        return;
    }

    /* Deliberately no 'ccmd_client_bring_family' call here, unlike
     * 'focus_apply' (in 'policy/focus.c'): this function is also
     * reached from purely automatic, internal focus restoration that
     * has nothing to do with someone actually interacting with 'client'
     * right now (foremost 'stage_client_show_all''s "restore
     * whichever client was last active on this desktop" step,
     * 'stage/actions/client.c', which runs on every single desktop
     * switch).  Calling it unconditionally here dragged a transient
     * family across onto whatever desktop merely happened to be
     * switched to, the moment its pinned parent's 'client_active_id'
     * from some earlier, unrelated visit to that desktop was restored,
     * leaving the family effectively "chasing" every desktop the parent
     * had ever been focused on, indistinguishable from actually being
     * pinned even though nothing pinned it.  'focus_apply' itself
     * already covers every genuine, deliberate focus request (a plain
     * click, sloppy focus, and the like) with its separate call, for
     * the same reason it needs its separate redirect to
     * 'ccmd_client_focus_target' right below (see that call's
     * comment). */

    /* Redirect to whichever mapped transient descendant should actually
     * receive focus in this client's place (a "save changes?" prompt
     * still sitting open on top of it, say); see
     * 'ccmd_client_focus_target''s comment for the full reasoning.
     * Applied here, at the one place every real focus- granting path
     * already converges on (the comment just below), so every one of
     * them redirects the same way regardless of which specific window
     * they originally asked for by name. */
    client = ccmd_client_focus_target(client);

    /* ICCCM §4.1.7: 'SetInputFocus' is called only for a client whose
     * input model actually wants it, meaning one whose 'WM_HINTS' input
     * field is true and that does not register 'WM_TAKE_FOCUS'.  That
     * is the Passive model, and it is the only one where the window
     * manager sets the focus itself.
     *
     * A client registering 'WM_TAKE_FOCUS' sets its focus, on receiving
     * the message sent further down, whatever its input field says:
     * with the field false that is the Globally Active model and with
     * it true the Locally Active one, and §4.1.7 describes both as the
     * client doing the setting.  Doing both, as this once did for
     * a Locally Active client, is not merely redundant but actively
     * breaks it: the timestamp is spent here first, and X ignores
     * a 'SetInputFocus' whose time is not later than the last focus
     * change, so the client's call with that same timestamp is
     * discarded.  The frame took the focus and lit its titlebar while
     * the application's focus stayed wherever it had been, which is
     * what cycling with a key binding looked like.
     *
     * Target 'client->window' itself, except while shaded.  Content is
     * unmapped then (that is the entire point of shading), and ICCCM
     * §4.1.7/X11 both require a 'SetInputFocus' target to be viewable,
     * so a shaded client's frame (still mapped, just visually collapsed
     * to its titlebar) stands in for it instead.  Without this,
     * a shaded client could never legitimately hold real input focus at
     * all: 's_client_focus_fallback_valid' (this same file) and
     * 'stage_client_show_all' ('stage/actions/client.c') both
     * relied on simply excluding a shaded client from ever being
     * offered here, over actually making this call safe for one, which
     * left nothing to give a desktop's keyboard focus anywhere valid
     * once its only client was shaded and the desktop was left and
     * returned to. */
    if (client->hints_icccm.hints.accepts_input) {
        xcb_window_t focus_win = (client_is_shaded(client) &&
                client->frame != 0) ? client->frame : client->window;
        xcb_set_input_focus(xcb_connection_get(), XCB_INPUT_FOCUS_PARENT,
                            focus_win, focus_time);
    }

    /* ICCCM §4.2.7: send 'WM_TAKE_FOCUS' 'ClientMessage' when the
     * client has registered that protocol.  This covers both the
     * Locally Active and Globally Active input models, and for those
     * this message is the whole of it: no 'SetInputFocus' was issued
     * above, precisely so that the timestamp below is still unspent
     * when the client answers with one of its own.
     *
     * The timestamp is the real one, never 'CurrentTime', which §4.1.7
     * forbids here in as many words.  The client is to echo this value
     * back in its 'SetInputFocus', and is itself forbidden from using
     * 'CurrentTime' there, so sending it one leaves it with nothing
     * valid to answer with.  A Locally or Globally Active client handed
     * 'CurrentTime' may simply decline to take focus, which looks from
     * the outside like a titlebar that lights up while the keyboard
     * goes elsewhere. */
    if (client->hints_icccm.protocols.has_take_focus &&
            ewmh != NULL) {
        xcb_client_message_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.response_type = XCB_CLIENT_MESSAGE;
        ev.format = 32;
        ev.window = client->window;
        ev.type = ewmh->WM_PROTOCOLS;
        ev.data.data32[0] = client->hints_icccm.protocols.take_focus_atom;
        ev.data.data32[1] = focus_time;
        xcb_send_event(xcb_connection_get(), 0, client->window,
                XCB_EVENT_MASK_NO_EVENT, (const char *) &ev);
    }

    if (!client_is_shaded(client)) {
        xcb_window_show(client->window);
    }

    ccmd_client_focus_publish(client);
}


/* Show, everywhere outside the X server's own focus, that a client no
 * longer holds input focus */
void ccmd_client_unfocus_publish(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* Clear the EWMH '_NET_WM_STATE_FOCUSED' on losing focus */
    client_unfocus_mark(client);
    ccmd_client_sync_states(client);

    client_unfocus(client);

    /* Same as the matching block in 'ccmd_client_focus_publish': an
     * undecorated client's border follows from the render pass, so
     * only a framed one has anything to resync here */
    if (client_is_decorated(client) && client->frame != 0) {
        client_theme_layout_resync(client, false);
    }
}


/* Unfocus the client */
void ccmd_client_unfocus(client_td *client)
{
    if (client == NULL) {
        return;
    }

    ccmd_client_unfocus_publish(client);

    /* Actually redirect the X server's real input focus away from this
     * client, not just the window manager's bookkeeping of which client
     * looks focused.  Without this, a client that keeps
     * 'WM_HINTS.input=true' (the default) still receives every
     * 'KeyPress'/'KeyRelease' after being visually unfocused (e.g., by
     * clicking the empty desktop), since nothing ever told the X server
     * to stop delivering keyboard events to its window.
     *
     * The timestamp is the same one 'ccmd_client_focus' uses, and that
     * matters more than it looks.  A caller unfocusing this client only
     * to focus another a moment later, which is what 'focus_apply' does
     * on every focus change, was once said to override this harmlessly;
     * that was true only while both calls carried 'CurrentTime', which
     * the server replaces with the current time on each one.  With
     * a real timestamp, relinquishing here with 'CurrentTime' records
     * a last focus change later than the one the next call carries, and
     * X ignores a 'SetInputFocus' older than the last focus change: the
     * new window would light its titlebar while the keyboard went
     * nowhere, following the pointer instead. */
    if (xcb_connection_get() != NULL) {
        const uint32_t relinquish_time =
            (client_last_user_time() != 0u)
                ? client_last_user_time() : (uint32_t) XCB_CURRENT_TIME;

        xcb_set_input_focus(xcb_connection_get(),
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                relinquish_time);
    }

    if (xcb_ewmh_connection_get() != NULL) {
        xcb_ewmh_set_active_window(xcb_ewmh_connection_get(),
                (int) client->screen_id,
                XCB_NONE);
    }
}


/* Make a client the active one of its own desktop */
void ccmd_client_make_active(client_td *client)
{
    stage_td *stage;
    desktop_td *desktop;

    if (client == NULL || !client_is_focusable(client)) {
        return;
    }

    stage = wm_get_stage_by_id(client->screen_id);
    desktop = wm_get_client_desktop(client);
    if (stage == NULL || desktop == NULL) {
        ccmd_client_focus(client);
        return;
    }

    /* Delegates to the same central path 'focus_apply'
     * ('policy/focus.c') already gives every other focus-granting call
     * site, rather than reimplementing its unfocus-previous, activate,
     * and raise steps by hand here.  Raised unconditionally ('raise'
     * true regardless of the user's raise-on-focus setting), matching
     * what this function's own raise call did on its own before, since
     * a window arriving back from an icon or otherwise being made the
     * active one is meant to actually be seen */
    focus_apply(wm_get_stages(), stage, desktop, client, true,
            wm_get_config());
}
