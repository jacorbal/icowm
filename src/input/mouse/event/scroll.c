/**
 * @file input/mouse/event/scroll.c
 *
 * @brief Scroll-wheel bindings on the desktop and on a titlebar
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

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Policy includes */
#include <policy/focus.h>

/* Command includes */
#include <cmds/client/focus.h>
#include <cmds/client/maximize.h>
#include <cmds/client/state.h>

/* Render includes */
#include <render/outdate.h>

/* Enact includes */
#include <enact/client.h>
#include <enact/stage.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <stage.h>
#include <wm.h>

/* Input includes */
#include <input/mouse/bind.h>
#include <input/mouse/event.h>

/* Local includes */
#include <input/mouse/internal.h>


/**
 * @brief Scroll north on a client's titlebar.  Maximize it,
 *        only when not already fully maximized
 *
 * Never moves focus: the client stays exactly as interactable, and
 * exactly as focused, either side of the change.
 *
 * @param client  Client whose titlebar the scroll landed on
 * @param desktop Desktop owning @p client; may be null
 * @param stage   Stage owning @p desktop; may be null
 *
 * @note Complexity: @e O(1)
 */
static void s_scroll_titlebar_maximize(client_td *client,
        desktop_td *desktop, stage_td *stage)
{
    if (client_is_maximized(client)) {
        return;
    }
    enact_client_maximize(client);
    if (desktop != NULL) {
        desktop->is_outdated = true;
    }
    if (stage != NULL) {
        stage->is_outdated = true;
    }
}


/**
 * @brief Scroll south on a client's titlebar.  Restore it from
 *        fully maximized, only when it currently is
 *
 * Calls the exact same toggle @a s_scroll_titlebar_maximize does,
 * guarded so it only ever runs when it would actually restore, not
 * maximize.  Never moves focus, for the same reason that one does
 * not either.
 *
 * @param client  Client whose titlebar the scroll landed on
 * @param desktop Desktop owning @p client; may be null
 * @param stage   Stage owning @p desktop; may be null
 *
 * @note Complexity: @e O(1)
 */
static void s_scroll_titlebar_restore(client_td *client,
        desktop_td *desktop, stage_td *stage)
{
    if (!client_is_maximized(client)) {
        return;
    }
    enact_client_maximize(client);
    if (desktop != NULL) {
        desktop->is_outdated = true;
    }
    if (stage != NULL) {
        stage->is_outdated = true;
    }
}


/**
 * @brief Scroll west on a client's titlebar (the exact same gesture
 *        @c DESKTOP_PREV always was); shade it
 *
 * Sends the client to the end of both orders the desktop keeps, the
 * stacking one and the focus one, and lets losing focus follow from
 * being last rather than arranging it separately.  A client that was
 * not the one holding focus keeps whatever had it.
 *
 * @param client  Client whose titlebar the scroll landed on
 * @param desktop Desktop owning @p client; may be null
 * @param stage   Stage owning @p desktop; may be null
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static void s_scroll_titlebar_shade(client_td *client,
        desktop_td *desktop, stage_td *stage)
{
    bool was_active;

    if (client_is_shaded(client)) {
        return;
    }

    was_active = (desktop != NULL &&
            desktop->client_active_id == client->id);

    ccmd_client_shade(client);

    /* Sent to the end of both orders the desktop keeps, and nothing
     * else done about focus.  Shading a window with the wheel is a way
     * of putting it out of the way, so it goes last among the windows
     * on this desktop: last in the stacking order, where a rolled-up
     * title bar would otherwise sit over whatever it was shaded to get
     * at, and last in the focus order.
     *
     * Losing focus then follows from being last rather than being
     * arranged separately, which is what this did before.  It picked
     * the replacement out of the stacking list, and so handed focus to
     * whichever window happened to be drawn highest rather than to the
     * one the user had been working in.
     *
     * Only the wheel does this.  Shading from a key binding or from an
     * EWMH request is a state change and nothing more, which is why
     * neither 'ccmd_client_shade' nor its other callers demote
     * anything. */
    if (desktop != NULL) {
        (void) desktop_action_client_send_back(desktop, client);
        focus_order_to_bottom(desktop, client);
    }

    if (was_active && desktop != NULL && stage != NULL) {
        client_focus_fallback(desktop, stage, client);
    }

    if (desktop != NULL) {
        desktop->is_outdated = true;
    }
    if (stage != NULL) {
        stage->is_outdated = true;
    }
}


/**
 * @brief Scroll east on a client's titlebar (the exact same gesture
 *        @c DESKTOP_NEXT always was); unshade it
 *
 * Regains focus only when @p client was the one actually holding it
 * before being shaded; unshading an already-inactive client leaves
 * whichever other client currently has real focus untouched.
 *
 * @param client  Client whose titlebar the scroll landed on
 * @param desktop Desktop owning @p client; may be null
 * @param stage   Stage owning @p desktop; may be null
 * @param stages  Full stage list, passed through to @c focus_apply
 * @param config  Active configuration, passed through to
 *                 @c focus_apply
 *
 * @note Complexity: @e O(1)
 */
static void s_scroll_titlebar_unshade(client_td *client,
        desktop_td *desktop, stage_td *stage, list_td *stages,
        const config_td *config)
{
    bool was_active;

    if (!client_is_shaded(client)) {
        return;
    }

    was_active = (desktop != NULL &&
            desktop->client_active_id == client->id);

    ccmd_client_unshade(client);

    if (was_active && stage != NULL && desktop != NULL) {
        focus_apply(stages, stage, desktop, client, false, config);
        im_sync_pinned_active(stage, desktop, client);
    }

    if (desktop != NULL) {
        desktop->is_outdated = true;
    }
    if (stage != NULL) {
        stage->is_outdated = true;
    }
}


/**
 * @brief Whether a scroll event's root coordinates land on
 *        @p client's titlebar
 *
 * Checks both the child-window identity and a Y-range, to handle
 * frame sync-grab events where @p event's child may be the
 * content window rather than the titlebar itself.
 *
 * @param client Client to check against
 * @param event  Incoming button-press event
 *
 * @return @c true if the scroll landed on @p client's titlebar
 *
 * @note Complexity: @e O(1)
 */
static bool s_scroll_on_titlebar(const client_td *client,
        const xcb_button_press_event_t *event)
{
    int32_t bw;
    int32_t fy;
    int32_t ty0;
    int32_t ty1;
    int32_t ry;

    if (client->titlebar == 0) {
        return false;
    }

    bw = client->layout.frame_extents.left;
    fy = client->layout.geometry.cur.pos.y;
    ty0 = fy + bw;
    ty1 = fy + client->layout.frame_extents.top;
    ry = (int32_t) event->root_y;

    return event->child == client->titlebar ||
        (ry >= ty0 && ry < ty1);
}


/* Handle a scroll-wheel event matched to a 'DESKTOP_NORTH' / '_SOUTH' /
 * '_EAST' / '_WEST' binding */
void im_press_scroll_binding(xcb_connection_t *connection,
        list_td *stages, xcb_button_press_event_t *event,
        client_td *client, desktop_td *desktop,
        enum wm_mousebind_type_e type, const config_td *config)
{
    stage_td *const stage =
        lookup_stage_for_root(stages, event->root);

    if (client != NULL) {
        if (s_scroll_on_titlebar(client, event)) {
            switch (type) {
            case MOUSEBIND_DESKTOP_NORTH:
                s_scroll_titlebar_maximize(client, desktop, stage);
                break;
            case MOUSEBIND_DESKTOP_SOUTH:
                s_scroll_titlebar_restore(client, desktop, stage);
                break;
            case MOUSEBIND_DESKTOP_WEST:
                s_scroll_titlebar_shade(client, desktop, stage);
                break;
            case MOUSEBIND_DESKTOP_EAST:
                s_scroll_titlebar_unshade(client, desktop, stage,
                        stages, config);
                break;
            case MOUSEBIND_NONE:
            case MOUSEBIND_MOVE:
            case MOUSEBIND_RESIZE:
            case MOUSEBIND_LOWER:
                /* Never actually reached, listed here anyway so this
                 * switch stays exhaustive under '-Wswitch-enum'; see
                 * the equivalent list further down in this same
                 * function for the fuller reasoning. */
                break;
            }

            im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                    event->time);
            return;
        }

        /* Scroll over client content area: replay so the application
         * receives the scroll event */
        im_allow_and_flush(connection, XCB_ALLOW_REPLAY_POINTER,
                event->time);
        return;
    }

    /* No client under pointer: switch desktop right away */
    if (stage != NULL) {
        switch (type) {
        case MOUSEBIND_DESKTOP_NORTH:
            enact_stage_desktop_switch_north(stage);
            break;
        case MOUSEBIND_DESKTOP_SOUTH:
            enact_stage_desktop_switch_south(stage);
            break;
        case MOUSEBIND_DESKTOP_EAST:
            enact_stage_desktop_switch_east(stage);
            break;
        case MOUSEBIND_DESKTOP_WEST:
            enact_stage_desktop_switch_west(stage);
            break;
        case MOUSEBIND_NONE:
        case MOUSEBIND_MOVE:
        case MOUSEBIND_RESIZE:
        case MOUSEBIND_LOWER:
            /* Never actually reached: this whole function is only ever
             * called for one of the four desktop-scroll types
             * above, gated by its caller.
             * See 'type == MOUSEBIND_DESKTOP_NORTH || ...' just before
             * the call to 'im_press_scroll_binding'.
             *
             * Listed here anyway, one per value rather than a catch-all
             * 'default', purely so this switch stays exhaustive under
             * '-Wswitch-enum' the same way every other switch on
             * a keybind/mousebind type in this project already does. */
            break;
        }
    }

    im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
            event->time);
}
