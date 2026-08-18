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
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Default initial values */
#include <defs/desktop.h>

/* Command includes */
#include <cmds/client/basic.h>
#include <cmds/client/layer.h>
#include <cmds/client/state.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <ipc.h>
#include <logger.h>
#include <policy/focus.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <rules.h>
#include <rules/internal.h>


/**
 * @brief Move a client to the desktop specified by a rule
 *
 * Removes @p client from the desktop pointed to by @p desktop_io, adds
 * it to the target desktop identified by @p apply->desktop, and updates
 * @p client->desktop_id and the EWMH @c _NET_WM_DESKTOP property.  If
 * the target desktop does not exist, falls back to 0th-desktop (logging
 * a warning); if it is the same as the current one, or 0th-desktop does
 * not exist either, the function returns without doing anything.  On
 * failure to add the client to the target desktop it is re-added to the
 * original one.
 *
 * @param wm         Window manager instance (used for the connection
 *                   and EWMH handle)
 * @param client     Client to move
 * @param surface    Surface on which the target desktop lives
 * @param desktop_io In/out pointer to the current desktop; updated to
 *                   point at the target on success
 * @param apply      Action descriptor; only evaluated when
 *                   @p apply->has_desktop is @c true
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       source or target desktop during the add/remove operations
 */
static void s_rules_apply_desktop(wm_td *wm, client_td *client,
        surface_td *surface, desktop_td **desktop_io,
        const struct rules_apply_s *apply)
{
    desktop_td *cur;
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

    (void) desktop_action_client_rem(cur, client);
    if (desktop_action_client_add(target, client) != 0) {
        (void) desktop_action_client_add(cur, client);
        return;
    }

    client->desktop_id = target->id;
    if (wm->ewmh != NULL) {
        uint32_t did = (client->properties.flags & CLIENT_FLAG_PIN)
            ? WM_DESKTOP_ID_ALL : target->id;

        xcb_change_property(wm->connection, XCB_PROP_MODE_REPLACE,
                client->window, wm->ewmh->_NET_WM_DESKTOP,
                XCB_ATOM_CARDINAL, 32, 1, &did);
    }

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
        ccmd_client_layer_above(client);
    } else if (apply->layer == (uint16_t) CLIENT_LAYER_BELOW) {
        ccmd_client_layer_below(client);
    } else {
        ccmd_client_layer_normal(client);
    }
}


/**
 * @brief Apply the geometry rule to a client
 *
 * Position (@p apply->x, @p apply->y, or @p apply->position_centered),
 * size (@p apply->w, @p apply->h), and monitor (@p apply->monitor) are
 * applied independently: only the fields flagged as present are
 * touched.  When the client has a decoration frame, the
 * synchronization helper is called to keep the inner window aligned.
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
 * @param connection XCB connection used to send the configure request
 * @param surface    Surface the client is on, used to compute the
 *                   center point for @p apply->position_centered and
 *                   to resolve @p apply->monitor
 * @param client     Client whose geometry is to be set
 * @param apply      Action descriptor
 *
 * @note The function is a no-op when none of @p apply->has_position,
 *       @p apply->has_size, or @p apply->has_monitor is @c true.
 * @note Negative Y values in @p apply are clamped to zero
 * @note Complexity: @e O(1)
 */
static void s_rules_apply_geometry(xcb_connection_t *connection,
        const surface_td *surface, client_td *client,
        const struct rules_apply_s *apply)
{
    xcb_window_t target;
    uint16_t mask = 0;
    uint32_t values[4];
    uint32_t vi = 0;
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
        bool center = (apply->has_position) ? apply->position_centered
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
        client->rule_position_locked = true;
        set_pos = true;
    }

    /* Value list order must ascend by 'XCB_CONFIG_WINDOW_*' bit value:
     * X, Y, then WIDTH, HEIGHT.  Built here in that order regardless of
     * which of position/size were actually resolved above */
    if (set_pos) {
        mask |= XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
        values[vi++] = (uint32_t) x;
        values[vi++] = (uint32_t) y;
    }
    if (set_size) {
        mask |= XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        values[vi++] = width;
        values[vi++] = height;
    }

    target = (client->frame != 0 && client_is_decorated(client))
        ? client->frame : client->window;

    xcb_configure_window(connection, target, mask, values);

    if (client->frame != 0 && client_is_decorated(client)) {
        client_decoration_layout_sync(client);
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
        if (apply->pinned) {
            ccmd_client_pin(client);
        } else {
            ccmd_client_unpin(client);
        }
    }

    if (apply->has_decorated) {
        if (apply->decorated != client_is_decorated(client)) {
            ccmd_client_toggle_decorate(client);
        }
    }

    if (apply->has_opacity_active) {
        client->opacity_override.is_set_active = true;
        client->opacity_override.active = apply->opacity_active;
    }
    if (apply->has_opacity_inactive) {
        client->opacity_override.is_set_inactive = true;
        client->opacity_override.inactive = apply->opacity_inactive;
    }
    if (apply->has_opacity_active || apply->has_opacity_inactive) {
        wm_request_client_redraw(client);
    }
}


/* Evaluate all loaded rules against a client and apply the merged
 * result */
bool rules_apply(wm_td *wm, client_td *client,
        surface_td **surface_io, desktop_td **desktop_io,
        enum rules_trigger_e trigger)
{
    struct rules_apply_s merged;
    bool has_match;
    bool changed;
    uint32_t prev_desktop_id;

    if (wm == NULL || wm->rules == NULL || client == NULL ||
            surface_io == NULL || desktop_io == NULL ||
            *surface_io == NULL || *desktop_io == NULL) {
        return false;
    }

    memset(&merged, 0, sizeof(merged));
    has_match = false;
    prev_desktop_id = client->desktop_id;

    for (uint32_t i = 0u; i < wm->rules->count; ++i) {
        struct rules_rule_s *rule = &wm->rules->rules[i];

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
            merged.focus = rule->apply.focus;
        }
        if (rule->apply.has_position) {
            merged.has_position = true;
            merged.position_centered = rule->apply.position_centered;
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
            merged.pinned = rule->apply.pinned;
        }
        if (rule->apply.has_decorated) {
            merged.has_decorated = true;
            merged.decorated = rule->apply.decorated;
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

    s_rules_apply_desktop(wm, client, *surface_io, desktop_io, &merged);

    if (trigger == RULES_TRIGGER_PROPERTY &&
            merged.has_desktop &&
            client->desktop_id != prev_desktop_id &&
            *surface_io != NULL) {
        if ((*surface_io)->desktop_cur != client->desktop_id) {
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
            client->ignore_unmap += 2u;
            xcb_unmap_window(wm->connection, client->window);
            if (client->frame != 0) {
                xcb_unmap_window(wm->connection, client->frame);
            }
        } else {
            if (client->frame != 0) {
                xcb_map_window(wm->connection, client->frame);
            }
            xcb_map_window(wm->connection, client->window);
        }
    }
    s_rules_apply_layer(client, &merged);
    s_rules_apply_flags(client, &merged);
    s_rules_apply_geometry(wm->connection, *surface_io, client, &merged);

    if (merged.has_focus && merged.focus &&
            wm->config != NULL && client_is_focusable(client)) {
        focus_apply(wm->surfaces, *surface_io, *desktop_io,
                client, true, wm->config);
    }

    changed = merged.has_desktop || merged.has_monitor ||
        merged.has_layer || merged.has_focus || merged.has_position ||
        merged.has_size || merged.has_sticky || merged.has_decorated ||
        merged.has_opacity_active || merged.has_opacity_inactive;

    if (changed) {
        cJSON *fields = cJSON_CreateObject();

        if (fields != NULL) {
            cJSON_AddNumberToObject(fields, "client_id",
                    (double) client->id);
            cJSON_AddNumberToObject(fields, "desktop_id",
                    (double) (*desktop_io)->id);
            cJSON_AddNumberToObject(fields, "surface_id",
                    (double) (*surface_io)->id);
        }
        ipc_broadcast_event(IPC_EVENT_RULE_APPLIED, fields);
    }

    return changed;
}
