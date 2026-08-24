/**
 * @file rules/internal.h
 *
 * @brief Private type definitions shared across rules implementation
 *        modules
 *
 * Declares the internal structures and constants used by both
 * @c rules.c and @c rules/match.c that must not be exposed as part of
 * the public rules API in @c rules.h.
 *
 * @note This header is private to the rules subsystem and must not be
 *       included outside of @c src/rules/
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RULES_INTERNAL_H
#define RULES_INTERNAL_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Default initial values */
#include <defs/config.h>

/* Rules includes */
#include <rules.h>


/** Maximum number of rule entries stored in a single rules table */
#define RULES_MAX (256u)

/**
 * @brief Maximum alternative values a single match criterion can hold
 *        when given as a JSON array instead of a single string
 *
 * A window matches the criterion if it matches ANY one of these (an
 * "or" within the field); a rule with several different criteria
 * present (e.g., both @c title and @c class) still requires ALL of them
 * to match (an "and" across fields).
 *
 * @see @a ri_client_matches
 */
#define RULES_MATCH_MAX_VALUES (6u)

/**
 * @brief Timing constraint controlling when a rule is evaluated
 */
enum rules_when_e {
    RULES_WHEN_MAP = 0,     /**< Rule applies on @c MAP_REQUEST only */
    RULES_WHEN_PROPERTY,    /**< Rule applies on property change only */
    RULES_WHEN_BOTH         /**< Rule applies on both events */
};

/**
 * @brief Criteria used to match a client against one rule entry
 *
 * Each @p has_* criterion, when present, may hold one or more
 * alternative values (see @c RULES_MATCH_MAX_VALUES): the client
 * matches that criterion if it matches any one of them.  A client must
 * match every criterion that is present to match the rule as a whole.
 */
struct rules_match_s {
    bool has_instance;
    bool has_class;
    bool has_role;
    bool has_title;
    bool has_type;
    bool has_transient;

    uint8_t class_count;
    uint8_t instance_count;
    uint8_t role_count;
    uint8_t title_count;
    uint8_t type_count;

    char instance[RULES_MATCH_MAX_VALUES][CONFIG_MAX_LENGTH_NAME];
    char klass[RULES_MATCH_MAX_VALUES][CONFIG_MAX_LENGTH_NAME];
    char role[RULES_MATCH_MAX_VALUES][CONFIG_MAX_LENGTH_NAME];
    char title[RULES_MATCH_MAX_VALUES][CONFIG_MAX_LENGTH_NAME];
    char type[RULES_MATCH_MAX_VALUES][CONFIG_MAX_LENGTH_NAME];
    bool is_transient;
};

/**
 * @brief Actions to apply to a client when a rule entry matches
 *
 * Each field pairs with its own @p has_* flag: @p has_focus,
 * @p has_position, @p has_sticky, and so on say whether the rule
 * specifies a value for that action at all; the paired field itself
 * (@p is_focused, @p is_position_centered, @p is_pinned, and so on) is
 * the value to apply, meaningful only when its own @p has_* flag is
 * @c true. A rule that omits a field entirely leaves the client
 * unchanged for that one action.
 */
struct rules_apply_s {
    bool has_desktop;
    bool has_monitor;       /**< Target monitor within the client's
                                 own surface; see 'monitor' below */
    bool has_layer;
    bool has_focus;
    /** @c x and @c y, or @c is_position_centered, set independently
     *  of @c size */
    bool has_position;
    bool is_position_centered; /**< @c ("position": "center") was given
                                 instead of an @c ({x,y}) object: center
                                 the client on its screen at apply time
                                 instead of using @p x and @p y */
    bool has_size;          /**< @p width & @p height independent of
                                 position */
    bool has_sticky;
    bool has_decoration;
    bool has_opacity_active;
    bool has_opacity_inactive;

    uint32_t desktop;

    /**
     * @brief Index into the client's own surface's monitor list
     *
     * Named @p monitor, not @p screen': this project's own @p screen_id
     * / @p (screens[]) terminology refers to a whole X screen, and this
     * codebase has no notion of moving a client between X screens at
     * all, desktop reassignment above included, so a rule field with
     * that name would misleadingly suggest a capability that does not
     * exist.  Scoped to one physical monitor within the client's
     * current surface only.
     */
    uint32_t monitor;

    uint16_t layer;
    bool is_focused;
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
    bool is_pinned;
    bool is_decorated;

    /**
     * @brief Percentage, 0 to 100, overriding the theme's own 'window
     *
     * Applies to @p active.opacity / @p window.inactive.opacity for
     * this one client.
     *
     * @see @a config_theme_opacity_to_raw in @c config.h for how this
     *      reaches @c _NET_WM_WINDOW_OPACITY
     */
    uint8_t opacity_active;
    uint8_t opacity_inactive;
};

/**
 * @brief A single rule entry combining match criteria and the action to
 *        apply
 */
struct rules_rule_s {
    enum rules_when_e when;
    struct rules_match_s match;
    struct rules_apply_s apply;
};

/**
 * @brief Rules table holding all loaded rule entries and their count
 *
 * @note The type name @c rules_s is declared as opaque @c rules_td in
 *       the public header; internal modules use the full structure
 *       directly
 */
struct rules_s {
    uint32_t count;
    struct rules_rule_s rules[RULES_MAX];
};


/* Match engine (implemented in 'rules/match.c') */
/**
 * @brief Test whether a rule's timing constraint is satisfied
 *
 * @param when    Timing constraint stored in the rule entry
 * @param trigger Event that caused the current evaluation
 *
 * @return @c true when @p when is compatible with @p trigger
 *
 * @note Complexity: @e O(1)
 */
bool ri_when_matches(enum rules_when_e when,
        enum rules_trigger_e trigger);

/**
 * @brief Test whether all match criteria of a rule entry match a client
 *
 * @param match  Match criteria from the rule entry
 * @param client Client whose properties are tested
 *
 * @return @c true when every active criterion is satisfied
 *
 * @note Complexity: @e O(1) per criterion evaluated
 */
bool ri_client_matches(const struct rules_match_s *match,
        const client_td *client);

/**
 * @brief Convert a layer name string to the corresponding client layer
 *
 * @param layer Layer name string from the configuration
 *
 * @return Corresponding @c client_layer_e value cast to @c uint16_t;
 *         defaults to @c CLIENT_LAYER_NORMAL
 *
 * @note Complexity: @e O(1)
 */
uint16_t ri_parse_layer(const char *layer);


#endif  /* ! RULES_INTERNAL_H */
