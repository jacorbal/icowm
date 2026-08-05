/**
 * @file rules/rules.c
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
#include <defs/wm.h>

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
#include <rules/internal.h>
#include <rules/rules.h>




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
 * Position (@p apply->x, @p apply->y) and size (@p apply->w,
 * @p apply->h) are applied independently: only the fields that are
 * flagged as present are touched.  When both are set the behaviour is
 * identical to the previous all-or-nothing mode.  When the client has
 * a decoration frame, the synchronisation helper is called to keep the
 * inner window aligned.  The function is a no-op when neither
 * @p apply->has_position nor @p apply->has_size is @c true.
 *
 * @param connection XCB connection used to send the configure request
 * @param client     Client whose geometry is to be set
 * @param apply      Action descriptor
 *
 * @note Negative Y values in @p apply are clamped to zero
 * @note Complexity: @e O(1)
 */
static void s_rules_apply_geometry(xcb_connection_t *connection,
        client_td *client, const struct rules_apply_s *apply)
{
    xcb_window_t target;
    uint16_t mask = 0;
    uint32_t values[4];
    uint32_t vi = 0;
    uint32_t width;
    uint32_t height;

    if (!apply->has_position && !apply->has_size) {
        return;
    }

    if (apply->has_position) {
        client->layout.geometry.cur.pos.x = apply->x;
        client->layout.geometry.cur.pos.y = (apply->y < 0) ? 0 : apply->y;
        mask |= XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
        values[vi++] = (uint32_t) client->layout.geometry.cur.pos.x;
        values[vi++] = (uint32_t) client->layout.geometry.cur.pos.y;
        client->rule_position_locked = true;
    }

    if (apply->has_size) {
        width = apply->w;
        height = apply->h;
        client_constrain_size(client, &width, &height);
        client->layout.geometry.cur.dim.w = width;
        client->layout.geometry.cur.dim.h = height;
        mask |= XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        values[vi++] = width;
        values[vi++] = height;
    }

    (void) vi;

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
            item = json_get_item(match_json, "instance");
            if (cJSON_IsString(item) && item->valuestring != NULL) {
                rule->match.has_instance = true;
                safe_strncpy(rule->match.instance, item->valuestring,
                        sizeof(rule->match.instance));
            }

            item = json_get_item(match_json, "class");
            if (cJSON_IsString(item) && item->valuestring != NULL) {
                rule->match.has_class = true;
                safe_strncpy(rule->match.klass, item->valuestring,
                        sizeof(rule->match.klass));
            }

            item = json_get_item(match_json, "role");
            if (cJSON_IsString(item) && item->valuestring != NULL) {
                rule->match.has_role = true;
                safe_strncpy(rule->match.role, item->valuestring,
                        sizeof(rule->match.role));
            }

            item = json_get_item(match_json, "title");
            if (cJSON_IsString(item) && item->valuestring != NULL) {
                rule->match.has_title = true;
                safe_strncpy(rule->match.title, item->valuestring,
                        sizeof(rule->match.title));
            }

            item = json_get_item(match_json, "type");
            if (cJSON_IsString(item) && item->valuestring != NULL) {
                rule->match.has_type = true;
                safe_strncpy(rule->match.type, item->valuestring,
                        sizeof(rule->match.type));
            }

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
                rule->apply.x = x->valueint;
                rule->apply.y = y->valueint;
            }
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

    LOGGER_INFO("Loaded %u window rule(s) from '%s'", rules->count,
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
    s_rules_apply_geometry(wm->connection, client, &merged);

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
