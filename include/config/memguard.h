/**
 * @file config/memguard.h
 *
 * @brief Public API for restricted-memory mode's own configuration
 *        profile (@c memguard.json)
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CONFIG_MEMGUARD_H
#define CONFIG_MEMGUARD_H


/* Project includes */
#include <config.h>


/**
 * @brief Allocate a new configuration structure, without populating it
 *
 * A thin counterpart to @c config_init in config.h, for restricted-
 * memory mode's own separate path: allocation only, deliberately not
 * followed by @a config_set_default_values_memguard here, since
 * @a config_load_memguard already calls that itself as its own first
 * step.  Calling it here too would just mean setting every field twice
 * for no reason.
 *
 * @return Pointer to the newly allocated (but not yet populated)
 *         configuration structure, or @c NULL on failure
 *
 * @note Complexity: @e O(1)
 */
config_td *config_init_memguard(void);

/**
 * @brief Populate @p config with restricted-memory mode's own fixed
 *        profile, the starting point @a config_load_memguard applies
 *        @c memguard.json on top of
 *
 * A completely separate profile from @a config_set_default_values's own
 * ordinary defaults, not a variant of it: restricted-memory mode is
 * meant to restrict a fixed, deliberately chosen set of behavior (no
 * aesthetic extras, one screen, one desktop, and so on) regardless of
 * what an ordinary session would otherwise default to, so the two are
 * kept as two independent functions rather than one with
 * restricted-memory branches woven through it.  Every field this
 * profile fixes outright (desktop count, icon placement policy, menu
 * position, and the rest documented alongside @a config_load_memguard
 * itself) is set here unconditionally; every field @c memguard.json is
 * still allowed to configure (the active theme's name, launched
 * programs, desktop margins, the systray block, and the emergency
 * shortcut) is set here too, as that field's own fallback, and
 * @a config_load_memguard's own @c memguard.json parsing pass may still
 * overwrite it afterward.
 *
 * @param config Configuration structure to populate; a null @p config
 *               is a no-op
 *
 * @note Complexity: @e O(1), a fixed number of fields
 */
void config_set_default_values_memguard(config_td *config);

/**
 * @brief Load restricted-memory mode's own configuration, entirely
 *        independent of @a config_load's own @c config.json path
 *
 * Populates @p config with @a config_set_default_values_memguard's own
 * fixed profile first, then layers exactly four things on top of it, in
 * order: @c memguard.json (the only file restricted-memory mode lets
 * a person configure at all, covering the active theme's name, launched
 * programs, desktop margins, the systray block, and the emergency
 * shortcut), @c bindings.json (key and mouse bindings, loaded the same
 * way an ordinary session loads them), the theme file @c memguard.json
 * named (colors and decoration load normally, but see below), and
 * finally this mode's own restrictions on top of whatever that theme
 * specified.
 *
 * @c config.json and @c randr.json are never read at all in this mode,
 * not even as a fallback.  RandR output-profile management stays
 * disabled outright, and every setting @c config.json would otherwise
 * carry either has no counterpart in @c memguard.json's own smaller
 * schema or is one of @a config_set_default_values_memguard's own fixed
 * values instead.
 *
 * The theme restrictions applied after loading: any font not already
 * naming some variant of the "fixed" X core font family (allowing
 * a caller to still pick its size or encoding, e.g., @c fixed-14 or
 * a full XLFD string) is replaced outright with plain @c fixed,
 * @c xsettings publishing is turned off, and both @c icon.show-pixmaps
 * and @c icon.show-hints are forced off; every other theme field
 * (colors, decoration, @c is-captioned included) is left exactly as the
 * theme file specified, since a compiled-in theme occupies the same
 * memory as one read from a file and there is nothing to save by
 * skipping it.
 *
 * @param config        Configuration structure to populate
 * @param config_prefix Configuration directory prefix, or @c NULL to
 *                      resolve it the same way @a config_load does
 *
 * @return Status of the operation
 * @retval  0 on success
 * @retval  1 if @c memguard.json itself could not be loaded (the fixed
 *          profile and whatever @c bindings.json and the theme file did
 *          load are still applied)
 *
 * @note Complexity: @e O(n), where @e n is the combined size of
 *       @c memguard.json, @c bindings.json, and the active theme file
 */
int config_load_memguard(config_td *config, const char *config_prefix);


#endif  /* ! CONFIG_MEMGUARD_H */
