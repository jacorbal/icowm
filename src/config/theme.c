/**
 * @file config/theme.c
 *
 * @brief Theme configuration loader and defaults implementation
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

/* JSON includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/config/json.h>
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/client.h>
#include <defs/ctxmenu.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>


/**
 * @brief Parse a titlebar button name into its enumeration value
 *
 * @param name Button name as it appears in a theme's
 *             @p window.titlebar.buttons.left / @p .right list
 * @param out  Receives the parsed value; untouched if @p name is not
 *             a recognized button name
 *
 * @return @c true if @p name was recognized
 *
 * @note Complexity: @e O(1)
 */
static bool s_parse_titlebar_button(const char *name,
        enum config_titlebar_button_e *out)
{
    if (safe_strcmp(name, "pin") == 0) {
        *out = CONFIG_TITLEBAR_BUTTON_PIN;
    } else if (safe_strcmp(name, "layer") == 0) {
        *out = CONFIG_TITLEBAR_BUTTON_LAYER;
    } else if (safe_strcmp(name, "iconize") == 0) {
        *out = CONFIG_TITLEBAR_BUTTON_ICONIZE;
    } else if (safe_strcmp(name, "hide") == 0) {
        *out = CONFIG_TITLEBAR_BUTTON_HIDE;
    } else if (safe_strcmp(name, "shade") == 0) {
        *out = CONFIG_TITLEBAR_BUTTON_SHADE;
    } else if (safe_strcmp(name, "maximize") == 0) {
        *out = CONFIG_TITLEBAR_BUTTON_MAXIMIZE;
    } else if (safe_strcmp(name, "fullscreen") == 0) {
        *out = CONFIG_TITLEBAR_BUTTON_FULLSCREEN;
    } else if (safe_strcmp(name, "close") == 0) {
        *out = CONFIG_TITLEBAR_BUTTON_CLOSE;
    } else {
        return false;
    }

    return true;
}


/**
 * @brief Load a titlebar button list (@c left or @c right) from
 *        @a window.titlebar.buttons
 *
 * A button name the theme repeats is kept only for its first
 * occurrence; later repeats are silently skipped rather than
 * consuming another one of the limited @c CONFIG_MAX_TITLEBAR_BUTTONS
 * slots for a visual duplicate that would add nothing. An
 * unrecognized name is also silently skipped rather than aborting
 * the whole list; recognized names beyond
 * @c CONFIG_MAX_TITLEBAR_BUTTONS are also silently dropped.
 * Omitting a side entirely (or listing zero buttons on it) simply
 * means no buttons are drawn there.
 *
 * @param buttons_json Parsed @c "buttons" JSON object
 * @param key          @c left or @c right
 * @param dest         Destination array, sized
 *                     @c CONFIG_MAX_TITLEBAR_BUTTONS
 * @param count_out    Receives the number of buttons actually loaded
 *
 * @note Complexity: @e O(n * m), where @e n is the length of the
 *       JSON array and @e m is @c CONFIG_MAX_TITLEBAR_BUTTONS (the
 *       most any one candidate is ever compared against for the
 *       already-present check)
 */
static void s_load_button_list(cJSON *buttons_json, const char *key,
        enum config_titlebar_button_e *dest, uint8_t *count_out)
{
    cJSON *item;
    cJSON *elem;
    uint8_t n;

    item = cJSON_GetObjectItem(buttons_json, key);
    if (item == NULL || !cJSON_IsArray(item)) {
        return;
    }

    n = 0u;
    cJSON_ArrayForEach(elem, item) {
        enum config_titlebar_button_e btn;
        bool already_present;

        if (n >= (uint8_t) CONFIG_MAX_TITLEBAR_BUTTONS) {
            break;
        }
        if (!cJSON_IsString(elem) || elem->valuestring == NULL ||
                !s_parse_titlebar_button(elem->valuestring, &btn)) {
            continue;
        }

        already_present = false;
        for (uint8_t i = 0u; i < n; ++i) {
            if (dest[i] == btn) {
                already_present = true;
                break;
            }
        }
        if (already_present) {
            continue;
        }

        dest[n] = btn;
        ++n;
    }

    *count_out = n;
}


/**
 * @brief Parse @c window.titlebar.alignment text into its enumeration
 *
 * @param value Alignment string from configuration
 *
 * @return The parsed alignment, or @c CONFIG_TITLEBAR_ALIGN_LEFT (the
 *         default) for an unrecognized or missing value
 *
 * @note Supported values are @c left, @c center, and @c right
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_titlebar_alignment_e s_parse_titlebar_alignment(
        const char *value)
{
    if (value == NULL) {
        return CONFIG_TITLEBAR_ALIGN_LEFT;
    }
    if (safe_strcmp(value, "right") == 0) {
        return CONFIG_TITLEBAR_ALIGN_RIGHT;
    }
    if (safe_strcmp(value, "center") == 0) {
        return CONFIG_TITLEBAR_ALIGN_CENTER;
    }

    return CONFIG_TITLEBAR_ALIGN_LEFT;
}


/**
 * @brief Parse systray clock/battery text vertical alignment into
 *        configuration
 *
 * @param value Alignment text from configuration, e.g., @c "top"
 *
 * @return Parsed systray text vertical alignment enumeration value
 *
 * @note Supported values are @c center, @c top, and @c bottom
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_systray_text_valign_e s_parse_systray_text_valign(
        const char *value)
{
    if (value == NULL) {
        return CONFIG_SYSTRAY_TEXT_VALIGN_CENTER;
    }
    if (safe_strcmp(value, "top") == 0) {
        return CONFIG_SYSTRAY_TEXT_VALIGN_TOP;
    }
    if (safe_strcmp(value, "bottom") == 0) {
        return CONFIG_SYSTRAY_TEXT_VALIGN_BOTTOM;
    }

    return CONFIG_SYSTRAY_TEXT_VALIGN_CENTER;
}


/**
 * @brief Load one @c { font, color: {background, foreground},
 *        border: {color, width} } block, the shape shared by every
 *        themeable surface (window active/inactive, icon
 *        active/inactive, systray)
 *
 * @param json_obj Parsed JSON object holding the block
 * @param dest     Destination colors structure
 *
 * @note Complexity: @e O(1)
 */
static void s_load_theme_colors(cJSON *json_obj,
        struct config_theme_style_s *dest)
{
    cJSON *color_obj;
    cJSON *border_obj;

    if (json_obj == NULL) {
        return;
    }

    json_load_string(json_obj, "font", dest->font,
            CONFIG_MAX_LENGTH_FONTNAME);

    color_obj = cJSON_GetObjectItem(json_obj, "color");
    if (color_obj) {
        json_load_color(color_obj, "background",
                &dest->color.background);
        json_load_color(color_obj, "foreground",
                &dest->color.foreground);
    }

    border_obj = cJSON_GetObjectItem(json_obj, "border");
    if (border_obj) {
        json_load_color(border_obj, "color", &dest->border.color);
        json_load_uint(border_obj, "width", &dest->border.width);
    }
}


/**
 * @brief Load and clamp a 0 to 100 opacity percentage from a JSON
 *        object, into any one field that holds one
 *
 * Deliberately not part of @c s_load_theme_colors above: unlike
 * @c font/@c color/@c border, opacity only makes sense for a
 * @c config_theme_style_s instance that stands for one real,
 * distinct window or window-state on its own (@c window.active/
 * @c inactive, @c icon.active/@c inactive, @c systray.style,
 * @c overlay), never for one that styles a row or button drawn
 * inside a window shared with others (@c menu.unselected/@c
 * selected/@c label, @c dialog.button.unselected/@c selected):
 * '_NET_WM_WINDOW_OPACITY' is a per-window property, so it cannot
 * vary per row or per button the way those share one window's own
 * background/border colors can.  Called individually, only at the
 * sites where it is actually meaningful, rather than folded into the
 * shared loader every one of those sites already calls.
 *
 * @param json_obj Object that may contain @p key
 * @param key      Key name to look up
 * @param dest     Destination; left untouched if @p key is absent or
 *                 not a number
 *
 * @note Complexity: @e O(1)
 */
static void s_load_theme_opacity(cJSON *json_obj, const char *key,
        uint8_t *dest)
{
    unsigned int raw;

    raw = *dest;
    if (json_load_uint(json_obj, key, &raw) == 0) {
        *dest = (uint8_t) ((raw > 100u) ? 100u : raw);
    }
}


/* Populate default values for one theme structure, used both as the
 * compiled-in fallback theme and, before applying any theme file
 * found, as the known-good starting point that file's own fields
 * then overlay */
void config_set_default_theme_values(struct config_theme_s *theme)
{
    LOGGER_TRACE("Setting default theme", L_NARG);
    /* Left empty here on purpose, rather than a name like "Default
     * theme" outright: 'ci_config_resolve_theme_name' (config.c)
     * settles on the final name afterward, once it knows whether a
     * theme file was actually loaded and whether that file set its
     * own "name" (empty here means it did not), and this field
     * staying empty is exactly the signal it checks for that. */
    theme->name[0] = '\0';

    theme->window.is_decorated = true;
    theme->window.titlebar.height = 22u;
    theme->window.titlebar.alignment = CONFIG_TITLEBAR_ALIGN_CENTER;
    theme->window.titlebar.padding.horizontal = 2u;
    theme->window.titlebar.padding.vertical = 2u;

    theme->window.titlebar.buttons.left[0] =
        CONFIG_TITLEBAR_BUTTON_PIN;
    theme->window.titlebar.buttons.left[1] =
        CONFIG_TITLEBAR_BUTTON_LAYER;
    theme->window.titlebar.buttons.left_count = 2u;

    theme->window.titlebar.buttons.right[0] =
        CONFIG_TITLEBAR_BUTTON_CLOSE;
    theme->window.titlebar.buttons.right[1] =
        CONFIG_TITLEBAR_BUTTON_MAXIMIZE;
    theme->window.titlebar.buttons.right[2] =
        CONFIG_TITLEBAR_BUTTON_SHADE;
    theme->window.titlebar.buttons.right[3] =
        CONFIG_TITLEBAR_BUTTON_ICONIZE;
    theme->window.titlebar.buttons.right_count = 4u;

    theme->window.titlebar.buttons.color.on =
        json_hex2uint32("253F60");
    theme->window.titlebar.buttons.color.off =
        json_hex2uint32("7086A0");

    safe_strncpy(theme->window.active.font,
            "fixed bold", sizeof(theme->window.active.font));
    theme->window.active.color.background =
        json_hex2uint32("9AAEC8");
    theme->window.active.color.foreground =
        json_hex2uint32("253040");
    theme->window.active.border.color = json_hex2uint32("4A5566");
    theme->window.active.border.width = 2u;
    theme->window.active.opacity = 100u;

    /* Same as 'window.active.border' by default: the scratchpad is
     * always undecorated (see 'scratchpad.h'), so this border is its
     * only themeable element */
    theme->scratchpad.border.color = json_hex2uint32("4A5566");
    theme->scratchpad.border.width = 2u;

    /* Deliberately outside 'window.active'/'inactive''s own cool
     * blue-gray family (see 'theme.cycle''s own doc comment,
     * config.h): a warm, muted amber, chosen specifically to still
     * read as distinct against this project's own default active/
     * inactive colors, not just a different shade of the same hue */
    theme->cycle.border.color = json_hex2uint32("C9A227");
    theme->cycle.border.width = 4u;

    safe_strncpy(theme->window.inactive.font,
            "fixed", sizeof(theme->window.inactive.font));
    theme->window.inactive.color.background =
        json_hex2uint32("D0D9E5");
    theme->window.inactive.color.foreground =
        json_hex2uint32("4A5566");
    theme->window.inactive.border.color = json_hex2uint32("7F9AB6");
    theme->window.inactive.border.width = 2u;
    theme->window.inactive.opacity = 100u;

    theme->icon.is_captioned = true;
    theme->icon.show_pixmaps = true;
    theme->icon.show_hints = true;

    safe_strncpy(theme->icon.active.font,
            "fixed bold", sizeof(theme->icon.active.font));
    theme->icon.active.color.background =
        json_hex2uint32("9AAEC8");
    theme->icon.active.color.foreground =
        json_hex2uint32("253040");
    theme->icon.active.border.color = json_hex2uint32("4A5566");
    theme->icon.active.border.width = 1u;
    theme->icon.active.opacity = 100u;

    safe_strncpy(theme->icon.inactive.font,
            "fixed", sizeof(theme->icon.inactive.font));
    theme->icon.inactive.color.background =
        json_hex2uint32("D0D9E5");
    theme->icon.inactive.color.foreground =
        json_hex2uint32("4A5566");
    theme->icon.inactive.border.color = json_hex2uint32("7F9AB6");
    theme->icon.inactive.border.width = 1u;
    theme->icon.inactive.opacity = 100u;

    safe_strncpy(theme->systray.style.font,
            "fixed bold", sizeof(theme->systray.style.font));
    theme->systray.style.color.background =
        json_hex2uint32("D0D9E5");
    theme->systray.style.color.foreground =
        json_hex2uint32("4A5566");
    theme->systray.style.border.color = json_hex2uint32("7F9AB6");
    theme->systray.style.border.width = 1u;
    theme->systray.style.opacity = 100u;
    theme->systray.height = 24u;
    theme->systray.pixmap.size = 24u;
    theme->systray.pixmap.padding = 4u;
    theme->systray.text.gap = 12u;
    theme->systray.text.valign = CONFIG_SYSTRAY_TEXT_VALIGN_CENTER;

    /* Same hue family (~213 degrees) as the rest of the theme's
     * D0D9E5/4A5566-family colors, but deliberately darker than the
     * UI chrome: a desktop background is a large, full-screen area
     * rather than a small UI element, so it wants a more neutral,
     * less attention-grabbing tone, and staying darker gives windows
     * placed on top of it more contrast to stand out against than a
     * light background would.  Landed on this specific value (rather
     * than an even darker one first tried) so it does not sit almost
     * as dark as the theme's own text/border colors, which left it
     * feeling heavier than a full-screen area calls for. */
    theme->desktop.color.background = json_hex2uint32("5F7187");

    safe_strncpy(theme->menu.unselected.font,
            "fixed", sizeof(theme->menu.unselected.font));
    theme->menu.unselected.color.background =
        json_hex2uint32("D0D9E5");
    theme->menu.unselected.color.foreground =
        json_hex2uint32("4A5566");
    theme->menu.unselected.border.color = json_hex2uint32("7F9AB6");
    theme->menu.unselected.border.width = 0u;

    safe_strncpy(theme->menu.selected.font,
            "fixed", sizeof(theme->menu.selected.font));
    theme->menu.selected.color.background =
        json_hex2uint32("9AAEC8");
    theme->menu.selected.color.foreground =
        json_hex2uint32("253040");
    theme->menu.selected.border.color = json_hex2uint32("4A5566");
    theme->menu.selected.border.width = 0u;

    safe_strncpy(theme->menu.label.font,
            "fixed", sizeof(theme->menu.label.font));
    theme->menu.label.color.background =
        json_hex2uint32("48607F");
    /* Picked for a WCAG contrast ratio of ~4.5:1 against this
     * background (the same bar as any other normal-weight text in
     * the theme): the border color this foreground used to reuse
     * only reached ~2:1 against a light background, too low for
     * text meant to be read normally rather than treated as a
     * de-emphasized secondary state; the ratio itself is the same
     * either way around, since contrast between two colors does not
     * depend on which one is foreground and which is background. */
    theme->menu.label.color.foreground =
        json_hex2uint32("D0D9E5");
    theme->menu.label.border.color = json_hex2uint32("7F9AB6");
    theme->menu.label.border.width = 0u;

    /* Picked for a WCAG contrast ratio of ~3:1 against the menu's own
     * background: low enough to still read as visibly de-emphasized
     * (this is disabled, secondary text, not meant to compete with
     * normal menu text), but not the ~1.7:1 the previous color gave,
     * which was too low to reliably read as text at all. */
    theme->menu.disabled_foreground = json_hex2uint32("717B88");
    theme->menu.separator_color = json_hex2uint32("7F9AB6");
    theme->menu.border.color = json_hex2uint32("7F9AB6");
    theme->menu.border.width = 2u;
    theme->menu.opacity = 100u;
    theme->menu.padding.horizontal = (uint32_t) WM_CTXMENU_PAD_X;
    theme->menu.padding.vertical = (uint32_t) WM_CTXMENU_PAD_Y;
    theme->menu.show_pixmaps = true;

    safe_strncpy(theme->search.input.font,
            "fixed", sizeof(theme->search.input.font));
    theme->search.input.color.background = json_hex2uint32("9AAEC8");
    theme->search.input.color.foreground = json_hex2uint32("253040");
    theme->search.input.opacity = 100u;
    safe_strncpy(theme->search.unselected.font,
            "fixed", sizeof(theme->search.unselected.font));
    theme->search.unselected.color.background =
        json_hex2uint32("D0D9E5");
    theme->search.unselected.color.foreground =
        json_hex2uint32("4A5566");
    theme->search.unselected.opacity = 100u;
    safe_strncpy(theme->search.selected.font,
            "fixed", sizeof(theme->search.selected.font));
    theme->search.selected.color.background = json_hex2uint32("9AAEC8");
    theme->search.selected.color.foreground = json_hex2uint32("253040");
    theme->search.selected.opacity = 100u;
    theme->search.border.color = json_hex2uint32("7F9AB6");
    theme->search.border.width = 2u;

    safe_strncpy(theme->prompt.label.font,
            "fixed bold", sizeof(theme->prompt.label.font));
    theme->prompt.label.color.background = json_hex2uint32("9AAEC8");
    theme->prompt.label.color.foreground = json_hex2uint32("253040");
    theme->prompt.label.opacity = 100u;
    safe_strncpy(theme->prompt.input.font,
            "fixed", sizeof(theme->prompt.input.font));
    theme->prompt.input.color.background = json_hex2uint32("9AAEC8");
    theme->prompt.input.color.foreground = json_hex2uint32("253040");
    theme->prompt.input.opacity = 100u;
    theme->prompt.border.color = json_hex2uint32("7F9AB6");
    theme->prompt.border.width = 2u;

    theme->dialog.background = json_hex2uint32("D0D9E5");
    theme->dialog.border.color = json_hex2uint32("7F9AB6");
    theme->dialog.border.width = 2u;
    theme->dialog.opacity = 100u;

    safe_strncpy(theme->dialog.label.font,
            "fixed bold", sizeof(theme->dialog.label.font));
    theme->dialog.label.foreground = json_hex2uint32("4A5566");
    theme->dialog.label.padding.horizontal = 12u;
    theme->dialog.label.padding.vertical = 12u;

    safe_strncpy(theme->dialog.button.unselected.font,
            "fixed", sizeof(theme->dialog.button.unselected.font));
    theme->dialog.button.unselected.color.background =
        json_hex2uint32("D0D9E5");
    theme->dialog.button.unselected.color.foreground =
        json_hex2uint32("4A5566");
    theme->dialog.button.unselected.border.color =
        json_hex2uint32("7F9AB6");
    theme->dialog.button.unselected.border.width = 1u;

    safe_strncpy(theme->dialog.button.selected.font,
            "fixed bold", sizeof(theme->dialog.button.selected.font));
    theme->dialog.button.selected.color.background =
        json_hex2uint32("9AAEC8");
    theme->dialog.button.selected.color.foreground =
        json_hex2uint32("253040");
    theme->dialog.button.selected.border.color =
        json_hex2uint32("4A5566");
    theme->dialog.button.selected.border.width = 1u;

    theme->dialog.button.gap = 24u;
    theme->dialog.button.padding.horizontal = 12u;
    theme->dialog.button.padding.vertical = 6u;

    safe_strncpy(theme->overlay.font, "fixed", sizeof(theme->overlay.font));
    theme->overlay.color.background = json_hex2uint32("D0D9E5");
    theme->overlay.color.foreground = json_hex2uint32("4A5566");
    theme->overlay.border.color = json_hex2uint32("7F9AB6");
    theme->overlay.border.width = 1u;
    theme->overlay.opacity = 100u;

    theme->xsettings.is_enabled = false;
    theme->xsettings.dpi = 96u;
    safe_strncpy(theme->xsettings.theme.gtk_theme_name, "Adwaita",
            CONFIG_MAX_LENGTH_NAME);
    safe_strncpy(theme->xsettings.theme.icon_theme_name, "Adwaita",
            CONFIG_MAX_LENGTH_NAME);
    safe_strncpy(theme->xsettings.theme.cursor_theme_name,
            "Adwaita", CONFIG_MAX_LENGTH_NAME);
    theme->xsettings.theme.cursor_theme_size = 24u;
}


/* Convert a 0-100 opacity percentage to _NET_WM_WINDOW_OPACITY's own
 * 32-bit range */
uint32_t config_theme_opacity_to_raw(uint8_t percent)
{
    uint8_t clamped;

    clamped = (percent > 100u) ? 100u : percent;

    return (uint32_t) ((double) clamped / 100.0 * (double) 0xffffffffu);
}


/* Load theme configuration */
int config_load_theme(const char *filename,
        struct config_theme_s *config_theme)
{
    cJSON *json;
    cJSON *window;
    cJSON *icon;
    cJSON *systray;
    cJSON *desktop;
    cJSON *menu;
    cJSON *search;
    cJSON *prompt;
    cJSON *dialog;
    cJSON *overlay;
    cJSON *xsettings;
    cJSON *scratchpad;
    cJSON *sp_border;
    cJSON *cycle;
    cJSON *cycle_border;

    LOGGER_TRACE("Parsing theme configuration from file '%s'",
            filename);

    if (json_load_config(filename, &json) != 0) {
        return 1;
    }

    json_load_string(json, "name", config_theme->name,
            sizeof(config_theme->name));

    window = cJSON_GetObjectItem(json, "window");
    if (window) {
        cJSON *titlebar;
        cJSON *active;
        cJSON *inactive;

        json_load_bool(window, "is-decorated",
                &config_theme->window.is_decorated);

        titlebar = cJSON_GetObjectItem(window, "titlebar");
        if (titlebar) {
            cJSON *alignment_item;
            cJSON *padding;
            cJSON *buttons;

            json_load_uint(titlebar, "height",
                    &config_theme->window.titlebar.height);

            alignment_item = json_get_item(titlebar, "alignment");
            if (alignment_item != NULL && cJSON_IsString(alignment_item)) {
                config_theme->window.titlebar.alignment =
                    s_parse_titlebar_alignment(
                            alignment_item->valuestring);
            }

            padding = cJSON_GetObjectItem(titlebar, "padding");
            if (padding) {
                json_load_uint(padding, "horizontal",
                        &config_theme->window.titlebar.padding.horizontal);
                json_load_uint(padding, "vertical",
                        &config_theme->window.titlebar.padding.vertical);
            }

            /* A titlebar shorter than its own buttons plus their
             * vertical padding would draw those buttons overflowing
             * its own bounds instead of centered within them (see
             * 'client_titlebar_layout''s own fallback branch,
             * client/geom.c, taken once 'title_h' falls below this
             * exact threshold); floored here to that same threshold,
             * unless 'height' is 0, which disables the titlebar
             * entirely and is left alone. */
            if (config_theme->window.titlebar.height != 0u) {
                uint32_t btn_floor = (uint32_t) WM_DECOR_BTN_SIZE +
                    2u * config_theme->window.titlebar.padding.vertical;

                if (config_theme->window.titlebar.height < btn_floor) {
                    config_theme->window.titlebar.height = btn_floor;
                }
            }

            buttons = cJSON_GetObjectItem(titlebar, "buttons");
            if (buttons) {
                cJSON *btn_color;

                s_load_button_list(buttons, "left",
                        config_theme->window.titlebar.buttons.left,
                        &config_theme->window.titlebar.buttons.left_count);
                s_load_button_list(buttons, "right",
                        config_theme->window.titlebar.buttons.right,
                        &config_theme->window.titlebar.buttons.right_count);

                btn_color = cJSON_GetObjectItem(buttons, "color");
                if (btn_color) {
                    json_load_color(btn_color, "on",
                            &config_theme->window.titlebar.buttons.color.on);
                    json_load_color(btn_color, "off",
                            &config_theme->window.titlebar.buttons.color.off);
                }
            }
        }

        active = cJSON_GetObjectItem(window, "active");
        s_load_theme_colors(active, &config_theme->window.active);
        s_load_theme_opacity(active, "opacity",
                &config_theme->window.active.opacity);

        inactive = cJSON_GetObjectItem(window, "inactive");
        s_load_theme_colors(inactive, &config_theme->window.inactive);
        s_load_theme_opacity(inactive, "opacity",
                &config_theme->window.inactive.opacity);
    }

    icon = cJSON_GetObjectItem(json, "icon");
    if (icon) {
        cJSON *active;
        cJSON *inactive;

        json_load_bool(icon, "is-captioned",
                &config_theme->icon.is_captioned);
        json_load_bool(icon, "show-pixmaps",
                &config_theme->icon.show_pixmaps);
        json_load_bool(icon, "show-hints",
                &config_theme->icon.show_hints);

        active = cJSON_GetObjectItem(icon, "active");
        s_load_theme_colors(active, &config_theme->icon.active);
        s_load_theme_opacity(active, "opacity",
                &config_theme->icon.active.opacity);

        inactive = cJSON_GetObjectItem(icon, "inactive");
        s_load_theme_colors(inactive, &config_theme->icon.inactive);
        s_load_theme_opacity(inactive, "opacity",
                &config_theme->icon.inactive.opacity);
    }

    systray = cJSON_GetObjectItem(json, "systray");
    s_load_theme_colors(systray, &config_theme->systray.style);
    s_load_theme_opacity(systray, "opacity",
            &config_theme->systray.style.opacity);
    if (systray) {
        cJSON *pixmap_obj;
        cJSON *text_obj;

        json_load_uint(systray, "height", &config_theme->systray.height);

        pixmap_obj = cJSON_GetObjectItem(systray, "pixmap");
        if (pixmap_obj) {
            json_load_uint(pixmap_obj, "size",
                    &config_theme->systray.pixmap.size);
            json_load_uint(pixmap_obj, "padding",
                    &config_theme->systray.pixmap.padding);
        }

        text_obj = cJSON_GetObjectItem(systray, "text");
        if (text_obj) {
            cJSON *valign_item;

            json_load_uint(text_obj, "gap",
                    &config_theme->systray.text.gap);
            valign_item = json_get_item(text_obj, "valign");
            if (valign_item != NULL && cJSON_IsString(valign_item)) {
                config_theme->systray.text.valign =
                    s_parse_systray_text_valign(valign_item->valuestring);
            }
        }
    }

    desktop = cJSON_GetObjectItem(json, "desktop");
    if (desktop) {
        cJSON *color_obj;

        color_obj = cJSON_GetObjectItem(desktop, "color");
        if (color_obj) {
            json_load_color(color_obj, "background",
                    &config_theme->desktop.color.background);
        }
    }

    menu = cJSON_GetObjectItem(json, "menu");
    if (menu) {
        cJSON *unselected;
        cJSON *selected;
        cJSON *label;
        cJSON *disabled;
        cJSON *separator;
        cJSON *border_obj;
        cJSON *padding;

        unselected = cJSON_GetObjectItem(menu, "unselected");
        s_load_theme_colors(unselected, &config_theme->menu.unselected);

        selected = cJSON_GetObjectItem(menu, "selected");
        s_load_theme_colors(selected, &config_theme->menu.selected);

        label = cJSON_GetObjectItem(menu, "label");
        s_load_theme_colors(label, &config_theme->menu.label);

        disabled = cJSON_GetObjectItem(menu, "disabled");
        if (disabled) {
            cJSON *const disabled_color = cJSON_GetObjectItem(disabled, "color");
            if (disabled_color) {
                json_load_color(disabled_color, "foreground",
                        &config_theme->menu.disabled_foreground);
            }
        }

        separator = cJSON_GetObjectItem(menu, "separator");
        if (separator) {
            json_load_color(separator, "color",
                    &config_theme->menu.separator_color);
        }

        border_obj = cJSON_GetObjectItem(menu, "border");
        if (border_obj) {
            json_load_color(border_obj, "color",
                    &config_theme->menu.border.color);
            json_load_uint(border_obj, "width",
                    &config_theme->menu.border.width);
        }

        s_load_theme_opacity(menu, "opacity",
                &config_theme->menu.opacity);

        padding = cJSON_GetObjectItem(menu, "padding");
        if (padding) {
            json_load_uint(padding, "horizontal",
                    &config_theme->menu.padding.horizontal);
            json_load_uint(padding, "vertical",
                    &config_theme->menu.padding.vertical);
        }

        json_load_bool(menu, "show-pixmaps",
                &config_theme->menu.show_pixmaps);
    }

    search = cJSON_GetObjectItem(json, "search");
    if (search) {
        cJSON *input;
        cJSON *unselected;
        cJSON *selected;
        cJSON *border_obj;

        input = cJSON_GetObjectItem(search, "input");
        s_load_theme_colors(input, &config_theme->search.input);

        unselected = cJSON_GetObjectItem(search, "unselected");
        s_load_theme_colors(unselected, &config_theme->search.unselected);

        selected = cJSON_GetObjectItem(search, "selected");
        s_load_theme_colors(selected, &config_theme->search.selected);

        border_obj = cJSON_GetObjectItem(search, "border");
        if (border_obj) {
            json_load_color(border_obj, "color",
                    &config_theme->search.border.color);
            json_load_uint(border_obj, "width",
                    &config_theme->search.border.width);
        }
    }

    prompt = cJSON_GetObjectItem(json, "prompt");
    if (prompt) {
        cJSON *label;
        cJSON *input;
        cJSON *border_obj;

        label = cJSON_GetObjectItem(prompt, "label");
        s_load_theme_colors(label, &config_theme->prompt.label);

        input = cJSON_GetObjectItem(prompt, "input");
        s_load_theme_colors(input, &config_theme->prompt.input);

        border_obj = cJSON_GetObjectItem(prompt, "border");
        if (border_obj) {
            json_load_color(border_obj, "color",
                    &config_theme->prompt.border.color);
            json_load_uint(border_obj, "width",
                    &config_theme->prompt.border.width);
        }
    }

    dialog = cJSON_GetObjectItem(json, "dialog");
    if (dialog) {
        cJSON *dialog_color;
        cJSON *border_obj;
        cJSON *label;
        cJSON *button;

        dialog_color = cJSON_GetObjectItem(dialog, "color");
        if (dialog_color) {
            json_load_color(dialog_color, "background",
                    &config_theme->dialog.background);
        }

        border_obj = cJSON_GetObjectItem(dialog, "border");
        if (border_obj) {
            json_load_color(border_obj, "color",
                    &config_theme->dialog.border.color);
            json_load_uint(border_obj, "width",
                    &config_theme->dialog.border.width);
        }

        s_load_theme_opacity(dialog, "opacity",
                &config_theme->dialog.opacity);

        label = cJSON_GetObjectItem(dialog, "label");
        if (label) {
            cJSON *label_color;
            cJSON *label_padding;

            json_load_string(label, "font", config_theme->dialog.label.font,
                    CONFIG_MAX_LENGTH_FONTNAME);
            label_color = cJSON_GetObjectItem(label, "color");
            if (label_color) {
                json_load_color(label_color, "foreground",
                        &config_theme->dialog.label.foreground);
            }
            label_padding = cJSON_GetObjectItem(label, "padding");
            if (label_padding) {
                json_load_uint(label_padding, "horizontal",
                        &config_theme->dialog.label.padding.horizontal);
                json_load_uint(label_padding, "vertical",
                        &config_theme->dialog.label.padding.vertical);
            }
        }

        button = cJSON_GetObjectItem(dialog, "button");
        if (button) {
            cJSON *const btn_unselected = cJSON_GetObjectItem(button,
                    "unselected");
            cJSON *const btn_selected = cJSON_GetObjectItem(button, "selected");
            cJSON *const btn_padding = cJSON_GetObjectItem(button, "padding");

            s_load_theme_colors(btn_unselected,
                    &config_theme->dialog.button.unselected);
            s_load_theme_colors(btn_selected,
                    &config_theme->dialog.button.selected);
            json_load_uint(button, "gap",
                    &config_theme->dialog.button.gap);
            if (btn_padding) {
                json_load_uint(btn_padding, "horizontal",
                        &config_theme->dialog.button.padding.horizontal);
                json_load_uint(btn_padding, "vertical",
                        &config_theme->dialog.button.padding.vertical);
            }
        }
    }

    overlay = cJSON_GetObjectItem(json, "overlay");
    s_load_theme_colors(overlay, &config_theme->overlay);
    s_load_theme_opacity(overlay, "opacity",
            &config_theme->overlay.opacity);

    xsettings = cJSON_GetObjectItem(json, "xsettings");
    if (xsettings) {
        unsigned int dpi_val;
        unsigned int cursor_size_val;
        cJSON *xs_theme;

        json_load_bool(xsettings, "is-enabled",
                &config_theme->xsettings.is_enabled);
        if (json_load_uint(xsettings, "dpi", &dpi_val) == 0) {
            config_theme->xsettings.dpi = dpi_val;
        }

        xs_theme = cJSON_GetObjectItem(xsettings, "theme");
        if (xs_theme) {
            json_load_string(xs_theme, "gtk-theme-name",
                    config_theme->xsettings.theme.gtk_theme_name,
                    CONFIG_MAX_LENGTH_NAME);
            json_load_string(xs_theme, "icon-theme-name",
                    config_theme->xsettings.theme.icon_theme_name,
                    CONFIG_MAX_LENGTH_NAME);
            json_load_string(xs_theme, "cursor-theme-name",
                    config_theme->xsettings.theme.cursor_theme_name,
                    CONFIG_MAX_LENGTH_NAME);
            if (json_load_uint(xs_theme, "cursor-theme-size",
                        &cursor_size_val) == 0) {
                config_theme->xsettings.theme.cursor_theme_size =
                    cursor_size_val;
            }
        }
    }

    scratchpad = cJSON_GetObjectItem(json, "scratchpad");

    sp_border = (scratchpad != NULL)
        ? cJSON_GetObjectItem(scratchpad, "border") : NULL;
    if (sp_border) {
        json_load_color(sp_border, "color",
                &config_theme->scratchpad.border.color);
        json_load_uint(sp_border, "width",
                &config_theme->scratchpad.border.width);
    }

    cycle = cJSON_GetObjectItem(json, "cycle");

    cycle_border = (cycle != NULL)
        ? cJSON_GetObjectItem(cycle, "border") : NULL;
    if (cycle_border) {
        json_load_color(cycle_border, "color",
                &config_theme->cycle.border.color);
        json_load_uint(cycle_border, "width",
                &config_theme->cycle.border.width);
    }

    cJSON_Delete(json);

    return 0;
}
