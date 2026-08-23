/**
 * @file rules/apply.c
 *
 * @brief Applies a merged rule action descriptor to a client
 *
 * Split out of @c rules.c: @c rules_apply and its per-field helpers
 * form one coherent responsibility (mutating a client to match a rule's
 * merged action set) that is independent from loading and parsing the
 * rules table itself, and shares no file-scope state with @c rules.c
 * beyond the types already exposed in @c rules/internal.h.
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
#include <string.h>     /* memset, NULL */

/* XCB includes */
#include <xcb/xcb.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Command includes */
#include <cmds/client/flags.h>
#include <cmds/client/move.h>
#include <cmds/client/state.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <enact.h>
#include <ipc.h>
#include <logger.h>
#include <policy/focus.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <rules.h>
#include <rules/internal.h>


/**
 * @brief Broadcast an IPC event carrying one client's own identifying
 *        fields
 *
 * Mirrors @a enact_broadcast_client_event's own field shape (@c
 * enact/internal.h), which this file cannot reach directly: that
 * header is deliberately private to @c enact/ itself (see its own
 * doc comment for why), so this is its own small, local copy of the
 * same fields instead.
 *
 * @param client Client the event is about
 * @param type   IPC event bitmask (a single @c IPC_EVENT_* value; see
 *               ipc.h)
 *
 * @note No-op if @p client is @c NULL
 * @note Complexity: @e O(1)
 */
static void s_rules_broadcast_client_event(client_td *client,
        uint32_t type)
{
    cJSON *fields;

    if (client == NULL) {
        return;
    }

    fields = cJSON_CreateObject();
    if (fields != NULL) {
        cJSON_AddNumberToObject(fields, "client_id",
                (double) client->id);
        cJSON_AddNumberToObject(fields, "desktop_id",
                (double) client->desktop_id);
        cJSON_AddNumberToObject(fields, "surface_id",
                (double) client->screen_id);
    }
    ipc_broadcast_event(type, fields);
}


/**
 * @brief Move a client to the desktop specified by a rule
 *
 * Resolves the target desktop identified by @p apply->desktop, then
 * delegates the actual move to @a enact_desktop_client_send, the
 * same shared primitive the "Send to desktop" menu and the move-to-
 * desktop keybind both already use, so a rule-driven move gets the
 * exact same family-wide cascade, visibility handling, focus
 * fallback, EWMH publish, and IPC broadcast every other trigger of
 * this same action already gets, rather than a second, narrower
 * reimplementation of its own.  If the target desktop does not
 * exist, falls back to 0th-desktop (logging a warning); if it is the
 * same as the current one, or 0th-desktop does not exist either, the
 * function returns without doing anything.
 *
 * @param client     Client to move
 * @param surface    Surface on which the target desktop lives
 * @param desktop_io In/out pointer to the current desktop; updated to
 *                   point at the target on success
 * @param apply      Action descriptor; only evaluated when
 *                   @p apply->has_desktop is @c true
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the client's own top parent's own desktop (see @a enact_
 *       desktop_client_send's own doc comment)
 */
static void s_rules_apply_desktop(client_td *client,
        surface_td *surface, desktop_td **desktop_io,
        const struct rules_apply_s *apply)
{
    const desktop_td *cur;
    desktop_td *target;

    if (!apply->has_desktop || surface == NULL || desktop_io == NULL ||
            *desktop_io == NULL) {
        return;
    }

    cur = *desktop_io;
    target = surface_desktop_get(surface, apply->desktop);
    if (target == NULL) {
        LOGGER_WARNING("Rule targets desktop %u, which does not" \
                " exist on surface %u; falling back to desktop 0",
                apply->desktop, surface->id);
        target = surface_desktop_get(surface, 0u);
        if (target == NULL) {
            return;
        }
    }
    if (target == cur) {
        return;
    }

    enact_desktop_client_send(cur, client, target);
    *desktop_io = target;
}


/**
 * @brief Apply the stacking layer rule to a client
 *
 * Calls the appropriate layer command based on @p apply->layer.
 * The function is a no-op when @p apply->has_layer is @c false.
 *
 * @param client Client whose stacking layer is to be changed
 * @param apply  Action descriptor
 *
 * @note Complexity: @e O(1)
 */
static void s_rules_apply_layer(client_td *client,
        const struct rules_apply_s *apply)
{
    if (!apply->has_layer) {
        return;
    }

    if (apply->layer == (uint16_t) CLIENT_LAYER_ABOVE) {
        enact_client_layer_above(client);
    } else if (apply->layer == (uint16_t) CLIENT_LAYER_BELOW) {
        enact_client_layer_below(client);
    } else {
        enact_client_layer_normal(client);
    }
}


/**
 * @brief Apply the geometry rule to a client
 *
 * Position (@p apply->x, @p apply->y, or @p apply->is_position_centered),
 * size (@p apply->w, @p apply->h), and monitor (@p apply->monitor) are
 * applied independently: only the fields flagged as present are
 * touched.  When the client has a decoration frame, the
 * synchronization helper is called to keep the inner window aligned.
 * The single, combined @c XCB_CONFIG_WINDOW_* call itself funnels
 * through @a ccmd_client_apply_geometry, the same shared primitive
 * every other geometry-changing operation in this project already
 * uses, rather than building its own values array by hand; flushed
 * and broadcast afterward (@c IPC_EVENT_WINDOW_MOVED and/or @c
 * _RESIZED, matching whichever of position/size actually changed),
 * the same as @a enact_client_move/@c _resize do for every other
 * trigger of the same two events.
 *
 * Size is resolved before position so that a rule combining
 * @c ("position": "center") with an explicit @c size centers the client
 * at its @e new size, not whatever size it happened to have already
 * been placed at.
 *
 * When @p apply->has_monitor is set, @p apply->monitor selects
 * a monitor within @p surface's own monitor list (out of range falls
 * back to 0th-monitor, logging a warning), and every position below
 * becomes relative to that monitor's own top-left corner instead of the
 * whole surface's, as explicit @c x / @c y are offset by it, and
 * centering targets that monitor instead of the whole surface.  A rule
 * that sets @c monitor without an explicit @c position centers on that
 * monitor by default, since otherwise @c monitor alone would have no
 * visible effect at all.
 *
 * @param surface    Surface the client is on, used to compute the
 *                   center point for @p apply->is_position_centered and
 *                   to resolve @p apply->monitor
 * @param client     Client whose geometry is to be set
 * @param apply      Action descriptor
 *
 * @note The function is a no-op when none of @p apply->has_position,
 *       @p apply->has_size, or @p apply->has_monitor is @c true.
 * @note Negative Y values in @p apply are clamped to zero
 * @note Complexity: @e O(1)
 */
static void s_rules_apply_geometry(const surface_td *surface,
        client_td *client, const struct rules_apply_s *apply)
{
    xcb_window_t target;
    uint16_t mask = 0;
    uint32_t width;
    uint32_t height;
    int32_t x = 0;
    int32_t y = 0;
    bool set_pos = false;
    bool set_size = false;
    bool has_monitor = false;
    monitor_td monitor_rect = {.x = 0, .y = 0, .w = 0u, .h = 0u};

    if (!apply->has_position && !apply->has_size && !apply->has_monitor) {
        return;
    }

    /* Fullscreen is a WM-forced override of the client's own preferred
     * geometry (see 'ccmd_client_fullscreen''s own doc comment,
     * cmds/client/state.c), and every source that might otherwise
     * change position/size while it holds respects that already
     * ('handler_configure_request', handler/configure.c, for the
     * client's own attempts).  A rule is no different: 'rules_apply'
     * itself re-runs on any property change this client's own window
     * happens to generate while fullscreen (not just its initial map),
     * via the generic fallback at the end of 'handler_property_notify'
     * (handler/focus.c), so without this a rule with its own
     * 'apply.size'/'apply.position' would silently undo fullscreen the
     * next time that client touched some unrelated property of its
     * own.  Every other rule effect (desktop, layer, flags) still
     * applies regardless; only geometry itself is skipped here. */
    if (client_is_fullscreen(client)) {
        return;
    }

    width = client->layout.geometry.cur.dim.w;
    height = client->layout.geometry.cur.dim.h;

    if (apply->has_size) {
        /* 'apply->w'/'apply->h' (rules.json's own 'apply.size.width'/
         * 'apply.size.height') name the decorated frame's own total,
         * border and titlebar included, the same as
         * 'client->layout.geometry.cur.dim' itself already does.
         *
         * But the ICCCM size hints 'client_size_constrain' enforces are
         * always about a client's own content alone, regardless of
         * decoration, so convert to content space first, apply them
         * there, then convert back, the same round trip
         * 'input/mouse/drag.c' and 's_kb_resize_axis_target'
         * (in 'input/kbd/interact.c') already make for their own resize
         * paths. */
        uint32_t ext_w = (uint32_t) client->layout.frame_extents.left +
            (uint32_t) client->layout.frame_extents.right;
        uint32_t ext_h = (uint32_t) client->layout.frame_extents.top +
            (uint32_t) client->layout.frame_extents.bottom;
        uint32_t content_w = (apply->w > ext_w) ? apply->w - ext_w : 0u;
        uint32_t content_h = (apply->h > ext_h) ? apply->h - ext_h : 0u;

        client_size_constrain(client, &content_w, &content_h);
        width = content_w + ext_w;
        height = content_h + ext_h;
        client->layout.geometry.cur.dim.w = width;
        client->layout.geometry.cur.dim.h = height;
        set_size = true;
    }

    if (apply->has_monitor && surface != NULL &&
            surface->monitor_count > 0u) {
        uint32_t monitor_idx = apply->monitor;

        if (monitor_idx >= surface->monitor_count) {
            LOGGER_WARNING("Rule targets monitor %u, which does not" \
                    " exist on surface %u (%u monitor(s)); falling" \
                    " back to monitor 0", apply->monitor, surface->id,
                    surface->monitor_count);
            monitor_idx = 0u;
        }
        monitor_rect = surface->monitors[monitor_idx];
        has_monitor = true;
    }

    if (apply->has_position || has_monitor) {
        bool center = (apply->has_position) ? apply->is_position_centered
            : true;

        if (center) {
            uint32_t area_w = (has_monitor) ? monitor_rect.w
                : ((surface != NULL) ? surface->properties.dim.w : 0u);
            uint32_t area_h = (has_monitor) ? monitor_rect.h
                : ((surface != NULL) ? surface->properties.dim.h : 0u);

            x = (area_w > width)
                ? (int32_t) ((area_w - width) / 2u) : 0;
            y = (area_h > height)
                ? (int32_t) ((area_h - height) / 2u) : 0;
        } else {
            x = apply->x;
            y = (apply->y < 0) ? 0 : apply->y;
        }

        if (has_monitor) {
            x += monitor_rect.x;
            y += monitor_rect.y;
        }

        client->layout.geometry.cur.pos.x = x;
        client->layout.geometry.cur.pos.y = y;
        client->has_rule_position_locked = true;
        set_pos = true;
    }

    if (set_pos) {
        mask |= XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
    }
    if (set_size) {
        mask |= XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    }

    target = (client->frame != 0 && client_is_decorated(client))
        ? client->frame : client->window;

    ccmd_client_apply_geometry(client, target, mask, x, y,
            width, height, 0u);

    if (client->frame != 0 && client_is_decorated(client)) {
        client_decoration_layout_sync(client);
    }

    xcb_flush(client->connection);
    if (set_pos) {
        s_rules_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
    if (set_size) {
        s_rules_broadcast_client_event(client,
                IPC_EVENT_WINDOW_RESIZED);
    }
}


/**
 * @brief Apply pinned and decoration flag rules to a client
 *
 * Sets or clears the pinned flag and toggles decoration according to
 * @p apply.  Each flag is only touched when its corresponding @c has_*
 * field is @c true.
 *
 * @param client Client whose flags are to be updated
 * @param apply  Action descriptor
 *
 * @note Complexity: @e O(1)
 */
static void s_rules_apply_flags(client_td *client,
        const struct rules_apply_s *apply)
{
    if (apply->has_sticky) {
        if (apply->is_pinned) {
            enact_client_pin(client);
        } else {
            enact_client_unpin(client);
        }
    }

    if (apply->has_decoration) {
        if (apply->is_decorated != client_is_decorated(client)) {
            enact_client_toggle_decorate(client);
        }
    }

    if (apply->has_opacity_active) {
        ccmd_client_set_opacity_active(client, apply->opacity_active);
    }
    if (apply->has_opacity_inactive) {
        ccmd_client_set_opacity_inactive(client,
                apply->opacity_inactive);
    }
}


/**
 * @brief Apply the map/unmap side effect of a desktop-reassignment rule
 *
 * Only relevant when a property-triggered rule (@p trigger @c ==
 * @c RULES_TRIGGER_PROPERTY) actually moved @p client to a different
 * desktop than it was already on: unmaps @p client's window (and its
 * frame, if decorated) when the desktop it landed on is not the one
 * currently shown, or maps them back when it is.  A no-op in every
 * other case, including every other trigger, since only a property
 * change can retarget an already-mapped, already-visible client this
 * way; @a s_rules_apply_desktop must have already run and updated
 * @p client->desktop_id before this is called.
 *
 * @param client          Client whose visibility is to be updated
 * @param connection      XCB connection
 * @param surface         Surface @p client lives on
 * @param trigger         What triggered this rule evaluation
 * @param apply           Action descriptor
 * @param prev_desktop_id @p client->desktop_id as it was before
 *                        @a s_rules_apply_desktop ran
 *
 * @note Complexity: @e O(1)
 */
static void s_rules_apply_visibility(client_td *client,
        xcb_connection_t *connection, const surface_td *surface,
        enum rules_trigger_e trigger,
        const struct rules_apply_s *apply, uint32_t prev_desktop_id)
{
    if (trigger != RULES_TRIGGER_PROPERTY || !apply->has_desktop ||
            client->desktop_id == prev_desktop_id || surface == NULL) {
        return;
    }

    if (surface->desktop_cur != client->desktop_id) {
        /* Two 'UnmapNotify' events arrive for 'client->window'
         * itself ('SubstructureNotify' on its parent +
         * 'StructureNotify' on the window), both matching
         * 'handler_unmap_notify''s 'event->window ==
         * client->window' check; without this, the first one
         * reaching it with 'ignore_unmap' still zero is read as
         * the client withdrawing itself rather than the window
         * manager hiding it for a desktop reassignment.  The
         * frame's own separate 'UnmapNotify' never carries
         * 'event->window == client->window', so it needs no
         * token of its own. */
        client->ignore.unmap += 2u;
        xcb_unmap_window(connection, client->window);
        if (client->frame != 0) {
            xcb_unmap_window(connection, client->frame);
        }
    } else {
        if (client->frame != 0) {
            xcb_map_window(connection, client->frame);
        }
        xcb_map_window(connection, client->window);
    }
}


/**
 * @brief Apply the focus rule to a client
 *
 * Focuses @p client when @p apply->has_focus and @p apply->is_focused are
 * both set, @p config is available, and @p client is focusable; a
 * no-op otherwise.
 *
 * @param wm      Window manager instance, for @a wm_surfaces
 * @param client  Client to focus
 * @param surface Surface @p client lives on
 * @param desktop Desktop @p client lives on
 * @param apply   Action descriptor
 * @param config  Active configuration
 *
 * @note Complexity: @e O(1)
 */
static void s_rules_apply_focus(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop,
        const struct rules_apply_s *apply, const config_td *config)
{
    if (!apply->has_focus || !apply->is_focused || config == NULL ||
            !client_is_focusable(client)) {
        return;
    }

    focus_apply(wm_surfaces(wm), surface, desktop, client, true,
            config);
}


/**
 * @brief Broadcast @c IPC_EVENT_RULE_APPLIED when a rule changed
 *        anything
 *
 * A rule that matched but left every @c has_* field in @p apply
 * false (theoretically possible, e.g. a rule with an empty @c apply
 * object) changes nothing and is not broadcast.
 *
 * @param client  Client the rule was applied to
 * @param desktop Client's own desktop after every other apply step
 * @param surface Client's own surface
 * @param apply   Action descriptor
 *
 * @return @c true when at least one field in @p apply was set, i.e.
 *         when the event was actually broadcast
 *
 * @note Complexity: @e O(1)
 */
static bool s_rules_notify_change(const client_td *client,
        const desktop_td *desktop, const surface_td *surface,
        const struct rules_apply_s *apply)
{
    bool changed = apply->has_desktop || apply->has_monitor ||
        apply->has_layer || apply->has_focus || apply->has_position ||
        apply->has_size || apply->has_sticky || apply->has_decoration ||
        apply->has_opacity_active || apply->has_opacity_inactive;

    if (changed) {
        cJSON *const fields = cJSON_CreateObject();

        if (fields != NULL) {
            cJSON_AddNumberToObject(fields, "client_id",
                    (double) client->id);
            cJSON_AddNumberToObject(fields, "desktop_id",
                    (double) desktop->id);
            cJSON_AddNumberToObject(fields, "surface_id",
                    (double) surface->id);
        }
        ipc_broadcast_event(IPC_EVENT_RULE_APPLIED, fields);
    }

    return changed;
}


/* Evaluate all loaded rules against a client and apply the merged
 * result */
bool rules_apply(const wm_td *wm, client_td *client,
        surface_td **surface_io, desktop_td **desktop_io,
        enum rules_trigger_e trigger)
{
    struct rules_apply_s merged;
    bool has_match;
    bool changed;
    uint32_t prev_desktop_id;
    rules_td *rules = wm_rules(wm);
    xcb_connection_t *connection = wm_connection(wm);
    const config_td *config = wm_config(wm);

    if (wm == NULL || rules == NULL || client == NULL ||
            surface_io == NULL || desktop_io == NULL ||
            *surface_io == NULL || *desktop_io == NULL) {
        return false;
    }

    memset(&merged, 0, sizeof(merged));
    has_match = false;
    prev_desktop_id = client->desktop_id;

    for (uint32_t i = 0u; i < rules->count; ++i) {
        struct rules_rule_s *rule = &rules->rules[i];

        if (!ri_when_matches(rule->when, trigger)) {
            continue;
        }

        if (!ri_client_matches(&rule->match, client)) {
            continue;
        }

        has_match = true;

        if (rule->apply.has_desktop) {
            merged.has_desktop = true;
            merged.desktop = rule->apply.desktop;
        }
        if (rule->apply.has_monitor) {
            merged.has_monitor = true;
            merged.monitor = rule->apply.monitor;
        }
        if (rule->apply.has_layer) {
            merged.has_layer = true;
            merged.layer = rule->apply.layer;
        }
        if (rule->apply.has_focus) {
            merged.has_focus = true;
            merged.is_focused = rule->apply.is_focused;
        }
        if (rule->apply.has_position) {
            merged.has_position = true;
            merged.is_position_centered = rule->apply.is_position_centered;
            merged.x = rule->apply.x;
            merged.y = rule->apply.y;
        }
        if (rule->apply.has_size) {
            merged.has_size = true;
            merged.w = rule->apply.w;
            merged.h = rule->apply.h;
        }
        if (rule->apply.has_sticky) {
            merged.has_sticky = true;
            merged.is_pinned = rule->apply.is_pinned;
        }
        if (rule->apply.has_decoration) {
            merged.has_decoration = true;
            merged.is_decorated = rule->apply.is_decorated;
        }
        if (rule->apply.has_opacity_active) {
            merged.has_opacity_active = true;
            merged.opacity_active = rule->apply.opacity_active;
        }
        if (rule->apply.has_opacity_inactive) {
            merged.has_opacity_inactive = true;
            merged.opacity_inactive = rule->apply.opacity_inactive;
        }
    }

    if (!has_match) {
        return false;
    }

    s_rules_apply_desktop(client, *surface_io, desktop_io, &merged);
    s_rules_apply_visibility(client, connection, *surface_io, trigger,
            &merged, prev_desktop_id);
    s_rules_apply_layer(client, &merged);
    s_rules_apply_flags(client, &merged);
    s_rules_apply_geometry(*surface_io, client, &merged);
    s_rules_apply_focus(wm, client, *surface_io, *desktop_io, &merged,
            config);

    changed = s_rules_notify_change(client, *desktop_io, *surface_io,
            &merged);

    return changed;
}
