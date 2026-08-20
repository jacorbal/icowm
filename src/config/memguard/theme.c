/**
 * @file config/memguard/theme.c
 *
 * @brief Restricted-memory mode's own theme restrictions
 *        implementation
 *
 * Split out of @c config/memguard.c to keep that file focused on
 * orchestrating restricted-memory mode's own config loading, not on
 * any one loaded file's own contents.
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
#include <string.h>     /* strchr, strcspn; NULL, size_t */

/* Utils includes */
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/config.h>
#include <defs/memguard.h>

/* Local includes */
#include <config.h>
#include <config/internal.h>


/**
 * @brief Whether @p font already names some variant of the @c "fixed"
 *        X core font family
 *
 * Recognizes both forms a theme's own font field can hold: a simple
 * alias, where the family is the leading word up to the first space
 * or hyphen (e.g., @c "fixed", @c "fixed bold", @c "fixed-14"), and a
 * full XLFD pattern, where the family is the second @c '-'-delimited
 * field (e.g., @c "-misc-fixed-bold-r-normal--0-120-75-75-c-0-iso10646-1").
 * Either form lets a person still pick a specific size or encoding
 * while staying on the light X core rendering path, rather than the
 * plain literal strings @c "fixed" and @c "fixed bold" alone.
 *
 * @param font Font field to check, e.g., @c
 *             config->theme.window.active.font
 *
 * @return @c true if @p font's own family is exactly the lowercase
 *         @c "fixed", case-sensitive
 *
 * @note Deliberately case-sensitive, not case-insensitive: a real
 *       Xft-only family can be named e.g., @c "Fixed Bold", capitalized
 *       and visually similar but a different, heavier font entirely,
 *       distinct from the plain lowercase @c "fixed bold" this
 *       restriction is actually meant to leave alone.  A case-
 *       insensitive match would wrongly let that Xft family through
 *       untouched instead of substituting it.
 * @note Complexity: @e O(n), where @e n is the length of @p font
 */
static bool s_memguard_is_fixed_variant(const char *font)
{
    const char *family_start;
    const char *family_end;
    size_t family_len;

    if (font == NULL || font[0] == '\0') {
        return false;
    }

    if (font[0] == '-') {
        const char *dash1 = strchr(font + 1, '-');

        if (dash1 == NULL) {
            return false;
        }
        family_start = dash1 + 1;
        family_end = strchr(family_start, '-');
    } else {
        size_t stop = strcspn(font, " -");

        family_start = font;
        family_end = (font[stop] != '\0') ? font + stop : NULL;
    }

    family_len = (family_end != NULL)
        ? (size_t) (family_end - family_start)
        : safe_strlen(family_start);

    return (family_len == 5u) &&
        (safe_strncmp(family_start, "fixed", 5u) == 0);
}


/* Apply restricted-memory mode's own theme restrictions on top of
 * whatever config->theme was just loaded from */
void ci_memguard_restrict_theme(config_td *config)
{
    char *const font_fields[] = {
        config->theme.dialog.button.selected.font,
        config->theme.dialog.button.unselected.font,
        config->theme.dialog.label.font,
        config->theme.icon.active.font,
        config->theme.icon.inactive.font,
        config->theme.menu.label.font,
        config->theme.menu.selected.font,
        config->theme.menu.unselected.font,
        config->theme.overlay.font,
        config->theme.systray.style.font,
        config->theme.window.active.font,
        config->theme.window.inactive.font,
    };

    for (size_t i = 0u; i < sizeof(font_fields) / sizeof(font_fields[0]);
            ++i) {
        if (!s_memguard_is_fixed_variant(font_fields[i])) {
            safe_strncpy(font_fields[i], MEMGUARD_FONT_NAME,
                    CONFIG_MAX_LENGTH_FONTNAME);
        }
    }

    config->theme.xsettings.is_enabled = false;
    config->theme.icon.show_pixmaps = false;
    config->theme.icon.show_hints = false;
    config->theme.menu.show_pixmaps = false;
}
