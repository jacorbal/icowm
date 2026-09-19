/**
 * @file rules/apply.c
 *
 * @brief Applies a merged rule action descriptor to a client
 *
 * Kept apart from @c rules.c: @a rules_apply and its per-field helpers
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

/* Policy includes */
#include <policy/focus.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <enact.h>
#include <enact/client.h>
#include <enact/desktop.h>
#include <ipc.h>
#include <logger.h>
#include <stage.h>
#include <stage/desktop.h>
#include <wm.h>

/* Local includes */
#include <rules.h>
#include <rules/internal.h>


/**
 * @brief Broadcast an IPC event carrying one client's identifying
 *        fields
 *
 * Mirrors @a enact_broadcast_client_event's field shape
 * (@c enact/internal.h), which this file cannot reach directly.
 * That header is deliberately private to @c enact/ itself (see its
 * comment for why), so this is its small, local copy of the same fields
 * instead.
 *
 * @param client Client the event is about
 * @param type   IPC event bitmask (a single @c IPC_EVENT_* value)
 *
 * @note No-op if @p client is @c NULL
 * @note Complexity: @e O(1)
 *
 * @see @c ipc.h
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
        cJSON_AddNumberToObject(fields, "stage_id",
                (double) client->screen_id);
    }
    ipc_broadcast_event(type, fields);
}


/**
 * @brief Move a client to the desktop specified by a rule
 *
 * Resolves the target desktop identified by @p apply->desktop, then
 * delegates the actual move to @a enact_desktop_client_send, the same
 * shared primitive the "Send to desktop" menu and the move-to-desktop
 * keybind both already use, so a rule-driven move gets the exact same
 * family-wide cascade, visibility handling, focus fallback, EWMH
 * publish, and IPC broadcast every other trigger of this same action
 * already gets, rather than a second, narrower reimplementation of its
 * own.
 *
 * If the target desktop does not exist, falls back to 0th-desktop
 * (logging a warning); if it is the same as the current one, or
 * 0th-desktop does not exist either, the function returns without doing
 * anything.
 *
 * @param client     Client to move
 * @param stage      Stage on which the target desktop lives
 * @param desktop_io In/out pointer to the current desktop; updated to
 *                   point at the target on success
 * @param apply Action descriptor; only evaluated when
 *                   @p apply->has_desktop is @c true
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the client's top parent's desktop, as
 *       @a enact_desktop_client_send's comment describes
 */
static void s_rules_apply_desktop(client_td *client,
        stage_td *stage, desktop_td **desktop_io,
        const struct rules_apply_s *apply)
{
    const desktop_td *cur;
    desktop_td *target;

    if (!apply->has_desktop || stage == NULL || desktop_io == NULL ||
            *desktop_io == NULL) {
        return;
    }

    cur = *desktop_io;
    target = stage_desktop_get(stage, apply->desktop);
    if (target == NULL) {
        LOGGER_WARNING("Rule targets desktop %u, which does not" \
                " exist on stage %u; falling back to desktop 0",
                apply->desktop, stage->id);
        target = stage_desktop_get(stage, 0u);
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
 * Position, through @c apply->x and @c apply->y or through
 * @c apply->is_position_centered, size, through @c apply->w and
 * @c apply->h, and monitor, through @c apply->monitor, are applied
 * independently: only the fields flagged as present are touched.  When
 * the client has a decoration frame, the synchronization helper is
 * called to keep the inner window aligned.
 *
 * The single, combined @c XCB_CONFIG_WINDOW_* call itself funnels
 * through @a ccmd_client_apply_geometry, the same shared primitive
 * every other geometry-changing operation in this project already uses,
 * rather than building its values array by hand; flushed and broadcast
 * afterward (@c IPC_EVENT_WINDOW_MOVED and/or @c _RESIZED, matching
 * whichever of position/size actually changed), the same as
 * @a enact_client_move/@c _resize do for every other trigger of the
 * same two events.
 *
 * Size is resolved before position so that a rule combining
 * @c ("position": "center") with an explicit @c size centers the client
 * at its @e new size, not whatever size it happened to have already
 * been placed at.
 *
 * When @p apply->has_monitor is set, @p apply->monitor selects
 * a monitor within @p stage's monitor list (out of range falls back
 * to 0th-monitor, logging a warning), and every position below becomes
 * relative to that monitor's top-left corner instead of the whole
 * stage's, as explicit @c x / @c y are offset by it, and centering
 * targets that monitor instead of the whole stage.  A rule that sets
 * @c monitor without an explicit @c position centers on that monitor by
 * default, since otherwise @c monitor alone would have no visible
 * effect at all.
 *
 * @param stage Stage the client is on, used to compute the center
 *                point for @p apply->is_position_centered and to
 *                resolve @p apply->monitor
 * @param client Client whose geometry is to be set
 * @param apply  Action descriptor
 *
 * @note The function is a no-op when none of @p apply->has_position,
 *       @p apply->has_size, or @p apply->has_monitor is @c true
 * @note Position and size are also skipped, silently, for
 *       a fullscreen or maximized client
 * @note Negative Y values in @p apply are clamped to zero
 * @note Complexity: @e O(1)
 */
static void s_rules_apply_geometry(const stage_td *stage,
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

    /* Fullscreen and any maximized state (full, or just one axis)
     * are both WM-forced overrides of the client's preferred
     * geometry (see 'ccmd_client_fullscreen'/'ccmd_client_maximize'
     * and friends, cmds/client/state.c and maximize.c), and every
     * source that might otherwise change position/size while either
     * holds respects that already ('handler_configure_request',
     * handler/configure.c, for the client's own attempts).
     *
     * A rule is no different: 'rules_apply' itself re-runs on any
     * property change this client's window happens to generate while
     * fullscreen or maximized (not just its initial map), via the
     * generic fallback at the end of 'handler_property_notify'
     * (handler/focus.c), so without this a rule with its
     * 'apply.size'/'apply.position' would silently undo either state
     * the next time that client touched some unrelated property of
     * its own.  Every other rule effect (desktop, layer, flags)
     * still applies regardless; only geometry itself is skipped
     * here. */
    if (client_is_fullscreen(client) || client_is_maximized_any(client)) {
        return;
    }

    width = client->layout.geometry.cur.dim.w;
    height = client->layout.geometry.cur.dim.h;

    if (apply->has_size) {
        /* 'apply->w'/'apply->h' (rules.json's 'apply.size.width'/
         * 'apply.size.height') name the decorated frame's total, border
         * and titlebar included, the same as
         * 'client->layout.geometry.cur.dim' itself already does.
         *
         * But the ICCCM size hints 'client_size_constrain' enforces are
         * always about a client's content alone, regardless of
         * decoration, so convert to content space first, apply them
         * there, then convert back, the same round trip
         * 'input/mouse/drag.c' and 's_kb_resize_axis_target' (in
         * 'input/kbd/interact.c') already make for their resize
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

    if (apply->has_monitor && stage != NULL &&
            stage->monitor_count > 0u) {
        uint32_t monitor_idx = apply->monitor;

        if (monitor_idx >= stage->monitor_count) {
            LOGGER_WARNING("Rule targets monitor %u, which does not" \
                    " exist on stage %u (%u monitor(s)); falling" \
                    " back to monitor 0", apply->monitor, stage->id,
                    stage->monitor_count);
            monitor_idx = 0u;
        }
        monitor_rect = stage->monitors[monitor_idx];
        has_monitor = true;
    }

    if (apply->has_position || has_monitor) {
        bool center = (apply->has_position) ? apply->is_position_centered
            : true;

        if (center) {
            uint32_t area_w = (has_monitor) ? monitor_rect.w
                : ((stage != NULL) ? stage->properties.dim.w : 0u);
            uint32_t area_h = (has_monitor) ? monitor_rect.h
                : ((stage != NULL) ? stage->properties.dim.h : 0u);

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
    if (apply->has_pinned) {
        if (apply->is_pinned) {
            enact_client_pin(client);
        } else {
            enact_client_unpin(client);
        }
    }

    /* Not to be confused with 'pinned' above; see 'is_sticky's own
     * comment ('rules/internal.h') for the full distinction */
    if (apply->has_sticky) {
        if (apply->is_sticky) {
            enact_client_stick(client);
        } else {
            enact_client_unstick(client);
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
 * @brief Record iconified/fullscreen/maximized/shaded/hidden rule
 *        requests on a client that is not mapped yet
 *
 * The setter used at @c RULES_TRIGGER_PROPERTY time
 * (@a s_rules_apply_state) is unsafe to call from @c RULES_TRIGGER_MAP
 * instead.  Yhe client's frame, titlebar, and window are not mapped yet
 * at that point (@a rules_apply runs before @a s_map_finish, see
 * @c handler/map.c), and several of those setters call
 * @a ccmd_client_focus internally, which needs a viewable window to
 * give X11 focus to (again, see that function's own doc comment).
 *
 * This just remembers what was asked for instead, the same way
 * @c has_rule_position_locked already does for @c apply.position
 * (@c include/client.h); @a s_map_finish reads these back once the
 * window is actually up, alongside the client's own EWMH initial- state
 * hints it already consults there.
 *
 * @param client Client to update
 * @param apply  Action descriptor
 *
 * @note Complexity: @e O(1)
 */
static void s_rules_defer_state_to_map(client_td *client,
        const struct rules_apply_s *apply)
{
    if (apply->has_focus) {
        client->has_rule_focus = true;
        client->is_rule_focus = apply->is_focused;
    }
    if (apply->has_iconified) {
        client->has_rule_iconified = true;
        client->is_rule_iconified = apply->is_iconified;
    }
    if (apply->has_fullscreen) {
        client->has_rule_fullscreen = true;
        client->is_rule_fullscreen = apply->is_fullscreen;
    }
    if (apply->has_maximized) {
        client->has_rule_maximized = true;
        client->is_rule_maximized = apply->is_maximized;
    }
    if (apply->has_shaded) {
        client->has_rule_shaded = true;
        client->is_rule_shaded = apply->is_shaded;
    }
    if (apply->has_hidden) {
        client->has_rule_hidden = true;
        client->is_rule_hidden = apply->is_hidden;
    }
}


/**
 * @brief Apply iconified/fullscreen/maximized/shaded/hidden rules to
 *        an already-mapped client
 *
 * These five track independent bits of the client's own state model
 * (@c include/client/state.h says as much for maximized/fullscreen;
 * @c include/client/state.h's @c CLIENT_FLAG_* set does the same for
 * hidden/shaded), so a rule is free to ask for any combination of them,
 * same as it already is for @c pinned/decorated in
 * @a s_rules_apply_flags.
 *
 * Order matters here in a way it does not there: leaving fullscreen
 * before touching maximized keeps a fullscreen-then-restore rule from
 * momentarily maximizing into the geometry fullscreen was about to give
 * up anyway, entering fullscreen after maximized lets a rule ask for
 * both and land on fullscreen (matching the precedence
 * @a ccmd_client_fullscreen itself already gives it over maximized on
 * the way in), and iconified last avoids the one call in this group
 * that is not a plain setter: @a enact_client_restore also undoes
 * fullscreen/maximized when the client is not currently iconified, so
 * calling it on every rule application, iconified or not, would
 * silently strip state this same function had just finished setting.
 *
 * @c apply.shaded can lose to three different already-true conditions
 * (not decorated, fullscreen, or (about to be) iconified) each logged
 * once here rather than left to the silent no-op @a ccmd_client_shade's
 * own guard would otherwise give, the same reasoning
 * @a s_rules_apply_desktop and @a s_rules_apply_geometry already log
 * for their own out-of-range/fullscreen fallbacks.
 *
 * Only ever called for @c RULES_TRIGGER_PROPERTY: a client reached
 * through @c RULES_TRIGGER_MAP is not mapped yet, and every function
 * called from here that also calls @a ccmd_client_focus requires
 * a viewable window to do that on (see that function's own doc comment,
 * @c cmds/client/focus.c); @a rules_apply defers these same five fields
 * to @c client_td's own @c has_rule_iconified and its four siblings
 * instead for that trigger, applied once @a s_map_finish has actually
 * mapped the window.
 *
 * @param client Client to update
 * @param apply  Action descriptor
 *
 * @note Complexity: @e O(1)
 */
static void s_rules_apply_state(client_td *client,
        const struct rules_apply_s *apply)
{
    if (apply->has_fullscreen && !apply->is_fullscreen &&
            client_is_fullscreen(client)) {
        enact_client_unfullscreen(client);
    }

    if (apply->has_maximized &&
            apply->is_maximized != client_is_maximized(client)) {
        enact_client_maximize(client);
    }

    if (apply->has_fullscreen && apply->is_fullscreen &&
            !client_is_fullscreen(client)) {
        enact_client_fullscreen(client);
    }

    if (apply->has_shaded) {
        if (!apply->is_shaded) {
            enact_client_unshade(client);
        } else if (!client_is_decorated(client)) {
            LOGGER_WARNING("Rule requests shaded but client is not" \
                    " decorated; shaded ignored", L_NARG);
        } else if (client_is_fullscreen(client)) {
            LOGGER_WARNING("Rule requests shaded while fullscreen is" \
                    " also requested; fullscreen takes precedence," \
                    " shaded ignored", L_NARG);
        } else if (apply->has_iconified && apply->is_iconified) {
            LOGGER_WARNING("Rule requests shaded while iconified is" \
                    " also requested; iconified takes precedence," \
                    " shaded ignored", L_NARG);
        } else {
            enact_client_shade(client);
        }
    }

    if (apply->has_hidden) {
        if (apply->is_hidden) {
            enact_client_hide(client);
        } else {
            enact_client_unhide(client);
        }
    }

    if (apply->has_iconified) {
        bool now_iconified = client_is_iconified(client);

        if (apply->is_iconified && !now_iconified) {
            enact_client_iconify(client);
        } else if (!apply->is_iconified && now_iconified) {
            enact_client_restore(client);
        }
    }
}


/* A property-triggered desktop reassignment ('s_rules_apply_desktop'
 * just above) already leaves the client correctly mapped or unmapped on
 * its own: 'enact_desktop_client_send' itself synchronously unmaps the
 * window when it was visible on the desktop it is leaving, and every
 * 'RULES_TRIGGER_PROPERTY' call site in 'handler/focus.c' marks the
 * client, its stage, and its (now-updated) desktop outdated right
 * after a matching rule fires, which is what maps the window back in on
 * arrival if the desktop it lands on turns out to be the one currently
 * shown ('desktop_render_one_client', in 'render/desktop.c').
 *
 * A second, ad-hoc remap/unmap pass here used to duplicate exactly
 * that, down to re-incrementing 'client->ignore.unmap' for an
 * 'UnmapNotify' that had already been accounted for once, leaving the
 * counter permanently one unmap too high and silently swallowing the
 * next genuine self-unmap (e.g., an application withdrawing to the
 * system tray). */


/**
 * @brief Apply the focus rule to a client
 *
 * Focuses @p client when @c apply->has_focus and @c apply->is_focused
 * are both set, @p config is available, and @p client is focusable;
 * a no-op otherwise.
 *
 * @param wm      Window manager instance, for @a wm_stages
 * @param client  Client to focus
 * @param stage   Stage @p client lives on
 * @param desktop Desktop @p client lives on
 * @param apply   Action descriptor
 * @param config  Active configuration
 *
 * @note Complexity: @e O(1)
 */
static void s_rules_apply_focus(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop,
        const struct rules_apply_s *apply, const config_td *config)
{
    if (!apply->has_focus || !apply->is_focused || config == NULL ||
            !client_is_focusable(client)) {
        return;
    }

    focus_apply(wm_stages(wm), stage, desktop, client, true,
            config);
}


/**
 * @brief Broadcast @c IPC_EVENT_RULE_APPLIED when a rule changed
 *        anything
 *
 * A rule that matched but left every @c has_* field in @p apply false
 * (theoretically possible, e.g., a rule with an empty @c apply object)
 * changes nothing and is not broadcast.
 *
 * @param client  Client the rule was applied to
 * @param desktop Client's desktop after every other apply step
 * @param stage   Client's stage
 * @param apply   Action descriptor
 *
 * @return @c true when at least one field in @p apply was set, i.e.,
 *         when the event was actually broadcast
 *
 * @note Complexity: @e O(1)
 */
static bool s_rules_notify_change(const client_td *client,
        const desktop_td *desktop, const stage_td *stage,
        const struct rules_apply_s *apply)
{
    bool changed = apply->has_desktop || apply->has_monitor ||
        apply->has_layer || apply->has_focus || apply->has_position ||
        apply->has_size || apply->has_pinned || apply->has_sticky ||
        apply->has_decoration ||
        apply->has_opacity_active || apply->has_opacity_inactive ||
        apply->has_iconified || apply->has_fullscreen ||
        apply->has_maximized || apply->has_shaded || apply->has_hidden;

    if (changed) {
        cJSON *const fields = cJSON_CreateObject();

        if (fields != NULL) {
            cJSON_AddNumberToObject(fields, "client_id",
                    (double) client->id);
            cJSON_AddNumberToObject(fields, "desktop_id",
                    (double) desktop->id);
            cJSON_AddNumberToObject(fields, "stage_id",
                    (double) stage->id);
        }
        ipc_broadcast_event(IPC_EVENT_RULE_APPLIED, fields);
    }

    return changed;
}


/* Evaluate all loaded rules against a client and apply the merged
 * result */
bool rules_apply(const wm_td *wm, client_td *client,
        stage_td **stage_io, desktop_td **desktop_io,
        enum rules_trigger_e trigger)
{
    struct rules_apply_s merged;
    bool has_match;
    bool changed;
    rules_td *rules = wm_rules(wm);
    const config_td *config = wm_config(wm);

    if (wm == NULL || rules == NULL || client == NULL ||
            stage_io == NULL || desktop_io == NULL ||
            *stage_io == NULL || *desktop_io == NULL) {
        return false;
    }

    memset(&merged, 0, sizeof(merged));
    has_match = false;

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
            merged.is_position_centered =
                rule->apply.is_position_centered;
            merged.x = rule->apply.x;
            merged.y = rule->apply.y;
        }
        if (rule->apply.has_size) {
            merged.has_size = true;
            merged.w = rule->apply.w;
            merged.h = rule->apply.h;
        }
        if (rule->apply.has_pinned) {
            merged.has_pinned = true;
            merged.is_pinned = rule->apply.is_pinned;
        }
        if (rule->apply.has_sticky) {
            merged.has_sticky = true;
            merged.is_sticky = rule->apply.is_sticky;
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
        if (rule->apply.has_iconified) {
            merged.has_iconified = true;
            merged.is_iconified = rule->apply.is_iconified;
        }
        if (rule->apply.has_fullscreen) {
            merged.has_fullscreen = true;
            merged.is_fullscreen = rule->apply.is_fullscreen;
        }
        if (rule->apply.has_maximized) {
            merged.has_maximized = true;
            merged.is_maximized = rule->apply.is_maximized;
        }
        if (rule->apply.has_shaded) {
            merged.has_shaded = true;
            merged.is_shaded = rule->apply.is_shaded;
        }
        if (rule->apply.has_hidden) {
            merged.has_hidden = true;
            merged.is_hidden = rule->apply.is_hidden;
        }
    }

    if (!has_match) {
        return false;
    }

    s_rules_apply_desktop(client, *stage_io, desktop_io, &merged);
    s_rules_apply_layer(client, &merged);
    s_rules_apply_flags(client, &merged);
    if (trigger == RULES_TRIGGER_MAP) {
        s_rules_defer_state_to_map(client, &merged);
    } else {
        s_rules_apply_state(client, &merged);
    }
    s_rules_apply_geometry(*stage_io, client, &merged);

    /* At map time the window is not mapped yet, so focus is decided
     * once it is, in place of 'windows.focus.focus-new'
     * ('s_rules_defer_state_to_map' above keeps the rule's say) */
    if (trigger != RULES_TRIGGER_MAP) {
        s_rules_apply_focus(wm, client, *stage_io, *desktop_io, &merged,
                config);
    }

    changed = s_rules_notify_change(client, *desktop_io, *stage_io,
            &merged);

    return changed;
}
