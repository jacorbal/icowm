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
#include <fnmatch.h>
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

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <policy/focus.h>
#include <surface.h>
#include <utils/config/json.h>
#include <utils/safe/safestr.h>

/* Command includes */
#include <cmds/ccmd.h>
#include <cmds/layer.h>

/* Local includes */
#include <rules/rules.h>


/** Maximum number of rule entries stored in a single rules table */
#define RULES_MAX (256u)

/** Timing constraint controlling when a rule is evaluated */
enum rules_when_e {
    RULES_WHEN_MAP = 0,     /**< Rule applies on @c MAP_REQUEST only */
    RULES_WHEN_PROPERTY,    /**< Rule applies on property change only */
    RULES_WHEN_BOTH         /**< Rule applies on both events */
};

/* Criteria used to match a client against one rule entry */
struct rules_match_s {
    bool has_instance;
    bool has_class;
    bool has_role;
    bool has_title;
    bool has_type;
    bool has_transient;

    char instance[CONFIG_MAX_LENGTH_NAME];
    char klass[CONFIG_MAX_LENGTH_NAME];
    char role[CONFIG_MAX_LENGTH_NAME];
    char title[CONFIG_MAX_LENGTH_NAME];
    char type[CONFIG_MAX_LENGTH_NAME];
    bool transient;
};

/** Actions to apply to a client when a rule entry matches */
struct rules_apply_s {
    bool has_desktop;
    bool has_layer;
    bool has_focus;
    bool has_position;  /* 'x' & 'y' set independently of 'size' */
    bool has_size;      /* 'width' & 'height' independent of 'position' */
    bool has_sticky;
    bool has_decorated;

    uint32_t desktop;
    uint16_t layer;
    bool focus;
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
    bool sticky;
    bool decorated;
};

/** A single rule entry combining match criteria and the action to apply */
struct rules_rule_s {
    enum rules_when_e when;
    struct rules_match_s match;
    struct rules_apply_s apply;
};

/** Rules table holding all loaded rule entries and their count */
struct rules_s {
    uint32_t count;
    struct rules_rule_s rules[RULES_MAX];
};


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
 * @brief Test whether a shell glob pattern matches a string value
 *
 * Wraps @c fnmatch with default flags, returning @c false whenever
 * either argument is null.
 *
 * @param pattern Shell glob pattern (may contain @c * and @c ?)
 * @param value   String to test against @p pattern
 *
 * @return Whether @p pattern matches @p value
 * @retval true  @p pattern matches @p value
 * @retval false @p pattern does not match, or either argument is null
 *
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static bool s_rules_match_str(const char *pattern, const char *value)
{
    if (pattern == NULL || value == NULL) {
        return false;
    }

    return fnmatch(pattern, value, 0) == 0;
}


/**
 * @brief Convert a layer name string to the corresponding client layer
 *
 * Recognised names are @c "above", @c "below", and anything else
 * (including @c NULL) which maps to @c CLIENT_LAYER_NORMAL.
 *
 * @param layer Layer name string from the configuration
 *
 * @return The @c client_layer_e value that corresponds to @p layer,
 *         cast to @c uint16_t; defaults to @c CLIENT_LAYER_NORMAL
 *
 * @note Complexity: @e O(1)
 */
static uint16_t s_rules_parse_layer(const char *layer)
{
    if (layer == NULL) {
        return (uint16_t) CLIENT_LAYER_NORMAL;
    }

    if (safe_strcmp(layer, "above") == 0) {
        return (uint16_t) CLIENT_LAYER_ABOVE;
    }
    if (safe_strcmp(layer, "below") == 0) {
        return (uint16_t) CLIENT_LAYER_BELOW;
    }

    return (uint16_t) CLIENT_LAYER_NORMAL;
}


/**
 * @brief Test whether a type name string matches a client's type value
 *
 * Compares the lower-case EWMH type name @p type against @p client_type
 * and returns @c true only when they correspond.  Returns @c false for
 * @c NULL or unrecognised type names.
 *
 * @param type        Lower-case EWMH type name from the configuration
 * @param client_type @c client_type_e value cast to @c uint16_t from
 *                    the client's property set
 *
 * @return Whether @p type names the same window type as @p client_type
 * @retval true  The names are equivalent
 * @retval false @p type is @c NULL, unrecognised, or does not match
 *
 * @note Complexity: @e O(1)
 */
static bool s_rules_parse_type(const char *type, uint16_t client_type)
{
    if (type == NULL) {
        return false;
    }

    if (safe_strcmp(type, "normal") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_NORMAL;
    }
    if (safe_strcmp(type, "dialog") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_DIALOG;
    }
    if (safe_strcmp(type, "toolbar") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_TOOLBAR;
    }
    if (safe_strcmp(type, "notification") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_NOTIFICATION;
    }
    if (safe_strcmp(type, "menu") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_MENU;
    }
    if (safe_strcmp(type, "desktop") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_DESKTOP;
    }
    if (safe_strcmp(type, "splash") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_SPLASH;
    }
    if (safe_strcmp(type, "utility") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_UTILITY;
    }
    if (safe_strcmp(type, "dock") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_DOCK;
    }

    return false;
}


/**
 * @brief Test whether a rule's timing constraint is satisfied
 *
 * Returns @c true when @p when is @c RULES_WHEN_BOTH, or when the
 * single-event value of @p when matches @p trigger.
 *
 * @param when    Timing constraint stored in the rule entry
 * @param trigger Event that caused the current evaluation
 *
 * @return Whether the constraint allows the rule to be evaluated now
 * @retval true  @p when is compatible with @p trigger
 * @retval false @p when is not compatible with @p trigger
 *
 * @note Complexity: @e O(1)
 */
static bool s_rules_when_matches(enum rules_when_e when,
        enum rules_trigger_e trigger)
{
    if (when == RULES_WHEN_BOTH) {
        return true;
    }

    if (trigger == RULES_TRIGGER_MAP) {
        return when == RULES_WHEN_MAP;
    }

    return when == RULES_WHEN_PROPERTY;
}


/**
 * @brief Test whether all match criteria of a rule entry match a client
 *
 * Each criterion that is flagged as present in @p match is evaluated
 * against the corresponding property of @p client.  The function
 * returns @c false as soon as any active criterion fails, so only
 * clients that satisfy every stated criterion will return @c true.
 *
 * @param match  Match criteria from the rule entry
 * @param client Client whose properties are tested
 *
 * @return Whether every active criterion in @p match is satisfied
 * @retval true  All active criteria match
 * @retval false At least one active criterion does not match
 *
 * @note Complexity: @e O(1) per criterion evaluated
 */
static bool s_rules_client_matches(const struct rules_match_s *match,
        const client_td *client)
{
    const char *instance = (client->info.class_name[0] != NULL)
        ? client->info.class_name[0] : "";
    const char *klass = (client->info.class_name[1] != NULL)
        ? client->info.class_name[1] : "";
    const char *role = (client->info.role_name != NULL)
        ? client->info.role_name : "";
    const char *title = (client->info.name != NULL)
        ? client->info.name : "";

    if (match->has_instance &&
            !s_rules_match_str(match->instance, instance)) {
        return false;
    }
    if (match->has_class && !s_rules_match_str(match->klass, klass)) {
        return false;
    }
    if (match->has_role && !s_rules_match_str(match->role, role)) {
        return false;
    }
    if (match->has_title && !s_rules_match_str(match->title, title)) {
        return false;
    }
    if (match->has_type && !s_rules_parse_type(match->type,
                client->properties.type)) {
        return false;
    }
    if (match->has_transient) {
        bool is_transient = client->transient_for != XCB_WINDOW_NONE;
        if (is_transient != match->transient) {
            return false;
        }
    }

    return true;
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
 * @p apply.  Each flag is only touched when its corresponding
 * @c has_* field is @c true.
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
            rule->apply.layer = s_rules_parse_layer(item->valuestring);
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

        if (!s_rules_when_matches(rule->when, trigger)) {
            continue;
        }

        if (!s_rules_client_matches(&rule->match, client)) {
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
