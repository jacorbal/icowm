/**
 * @file rules.h
 *
 * @brief Window matching rules loader and applier
 *
 * Provides a persistent rules table that maps client properties to
 * actions applied when a window is first mapped or when one of its
 * ICCCM/EWMH properties changes.  Rules are loaded from a JSON file and
 * evaluated in declaration order; when multiple entries match the same
 * client, later ones take precedence for each individual field.
 *
 * @defgroup rules Window matching rules
 * @ingroup client
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RULES_RULES_H
#define RULES_RULES_H


/* System includes */
#include <stdbool.h>

/* Project includes */
#include <types/handles.h>


/**
 * @brief Event that causes a rule entry to be evaluated against a
 *        client
 */
enum rules_trigger_e {
    RULES_TRIGGER_MAP = 0,  /**< Triggered when a window is mapped */
    /** Triggered on an ICCCM or EWMH property change */
    RULES_TRIGGER_PROPERTY,
};


#ifndef RULES_TD_DECLARED
#define RULES_TD_DECLARED
/**
 * @brief Opaque window matching rules table
 */
typedef struct rules_s rules_td;
#endif


/* Public interface */
/**
 * @brief Allocate and zero-initialize a rules table
 *
 * Allocates a new @c rules_td structure and returns it ready for use
 * with @a rules_load.
 *
 * @return Pointer to the newly allocated rules table, or @c NULL on
 *         allocation failure
 *
 * @note The caller owns the returned pointer and must eventually pass
 *       it to @a rules_destroy
 * @note Complexity: @e O(1)
 */
rules_td *rules_init(void);

/**
 * @brief Destroy a rules table and free all of its resources
 *
 * @param rules Rules table to destroy, or @c NULL (no-op)
 *
 * @note Complexity: @e O(1)
 */
void rules_destroy(rules_td *rules);


/**
 * @brief Load window matching rules from a JSON configuration file
 *
 * Reads the rules file in the directory resolved from
 * @p config_dir_prefix (or the XDG/home default when @c NULL) and
 * populates @p rules with up to the internal maximum number of entries.
 * A missing or malformed file is silently treated as an empty rule set
 * so the window manager can start without one.
 *
 * @param rules Rules table to populate, previously returned
 *                          by @a rules_init
 * @param config_dir_prefix Path to the configuration directory, or
 *                          @c NULL to use the XDG default
 *
 * @return Status of the operation
 * @retval  0 Success (even when no rules file was found)
 * @retval  1 @p rules is @c NULL
 *
 * @note Complexity: @e O(n), where @e n is the number of rule entries
 *       found in the file
 */
int rules_load(rules_td *rules, const char *config_dir_prefix);

/**
 * @brief Apply all matching window rules to a client
 *
 * Iterates over every rule in @p wm->rules, merging the actions from
 * all entries that match the client's current properties and whose
 * @p when condition is compatible with @p trigger.  Later rules take
 * precedence for each individual field.  The merged action set is then
 * applied: desktop assignment, monitor assignment, stacking layer,
 * geometry, flags, and focus.
 *
 * @param wm       Pointer to the window manager singleton instance
 * @param client   Client to evaluate rules against
 * @param stage_io In/out pointer to the stage that currently owns
 *                   @p client; updated if a rule moves the client to a
 *                   desktop on a different stage
 * @param desktop_io In/out pointer to the desktop that currently owns
 *                   @p client; updated when a rule assigns a new
 *                   desktop
 * @param trigger Event that caused this evaluation
 *
 * @return Whether any rule matched and produced a visible change
 * @retval true  At least one rule matched and at least one property was
 *               altered
 * @retval false No rule matched, or any required pointer is invalid
 *
 * @note Complexity: @e O(n), where @e n is the number of loaded rules
 */
bool rules_apply(const wm_td *wm, client_td *client,
        stage_td **stage_io, desktop_td **desktop_io,
        enum rules_trigger_e trigger);


#endif  /* ! RULES_RULES_H */
