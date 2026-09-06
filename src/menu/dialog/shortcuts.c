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

/* Defauilt initial values */
#include <defs/uistr.h>

/* Project includes */
#include <config.h>
#include <i18n.h>
#include <surface.h>

/* Local includes */
#include <menu/dialog/shortcuts.h>
#include <menu/dialog/message.h>


/**
 * @brief What the three helpers below are filling in
 *
 * The dialog is handed pairs rather than one block of text, so that
 * every binding's combination lines up in a column of its own however
 * long the label beside it turned out in the language it was translated
 * into.
 *
 * A label and a combination both come from storage that outlives this
 * dialog, the message catalogue and the configuration, so those are
 * kept as pointers.  Only a line composed here needs @p store to
 * survive the call that built it.
 *
 * @note The pointer count and the store share one index, since
 *       a composed line occupies one of each
 */
struct s_shortcuts_ctx_s {
    struct dialog_pair_s pairs[DIALOG_MSG_MAX_LINES];
    char store[DIALOG_MSG_MAX_LINES][DIALOG_MSG_LINE_MAX_LENGTH];
    char lstore[DIALOG_MSG_MAX_LINES][DIALOG_MSG_LINE_MAX_LENGTH];
    uint8_t count;
};


/**
 * @brief Append one formatted line (plus a trailing newline) to
 *        a growing text buffer, silently doing nothing once it no
 *        longer fits
 *
 * @param ctx Rows being gathered
 * @param fmt @c printf-style format string for the line's content (the
 *            trailing newline is added automatically, do not include
 *            one)
 * @param ... Arguments @p fmt consumes
 *
 * @note Complexity: @e O(n), where @e n is the formatted line's length
 */
static void s_append_line(struct s_shortcuts_ctx_s *ctx,
        const char *restrict fmt, ...)
{
    va_list args;

    if (ctx->count >= (uint8_t) DIALOG_MSG_MAX_LINES) {
        return;
    }

    va_start(args, fmt);
    (void) vsnprintf(ctx->store[ctx->count],
            sizeof(ctx->store[ctx->count]), fmt, args);
    va_end(args);

    ctx->pairs[ctx->count].label = ctx->store[ctx->count];
    ctx->pairs[ctx->count].value = NULL;
    ctx->count++;
}


/**
 * @brief Append a row whose value has to be composed here
 *
 * For the rows whose right-hand side is built rather than taken whole
 * from the configuration: a run of grouped bindings, or a desktop
 * number folded into its combination.  The label is kept as a pointer
 * like any other, since it comes from the message catalogue.
 *
 * @param ctx   Rows being gathered
 * @param label Left column, copied rather than pointed at
 * @param fmt   'printf'-style format for the right column
 * @param ...   Arguments @p fmt consumes
 *
 * @note Silently does nothing once the dialog is full, as its
 *       neighbours do
 * @note Complexity: @e O(n), where @e n is the composed value's length
 */
static void s_append_pair_fmt(struct s_shortcuts_ctx_s *ctx,
        const char *restrict label, const char *restrict fmt, ...)
{
    va_list args;

    if (ctx->count >= (uint8_t) DIALOG_MSG_MAX_LINES) {
        return;
    }

    va_start(args, fmt);
    (void) vsnprintf(ctx->store[ctx->count],
            sizeof(ctx->store[ctx->count]), fmt, args);
    va_end(args);

    /* The label is copied rather than pointed at: a caller composing
     * one on its own stack, as the desktop rows do, would otherwise
     * leave this holding a pointer into a frame already gone */
    (void) safe_strncpy(ctx->lstore[ctx->count], label,
            sizeof(ctx->lstore[ctx->count]));
    ctx->pairs[ctx->count].label = ctx->lstore[ctx->count];
    ctx->pairs[ctx->count].value = ctx->store[ctx->count];
    ctx->count++;
}


/**
 * @brief Append one label-and-combination row, skipping it entirely
 *        when @p combo is empty (unbound)
 *
 * @param ctx   Rows being gathered
 * @param label Human-readable label for this binding
 * @param combo Configured binding string, or an empty string if this
 *              action has no binding
 *
 * @note Complexity: @e O(1)
 */
static void s_append_binding(struct s_shortcuts_ctx_s *ctx,
        const char *restrict label, const char *restrict combo)
{
    if (combo == NULL || combo[0] == '\0' ||
            ctx->count >= (uint8_t) DIALOG_MSG_MAX_LINES) {
        return;
    }

    ctx->pairs[ctx->count].label = label;
    ctx->pairs[ctx->count].value = combo;
    ctx->count++;
}


/**
 * @brief Append the ten go-to-desktop bindings as one line when they
 *        all share a common prefix followed by their digit, or as ten
 *        individual lines otherwise
 *
 * The default configuration follows the shared-prefix pattern (e.g.,
 * every one of them is @c "modc+mod1+" followed by its digit), so this
 * is what keeps the common case to a single line instead of ten; a user
 * who rebound them to unrelated combinations still gets a complete,
 * correct listing, just a longer one.
 *
 * @param ctx    Rows being gathered
 * @param config Active configuration
 *
 * @note Complexity: @e O(1), ten fixed-size string comparisons
 */
static void s_append_goto_desktop(struct s_shortcuts_ctx_s *ctx,
        const config_td *config)
{
    const char (*desktop)[CONFIG_MAX_LENGTH_BINDING] =
        config->bindings.keyboard.desktop.go_to.desktop;
    size_t prefix_len = 0u;
    bool shared_prefix = (desktop[0][0] != '\0');

    if (shared_prefix) {
        prefix_len = safe_strlen(desktop[0]) - 1u;
        for (uint8_t i = 0u; i < 10u; ++i) {
            char expect_digit = (char) ('0' + i);

            if (safe_strlen(desktop[i]) != prefix_len + 1u ||
                    desktop[i][prefix_len] != expect_digit ||
                    safe_strncmp(desktop[i], desktop[0],
                        prefix_len) != 0) {
                shared_prefix = false;
                break;
            }
        }
    }

    if (shared_prefix) {
        s_append_pair_fmt(ctx, _(STR_SHORTCUTS_GOTO_DESKTOP_RANGE),
                "%.*s<0-9>", (int) prefix_len, desktop[0]);
        return;
    }

    for (uint8_t i = 0u; i < 10u; ++i) {
        char label[DIALOG_MSG_LINE_MAX_LENGTH];

        if (desktop[i][0] == '\0') {
            continue;
        }

        (void) snprintf(label, sizeof(label),
                _(STR_SHORTCUTS_GOTO_DESKTOP_FMT), i);
        s_append_pair_fmt(ctx, label, "%s", desktop[i]);
    }
}


/**
 * @brief Append the viewport go-to-page bindings as one line when they
 *        all share a common prefix followed by their digit, or as
 *        individual lines otherwise, the same idea as
 *        @a s_append_goto_desktop above, just over @p page_count
 *        pages instead of a fixed ten desktops
 *
 * @param ctx        Rows being gathered
 * @param config     Active configuration
 * @param page_count Number of viewport pages actually laid out
 *                   (@c columns times @c rows), capped by the caller
 *                   to the 10 bindings that exist at all
 *
 * @note Complexity: @e O(1), at most ten fixed-size string
 *       comparisons
 */
static void s_append_goto_viewport(struct s_shortcuts_ctx_s *ctx,
        const config_td *config, uint32_t page_count)
{
    const char (*page)[CONFIG_MAX_LENGTH_BINDING] =
        config->bindings.keyboard.viewport.go_to.page;
    size_t prefix_len = 0u;
    bool shared_prefix = (page_count > 0u && page[0][0] != '\0');

    if (shared_prefix) {
        prefix_len = safe_strlen(page[0]) - 1u;
        for (uint32_t i = 0u; i < page_count; ++i) {
            char expect_digit = (char) ('0' + i);

            if (safe_strlen(page[i]) != prefix_len + 1u ||
                    page[i][prefix_len] != expect_digit ||
                    safe_strncmp(page[i], page[0], prefix_len) != 0) {
                shared_prefix = false;
                break;
            }
        }
    }

    if (shared_prefix) {
        s_append_pair_fmt(ctx, _(STR_SHORTCUTS_GOTO_VIEWPORT_RANGE),
                "%.*s<0-9>", (int) prefix_len, page[0]);
        return;
    }

    for (uint32_t i = 0u; i < page_count; ++i) {
        char label[DIALOG_MSG_LINE_MAX_LENGTH];

        if (page[i][0] == '\0') {
            continue;
        }

        (void) snprintf(label, sizeof(label),
                _(STR_SHORTCUTS_GOTO_VIEWPORT_FMT), i);
        s_append_pair_fmt(ctx, label, "%s", page[i]);
    }
}


/**
 * @brief Append one line combining several related bindings, skipping
 *        any of them that is empty (unbound)
 *
 * @param ctx    Rows being gathered
 * @param label  Human-readable label for the whole group
 * @param names  Short name for each binding in @p combos, same order
 * @param combos Configured binding string for each entry
 * @param count  Number of entries in @p names and @p combos
 *
 * @note Complexity: @e O(n), where @e n is @p count
 */
static void s_append_group(struct s_shortcuts_ctx_s *ctx,
        const char *restrict label,
        const char *const *names,
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

        written = snprintf(line + line_offset,
                sizeof(line) - line_offset,
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
        s_append_pair_fmt(ctx, label, "%s", line);
    }
}


/* Open a message dialog listing every currently active keyboard
 * shortcut */
void dialog_shortcuts_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config)
{
    struct s_shortcuts_ctx_s ctx;

    if (connection == NULL || surface == NULL || config == NULL) {
        return;
    }

    ctx.count = 0u;

    s_append_line(&ctx,
            "modc=%s, mods=%s, mod1=%s, mod4=%s",
            config->bindings.modc, config->bindings.mods,
            config->bindings.mod1, config->bindings.mod4);
    dialog_pair_append_blank(ctx.pairs, &ctx.count);

    s_append_line(&ctx, "[%s]",
            _(STR_SHORTCUTS_HEADER_WM));
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_ROOT_MENU),
            config->bindings.keyboard.wm.menus.root);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_WINDOWS_MENU),
            config->bindings.keyboard.wm.menus.windows);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_SEARCH_WINDOWS),
            config->bindings.keyboard.wm.search);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_TOGGLE_STRUTLESS_MAXIMIZE),
            config->bindings.keyboard.wm.toggle_strutless_maximize);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_REDRAW),
            config->bindings.keyboard.wm.redraw);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_RELOAD_CONFIG),
            config->bindings.keyboard.wm.reload);
    if (config->base.fortune.is_enabled) {
        s_append_binding(&ctx,
                _(STR_SHORTCUTS_FORTUNE),
                config->bindings.keyboard.wm.fortune);
    }
    if (config->base.scratchpad.is_enabled) {
        s_append_binding(&ctx,
                _(STR_SHORTCUTS_SCRATCHPAD),
                config->bindings.keyboard.wm.scratchpad);
    }
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_QUIT),
            config->bindings.keyboard.wm.quit);
    if (config->base.shutdown.enable_emergency_shortcut) {
        s_append_line(&ctx, "%s",
                _(STR_SHORTCUTS_EMERGENCY_EXIT));
    }
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_THIS_LIST),
            config->bindings.keyboard.wm.shortcuts);

    dialog_pair_append_blank(ctx.pairs, &ctx.count);
    s_append_line(&ctx, "[%s]",
            _(STR_SHORTCUTS_HEADER_DESKTOP));
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_SHOW_DESKTOP),
            config->bindings.keyboard.desktop.show);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_DESKTOP_ADD),
            config->bindings.keyboard.desktop.add);
    if (surface->desktop_count > 1u) {
        s_append_binding(&ctx,
                _(STR_SHORTCUTS_DESKTOP_REMOVE),
                config->bindings.keyboard.desktop.remove);
        s_append_goto_desktop(&ctx, config);
    }

    dialog_pair_append_blank(ctx.pairs, &ctx.count);
    s_append_line(&ctx, "[%s]",
            _(STR_SHORTCUTS_HEADER_LAUNCH));
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_TERMINAL),
            config->bindings.keyboard.launch.terminal);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_LAUNCHER),
            config->bindings.keyboard.launch.launcher);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_FILE_MANAGER),
            config->bindings.keyboard.launch.file_manager);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_WEB_BROWSER),
            config->bindings.keyboard.launch.web_browser);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_EDITOR),
            config->bindings.keyboard.launch.editor);

    dialog_pair_append_blank(ctx.pairs, &ctx.count);
    s_append_line(&ctx, "[%s]",
            _(STR_SHORTCUTS_HEADER_WINDOW));
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_CLOSE),
            config->bindings.keyboard.window.close);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_KILL),
            config->bindings.keyboard.window.kill);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_DECORATE),
            config->bindings.keyboard.window.decorate);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_FULLSCREEN),
            config->bindings.keyboard.window.fullscreen);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_HIDE),
            config->bindings.keyboard.window.hide);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_ICONIFY),
            config->bindings.keyboard.window.iconify);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_ICONIFY_ALL),
            config->bindings.keyboard.window.iconify_all);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_DEICONIFY_ALL),
            config->bindings.keyboard.window.deiconify_all);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_ARRANGE),
            config->bindings.keyboard.window.arrange);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_INFO),
            config->bindings.keyboard.window.info);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_INSPECT),
            config->bindings.keyboard.window.inspect);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_LAYER),
            config->bindings.keyboard.window.layer);
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_MAXIMIZE),
            config->bindings.keyboard.window.maximize);
    if (surface->monitor_count > 1u) {
        s_append_binding(&ctx,
                _(STR_SHORTCUTS_MONITOR_NORTH),
                config->bindings.keyboard.window.send_to.monitor.north);
        s_append_binding(&ctx,
                _(STR_SHORTCUTS_MONITOR_SOUTH),
                config->bindings.keyboard.window.send_to.monitor.south);
        s_append_binding(&ctx,
                _(STR_SHORTCUTS_MONITOR_EAST),
                config->bindings.keyboard.window.send_to.monitor.east);
        s_append_binding(&ctx,
                _(STR_SHORTCUTS_MONITOR_WEST),
                config->bindings.keyboard.window.send_to.monitor.west);
    }
    if (surface->desktop_count > 1u) {
        s_append_binding(&ctx,
                _(STR_SHORTCUTS_PIN),
                config->bindings.keyboard.window.pin);
        if (surface->config != NULL &&
                surface->id < (uint32_t) CONFIG_MAX_SCREENS &&
                surface->config->base.screens[surface->id]
                    .desktop_layout.rows > 1u) {
            s_append_binding(&ctx,
                    _(STR_SHORTCUTS_SEND_TO_DESKTOP_NORTH),
                    config->bindings.keyboard.window.send_to.
                        desktop.north);
            s_append_binding(&ctx,
                    _(STR_SHORTCUTS_SEND_TO_DESKTOP_SOUTH),
                    config->bindings.keyboard.window.send_to.
                        desktop.south);
        }
        s_append_binding(&ctx,
                _(STR_SHORTCUTS_SEND_TO_DESKTOP_EAST),
                config->bindings.keyboard.window.send_to.desktop.east);
        s_append_binding(&ctx,
                _(STR_SHORTCUTS_SEND_TO_DESKTOP_WEST),
                config->bindings.keyboard.window.send_to.desktop.west);
    }
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_SHADE),
            config->bindings.keyboard.window.shade);
    /* Not to be confused with STR_SHORTCUTS_PIN above; see
     * CLIENT_FLAG_STICKY's comment in client/state.h for the full
     * distinction between the two */
    s_append_binding(&ctx,
            _(STR_SHORTCUTS_STICKY),
            config->bindings.keyboard.window.sticky);

    s_append_group(&ctx,
            _(STR_SHORTCUTS_MOVE_RELATIVE),
            (const char *const []) {"Right", "Left", "Up", "Down"},
            (const char *const []) {
                config->bindings.keyboard.window.move.relative.right,
                config->bindings.keyboard.window.move.relative.left,
                config->bindings.keyboard.window.move.relative.up,
                config->bindings.keyboard.window.move.relative.down
            }, 4u);
    s_append_group(&ctx,
            _(STR_SHORTCUTS_MOVE_ABSOLUTE),
            (const char *const [])
                {"TopLeft", "TopRight", "BotLeft", "BotRight", "Center"},
            (const char *const []) {
                config->bindings.keyboard.window.move.absolute.top_left,
                config->bindings.keyboard.window.move.absolute.top_right,
                config->bindings.keyboard.window.move.absolute.bottom_left,
                config->bindings.keyboard.window.move.absolute.bottom_right,
                config->bindings.keyboard.window.move.absolute.center
            }, 5u);
    s_append_group(&ctx,
            _(STR_SHORTCUTS_RESIZE),
            (const char *const []) {"Right", "Left", "Up", "Down"},
            (const char *const []) {
                config->bindings.keyboard.window.resize.right,
                config->bindings.keyboard.window.resize.left,
                config->bindings.keyboard.window.resize.up,
                config->bindings.keyboard.window.resize.down
            }, 4u);

    dialog_pair_append_blank(ctx.pairs, &ctx.count);
    s_append_line(&ctx, "[%s]",
            _(STR_SHORTCUTS_HEADER_CYCLE));
    if (surface->desktop_count > 1u) {
        bool has_rows = surface->config != NULL &&
            surface->id < (uint32_t) CONFIG_MAX_SCREENS &&
            surface->config->base.screens[surface->id]
                .desktop_layout.rows > 1u;

        if (has_rows) {
            s_append_group(&ctx,
                    _(STR_SHORTCUTS_DESKTOPS),
                    (const char *const [])
                        {"north", "south", "east", "west"},
                    (const char *const []) {
                        config->bindings.keyboard.cycle.desktop.north,
                        config->bindings.keyboard.cycle.desktop.south,
                        config->bindings.keyboard.cycle.desktop.east,
                        config->bindings.keyboard.cycle.desktop.west
                    }, 4u);
        } else {
            s_append_group(&ctx,
                    _(STR_SHORTCUTS_DESKTOPS),
                    (const char *const []) {"east", "west"},
                    (const char *const []) {
                        config->bindings.keyboard.cycle.desktop.east,
                        config->bindings.keyboard.cycle.desktop.west
                    }, 2u);
        }
    }
    s_append_group(&ctx,
            _(STR_SHORTCUTS_ICONS),
            (const char *const []) {"prev", "next"},
            (const char *const []) {
                config->bindings.keyboard.cycle.icon.prev,
                config->bindings.keyboard.cycle.icon.next
            }, 2u);
    s_append_group(&ctx,
            _(STR_SHORTCUTS_WINDOWS),
            (const char *const []) {"prev", "next"},
            (const char *const []) {
                config->bindings.keyboard.cycle.window.prev,
                config->bindings.keyboard.cycle.window.next
            }, 2u);

    if (surface->config != NULL &&
            surface->id < (uint32_t) CONFIG_MAX_SCREENS) {
        const struct config_viewport_s *const viewport =
            &surface->config->base.screens[surface->id].viewport;

        if (viewport->columns > 1u || viewport->rows > 1u) {
            uint32_t page_count = viewport->columns * viewport->rows;

            if (page_count > 10u) {
                page_count = 10u;
            }
            dialog_pair_append_blank(ctx.pairs, &ctx.count);
            s_append_line(&ctx, "[%s]",
                    _(STR_SHORTCUTS_HEADER_VIEWPORT));
            if (viewport->rows > 1u) {
                s_append_group(&ctx,
                        _(STR_SHORTCUTS_VIEWPORT_PAN),
                        (const char *const [])
                            {"north", "south", "east", "west"},
                        (const char *const []) {
                            config->bindings.keyboard.viewport.pan.north,
                            config->bindings.keyboard.viewport.pan.south,
                            config->bindings.keyboard.viewport.pan.east,
                            config->bindings.keyboard.viewport.pan.west
                        }, 4u);
            } else {
                s_append_group(&ctx,
                        _(STR_SHORTCUTS_VIEWPORT_PAN),
                        (const char *const []) {"east", "west"},
                        (const char *const []) {
                            config->bindings.keyboard.viewport.pan.east,
                            config->bindings.keyboard.viewport.pan.west
                        }, 2u);
            }
            s_append_goto_viewport(&ctx, config, page_count);
        }
    }

    menu_message_dialog_show_pairs(connection, surface, config,
            ctx.pairs, ctx.count, MENU_MSG_LEVEL_NONE);
}
