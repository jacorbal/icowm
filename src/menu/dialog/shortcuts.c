/**
 * @file menu/dialog/shortcuts.c
 *
 * @brief Keyboard-shortcuts list dialog implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>      /* vsnprintf */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <config.h>
#include <surface.h>

/* Defs includes */
#include <defs/uistr.h>
#include <i18n.h>

/* Local includes */
#include <menu/dialog/shortcuts.h>
#include <menu/dialog/message.h>


/**
 * @brief Append one formatted line (plus a trailing newline) to a
 *        growing text buffer, silently doing nothing once it no
 *        longer fits
 *
 * @param buf     Buffer being built
 * @param buf_size Size of @p buf in bytes
 * @param offset  Current write offset into @p buf; advanced past
 *                whatever this call adds
 * @param fmt     'printf'-style format string for the line's content
 *                (the trailing newline is added automatically, do not
 *                include one)
 *
 * @note Complexity: @e O(n), where @e n is the formatted line's length
 */
static void s_append_line(char *buf, size_t buf_size, size_t *offset,
        const char *fmt, ...)
{
    va_list args;
    int written;

    if (*offset >= buf_size - 1u) {
        return;
    }

    va_start(args, fmt);
    written = vsnprintf(buf + *offset, buf_size - *offset, fmt, args);
    va_end(args);

    if (written < 0) {
        return;
    }
    if ((size_t) written >= buf_size - *offset) {
        /* Truncated: 'written' is how many bytes would have been
         * needed, not how many actually fit, so advancing by it
         * would push 'offset' past 'buf_size' and underflow the next
         * call's 'buf_size - *offset'.  Stop at the end of the buffer
         * instead. */
        *offset = buf_size - 1u;
        return;
    }
    *offset += (size_t) written;

    if (*offset < buf_size - 1u) {
        buf[*offset] = '\n';
        *offset += 1u;
        buf[*offset] = '\0';
    }
}


/**
 * @brief Append one blank line, for visual separation between sections
 *
 * @param buf     Buffer being built
 * @param buf_size Size of @p buf in bytes
 * @param offset  Current write offset into @p buf
 *
 * @note Complexity: @e O(1)
 */
static void s_append_blank_line(char *buf, size_t buf_size, size_t *offset)
{
    s_append_line(buf, buf_size, offset, "%s", "");
}


/**
 * @brief Append one "Label: combo" line, skipping it entirely when
 *        @p combo is empty (unbound)
 *
 * @param buf     Buffer being built
 * @param buf_size Size of @p buf in bytes
 * @param offset  Current write offset into @p buf
 * @param label   Human-readable label for this binding
 * @param combo   Configured binding string, or an empty string if
 *                this action has no binding
 *
 * @note Complexity: @e O(1)
 */
static void s_append_binding(char *buf, size_t buf_size, size_t *offset,
        const char *label, const char *combo)
{
    if (combo == NULL || combo[0] == '\0') {
        return;
    }

    s_append_line(buf, buf_size, offset, "%s: %s", label, combo);
}


/**
 * @brief Append the ten go-to-desktop bindings as one line when they
 *        all share a common prefix followed by their own digit, or
 *        as ten individual lines otherwise
 *
 * The default configuration follows the shared-prefix pattern (e.g.,
 * every one of them is @c "modc+mod1+" followed by its own digit), so
 * this is what keeps the common case to a single line instead of ten;
 * a person who rebound them to unrelated combinations still gets a
 * complete, correct listing, just a longer one.
 *
 * @param buf     Buffer being built
 * @param buf_size Size of @p buf in bytes
 * @param offset  Current write offset into @p buf
 * @param config  Active configuration
 *
 * @note Complexity: @e O(1), ten fixed-size string comparisons
 */
static void s_append_goto_desktop(char *buf, size_t buf_size,
        size_t *offset, const config_td *config)
{
    const char (*desktop)[CONFIG_MAX_LENGTH_BINDING] =
        config->bindings.keyboard.wm.go_to.desktop;
    size_t prefix_len = 0u;
    bool shared_prefix = (desktop[0][0] != '\0');

    if (shared_prefix) {
        prefix_len = safe_strlen(desktop[0]) - 1u;
        for (uint8_t i = 0u; i < 10u; ++i) {
            char expect_digit = (char) ('0' + i);

            if (safe_strlen(desktop[i]) != prefix_len + 1u ||
                    desktop[i][prefix_len] != expect_digit ||
                    safe_strncmp(desktop[i], desktop[0], prefix_len) != 0) {
                shared_prefix = false;
                break;
            }
        }
    }

    if (shared_prefix) {
        s_append_line(buf, buf_size, offset,
                _(STR_SHORTCUTS_GOTO_DESKTOP_RANGE_FMT), (int) prefix_len,
                desktop[0]);
        return;
    }

    for (uint8_t i = 0u; i < 10u; ++i) {
        if (desktop[i][0] == '\0') {
            continue;
        }
        s_append_line(buf, buf_size, offset, _(STR_SHORTCUTS_GOTO_DESKTOP_FMT),
                i, desktop[i]);
    }
}


/**
 * @brief Append one line combining several related bindings, skipping
 *        any of them that is empty (unbound)
 *
 * @param buf     Buffer being built
 * @param buf_size Size of @p buf in bytes
 * @param offset  Current write offset into @p buf
 * @param label   Human-readable label for the whole group
 * @param names   Short name for each binding in @p combos, same order
 * @param combos  Configured binding string for each entry
 * @param count   Number of entries in @p names and @p combos
 *
 * @note Complexity: @e O(n), where @e n is @p count
 */
static void s_append_group(char *buf, size_t buf_size, size_t *offset,
        const char *label, const char *const *names,
        const char *const *combos, uint8_t count)
{
    char line[DIALOG_MSG_LINE_MAX_LENGTH];
    size_t line_offset = 0u;
    bool any = false;

    line[0] = '\0';
    for (uint8_t i = 0u; i < count; ++i) {
        int written;

        if (combos[i] == NULL || combos[i][0] == '\0') {
            continue;
        }
        if (line_offset >= sizeof(line) - 1u) {
            break;
        }

        written = snprintf(line + line_offset, sizeof(line) - line_offset,
                "%s%s=%s", (any) ? ", " : "", names[i], combos[i]);
        if (written > 0) {
            /* Same truncation hazard as 's_append_line': clamp rather
             * than advance past 'line' on a truncated write. */
            line_offset += ((size_t) written < sizeof(line) - line_offset)
                ? (size_t) written : sizeof(line) - 1u - line_offset;
        }
        any = true;
    }

    if (any) {
        s_append_line(buf, buf_size, offset, "%s: %s", label, line);
    }
}


/* Open a message dialog listing every currently active keyboard
 * shortcut */
void dialog_shortcuts_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config)
{
    char text[DIALOG_MSG_RAW_MAX_LENGTH];
    size_t offset = 0u;

    if (connection == NULL || surface == NULL || config == NULL) {
        return;
    }

    text[0] = '\0';

    s_append_line(text, sizeof(text), &offset,
            "modc=%s, mods=%s, mod1=%s, mod4=%s",
            config->bindings.modc, config->bindings.mods,
            config->bindings.mod1, config->bindings.mod4);
    s_append_blank_line(text, sizeof(text), &offset);

    s_append_line(text, sizeof(text), &offset, "%s",
            _(STR_SHORTCUTS_HEADER_WM));
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_ROOT_MENU),
            config->bindings.keyboard.wm.menus.root);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_WINDOWS_MENU),
            config->bindings.keyboard.wm.menus.windows);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_SEARCH_WINDOWS),
            config->bindings.keyboard.wm.search);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_SHOW_DESKTOP),
            config->bindings.keyboard.wm.show_desktop);
    if (surface->desktop_count > 1u) {
        s_append_goto_desktop(text, sizeof(text), &offset, config);
    }
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_REDRAW),
            config->bindings.keyboard.wm.redraw);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_RELOAD_CONFIG),
            config->bindings.keyboard.wm.reload);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_QUIT),
            config->bindings.keyboard.wm.quit);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_THIS_LIST),
            config->bindings.keyboard.wm.shortcuts);
    if (config->base.shutdown.enable_emergency_shortcut) {
        s_append_line(text, sizeof(text), &offset, "%s",
                _(STR_SHORTCUTS_EMERGENCY_EXIT));
    }
    if (config->base.fortune.is_enabled) {
        s_append_binding(text, sizeof(text), &offset,
                _(STR_SHORTCUTS_FORTUNE),
                config->bindings.keyboard.wm.fortune);
    }
    if (config->base.scratchpad.is_enabled) {
        s_append_binding(text, sizeof(text), &offset,
                _(STR_SHORTCUTS_SCRATCHPAD),
                config->bindings.keyboard.wm.scratchpad);
    }

    s_append_blank_line(text, sizeof(text), &offset);
    s_append_line(text, sizeof(text), &offset, "%s",
            _(STR_SHORTCUTS_HEADER_LAUNCH));
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_TERMINAL),
            config->bindings.keyboard.launch.terminal);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_LAUNCHER),
            config->bindings.keyboard.launch.launcher);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_FILE_MANAGER),
            config->bindings.keyboard.launch.file_manager);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_WEB_BROWSER),
            config->bindings.keyboard.launch.web_browser);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_EDITOR),
            config->bindings.keyboard.launch.editor);

    s_append_blank_line(text, sizeof(text), &offset);
    s_append_line(text, sizeof(text), &offset, "%s",
            _(STR_SHORTCUTS_HEADER_WINDOW));
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_CLOSE),
            config->bindings.keyboard.window.close);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_KILL),
            config->bindings.keyboard.window.kill);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_DECORATE),
            config->bindings.keyboard.window.decorate);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_FULLSCREEN),
            config->bindings.keyboard.window.fullscreen);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_HIDE),
            config->bindings.keyboard.window.hide);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_ICONIFY),
            config->bindings.keyboard.window.iconify);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_ICONIFY_ALL),
            config->bindings.keyboard.window.iconify_all);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_DEICONIFY_ALL),
            config->bindings.keyboard.window.deiconify_all);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_ARRANGE),
            config->bindings.keyboard.window.arrange);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_INFO),
            config->bindings.keyboard.window.info);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_LAYER),
            config->bindings.keyboard.window.layer);
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_MAXIMIZE),
            config->bindings.keyboard.window.maximize);
    if (surface->monitor_count > 1u) {
        s_append_binding(text, sizeof(text), &offset,
                _(STR_SHORTCUTS_NEXT_MONITOR),
                config->bindings.keyboard.window.next_monitor);
    }
    if (surface->desktop_count > 1u) {
        s_append_binding(text, sizeof(text), &offset,
                _(STR_SHORTCUTS_PIN),
                config->bindings.keyboard.window.pin);
    }
    s_append_binding(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_SHADE),
            config->bindings.keyboard.window.shade);

    s_append_group(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_MOVE_RELATIVE),
            (const char *const []) {"Right", "Left", "Up", "Down"},
            (const char *const []) {
                config->bindings.keyboard.window.move.relative.right,
                config->bindings.keyboard.window.move.relative.left,
                config->bindings.keyboard.window.move.relative.up,
                config->bindings.keyboard.window.move.relative.down
            }, 4u);
    s_append_group(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_MOVE_ABSOLUTE),
            (const char *const [])
                {"Center", "TopLeft", "TopRight", "BotLeft", "BotRight"},
            (const char *const []) {
                config->bindings.keyboard.window.move.absolute.center,
                config->bindings.keyboard.window.move.absolute.top_left,
                config->bindings.keyboard.window.move.absolute.top_right,
                config->bindings.keyboard.window.move.absolute.bottom_left,
                config->bindings.keyboard.window.move.absolute.bottom_right
            }, 5u);
    s_append_group(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_RESIZE),
            (const char *const []) {"Right", "Left", "Up", "Down"},
            (const char *const []) {
                config->bindings.keyboard.window.resize.right,
                config->bindings.keyboard.window.resize.left,
                config->bindings.keyboard.window.resize.up,
                config->bindings.keyboard.window.resize.down
            }, 4u);

    s_append_blank_line(text, sizeof(text), &offset);
    s_append_line(text, sizeof(text), &offset, "%s",
            _(STR_SHORTCUTS_HEADER_CYCLE));
    if (surface->desktop_count > 1u) {
        s_append_group(text, sizeof(text), &offset,
                _(STR_SHORTCUTS_DESKTOPS),
                (const char *const []) {"prev", "next"},
                (const char *const []) {
                    config->bindings.keyboard.cycle.desktop.prev,
                    config->bindings.keyboard.cycle.desktop.next
                }, 2u);
    }
    s_append_group(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_ICONS),
            (const char *const []) {"prev", "next"},
            (const char *const []) {
                config->bindings.keyboard.cycle.icon.prev,
                config->bindings.keyboard.cycle.icon.next
            }, 2u);
    s_append_group(text, sizeof(text), &offset,
            _(STR_SHORTCUTS_WINDOWS),
            (const char *const []) {"prev", "next"},
            (const char *const []) {
                config->bindings.keyboard.cycle.window.prev,
                config->bindings.keyboard.cycle.window.next
            }, 2u);

    menu_message_dialog_show(connection, surface, config, text,
            MENU_MSG_LEVEL_NONE);
}
