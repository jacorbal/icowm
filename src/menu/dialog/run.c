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
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* free */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/run.h>
#include <defs/uistr.h>

/* Project includes */
#include <config.h>
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


/** All state for the currently open run-box; a single global instance,
 *  the same way the fuzzy window-search widget's own @c s_search is */
static struct {
    xcb_window_t window;
    surface_td *surface;
    const config_td *config;
    xcb_window_t prev_focus;
    char command[WM_RUN_COMMAND_MAX_LENGTH];
    int command_len;
} s_run;


/**
 * @brief Attempt to launch the currently typed command, showing an
 *        informational dialog if it could not be found or run
 *
 * A no-op if the command is empty.  Always closes the run-box itself
 * first, whether the attempt succeeds or not, the same way
 * @a search_init already shows its own "nothing to search for"
 * dialog with the search widget itself never open behind it.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
static void s_run_attempt_launch(xcb_connection_t *connection)
{
    surface_td *surface = s_run.surface;
    const config_td *cfg = s_run.config;
    desktop_td *desktop;
    char command[WM_RUN_COMMAND_MAX_LENGTH];
    char message[WM_RUN_COMMAND_MAX_LENGTH + 32];
    int result;

    if (s_run.command_len == 0) {
        return;
    }

    safe_strncpy(command, s_run.command, sizeof(command));

    run_destroy(connection);

    desktop = (surface != NULL)
        ? surface_desktop_get(surface, surface->desktop_cur) : NULL;
    if (desktop == NULL) {
        return;
    }

    result = desktop_action_process_launch(desktop, command);
    if (result != 0) {
        snprintf(message, sizeof(message),
                _(STR_RUN_COMMAND_NOT_FOUND_FMT), command);
        dialog_info_show(connection, surface, cfg, message,
                MENU_MSG_LEVEL_INFO);
    }
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
        run_destroy(connection);
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
     * widget's own one-third-from-the-top position: one more visual
     * cue, alongside the "Run:" prompt itself, that the two are not
     * the same widget. */
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
            XCB_CURRENT_TIME);

    run_draw(connection, cfg);
}


/* Close the run-box and restore whichever window had input focus
 * before it opened */
void run_destroy(xcb_connection_t *connection)
{
    if (connection == NULL || s_run.window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_ungrab_keyboard(connection, XCB_CURRENT_TIME);
    xcb_destroy_window(connection, s_run.window);

    if (s_run.prev_focus != XCB_WINDOW_NONE) {
        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_PARENT,
                s_run.prev_focus, XCB_CURRENT_TIME);
    }

    memset(&s_run, 0, sizeof(s_run));
    s_run.window = XCB_WINDOW_NONE;
    xcb_flush(connection);
}


/* Query whether the run-box is currently open */
bool run_is_open(void)
{
    return s_run.window != XCB_WINDOW_NONE;
}


/* Query whether 'win' is the run-box's own window */
bool run_owns_window(xcb_window_t win)
{
    return run_is_open() && win == s_run.window;
}


/* Handle a key press while the run-box is open */
void run_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym, const config_td *cfg)
{
    (void) surface;

    if (!run_is_open()) {
        return;
    }

    if (keysym == 0xff1bu) {   /* Escape */
        run_destroy(connection);
        return;
    }

    if (keysym == 0xff0du || keysym == 0xff8du) {   /* Return / KP_Enter */
        s_run_attempt_launch(connection);
        return;
    }

    if (keysym == 0xff08u) {   /* Backspace */
        if (s_run.command_len > 0) {
            s_run.command[--s_run.command_len] = '\0';
            run_draw(connection, cfg);
        }
        return;
    }

    if (keysym <= 0xFFu && isprint((int) keysym) &&
            s_run.command_len + 1 < WM_RUN_COMMAND_MAX_LENGTH) {
        s_run.command[s_run.command_len++] = (char) keysym;
        s_run.command[s_run.command_len] = '\0';
        run_draw(connection, cfg);
    }
}


/**
 * @brief Fill a rectangle at an arbitrary horizontal offset
 *
 * The same drawing this shares with @a menu_draw_row_bg
 * (menu/draw.c), just with @p x configurable: that shared helper
 * always starts at the window's own left edge, which is exactly
 * right for every one of its other callers (a whole-width row
 * background) but not for painting @p label's and @p input's own
 * independently colored halves of the run-box side by side.
 *
 * @param connection XCB connection
 * @param window     Window to draw into
 * @param color      Fill color
 * @param x          Left edge, in pixels
 * @param w          Width, in pixels
 * @param h          Height, in pixels
 *
 * @note Complexity: @e O(1)
 */
static void s_run_fill_rect(xcb_connection_t *connection,
        xcb_window_t window, uint32_t color,
        int16_t x, uint16_t w, uint16_t h)
{
    xcb_gcontext_t gc;
    xcb_rectangle_t rect;
    uint32_t gc_vals[1];

    gc = xcb_generate_id(connection);
    gc_vals[0] = color;
    xcb_create_gc(connection, gc, window, XCB_GC_FOREGROUND, gc_vals);

    rect.x = x;
    rect.y = 0;
    rect.width = w;
    rect.height = h;
    xcb_poly_fill_rectangle(connection, window, gc, 1, &rect);

    xcb_free_gc(connection, gc);
}


/* Repaint the run-box */
void run_draw(xcb_connection_t *connection, const config_td *cfg)
{
    char shown[WM_RUN_COMMAND_MAX_LENGTH + 8];
    const char *prompt;
    uint16_t height;
    uint16_t label_w;
    int16_t input_x;

    if (!run_is_open() || cfg == NULL) {
        return;
    }

    height = (uint16_t) (WM_RUN_BAR_HEIGHT + 2 * WM_RUN_PAD_Y);
    prompt = _(STR_RUN_PROMPT);

    /* Measured against 'label''s own font, which is also the one it
     * gets drawn in just below, since 'text_measure_string' reports
     * against whichever font 'text_renderer_init' was last set to. */
    text_renderer_init(connection, cfg->theme.prompt.label.font);
    label_w = (uint16_t) (WM_RUN_PAD_X +
            text_measure_string(prompt) + WM_RUN_PAD_X / 2);
    input_x = (int16_t) label_w;

    s_run_fill_rect(connection, s_run.window,
            cfg->theme.prompt.label.color.background,
            0, label_w, height);
    s_run_fill_rect(connection, s_run.window,
            cfg->theme.prompt.input.color.background,
            input_x, (uint16_t) (WM_RUN_WIDTH - label_w), height);

    text_renderer_set_color(cfg->theme.prompt.label.color.foreground,
            cfg->theme.prompt.label.color.background);
    menu_draw_label(connection, s_run.window,
            (int16_t) WM_RUN_PAD_X,
            (int16_t) (WM_RUN_PAD_Y + WM_RUN_BAR_HEIGHT - 7),
            prompt);

    snprintf(shown, sizeof(shown), "%s_", s_run.command);

    text_renderer_init(connection, cfg->theme.prompt.input.font);
    text_renderer_set_color(cfg->theme.prompt.input.color.foreground,
            cfg->theme.prompt.input.color.background);
    menu_draw_label(connection, s_run.window,
            (int16_t) (input_x + WM_RUN_PAD_X / 2),
            (int16_t) (WM_RUN_PAD_Y + WM_RUN_BAR_HEIGHT - 7),
            shown);

    xcb_flush(connection);
}
