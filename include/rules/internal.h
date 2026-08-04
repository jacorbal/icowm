/**
 * @file rules/internal.h
 *
 * @brief Private type definitions shared across rules implementation
 *        modules
 *
 * Declares the internal structures and constants used by both
 * @c rules/rules.c and @c rules/match.c that must not be exposed as
 * part of the public rules API in @c rules/rules.h.
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

/* Public rules header (for rules_trigger_e) */
#include <rules/rules.h>


/** Maximum number of rule entries stored in a single rules table */
#define RULES_MAX (256u)

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
 */
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

/**
 * @brief Actions to apply to a client when a rule entry matches
 */
struct rules_apply_s {
    bool has_desktop;
    bool has_layer;
    bool has_focus;
    bool has_position;  /**< @c x & @c y set independently of @c size */
    bool has_size;      /**< @c width & @c height independent of position */
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

/**
 * @brief A single rule entry combining match criteria and the action
 *        to apply
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
 *       the public header; internal modules use the full struct directly
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
