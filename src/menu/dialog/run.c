/**
 * @file menu/dialog/run.c
 *
 * @brief Built-in run-box implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* free */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safe/safestr.h>
#include <utils/xcb/connection.h>

/* Default initial values */
#include <defs/run.h>
#include <defs/uistr.h>

/* Project includes */
#include <config.h>
#include <client.h>
#include <desktop.h>
#include <i18n.h>
#include <render/text.h>
#include <surface.h>

/* Menu includes */
#include <menu/dialog/info.h>
#include <menu/dialog/message.h>   /* menu_msg_level_e */
#include <menu/draw.h>

/* Local includes */
#include <menu/dialog/run.h>
#include <utils/xcb/window.h>


/**
 * @brief All state for the currently open run-box
 *
 * A single global instance, the same way the fuzzy window-search
 * widget's @c s_search is
 */
static struct {
    xcb_window_t window;
    surface_td *surface;
    const config_td *config;
    xcb_window_t prev_focus;
    char command[WM_RUN_COMMAND_MAX_LENGTH];
    unsigned int command_len;

    unsigned int cursor;        /**< Where the next character goes, from
                                     @c 0 to @c command_len; the arrows,
                                     @c Home and @c End move it and
                                     everything else acts there */

    unsigned int view_start;    /**< First character drawn, which trails
                                     the cursor far enough to keep it
                                     inside the box however long the
                                     command grows */
} s_run;


/**
 * @brief Close the run-box and restore whichever window had input
 *        focus before it opened
 *
 * @param connection XCB connection
 *
 * @note A no-op if the run-box is not currently open
 * @note Complexity: @e O(1)
 */
static void s_run_destroy(xcb_connection_t *connection)
{
    if (connection == NULL || s_run.window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_ungrab_keyboard(connection, XCB_CURRENT_TIME);
    xcb_window_destroy(s_run.window);

    if (s_run.prev_focus != XCB_WINDOW_NONE) {
        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_PARENT,
                s_run.prev_focus, (client_last_user_time() != 0u)
                ? client_last_user_time()
                : (uint32_t) XCB_CURRENT_TIME);
    }

    memset(&s_run, 0, sizeof(s_run));
    s_run.window = XCB_WINDOW_NONE;
}


/**
 * @brief Attempt to launch the currently typed command, showing an
 *        informational dialog if it could not be found or run
 *
 * A no-op if the command is empty.  Always closes the run-box itself
 * first, whether the attempt succeeds or not, the same way
 * @a search_init already shows its "nothing to search for"
 * dialog with the search widget itself never open behind it.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
static void s_run_attempt_launch(xcb_connection_t *connection)
{
    surface_td *const surface = s_run.surface;
    const config_td *cfg = s_run.config;
    desktop_td *desktop;
    char command[WM_RUN_COMMAND_MAX_LENGTH];
    int result;

    if (s_run.command_len == 0u) {
        return;
    }

    safe_strncpy(command, s_run.command, sizeof(command));

    s_run_destroy(connection);

    desktop = (surface != NULL)
        ? surface_desktop_get(surface, surface->desktop_cur) : NULL;
    if (desktop == NULL) {
        return;
    }

    result = desktop_action_process_launch(desktop, command);
    if (result != 0) {
        char message[WM_RUN_COMMAND_MAX_LENGTH + 32];
        (void) snprintf(message, sizeof(message),
                _(STR_RUN_COMMAND_NOT_FOUND_FMT), command);
        dialog_info_show(connection, surface, cfg, message,
                MENU_MSG_LEVEL_INFO);
    }
}


/**
 * @brief Remove the character at one position, closing the gap
 *
 * @param at Index to remove, which the caller has established is
 *           below the current length
 *
 * @note Moves the terminator along with the tail, so the command
 *       stays a string whatever the caller does next
 * @note Complexity: @e O(n), where @e n is how much of the command
 *       follows @p at
 */
static void s_run_erase_at(unsigned int at)
{
    unsigned int i;

    for (i = at; i + 1u <= s_run.command_len; ++i) {
        s_run.command[i] = s_run.command[i + 1u];
    }
    s_run.command_len--;
    s_run.command[s_run.command_len] = '\0';
}


/**
 * @brief Insert one character at a position, pushing the tail along
 *
 * @param at Index to insert at, from zero to the current length
 * @param ch Character to place there
 *
 * @note The caller has already established there is room, this being
 *       reached only where the length is below the buffer's last slot
 * @note Complexity: @e O(n), where @e n is how much of the command
 *       follows @p at
 */
static void s_run_insert_at(unsigned int at, char ch)
{
    unsigned int i;

    for (i = s_run.command_len; i > at; --i) {
        s_run.command[i] = s_run.command[i - 1u];
    }
    s_run.command[at] = ch;
    s_run.command_len++;
    s_run.command[s_run.command_len] = '\0';
}


/**
 * @brief Fill a rectangle at an arbitrary horizontal offset
 *
 * The same drawing this shares with @a menu_draw_row_bg
 * (menu/draw.c), just with @p x configurable.  That shared helper
 * always starts at the window's left edge, which is exactly
 * right for every one of its other callers (a whole-width row
 * background) but not for painting @p label's and @p input's
 * independently colored halves of the run-box side by side.
 *
 * @param connection XCB connection
 * @param window     Window to draw into
 * @param color      Fill color
 * @param x          Left edge, in pixels
 * @param dim        Width/height, in pixels
 *
 * @note Complexity: @e O(1)
 */
static void s_run_fill_rect(xcb_connection_t *connection,
        xcb_window_t window, uint32_t color,
        int16_t x, struct dimensions_s dim)
{
    xcb_gcontext_t gc;
    xcb_rectangle_t rect;
    uint32_t gc_vals[1];

    gc = xcb_generate_id(connection);
    gc_vals[0] = color;
    xcb_create_gc(connection, gc, window, XCB_GC_FOREGROUND, gc_vals);

    rect.x = x;
    rect.y = 0;
    rect.width = (uint16_t) dim.w;
    rect.height = (uint16_t) dim.h;
    xcb_poly_fill_rectangle(connection, window, gc, 1, &rect);

    xcb_free_gc(connection, gc);
}


/**
 * @brief Bring the view back around the cursor
 *
 * Two corrections, one for each way the cursor can leave the box: it
 * walked left of what is drawn, or the text before it grew wider than
 * the room there is.  Everything else leaves the view where it was,
 * so typing in the middle of a long command does not make it jump.
 *
 * @param avail Width the text has, in pixels
 *
 * @note Measures against the input font, which the caller has already
 *       selected
 * @note Complexity: @e O(n * n), where @e n is the command's length,
 *       from measuring one candidate view per step
 */
static void s_run_view_follow_cursor(uint16_t avail)
{
    char probe[WM_RUN_COMMAND_MAX_LENGTH];

    if (s_run.cursor < s_run.view_start) {
        s_run.view_start = s_run.cursor;
        return;
    }

    /* Walked right off the far edge: the view creeps forward one
     * character at a time until what lies between it and the cursor
     * fits again */
    while (s_run.view_start < s_run.cursor) {
        (void) safe_strncpy(probe, s_run.command + s_run.view_start,
                (size_t) (s_run.cursor - s_run.view_start) + 1u);
        if (text_string_measure(probe) <= avail) {
            break;
        }
        s_run.view_start++;
    }
}


/**
 * @brief Draw the insertion cursor at its place in the text
 *
 * @param connection XCB connection
 * @param cfg        Active configuration, for the input colors
 * @param text_x     Where the drawn text begins, in window
 *                    coordinates
 * @param baseline   The text's baseline, which the bar is centred on
 *
 * @note Drawn in the foreground color, the bar standing between two
 *       characters where a block would hide the one under it
 * @note Complexity: @e O(n), where @e n is how much of the command
 *       lies between the view's start and the cursor
 */
static void s_run_draw_cursor(xcb_connection_t *connection,
        const config_td *cfg, int16_t text_x, int16_t baseline)
{
    char probe[WM_RUN_COMMAND_MAX_LENGTH];
    xcb_gcontext_t gc;
    xcb_rectangle_t rect;
    uint32_t gc_vals[1];
    uint16_t before;
    int16_t ascent = text_font_ascent();
    int16_t descent = text_font_descent();

    probe[0] = '\0';
    if (s_run.cursor > s_run.view_start) {
        (void) safe_strncpy(probe, s_run.command + s_run.view_start,
                (size_t) (s_run.cursor - s_run.view_start) + 1u);
    }
    before = (uint16_t) text_string_measure(probe);

    gc = xcb_generate_id(connection);
    gc_vals[0] = cfg->theme.prompt.input.color.foreground;
    xcb_create_gc(connection, gc, s_run.window, XCB_GC_FOREGROUND,
            gc_vals);

    /* Spanning the line the text occupies rather than the whole bar:
     * from as far above the baseline as the font reaches to as far
     * below it as the font descends, which puts the bar beside the
     * characters instead of standing over them from the box's top
     * edge */
    rect.x = (int16_t) (text_x + (int16_t) before);
    rect.y = (int16_t) (baseline - ascent);
    rect.width = (uint16_t) WM_RUN_CURSOR_WIDTH;
    rect.height = (uint16_t) (ascent + descent);
    xcb_poly_fill_rectangle(connection, s_run.window, gc, 1, &rect);

    xcb_free_gc(connection, gc);
}



/* Open the run-box, centered on 'surface' */
void run_init(xcb_connection_t *connection, surface_td *surface,
        const config_td *cfg)
{
    xcb_get_input_focus_cookie_t foc_cookie;
    xcb_get_input_focus_reply_t *foc_reply;
    uint32_t mask;
    uint32_t values[4];
    uint16_t height;
    int16_t widget_x;
    int16_t widget_y;

    if (connection == NULL || surface == NULL || cfg == NULL) {
        return;
    }

    if (run_is_open()) {
        s_run_destroy(connection);
    }

    memset(&s_run, 0, sizeof(s_run));
    s_run.window = XCB_WINDOW_NONE;
    s_run.surface = surface;
    s_run.config = cfg;

    foc_cookie = xcb_get_input_focus(connection);
    foc_reply = xcb_get_input_focus_reply(connection, foc_cookie, NULL);
    s_run.prev_focus = (foc_reply != NULL &&
            foc_reply->focus != XCB_WINDOW_NONE &&
            foc_reply->focus != XCB_INPUT_FOCUS_POINTER_ROOT &&
            foc_reply->focus != XCB_INPUT_FOCUS_NONE)
        ? foc_reply->focus : XCB_WINDOW_NONE;
    if (foc_reply != NULL) {
        free(foc_reply);
    }

    height = (uint16_t) (WM_RUN_BAR_HEIGHT + 2 * WM_RUN_PAD_Y);

    /* Centered fully (both axes), unlike the fuzzy window-search
     * widget's one-third-from-the-top position.  One more visual cue,
     * alongside the "Run:" prompt itself, that the two are not the same
     * widget. */
    widget_x = (int16_t) (((int32_t) surface->properties.dim.w -
                (int32_t) WM_RUN_WIDTH) / 2);
    widget_y = (int16_t) (((int32_t) surface->properties.dim.h -
                (int32_t) height) / 2);
    if (widget_x < 0) { widget_x = 0; }
    if (widget_y < 0) { widget_y = 0; }

    s_run.window = xcb_generate_id(connection);

    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
        XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    values[0] = cfg->theme.prompt.input.color.background;
    values[1] = cfg->theme.prompt.border.color;
    values[2] = 1;  /* override_redirect: prevent WM from managing it */
    values[3] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS |
        XCB_EVENT_MASK_KEY_RELEASE;

    xcb_create_window(connection,
            XCB_COPY_FROM_PARENT,
            s_run.window,
            surface->screen->root,
            widget_x, widget_y,
            (uint16_t) WM_RUN_WIDTH, height,
            (uint16_t) cfg->theme.prompt.border.width,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    /* Advertise this as a dialog window, so a compositor or pager
     * that inspects '_NET_WM_WINDOW_TYPE' recognizes it for what
     * it is */
    if (xcb_ewmh_connection_get() != NULL) {
        xcb_atom_t window_type =
            xcb_ewmh_connection_get()->_NET_WM_WINDOW_TYPE_DIALOG;

        xcb_ewmh_set_wm_window_type(xcb_ewmh_connection_get(),
                s_run.window, 1, &window_type);
    }

    xcb_map_window(connection, s_run.window);
    xcb_grab_keyboard(connection,
            0,
            surface->screen->root,
            XCB_CURRENT_TIME,
            XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC);
    xcb_set_input_focus(connection,
            XCB_INPUT_FOCUS_POINTER_ROOT,
            s_run.window,
            (client_last_user_time() != 0u)
                ? client_last_user_time()
                : (uint32_t) XCB_CURRENT_TIME);

    run_draw(connection, cfg);
}


/* Query whether the run-box is currently open */
bool run_is_open(void)
{
    return s_run.window != XCB_WINDOW_NONE;
}


/* Query whether 'win' is the run-box's window */
bool run_owns_window(xcb_window_t win)
{
    return run_is_open() && win == s_run.window;
}


/* Handle a key press while the run-box is open */
void run_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym, uint16_t modmask,
        const config_td *cfg)
{
    const bool is_ctrl = ((modmask & XCB_MOD_MASK_CONTROL) != 0u);

    (void) surface;

    if (!run_is_open()) {
        return;
    }

    if (keysym == 0xff1bu) {    /* Escape */
        s_run_destroy(connection);
        return;
    }

    if (keysym == 0xff0du ||    /* Return or KP_Enter */
            keysym == 0xff8du) {
        s_run_attempt_launch(connection);
        return;
    }

    if (keysym == 0xff08u) {    /* Backspace */
        if (s_run.cursor > 0u) {
            s_run_erase_at(s_run.cursor - 1u);
            s_run.cursor--;
            run_draw(connection, cfg);
        }
        return;
    }

    if (keysym == 0xffffu) {    /* Delete */
        if (s_run.cursor < s_run.command_len) {
            s_run_erase_at(s_run.cursor);
            run_draw(connection, cfg);
        }
        return;
    }

    if (keysym == 0xff51u) {    /* Left */
        if (s_run.cursor > 0u) {
            s_run.cursor--;
            run_draw(connection, cfg);
        }
        return;
    }

    if (keysym == 0xff53u) {    /* Right */
        if (s_run.cursor < s_run.command_len) {
            s_run.cursor++;
            run_draw(connection, cfg);
        }
        return;
    }

    /* 'Home', or the 'Ctrl+A' that means the same on a command line
     * when it is not in 'vi' mode */
    if (keysym == 0xff50u ||
            (is_ctrl && (keysym == 0x61u || keysym == 0x41u))) {
        s_run.cursor = 0u;
        run_draw(connection, cfg);
        return;
    }

    /* 'End', or 'Ctrl+E', for the same reason */
    if (keysym == 0xff57u ||
            (is_ctrl && (keysym == 0x65u || keysym == 0x45u))) {
        s_run.cursor = s_run.command_len;
        run_draw(connection, cfg);
        return;
    }

    /* Compared against the room left rather than against the length
     * plus one: a signed sum tested against a constant is what lets the
     * optimizer assume the sum never overflows, and the length is
     * a count, which belongs in an unsigned type anyway */
    if (keysym <= 0xFFu && !is_ctrl && isprint((int) keysym) &&
            s_run.command_len < (unsigned int)
                (WM_RUN_COMMAND_MAX_LENGTH - 1)) {
        s_run_insert_at(s_run.cursor, (char) keysym);
        s_run.cursor++;
        run_draw(connection, cfg);
    }
}


/* Repaint the run-box */
void run_draw(xcb_connection_t *connection, const config_td *cfg)
{
    char shown[WM_RUN_COMMAND_MAX_LENGTH + 8];
    const char *prompt;
    uint16_t height;
    uint16_t label_w;
    int16_t input_x;
    uint16_t avail;
    uint16_t mark_w;
    unsigned int shown_len;
    int16_t text_x;
    const int16_t baseline =
        (int16_t) (WM_RUN_PAD_Y + WM_RUN_BAR_HEIGHT - 7);

    if (!run_is_open() || cfg == NULL) {
        return;
    }

    height = (uint16_t) (WM_RUN_BAR_HEIGHT + 2 * WM_RUN_PAD_Y);
    prompt = _(STR_RUN_PROMPT);

    /* Measured against 'label''s font, which is also the one it gets
     * drawn in just below, since 'text_string_measure' reports against
     * whichever font 'text_renderer_use_font' selected last. */
    (void) text_renderer_use_font(connection,
            cfg->theme.prompt.label.font);
    label_w = (uint16_t) (WM_RUN_PAD_X +
            text_string_measure(prompt) + WM_RUN_PAD_X / 2);
    input_x = (int16_t) label_w;

    s_run_fill_rect(connection, s_run.window,
            cfg->theme.prompt.label.color.background,
            0, (struct dimensions_s) { label_w, height });
    s_run_fill_rect(connection, s_run.window,
            cfg->theme.prompt.input.color.background,
            input_x,
            (struct dimensions_s) { WM_RUN_WIDTH - label_w, height });

    text_renderer_set_color(cfg->theme.prompt.label.color.foreground,
            cfg->theme.prompt.label.color.background);
    menu_draw_label(connection, s_run.window,
            (struct position_s) { WM_RUN_PAD_X,
                WM_RUN_PAD_Y + WM_RUN_BAR_HEIGHT - 7 },
            prompt);

    /* Room the text has, once the two edges are left free for the marks
     * that say the command carries on past them */
    avail = (uint16_t) (WM_RUN_WIDTH - label_w - WM_RUN_PAD_X);
    (void) text_renderer_use_font(connection,
            cfg->theme.prompt.input.font);
    mark_w = (uint16_t) text_string_measure(WM_RUN_MARK_LEFT);
    if (avail > (uint16_t) (mark_w * 2u)) {
        avail = (uint16_t) (avail - (uint16_t) (mark_w * 2u));
    }

    s_run_view_follow_cursor(avail);

    /* From the first character in view up to the last one that fits,
     * which is what the box shows and what the cursor is placed against
     * below */
    shown_len = 0u;
    while (s_run.view_start + shown_len < s_run.command_len) {
        char probe[WM_RUN_COMMAND_MAX_LENGTH];

        (void) safe_strncpy(probe, s_run.command + s_run.view_start,
                shown_len + 2u);
        if (text_string_measure(probe) > avail) {
            break;
        }
        shown_len++;
    }
    (void) safe_strncpy(shown, s_run.command + s_run.view_start,
            shown_len + 1u);

    (void) text_renderer_use_font(connection,
            cfg->theme.prompt.input.font);
    text_renderer_set_color(cfg->theme.prompt.input.color.foreground,
            cfg->theme.prompt.input.color.background);

    text_x = (int16_t) (input_x + WM_RUN_PAD_X / 2);

    /* Marks first, at the two edges, so the text between them starts
     * clear of whichever one is drawn */
    if (s_run.view_start > 0u) {
        menu_draw_label(connection, s_run.window,
                (struct position_s) { text_x, baseline },
                WM_RUN_MARK_LEFT);
    }
    if (s_run.view_start + shown_len < s_run.command_len) {
        menu_draw_label(connection, s_run.window,
                (struct position_s) {
                    (int16_t) (text_x + (int16_t) mark_w +
                        (int16_t) avail),
                    baseline },
                WM_RUN_MARK_RIGHT);
    }

    text_x = (int16_t) (text_x + (int16_t) mark_w);
    menu_draw_label(connection, s_run.window,
            (struct position_s) { text_x, baseline }, shown);

    /* A bar between two characters rather than a block over one, which
     * is what a text field draws and what leaves the character beside
     * it legible */
    s_run_draw_cursor(connection, cfg, text_x, baseline);
}
