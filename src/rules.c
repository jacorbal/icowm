/**
 * @file rules.c
 *
 * @brief Window matching rules loader and applier implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L

/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Default initial values */
#include <defs/config.h>
#include <defs/desktop.h>

/* Utils includes */
#include <utils/config/json.h>
#include <utils/safe/safestr.h>

/* Command includes */
#include <cmds/ccmd.h>
#include <cmds/layer.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <policy/focus.h>
#include <surface.h>

/* Local includes */
#include <rules.h>
#include <rules/internal.h>




/**
 * @brief Resolve the configuration directory base path
 *
 * Writes the effective configuration directory path into
 * @p config_dir_base.  The resolution order is: @p config_dir_prefix
 * (when non-empty) > @c $XDG_CONFIG_HOME/icowm > @c $HOME/.icowm >
 * @c ./icowm.
 *
 * @param config_dir_prefix Caller-supplied prefix, or @c NULL to use
 *                          the environment-based default
 * @param config_dir_base   Buffer that receives the resolved path;
 *                          should be at least
 *                          @c CONFIG_MAX_LENGTH_PATH_BASE bytes long
 */
static void s_rules_config_dir_set(const char *config_dir_prefix,
        char *config_dir_base)
{
    const char *config_xdg_config_home = getenv("XDG_CONFIG_HOME");
    const char *config_home = getenv("HOME");

    if (config_dir_prefix != NULL && config_dir_prefix[0] != '\0') {
        snprintf(config_dir_base, CONFIG_MAX_LENGTH_PATH_BASE,
                "%s", config_dir_prefix);
    } else if (config_xdg_config_home != NULL) {
        snprintf(config_dir_base, CONFIG_MAX_LENGTH_PATH_BASE,
                "%s/%s", config_xdg_config_home, CONFIG_DIR_BASE);
    } else if (config_home != NULL) {
        snprintf(config_dir_base, CONFIG_MAX_LENGTH_PATH_BASE,
                "%s/.%s", config_home, CONFIG_DIR_BASE);
    } else {
        snprintf(config_dir_base, CONFIG_MAX_LENGTH_PATH_BASE,
                "./%s", CONFIG_DIR_BASE);
    }
}


/**
 * @brief Move a client to the desktop specified by a rule
 *
 * Removes @p client from the desktop pointed to by @p desktop_io, adds
 * it to the target desktop identified by @p apply->desktop, and updates
 * @p client->desktop_id and the EWMH @c _NET_WM_DESKTOP property.  If
 * the target desktop does not exist or is the same as the current one
 * the function returns without doing anything.  On failure to add the
 * client to the target desktop it is re-added to the original one.
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
    if (target == NULL || target == cur) {
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
 * Position (@p apply->x, @p apply->y, or @p apply->position_centered)
 * and size (@p apply->w, @p apply->h) are applied independently: only
 * the fields that are flagged as present are touched.  When both are
 * set the behavior is identical to the previous all-or-nothing mode.
 * When the client has a decoration frame, the synchronisation helper is
 * called to keep the inner window aligned.  The function is a no-op
 * when neither @p apply->has_position nor @p apply->has_size is
 * @c true.
 *
 * Size is resolved before position so that a rule combining
 * @c ("position": "center") with an explicit @c size centers the client
 * at its @e new size, not whatever size it happened to have already been
 * placed at.
 *
 * @param connection XCB connection used to send the configure request
 * @param surface    Surface the client is on, used to compute the
 *                   center point for @p apply->position_centered
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

    if (!apply->has_position && !apply->has_size) {
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

    if (apply->has_position) {
        if (apply->position_centered && surface != NULL) {
            uint32_t screen_w = surface->properties.dim.w;
            uint32_t screen_h = surface->properties.dim.h;

            x = (screen_w > width)
                ? (int32_t) ((screen_w - width) / 2u) : 0;
            y = (screen_h > height)
                ? (int32_t) ((screen_h - height) / 2u) : 0;
        } else {
            x = apply->x;
            y = (apply->y < 0) ? 0 : apply->y;
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


/* Allocate and zero-initialise a new rules table */
rules_td *rules_init(void)
{
    return calloc(1, sizeof(rules_td));
}


/* Destroy a rules table and free its allocated memory */
void rules_destroy(rules_td *rules)
{
    if (rules != NULL) {
        free(rules);
    }
}


/**
 * @brief Load a match criterion that may be a single string or an array
 *        of strings into a fixed-size list
 *
 * Reads @p key from @p match_json: a plain JSON string is stored as the
 * list's only entry; a JSON array has each of its string elements
 * copied in order, up to @c RULES_MATCH_MAX_VALUES (any beyond that are
 * silently ignored). Non-string array elements are skipped rather than
 * aborting the whole list. Leaves @p has_flag and @p count untouched
 * (so already-loaded defaults survive) when @p key is absent or is
 * neither a string nor an array.
 *
 * @param match_json   Parsed @c match JSON object
 * @param key          Field name to read (e.g., @c title)
 * @param dest         Destination fixed-size string array
 * @param count_out    Receives the number of values actually stored
 * @param has_flag_out Set to @c true when at least one value was
 *                      stored
 *
 * @note Complexity: @e O(n), where @e n is @c RULES_MATCH_MAX_VALUES
 */
static void s_rules_load_match_list(cJSON *match_json, const char *key,
        char dest[][CONFIG_MAX_LENGTH_NAME], uint8_t *count_out,
        bool *has_flag_out)
{
    cJSON *item;
    cJSON *elem;
    uint8_t n;

    item = json_get_item(match_json, key);
    if (item == NULL) {
        return;
    }

    n = 0u;
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        safe_strncpy(dest[0], item->valuestring, CONFIG_MAX_LENGTH_NAME);
        n = 1u;
    } else if (cJSON_IsArray(item)) {
        cJSON_ArrayForEach(elem, item) {
            if (n >= (uint8_t) RULES_MATCH_MAX_VALUES) {
                break;
            }
            if (cJSON_IsString(elem) && elem->valuestring != NULL) {
                safe_strncpy(dest[n], elem->valuestring,
                        CONFIG_MAX_LENGTH_NAME);
                ++n;
            }
        }
    }

    if (n > 0u) {
        *count_out = n;
        *has_flag_out = true;
    }
}


/* Load window matching rules from the JSON configuration file */
int rules_load(rules_td *rules, const char *config_dir_prefix)
{
    char config_dir[CONFIG_MAX_LENGTH_PATH_BASE];
    char rules_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    cJSON *json = NULL;
    cJSON *rules_array;
    cJSON *rule_json;
    cJSON *match_json;
    cJSON *apply_json;
    cJSON *item;
    cJSON *x;
    cJSON *y;
    cJSON *w;
    cJSON *h;
    struct rules_rule_s *rule;
    uint32_t loaded = 0u;

    if (rules == NULL) {
        return 1;
    }

    memset(rules, 0, sizeof(*rules));

    s_rules_config_dir_set(config_dir_prefix, config_dir);
    snprintf(rules_file, sizeof(rules_file), "%s/%s",
            config_dir, CONFIG_FILENAME_RULES);

    if (json_load_config(rules_file, &json) != 0 || json == NULL) {
        LOGGER_DEBUG("Rules file '%s' not loaded;" \
                " continuing without rules", rules_file);
        return 0;
    }

    rules_array = (cJSON_IsArray(json))
        ? json : json_get_item(json, "rules");
    if (!cJSON_IsArray(rules_array)) {
        cJSON_Delete(json);
        return 0;
    }

    cJSON_ArrayForEach(rule_json, rules_array) {
        if (!cJSON_IsObject(rule_json) || loaded >= RULES_MAX) {
            continue;
        }

        rule = &rules->rules[loaded];
        rule->when = RULES_WHEN_MAP;

        item = json_get_item(rule_json, "when");
        if (cJSON_IsString(item) && item->valuestring != NULL) {
            if (safe_strcmp(item->valuestring, "property") == 0) {
                rule->when = RULES_WHEN_PROPERTY;
            } else if (safe_strcmp(item->valuestring, "both") == 0) {
                rule->when = RULES_WHEN_BOTH;
            }
        }

        match_json = json_get_item(rule_json, "match");
        if (cJSON_IsObject(match_json)) {
            s_rules_load_match_list(match_json, "instance",
                    rule->match.instance, &rule->match.instance_count,
                    &rule->match.has_instance);
            s_rules_load_match_list(match_json, "class",
                    rule->match.klass, &rule->match.class_count,
                    &rule->match.has_class);
            s_rules_load_match_list(match_json, "role",
                    rule->match.role, &rule->match.role_count,
                    &rule->match.has_role);
            s_rules_load_match_list(match_json, "title",
                    rule->match.title, &rule->match.title_count,
                    &rule->match.has_title);
            s_rules_load_match_list(match_json, "type",
                    rule->match.type, &rule->match.type_count,
                    &rule->match.has_type);

            item = json_get_item(match_json, "transient");
            if (cJSON_IsBool(item)) {
                rule->match.has_transient = true;
                rule->match.transient = cJSON_IsTrue(item);
            }
        }

        apply_json = json_get_item(rule_json, "apply");
        if (!cJSON_IsObject(apply_json)) {
            continue;
        }

        item = json_get_item(apply_json, "desktop");
        if (cJSON_IsNumber(item) && item->valueint >= 0) {
            rule->apply.has_desktop = true;
            rule->apply.desktop = (uint32_t) item->valueint;
        }

        item = json_get_item(apply_json, "layer");
        if (cJSON_IsString(item) && item->valuestring != NULL) {
            rule->apply.has_layer = true;
            rule->apply.layer = ri_parse_layer(item->valuestring);
        }

        item = json_get_item(apply_json, "focus");
        if (cJSON_IsBool(item)) {
            rule->apply.has_focus = true;
            rule->apply.focus = cJSON_IsTrue(item);
        }

        item = json_get_item(apply_json, "sticky");
        if (cJSON_IsBool(item)) {
            rule->apply.has_sticky = true;
            rule->apply.sticky = cJSON_IsTrue(item);
        }

        item = json_get_item(apply_json, "decorated");
        if (cJSON_IsBool(item)) {
            rule->apply.has_decorated = true;
            rule->apply.decorated = cJSON_IsTrue(item);
        }

        item = json_get_item(apply_json, "position");
        if (cJSON_IsObject(item)) {
            x = json_get_item(item, "x");
            y = json_get_item(item, "y");

            if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
                rule->apply.has_position = true;
                rule->apply.position_centered = false;
                rule->apply.x = x->valueint;
                rule->apply.y = y->valueint;
            }
        } else if (cJSON_IsString(item) && item->valuestring != NULL &&
                safe_strcmp(item->valuestring, "center") == 0) {
            rule->apply.has_position = true;
            rule->apply.position_centered = true;
        }

        item = json_get_item(apply_json, "size");
        if (cJSON_IsObject(item)) {
            w = json_get_item(item, "width");
            h = json_get_item(item, "height");

            if (cJSON_IsNumber(w) && cJSON_IsNumber(h) &&
                    w->valueint > 0 && h->valueint > 0) {
                rule->apply.has_size = true;
                rule->apply.w = (uint32_t) w->valueint;
                rule->apply.h = (uint32_t) h->valueint;
            }
        }

        loaded++;
    }

    rules->count = loaded;
    cJSON_Delete(json);

    LOGGER_DEBUG("Loaded %u window rule(s) from '%s'", rules->count,
            rules_file);
    return 0;
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

    changed = merged.has_desktop || merged.has_layer ||
        merged.has_focus || merged.has_position || merged.has_size ||
        merged.has_sticky || merged.has_decorated;

    return changed;
}
