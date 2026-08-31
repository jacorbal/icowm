/**
 * @file menu/dialog/inspect.c
 *
 * @brief Window property inspector dialog
 *
 * @copyright Copyright (c) 2026, J. A. Corbal
 *
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdarg.h>     /* va_list, va_start, va_end */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>      /* snprintf */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <adt/cdlist.h>
#include <client.h>
#include <client/predicates.h>
#include <config.h>
#include <defs/dialog.h>
#include <defs/uistr.h>
#include <i18n.h>
#include <monitor.h>
#include <surface.h>

/* Local includes */
#include <menu/dialog/inspect.h>
#include <menu/dialog/message.h>


/**
 * @brief What the helpers below are filling in
 *
 * Every value shown is composed here, from fields that are numbers or
 * that may be absent, so unlike the shortcut list this keeps a store
 * for both halves of every row rather than pointing at strings the
 * configuration already holds.
 */
struct s_inspect_ctx_s {
    struct dialog_pair_s pairs[DIALOG_MSG_MAX_LINES];
    char lstore[DIALOG_MSG_MAX_LINES][DIALOG_MSG_LINE_MAX_LENGTH];
    char vstore[DIALOG_MSG_MAX_LINES][DIALOG_MSG_LINE_MAX_LENGTH];
    uint8_t count;
};


/**
 * @brief Append one blank line, separating two groups
 *
 * @param ctx Rows being gathered
 *
 * @note Complexity: @e O(1)
 */
static void s_inspect_blank(struct s_inspect_ctx_s *ctx)
{
    if (ctx->count >= (uint8_t) DIALOG_MSG_MAX_LINES) {
        return;
    }

    ctx->pairs[ctx->count].label = NULL;
    ctx->pairs[ctx->count].value = NULL;
    ctx->count++;
}


/**
 * @brief Append a group heading, drawn across the full width
 *
 * @param ctx  Rows being gathered
 * @param text Heading, already translated
 *
 * @note Complexity: @e O(n), where @e n is @p text's length
 */
static void s_inspect_heading(struct s_inspect_ctx_s *ctx,
        const char *text)
{
    if (ctx->count >= (uint8_t) DIALOG_MSG_MAX_LINES) {
        return;
    }

    /* Bracketed the way the shortcut list brackets its own headings:
     * a heading has no right-hand column, so without something to set
     * it apart it reads as a row whose value went missing */
    (void) snprintf(ctx->lstore[ctx->count],
            sizeof(ctx->lstore[ctx->count]), "[%s]", text);
    ctx->pairs[ctx->count].label = ctx->lstore[ctx->count];
    ctx->pairs[ctx->count].value = NULL;
    ctx->count++;
}


/**
 * @brief Append one labelled row, composing its value
 *
 * @param ctx   Rows being gathered
 * @param label Left column, already translated
 * @param fmt   'printf'-style format for the right column
 *
 * @note Both halves are copied, since a caller composing either on
 *       its own stack would otherwise leave this holding a pointer
 *       into a frame already gone
 * @note Complexity: @e O(n), where @e n is the composed value's length
 */
static void s_inspect_row(struct s_inspect_ctx_s *ctx,
        const char *restrict label, const char *restrict fmt, ...)
{
    va_list args;

    if (ctx->count >= (uint8_t) DIALOG_MSG_MAX_LINES) {
        return;
    }

    va_start(args, fmt);
    (void) vsnprintf(ctx->vstore[ctx->count],
            sizeof(ctx->vstore[ctx->count]), fmt, args);
    va_end(args);

    (void) safe_strncpy(ctx->lstore[ctx->count], label,
            sizeof(ctx->lstore[ctx->count]));
    ctx->pairs[ctx->count].label = ctx->lstore[ctx->count];
    ctx->pairs[ctx->count].value = ctx->vstore[ctx->count];
    ctx->count++;
}


/**
 * @brief Answer a string, or the placeholder where there is none
 *
 * @param text String to show, possibly null or empty
 *
 * @return @p text, or the translated "(none)"
 *
 * @note Distinguishes a property a client did not set from one the
 *       inspector forgot to list, which omitting the row would not
 * @note Complexity: @e O(1)
 */
static const char *s_inspect_or_none(const char *text)
{
    return (text != NULL && text[0] != '\0')
        ? text : _(STR_INSPECT_NONE);
}


/**
 * @brief Name the window type a client declared
 *
 * @param type Type recorded for the client
 *
 * @return A short lowercase name for it
 *
 * @note Answers @c "normal" for a value outside the enumeration,
 *       which is what an unset @c _NET_WM_WINDOW_TYPE amounts to
 * @note Complexity: @e O(1)
 */
static const char *s_inspect_type_name(uint16_t type)
{
    switch ((enum client_type_e) type) {
        case CLIENT_TYPE_DIALOG:        return "dialog";
        case CLIENT_TYPE_TOOLBAR:       return "toolbar";
        case CLIENT_TYPE_NOTIFICATION:  return "notification";
        case CLIENT_TYPE_MENU:          return "menu";
        case CLIENT_TYPE_DESKTOP:       return "desktop";
        case CLIENT_TYPE_SPLASH:        return "splash";
        case CLIENT_TYPE_UTILITY:       return "utility";
        case CLIENT_TYPE_DROPDOWN_MENU: return "dropdown menu";
        case CLIENT_TYPE_POPUP_MENU:    return "popup menu";
        case CLIENT_TYPE_COMBO:         return "combo";
        case CLIENT_TYPE_TOOLTIP:       return "tooltip";
        case CLIENT_TYPE_DOCK:          return "dock";
        case CLIENT_TYPE_DND:           return "drag and drop";
        case CLIENT_TYPE_NORMAL:        break;
    }

    return "normal";
}


/**
 * @brief Name the layer a client sits in
 *
 * @param layer Layer recorded for the client
 *
 * @return A short lowercase name for it
 *
 * @note Complexity: @e O(1)
 */
static const char *s_inspect_layer_name(uint16_t layer)
{
    switch ((enum client_layer_e) layer) {
        case CLIENT_LAYER_ABOVE:    return "above";
        case CLIENT_LAYER_BELOW:    return "below";
        case CLIENT_LAYER_NORMAL:   break;
    }

    return "normal";
}


/**
 * @brief Add one flag's name to whichever of the two lists it belongs
 *
 * @param buf  List being built
 * @param size Its size in bytes
 * @param name Flag name to add
 *
 * @note Separates with a comma once the list has something in it
 * @note Complexity: @e O(n), where @e n is the list's current length
 */
static void s_inspect_flag(char *buf, size_t size, const char *name)
{
    size_t at = safe_strlen(buf);

    if (at + safe_strlen(name) + 3u >= size) {
        return;
    }
    if (at > 0u) {
        (void) safe_strncpy(buf + at, ", ", size - at);
        at += 2u;
    }
    (void) safe_strncpy(buf + at, name, size - at);
}


/* Show the inspector for one client */
void dialog_inspect_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const client_td *client)
{
    struct s_inspect_ctx_s ctx;
    char yes[DIALOG_MSG_LINE_MAX_LENGTH];
    char no[DIALOG_MSG_LINE_MAX_LENGTH];
    monitor_td monitor;

    if (connection == NULL || surface == NULL || config == NULL ||
            client == NULL) {
        return;
    }

    ctx.count = 0u;
    yes[0] = '\0';
    no[0] = '\0';

    s_inspect_heading(&ctx, _(STR_INSPECT_GROUP_IDENTITY));
    s_inspect_row(&ctx, _(STR_INSPECT_TITLE), "%s",
            s_inspect_or_none(client->info.name));
    s_inspect_row(&ctx, _(STR_INSPECT_CLASS), "%s / %s",
            s_inspect_or_none(client->info.class_name[0]),
            s_inspect_or_none(client->info.class_name[1]));
    s_inspect_row(&ctx, _(STR_INSPECT_ROLE), "%s",
            s_inspect_or_none(client->info.role_name));
    s_inspect_row(&ctx, _(STR_INSPECT_TYPE), "%s",
            s_inspect_type_name(client->properties.type));
    if (client->process.pid > 0) {
        s_inspect_row(&ctx, _(STR_INSPECT_PROCESS), "%ld",
                (long) client->process.pid);
    }

    s_inspect_blank(&ctx);
    s_inspect_heading(&ctx, _(STR_INSPECT_GROUP_PLACEMENT));
    if (client_is_pinned(client)) {
        s_inspect_row(&ctx, _(STR_INSPECT_DESKTOP), "%u (%s)",
                (unsigned int) client->desktop_id,
                _(STR_INSPECT_PINNED_ALL));
    } else {
        s_inspect_row(&ctx, _(STR_INSPECT_DESKTOP), "%u",
                (unsigned int) client->desktop_id);
    }
    s_inspect_row(&ctx, _(STR_INSPECT_SCREEN), "%u",
            (unsigned int) client->screen_id);

    /* Resolved from the window's own centre rather than its origin, so
     * a window straddling two monitors is reported on the one it
     * mostly occupies, which is the one every other part of the
     * manager already treats it as being on */
    monitor = surface_monitor_for_point(surface,
            (struct position_s) {
                client->layout.geometry.cur.pos.x +
                    (int32_t) (client->layout.geometry.cur.dim.w / 2u),
                client->layout.geometry.cur.pos.y +
                    (int32_t) (client->layout.geometry.cur.dim.h / 2u)
            });
    s_inspect_row(&ctx, _(STR_INSPECT_MONITOR), "%ux%u%+d%+d",
            (unsigned int) monitor.w, (unsigned int) monitor.h,
            (int) monitor.x, (int) monitor.y);
    s_inspect_row(&ctx, _(STR_INSPECT_GEOMETRY), "%ux%u%+d%+d",
            (unsigned int) client->layout.geometry.cur.dim.w,
            (unsigned int) client->layout.geometry.cur.dim.h,
            (int) client->layout.geometry.cur.pos.x,
            (int) client->layout.geometry.cur.pos.y);
    s_inspect_row(&ctx, _(STR_INSPECT_FRAME_EXT), "%u, %u, %u, %u",
            (unsigned int) client->layout.frame_extents.left,
            (unsigned int) client->layout.frame_extents.top,
            (unsigned int) client->layout.frame_extents.right,
            (unsigned int) client->layout.frame_extents.bottom);
    s_inspect_row(&ctx, _(STR_INSPECT_LAYER), "%s",
            s_inspect_layer_name(client->properties.layer));

    s_inspect_blank(&ctx);
    s_inspect_heading(&ctx, _(STR_INSPECT_GROUP_STATE));
    s_inspect_flag(client_is_focused(client) ? yes : no,
            sizeof(yes), "focused");
    s_inspect_flag(client_is_decorated(client) ? yes : no,
            sizeof(yes), "decorated");
    s_inspect_flag(client_is_resizable(client) ? yes : no,
            sizeof(yes), "resizable");
    s_inspect_flag(client_is_modal(client) ? yes : no,
            sizeof(yes), "modal");
    s_inspect_flag(client_is_iconified(client) ? yes : no,
            sizeof(yes), "iconified");
    s_inspect_flag(client_is_shaded(client) ? yes : no,
            sizeof(yes), "shaded");
    s_inspect_flag(client_is_pinned(client) ? yes : no,
            sizeof(yes), "pinned");
    s_inspect_flag(client_is_maximized(client) ? yes : no,
            sizeof(yes), "maximized");
    s_inspect_flag(client_is_fullscreen(client) ? yes : no,
            sizeof(yes), "fullscreen");
    s_inspect_flag(client_is_urgent(client) ? yes : no,
            sizeof(yes), "urgent");
    s_inspect_row(&ctx, _(STR_INSPECT_IS), "%s",
            s_inspect_or_none(yes));
    s_inspect_row(&ctx, _(STR_INSPECT_IS_NOT), "%s",
            s_inspect_or_none(no));

    s_inspect_blank(&ctx);
    s_inspect_heading(&ctx, _(STR_INSPECT_GROUP_SIZE));
    s_inspect_row(&ctx, _(STR_INSPECT_MINIMUM), "%ux%u",
            (unsigned int) client->hints_icccm.size.min.w,
            (unsigned int) client->hints_icccm.size.min.h);
    if (client->hints_icccm.size.max.w == 0u ||
            client->hints_icccm.size.max.h == 0u) {
        s_inspect_row(&ctx, _(STR_INSPECT_MAXIMUM), "%s",
                _(STR_INSPECT_UNLIMITED));
    } else {
        s_inspect_row(&ctx, _(STR_INSPECT_MAXIMUM), "%ux%u",
                (unsigned int) client->hints_icccm.size.max.w,
                (unsigned int) client->hints_icccm.size.max.h);
    }

    s_inspect_blank(&ctx);
    s_inspect_heading(&ctx, _(STR_INSPECT_GROUP_RELATIONS));
    if (client->transient_parent != NULL) {
        s_inspect_row(&ctx, _(STR_INSPECT_TRANSIENT), "0x%x (%s)",
                (unsigned int) client->transient_parent->window,
                s_inspect_or_none(client->transient_parent->info.name));
    } else {
        s_inspect_row(&ctx, _(STR_INSPECT_TRANSIENT), "%s",
                _(STR_INSPECT_NONE));
    }
    s_inspect_row(&ctx, _(STR_INSPECT_TRANSIENTS), "%u",
            (client->transients != NULL)
                ? (unsigned int) cdlist_size(client->transients) : 0u);

    s_inspect_blank(&ctx);
    s_inspect_heading(&ctx, _(STR_INSPECT_GROUP_WINDOWS));
    s_inspect_row(&ctx, _(STR_INSPECT_CLIENT_WIN), "0x%x",
            (unsigned int) client->window);
    s_inspect_row(&ctx, _(STR_INSPECT_FRAME_WIN), "0x%x",
            (unsigned int) client->frame);
    if (client->titlebar != XCB_WINDOW_NONE) {
        s_inspect_row(&ctx, _(STR_INSPECT_TITLEBAR), "0x%x",
                (unsigned int) client->titlebar);
    }
    if (client->icon_window != XCB_WINDOW_NONE) {
        s_inspect_row(&ctx, _(STR_INSPECT_ICON_WIN), "0x%x",
                (unsigned int) client->icon_window);
    }

    menu_message_dialog_show_pairs(connection, surface, config,
            ctx.pairs, ctx.count, MENU_MSG_LEVEL_INFO);
}
