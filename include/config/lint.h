/**
 * @file config/lint.h
 *
 * @brief Configuration file linter
 *
 * A schema-driven check for unknown keys in every JSON configuration
 * file IcoWM reads.  It exists to catch the class of mistake the
 * loaders themselves cannot: a misspelled or mistyped key is silently
 * ignored by @c json_load_string and friends (the field simply keeps
 * its default value), which is the right behavior for a window
 * manager starting up, but leaves a typo undetected until someone
 * notices the setting never took effect.
 *
 * Keys are matched the same way the real loaders match them (see
 * @c json_field_normalize in utils/config/json.h): case-insensitively,
 * with @c '-' and @c '_' treated as equivalent, so a key this checker
 * accepts is exactly one the loaders would also recognize, and vice
 * versa.
 *
 * A key whose name starts with @c '-' or @c '_' is treated as a
 * comment and never flagged, regardless of whether it matches
 * anything in the schema: fields such as @c "_comment" or
 * @c "-author" can be used freely to annotate a configuration file
 * without tripping the checker.
 *
 * A key with more than one genuinely different accepted shape (@c
 * topology.screens.desktops, being the only one so far, each entry
 * either a plain desktop or a whole per-screen block) is checked
 * against whichever of its shapes the first array element actually
 * looks like it is using, the same way the loader itself tells the
 * two apart, rather than left opaque: this still catches a typo
 * inside either shape without risking a false positive on the other.
 * A polymorphic key with no discernible shape at all would instead be
 * left opaque once recognized, undetectable from here, but no config
 * file this checker knows about currently needs that.
 *
 * Findings are grouped by file, each one headed by its name
 * (printed once, only if that file actually has something to report)
 * with every finding for it listed underneath, rather than a flat
 * list with the file repeated on every line: with as many files as
 * IcoWM reads, telling at a glance which file a given finding belongs
 * to matters more than it would for just one.
 *
 * @ingroup config
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CONFIG_LINT_H
#define CONFIG_LINT_H


/* Public interface */
/**
 * @brief Check every known JSON configuration file under @p config_dir
 *        for unknown and duplicate keys, printing one line per
 *        finding to @c stderr
 *
 * Files that do not exist under @p config_dir are silently skipped
 * (every configuration file is optional, falling back to built-in
 * defaults), except @c config.json itself, whose absence is reported
 * since it is where @c theme is set and therefore where a missing
 * file is most likely to be an oversight rather than a deliberate
 * all-defaults setup.  Every @c .json file found directly under
 * @c themes/ is checked, not only the one @c config.json's @c theme
 * field names.
 *
 * A key given twice within the same object is reported too, and the
 * report says which of the two takes effect: the first, since
 * @a json_get_item returns the first child whose name matches and the
 * loaders all go through it.  That is the opposite of what most
 * people expect from writing a line twice, so leaving it unreported
 * meant someone appending a corrected value saw no change, no error
 * and nothing in the log.  JSON itself only says names should be
 * unique and leaves the behavior undefined, so this is a decision
 * rather than a rule being followed.
 *
 * @param config_dir Configuration directory to check, the same one
 *                    @c -c selects for normal startup
 *
 * @return The number of unknown and duplicate keys found across every
 *         file, or a negative value if @p config_dir itself could not
 *         be read at all
 *
 * @note Complexity: @e O(n), where @e n is the total number of keys
 *       across every configuration file checked
 */
int config_lint_run(const char *config_dir);


#endif  /* ! CONFIG_LINT_H */
