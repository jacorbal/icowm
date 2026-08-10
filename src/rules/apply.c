/**
 * @file rules/apply.c
 *
 * @brief Applies a merged rule action descriptor to a client
 *
 * Split out of @c rules.c: @c rules_apply and its per-field helpers
 * form one coherent responsibility (mutating a client to match a
 * rule's merged action set) that is independent from loading and
 * parsing the rules table itself, and shares no file-scope state with
 * @c rules.c beyond the types already exposed in @c rules/internal.h.
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

/* Default initial values */
#include <defs/desktop.h>

/* Command includes */
#include <cmds/ccmd.h>
#include <cmds/layer.h>
#include <cmds/state.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <policy/focus.h>
#include <surface.h>

/* Local includes */
#include <rules.h>
#include <rules/internal.h>


/**
 * @brief Move a client to the desktop specified by a rule
 *
 * Removes @p client from the desktop pointed to by @p desktop_io, adds
 * it to the target desktop identified by @p apply->desktop, and updates
 * @p client->desktop_id and the EWMH @c _NET_WM_DESKTOP property.  If
 * the target desktop does not exist, falls back to desktop 0 (logging
 * a warning); if it is the same as the current one, or desktop 0 does
 * not exist either, the function returns without doing anything.  On
 * failure to add the client to the target desktop it is re-added to
 * the original one.
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
        uint32_t did = (client->properties.flags & CLIENT_FLAG_STICKY)
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
        wcmd_client_layer_above(client);
    } else if (apply->layer == (uint16_t) CLIENT_LAYER_BELOW) {
        wcmd_client_layer_below(client);
    } else {
        wcmd_client_layer_normal(client);
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
 * The function is a no-op when none of @p apply->has_position, @p
 * apply->has_size, or @p apply->has_monitor is @c true.
 *
 * Size is resolved before position so that a rule combining
 * @c ("position": "center") with an explicit @c size centers the client
 * at its @e new size, not whatever size it happened to have already been
 * placed at.
 *
 * When @p apply->has_monitor is set, @p apply->monitor selects a
 * monitor within @p surface's own monitor list (out of range falls
 * back to monitor 0, logging a warning), and every position below
 * becomes relative to that monitor's own top-left corner instead of
 * the whole surface's: explicit @c x/@c y are offset by it, and
 * centering targets that monitor instead of the whole surface.  A
 * rule that sets @c monitor without an explicit @c position centers
 * on that monitor by default, since otherwise @c monitor alone would
 * have no visible effect at all.
 *
 * @param connection XCB connection used to send the configure request
 * @param surface    Surface the client is on, used to compute the
 *                   center point for @p apply->position_centered and
 *                   to resolve @p apply->monitor
 * @param client     Client whose geometry is to be set
 * @param apply      Action descriptor
 *
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

    width = client->layout.geometry.cur.dim.w;
    height = client->layout.geometry.cur.dim.h;

    if (apply->has_size) {
        width = apply->w;
        height = apply->h;
        client_constrain_size(client, &width, &height);
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
     * X, Y, then WIDTH, HEIGHT.  Built here in that order regardless
     * of which of position/size were actually resolved above */
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
        client_sync_decoration_layout(client);
    }
}


/**
 * @brief Apply sticky and decoration flag rules to a client
 *
 * Sets or clears the sticky flag and toggles decoration according to
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
        if (apply->sticky) {
            wcmd_client_sticky(client);
        } else {
            wcmd_client_unsticky(client);
        }
    }

    if (apply->has_decorated) {
        if (apply->decorated != client_is_decorated(client)) {
            wcmd_client_toggle_decoration(client);
        }
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
            merged.sticky = rule->apply.sticky;
        }
        if (rule->apply.has_decorated) {
            merged.has_decorated = true;
            merged.decorated = rule->apply.decorated;
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
        merged.has_size || merged.has_sticky || merged.has_decorated;

    return changed;
}
