/**
 * @file config/lint.c
 *
 * @brief Configuration file linter implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>      /* fprintf, snprintf */

/* Third-party includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/config/json.h>
#include <utils/safe/safestr.h>

/* Local includes */
#include <config/lint.h>
#include <config/lint/a11y.h>
#include <config/lint/bindings.h>
#include <config/lint/config.h>
#include <config/lint/internal.h>
#include <config/lint/memguard.h>
#include <config/lint/misc.h>
#include <config/lint/theme.h>


/**
 * @brief Shared state threaded through one file's recursive schema
 *        check
 *
 * Lets every finding for a file be grouped under that file's name,
 * printed once as a header right before the first finding rather than
 * repeated on every line, which matters once more than one file has
 * something to report.
 */
typedef struct {
    const char *display_name; /**< Name to head the report with; may
                                   differ from the file's bare name on
                                   disk (see 'themes/' entries, headed
                                   by 'themes/<name>.json') */
    int unknown_count;
    int duplicate_count;
    bool header_printed;
} config_lint_report_td;


/* 'config.json' schema */


/* 'bindings.json schema' */


/* `theme.json` schema: validated to the same full depth as every other
 *      fixed-shape file ('config.json', 'bindings.json', 'a11y.json').
 *      Unlike 'randr.json''s "outputs" or 'rules.json''s
 *      "rules", nothing under theme.json is genuinely polymorphic (see
 *      the opaque-subtree rule in 'config/lint.h' for what that means
 *      and why it does not apply here).  Every field's shape is
 *      fixed and known ahead of time, so there is no risk of a false
 *      positive on a legitimate but less common shape the way there
 *      would be for those. */


/* memguard.json schema: a stricter subset of config.json's,
 *      since restricted-memory mode accepts fewer fields per section
 *      than an ordinary session does (see 'config.md' §10.1) */


static const config_lint_file_spec_td s_files[] = {
    {"config.json", s_schema_config,
        sizeof(s_schema_config) / sizeof(s_schema_config[0]), true},
    {"bindings.json", s_schema_bindings,
        sizeof(s_schema_bindings) / sizeof(s_schema_bindings[0]), false},
    {"randr.json", s_schema_randr,
        sizeof(s_schema_randr) / sizeof(s_schema_randr[0]), false},
    {"rules.json", s_schema_rules,
        sizeof(s_schema_rules) / sizeof(s_schema_rules[0]), false},
    {"session.json", s_schema_session,
        sizeof(s_schema_session) / sizeof(s_schema_session[0]), false},
    {"menu.json", s_schema_menu,
        sizeof(s_schema_menu) / sizeof(s_schema_menu[0]), false},
    {"a11y.json", s_schema_a11y,
        sizeof(s_schema_a11y) / sizeof(s_schema_a11y[0]),
        false},
    {"memguard.json", s_schema_memguard,
        sizeof(s_schema_memguard) / sizeof(s_schema_memguard[0]),
        false}
};


/**
 * @brief Count how often a key's name appears in its object, and how
 *        often before this occurrence of it
 *
 * Compared the way every other name comparison in this file is,
 * normalized, so that @c move-step and @c move_step count as the same
 * key rather than as two.
 *
 * @param obj    Object being walked
 * @param item   Child currently reached in that walk
 * @param before Receives how many earlier children carry the same
 *               name; may be @c NULL
 *
 * @return How many children of @p obj carry that name in all
 *
 * @note Complexity: @e O(n), where @e n is the number of children of
 *       @p obj
 */
static int s_key_occurrences(const cJSON *obj, const cJSON *item,
        int *before)
{
    const cJSON *other;
    char item_norm[JSON_FIELD_MAX];
    char other_norm[JSON_FIELD_MAX];
    bool reached = false;
    int total = 0;

    if (before != NULL) {
        *before = 0;
    }

    if (item->string == NULL ||
            !json_field_normalize(item->string, item_norm,
                sizeof(item_norm))) {
        return 0;
    }

    cJSON_ArrayForEach(other, obj) {
        if (other == item) {
            reached = true;
        }
        if (other->string == NULL ||
                !json_field_normalize(other->string, other_norm,
                    sizeof(other_norm))) {
            continue;
        }
        if (safe_strcmp(item_norm, other_norm) != 0) {
            continue;
        }

        total++;
        if (!reached && before != NULL) {
            *before += 1;
        }
    }

    return total;
}


/**
 * @brief Find the schema entry matching @p key, if any
 *
 * Matches the same way the real loaders do.  Case-insensitively, with
 * @c '-' and @c '_' treated as equivalent.
 *
 * @param key      Key name as it appears in the JSON file
 * @param schema   Schema entries to search
 * @param schema_count Number of entries in @p schema
 *
 * @return The matching entry, or @c NULL if none matches
 *
 * @note Complexity: @e O(n), where @e n is @p schema_count
 */
static const config_lint_key_td *s_find_key(const char *key,
        const config_lint_key_td *schema, size_t schema_count)
{
    char key_norm[JSON_FIELD_MAX];
    char entry_norm[JSON_FIELD_MAX];

    if (!json_field_normalize(key, key_norm, sizeof(key_norm))) {
        return NULL;
    }

    for (size_t i = 0u; i < schema_count; ++i) {
        if (!json_field_normalize(schema[i].name, entry_norm,
                    sizeof(entry_norm))) {
            continue;
        }
        if (safe_strcmp(key_norm, entry_norm) == 0) {
            return &schema[i];
        }
    }

    return NULL;
}


/**
 * @brief Recursively check one JSON object's keys against a schema
 *
 * @param obj          JSON object to check
 * @param schema       Schema entries valid at this level
 * @param schema_count Number of entries in @p schema
 * @param path         Dotted path to @p obj so far, for the report
 * @param report       This file's shared report state; the header is
 *                      printed here, lazily, on the first finding
 *
 * @note Complexity: @e O(n), where @e n is the number of keys in
 *       @p obj and everything nested under it
 */
static void s_lint_object(const cJSON *obj,
        const config_lint_key_td *schema, size_t schema_count,
        const char *path, config_lint_report_td *report)
{
    const cJSON *item;

    /* The cast is cJSON's doing, not this file's.  Its predicates
     * take a non-const pointer even though they only ever read, so
     * asking one of them a question about a 'const cJSON *' cannot be
     * done without dropping the qualifier here. */
    if (obj == NULL || !cJSON_IsObject((cJSON *) obj)) {
        return;
    }

    cJSON_ArrayForEach(item, obj) {
        char child_path[JSON_FIELD_MAX * 2u];
        const config_lint_key_td *match;
        int occurrences;
        int earlier;

        if (item->string == NULL) {
            continue;
        }
        if (item->string[0] == '-' || item->string[0] == '_') {
            continue;
        }

        match = s_find_key(item->string, schema, schema_count);
        (void) snprintf(child_path, sizeof(child_path), "%s%s%s", path,
                (path[0] != '\0') ? "." : "", item->string);

        occurrences = s_key_occurrences(obj, item, &earlier);
        if (occurrences > 1) {
            /* Reported once per repeated key, on reaching its second
             * occurrence, rather than once per repetition.  A key
             * given four times is one mistake to fix, not three. */
            if (earlier == 1) {
                if (!report->header_printed) {
                    fprintf(stderr, "%s:\n", report->display_name);
                    report->header_printed = true;
                }
                fprintf(stderr, "  %s: duplicate key (%d times), only"
                        " the first is used\n", child_path,
                        occurrences);
                report->duplicate_count += 1;
            }
            continue;
        }

        if (match == NULL) {
            if (!report->header_printed) {
                fprintf(stderr, "%s:\n", report->display_name);
                report->header_printed = true;
            }
            fprintf(stderr, "  %s: unknown key\n", child_path);
            report->unknown_count += 1;
            continue;
        }

        if (match->children != NULL && cJSON_IsObject((cJSON *) item)) {
            s_lint_object(item, match->children, match->children_count,
                    child_path, report);
        }
    }
}


/**
 * @brief Check one configuration file against its schema
 *
 * @param config_dir   Configuration directory the file lives under
 * @param spec         This file's name, schema, and whether it must
 *                      exist
 * @param display_name Name to head the report with if anything is
 *                      found; distinct from @c spec->filename so a
 *                      theme file can be headed by its path relative
 *                      to @p config_dir, such as
 *                      @c "themes/default.json", rather than its
 *                      bare name alone
 * @param unknown_count Running count of unknown keys found; advanced
 *                      by this call
 * @param duplicate_count Running count of duplicated keys found;
 *                      advanced by this call
 *
 * @note Complexity: @e O(n), where @e n is the number of keys in the
 *       file
 */
static void s_lint_file(const char *restrict config_dir,
        const config_lint_file_spec_td *spec,
        const char *restrict display_name,
        int *unknown_count, int *duplicate_count)
{
    char path[512];
    cJSON *json = NULL;
    config_lint_report_td report;

    (void) snprintf(path, sizeof(path), "%s/%s", config_dir,
            spec->filename);

    if (json_load_config(path, &json) != 0 || json == NULL) {
        if (spec->required) {
            fprintf(stderr, "%s:\n  file not found or unreadable" \
                    " ('%s')\n", display_name, path);
        }
        return;
    }

    report.display_name = display_name;
    report.header_printed = false;
    report.unknown_count = 0;
    report.duplicate_count = 0;

    if (cJSON_IsObject(json)) {
        s_lint_object(json, spec->schema, spec->schema_count, "",
                &report);
    }

    *unknown_count += report.unknown_count;
    *duplicate_count += report.duplicate_count;

    cJSON_Delete(json);
}


/**
 * @brief Check every @c *.json file directly under
 *        @c <config_dir>/themes against the theme schema
 *
 * @param config_dir Configuration directory the @c themes
 *                    subdirectory lives under
 * @param unknown_count Running count of unknown keys found; advanced
 *                      by this call
 * @param duplicate_count Running count of duplicated keys found;
 *                      advanced by this call
 *
 * @note A missing @c themes subdirectory is not reported
 * @note Unlike @c config.json, having no themes of one's (using only
 *       whichever theme name @c config.json's @c theme field names,
 *       which may resolve to a built-in default elsewhere) is an
 *       entirely ordinary setup, not an oversight
 * @note Complexity: @e O(n), where @e n is the total number of keys
 *       across every theme file found
 */
static void s_lint_themes(const char *config_dir,
        int *unknown_count, int *duplicate_count)
{
    char themes_dir[512];
    DIR *dir;
    const struct dirent *entry;

    (void) snprintf(themes_dir, sizeof(themes_dir), "%s/themes",
            config_dir);

    dir = opendir(themes_dir);
    if (dir == NULL) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        config_lint_file_spec_td spec;
        char display_name[512];
        size_t name_len = safe_strlen(entry->d_name);

        if (name_len < 6u ||
                safe_strcmp(entry->d_name + name_len - 5u, ".json") != 0) {
            continue;
        }

        spec.filename = entry->d_name;
        spec.schema = s_schema_theme;
        spec.schema_count =
            sizeof(s_schema_theme) / sizeof(s_schema_theme[0]);
        spec.required = false;

        (void) snprintf(display_name, sizeof(display_name),
                "themes/%s", entry->d_name);
        s_lint_file(themes_dir, &spec, display_name, unknown_count,
                duplicate_count);
    }

    closedir(dir);
}


/* Check every known JSON configuration file for unknown keys */
int config_lint_run(const char *config_dir)
{
    DIR *dir;
    int unknown_count = 0;
    int duplicate_count = 0;

    if (config_dir == NULL) {
        return -1;
    }

    dir = opendir(config_dir);
    if (dir == NULL) {
        /* Quoted so a stray leading character (e.g., an '=' from
         * typing '-c=<dir>', a GNU long-option convention getopt does
         * not apply to a short option like '-c') stands out rather
         * than blending into the surrounding text */
        fprintf(stderr, "'%s': cannot open configuration directory\n",
                config_dir);
        return -1;
    }
    closedir(dir);

    for (size_t i = 0u; i < sizeof(s_files) / sizeof(s_files[0]); ++i) {
        s_lint_file(config_dir, &s_files[i], s_files[i].filename,
                &unknown_count, &duplicate_count);
    }
    s_lint_themes(config_dir, &unknown_count, &duplicate_count);

    if (unknown_count == 0) {
        fprintf(stderr, "No unknown keys found.\n");
    } else {
        fprintf(stderr, "%d unknown key%s found.\n", unknown_count,
                (unknown_count == 1) ? "" : "s");
    }

    /* Only reported when there are any, unlike the line above.  A
     * duplicate is rare enough that saying so every time would be
     * noise on an otherwise clean run */
    if (duplicate_count != 0) {
        fprintf(stderr, "%d duplicate key%s found.\n", duplicate_count,
                (duplicate_count == 1) ? "" : "s");
    }

    /* Both counted against the exit status.  A duplicate key in a
     * hand-written file is a mistake every time, and catching such a
     * mistake is what this option is for */
    return unknown_count + duplicate_count;
}
