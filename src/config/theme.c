/**
 * @file config/theme.c
 *
 * @brief Theme configuration loader implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* JSON includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/config/json.h>
#include <utils/safe/safestr.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>


/**
 * @brief Parse a titlebar button name into its enumeration value
 *
 * @param name  Button name as it appears in a theme's
 *              @c window.titlebar.buttons.left / .right list
 * @param out   Receives the parsed value; untouched if @p name is not
 *              a recognised button name
 *
 * @return @c true if @p name was recognised
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
 * @brief Load a titlebar button list (@c "left" or @c "right") from
 *        @c window.titlebar.buttons
 *
 * A button name the theme repeats, or does not recognise, is silently
 * skipped rather than aborting the whole list; recognised names beyond
 * @c CONFIG_MAX_TITLEBAR_BUTTONS are also silently dropped.  Omitting
 * a side entirely (or listing zero buttons on it) simply means no
 * buttons are drawn there.
 *
 * @param buttons_json Parsed @c "buttons" JSON object
 * @param key          @c left or @c right
 * @param dest         Destination array, sized
 *                     @c CONFIG_MAX_TITLEBAR_BUTTONS
 * @param count_out    Receives the number of buttons actually loaded
 *
 * @note Complexity: @e O(n), where @e n is
 *       @c CONFIG_MAX_TITLEBAR_BUTTONS
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

        if (n >= (uint8_t) CONFIG_MAX_TITLEBAR_BUTTONS) {
            break;
        }
        if (cJSON_IsString(elem) && elem->valuestring != NULL &&
                s_parse_titlebar_button(elem->valuestring, &btn)) {
            dest[n] = btn;
            ++n;
        }
    }

    *count_out = n;
}


/**
 * @brief Parse @c window.titlebar.alignment text into its enumeration
 *
 * @param value Alignment string from configuration
 *
 * @return The parsed alignment, or @c CONFIG_TITLEBAR_ALIGN_LEFT (the
 *         default) for an unrecognised or missing value
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
 * @brief Parse systray clock vertical alignment text into configuration
 *
 * @param value Alignment text from configuration, e.g., @c "top"
 *
 * @return Parsed systray clock vertical alignment enumeration value
 *
 * @note Supported values are @c center, @c top, and @c bottom
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_systray_clock_valign_e
    s_parse_systray_clock_valign(const char *value)
{
    if (value == NULL) {
        return CONFIG_SYSTRAY_CLOCK_VALIGN_CENTER;
    }
    if (safe_strcmp(value, "top") == 0) {
        return CONFIG_SYSTRAY_CLOCK_VALIGN_TOP;
    }
    if (safe_strcmp(value, "bottom") == 0) {
        return CONFIG_SYSTRAY_CLOCK_VALIGN_BOTTOM;
    }

    return CONFIG_SYSTRAY_CLOCK_VALIGN_CENTER;
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


/* Load theme configuration */
int config_load_theme(const char *filename,
        struct config_theme_s *config_theme)
{
    cJSON *json;
    cJSON *window;
    cJSON *icon;
    cJSON *systray;
    cJSON *menu;
    cJSON *dialog;
    cJSON *overlay;

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

            buttons = cJSON_GetObjectItem(titlebar, "buttons");
            if (buttons) {
                s_load_button_list(buttons, "left",
                        config_theme->window.titlebar.buttons.left,
                        &config_theme->window.titlebar.buttons.left_count);
                s_load_button_list(buttons, "right",
                        config_theme->window.titlebar.buttons.right,
                        &config_theme->window.titlebar.buttons.right_count);
            }
        }

        active = cJSON_GetObjectItem(window, "active");
        s_load_theme_colors(active, &config_theme->window.active);

        inactive = cJSON_GetObjectItem(window, "inactive");
        s_load_theme_colors(inactive, &config_theme->window.inactive);
    }

    icon = cJSON_GetObjectItem(json, "icon");
    if (icon) {
        cJSON *active;
        cJSON *inactive;

        json_load_bool(icon, "is-captioned",
                &config_theme->icon.is_captioned);

        active = cJSON_GetObjectItem(icon, "active");
        s_load_theme_colors(active, &config_theme->icon.active);

        inactive = cJSON_GetObjectItem(icon, "inactive");
        s_load_theme_colors(inactive, &config_theme->icon.inactive);
    }

    systray = cJSON_GetObjectItem(json, "systray");
    s_load_theme_colors(systray, &config_theme->systray.style);
    if (systray) {
        cJSON *clock_obj;
        cJSON *valign_item;

        json_load_uint(systray, "height", &config_theme->systray.height);

        clock_obj = cJSON_GetObjectItem(systray, "clock");
        if (clock_obj) {
            valign_item = json_get_item(clock_obj, "valign");
            if (valign_item != NULL && cJSON_IsString(valign_item)) {
                config_theme->systray.clock.valign =
                    s_parse_systray_clock_valign(valign_item->valuestring);
            }
        }
    }

    menu = cJSON_GetObjectItem(json, "menu");
    if (menu) {
        cJSON *unselected;
        cJSON *selected;
        cJSON *disabled;
        cJSON *separator;

        unselected = cJSON_GetObjectItem(menu, "unselected");
        s_load_theme_colors(unselected, &config_theme->menu.unselected);

        selected = cJSON_GetObjectItem(menu, "selected");
        s_load_theme_colors(selected, &config_theme->menu.selected);

        disabled = cJSON_GetObjectItem(menu, "disabled");
        if (disabled) {
            cJSON *disabled_color = cJSON_GetObjectItem(disabled, "color");
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

        label = cJSON_GetObjectItem(dialog, "label");
        if (label) {
            cJSON *label_color;

            json_load_string(label, "font", config_theme->dialog.label.font,
                    CONFIG_MAX_LENGTH_FONTNAME);
            label_color = cJSON_GetObjectItem(label, "color");
            if (label_color) {
                json_load_color(label_color, "foreground",
                        &config_theme->dialog.label.foreground);
            }
        }

        button = cJSON_GetObjectItem(dialog, "button");
        if (button) {
            cJSON *btn_unselected = cJSON_GetObjectItem(button,
                    "unselected");
            cJSON *btn_selected = cJSON_GetObjectItem(button, "selected");

            s_load_theme_colors(btn_unselected,
                    &config_theme->dialog.button.unselected);
            s_load_theme_colors(btn_selected,
                    &config_theme->dialog.button.selected);
        }
    }

    overlay = cJSON_GetObjectItem(json, "overlay");
    s_load_theme_colors(overlay, &config_theme->overlay);

    cJSON_Delete(json);

    return 0;
}
