/**
 * @file menu/dialog.c
 *
 * @brief Generic dialog infrastructure for confirm and message dialogs
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
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <render/text.h>
#include <surface.h>

/* Local includes */
#include <menu/dialog.h>
#include <menu/draw.h>


/* Shared layout constants */
/** Horizontal dialog padding (pixels) */
#define DIALOG_PAD_X (12u)

/** Bottom padding below buttons (pixels) */
#define DIALOG_PAD_BOTTOM (8u)

/** Minimum dialog width (pixels) */
#define DIALOG_MIN_W (220u)

/** Minimum dialog height (pixels) */
#define DIALOG_MIN_H (90u)

/** Minimum button width (pixels) */
#define DIALOG_BTN_MIN_W (60u)

/** Button height (pixels) */
#define DIALOG_BTN_H (24u)

/** Gap between buttons in a two-button dialog (pixels) */
#define DIALOG_BTN_GAP (12u)

/** Horizontal padding between button border and label (pixels) */
#define DIALOG_BTN_LABEL_PAD_X (6u)

/** Prompt baseline position from dialog top (pixels) */
#define DIALOG_PROMPT_BASELINE_Y (22u)

/** Vertical gap between prompt baseline and button top (pixels) */
#define DIALOG_PROMPT_TO_BTN_GAP (34u)

/** Baseline offset for button labels from button top (pixels) */
#define DIALOG_BTN_LABEL_BASELINE_Y (17u)

/** Maximum text length for dialogs (prompt + level prefix) */
#define DIALOG_TEXT_MAX_LEN (256u)

/** Label for the dismiss button in the message dialog */
#define DIALOG_MSG_LABEL_OK ("[ OK ]")

/** Prefix for info-level messages */
#define DIALOG_MSG_PREFIX_INFO ("[i] ")

/** Prefix for warning-level messages */
#define DIALOG_MSG_PREFIX_WARNING ("[!] ")

/** Prefix for error-level messages */
#define DIALOG_MSG_PREFIX_ERROR ("[X] ")


/**
 * @brief Return the greater of two @c uint16_t values
 */
static uint16_t s_u16max(uint16_t a, uint16_t b)
{
    return (a > b) ? a : b;
}


/* Confirm dialog state and layout */
/** Internal layout record for the confirm dialog */
typedef struct {
    uint16_t w;
    uint16_t h;
    uint16_t btn_w;
    uint16_t btn_h;
    int16_t prompt_x;
    int16_t prompt_y;
    int16_t btn_y;
    int16_t cancel_x;
    int16_t confirm_x;
    int16_t cancel_label_x;
    int16_t cancel_label_y;
    int16_t confirm_label_x;
    int16_t confirm_label_y;
    char prompt[DIALOG_TEXT_MAX_LEN];
    char cancel_label[DIALOG_TEXT_MAX_LEN];
    char confirm_label[DIALOG_TEXT_MAX_LEN];
} s_confirm_layout_td;


/** XCB window of the currently visible confirm dialog */
static xcb_window_t s_confirm_window = XCB_WINDOW_NONE;

/** Currently highlighted button: 0 = cancel (default), 1 = confirm */
static int s_confirm_selected = 0;

/** Cached layout used for both creation and repaint */
static s_confirm_layout_td s_confirm_layout;


/**
 * @brief Compute layout geometry for the confirm dialog
 *
 * Fills @p layout from the prompt and button label text already stored
 * in @c layout->prompt, @c layout->cancel_label, and
 * @c layout->confirm_label.
 *
 * @param layout Layout structure containing input text and receiving
 *               the computed dialog geometry
 */
static void s_confirm_compute_layout(s_confirm_layout_td *layout)
{
    uint16_t prompt_w;
    uint16_t cancel_w;
    uint16_t confirm_w;
    uint16_t btn_label_w;
    uint16_t btns_group_w;
    uint16_t btns_span_w;
    uint16_t prompt_span_w;

    if (layout == NULL) {
        return;
    }

    prompt_w = menu_draw_measure(layout->prompt);
    cancel_w = menu_draw_measure(layout->cancel_label);
    confirm_w = menu_draw_measure(layout->confirm_label);
    btn_label_w = s_u16max(cancel_w, confirm_w);

    layout->btn_w = s_u16max(DIALOG_BTN_MIN_W,
            (uint16_t) (btn_label_w + (DIALOG_BTN_LABEL_PAD_X * 2u)));
    layout->btn_h = DIALOG_BTN_H;

    btns_group_w = (uint16_t) ((layout->btn_w * 2u) + DIALOG_BTN_GAP);
    btns_span_w = (uint16_t) (btns_group_w + (DIALOG_PAD_X * 2u));

    prompt_span_w = (uint16_t) (prompt_w + (DIALOG_PAD_X * 2u));

    layout->w = s_u16max(DIALOG_MIN_W,
            s_u16max(btns_span_w, prompt_span_w));
    layout->h = s_u16max(DIALOG_MIN_H,
            (uint16_t) (DIALOG_PROMPT_BASELINE_Y +
                DIALOG_PROMPT_TO_BTN_GAP +
                layout->btn_h +
                DIALOG_PAD_BOTTOM));
    layout->cancel_x = (int16_t) ((layout->w - btns_group_w) / 2u);
    layout->confirm_x = (int16_t) (layout->cancel_x +
            (int16_t) layout->btn_w +
            (int16_t) DIALOG_BTN_GAP);
    layout->btn_y = (int16_t) ((int16_t) layout->h -
            (int16_t) DIALOG_PAD_BOTTOM -
            (int16_t) layout->btn_h);
    layout->prompt_x = (int16_t) ((layout->w > prompt_w)
            ? (layout->w - prompt_w) / 2u : DIALOG_PAD_X);

    if (layout->prompt_x < (int16_t) DIALOG_PAD_X) {
        layout->prompt_x = (int16_t) DIALOG_PAD_X;
    }

    layout->prompt_y = (int16_t) DIALOG_PROMPT_BASELINE_Y;
    layout->cancel_label_x = (int16_t) (layout->cancel_x +
            (int16_t) ((layout->btn_w - cancel_w) / 2u));
    layout->cancel_label_y = (int16_t) (layout->btn_y +
            (int16_t) DIALOG_BTN_LABEL_BASELINE_Y);
    layout->confirm_label_x = (int16_t) (layout->confirm_x +
            (int16_t) ((layout->btn_w - confirm_w) / 2u));
    layout->confirm_label_y = (int16_t) (layout->btn_y +
            (int16_t) DIALOG_BTN_LABEL_BASELINE_Y);
}

/**
 * @brief Render the confirm dialog (prompt and both buttons)
 *
 * Draws the dialog background, both buttons, and their labels using the
 * current confirm-dialog layout and selection state.
 *
 * @param connection XCB connection
 * @param config     Configuration providing the dialog theme colors
 */
static void s_confirm_draw(xcb_connection_t *connection,
        const config_td *config)
{
    xcb_gcontext_t gc;
    xcb_rectangle_t rect;
    uint32_t gc_vals[1];
    uint32_t bg_win;
    uint32_t fg_sel;
    uint32_t bg_sel;
    uint32_t fg_nor;
    uint32_t bg_nor;
    const s_confirm_layout_td *lo = &s_confirm_layout;

    if (connection == NULL || config == NULL ||
            s_confirm_window == XCB_WINDOW_NONE) {
        return;
    }

    bg_win = config->theme.window.inactive.background_color;
    fg_sel = config->theme.window.active.foreground_color;
    bg_sel = config->theme.window.active.background_color;
    fg_nor = config->theme.window.inactive.foreground_color;
    bg_nor = config->theme.window.inactive.background_color;

    gc = xcb_generate_id(connection);

    /* Clear background */
    gc_vals[0] = bg_win;
    xcb_create_gc(connection, gc, s_confirm_window,
            XCB_GC_FOREGROUND, gc_vals);
    rect.x = 0;
    rect.y = 0;
    rect.width = lo->w;
    rect.height = lo->h;
    xcb_poly_fill_rectangle(connection, s_confirm_window, gc, 1, &rect);

    /* Cancel button */
    gc_vals[0] = (s_confirm_selected == 0) ? bg_sel : bg_nor;
    xcb_change_gc(connection, gc, XCB_GC_FOREGROUND, gc_vals);
    rect.x = lo->cancel_x;
    rect.y = lo->btn_y;
    rect.width = lo->btn_w;
    rect.height = lo->btn_h;
    xcb_poly_fill_rectangle(connection, s_confirm_window, gc, 1, &rect);

    /* Confirm button */
    gc_vals[0] = (s_confirm_selected == 1) ? bg_sel : bg_nor;
    xcb_change_gc(connection, gc, XCB_GC_FOREGROUND, gc_vals);
    rect.x = lo->confirm_x;
    xcb_poly_fill_rectangle(connection, s_confirm_window, gc, 1, &rect);
    xcb_free_gc(connection, gc);

    /* Prompt text */
    text_renderer_set_color(fg_nor, bg_win);
    menu_draw_label(connection, s_confirm_window,
            lo->prompt_x, lo->prompt_y, lo->prompt);

    /* Cancel label */
    text_renderer_set_color(
            (s_confirm_selected == 0) ? fg_sel : fg_nor,
            (s_confirm_selected == 0) ? bg_sel : bg_nor);
    menu_draw_label(connection, s_confirm_window,
            lo->cancel_label_x, lo->cancel_label_y,
            lo->cancel_label);

    /* Confirm label */
    text_renderer_set_color(
            (s_confirm_selected == 1) ? fg_sel : fg_nor,
            (s_confirm_selected == 1) ? bg_sel : bg_nor);
    menu_draw_label(connection, s_confirm_window,
            lo->confirm_label_x, lo->confirm_label_y,
            lo->confirm_label);

    xcb_flush(connection);
}


/* Open the confirm dialog */
void menu_confirm_dialog_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const char *prompt,
        const char *cancel_label, const char *confirm_label)
{
    int16_t x;
    int16_t y;
    uint32_t mask;
    uint32_t values[3];

    if (connection == NULL || surface == NULL || config == NULL ||
            surface->screen == NULL) {
        return;
    }

    if (s_confirm_window != XCB_WINDOW_NONE) {
        return;
    }

    (void) strncpy(s_confirm_layout.prompt,
            (prompt != NULL) ? prompt : "",
            sizeof(s_confirm_layout.prompt) - 1u);
    s_confirm_layout.prompt[sizeof(s_confirm_layout.prompt) - 1u] = '\0';

    (void) strncpy(s_confirm_layout.cancel_label,
            (cancel_label != NULL) ? cancel_label : "",
            sizeof(s_confirm_layout.cancel_label) - 1u);
    s_confirm_layout.cancel_label[
        sizeof(s_confirm_layout.cancel_label) - 1u] = '\0';

    (void) strncpy(s_confirm_layout.confirm_label,
            (confirm_label != NULL) ? confirm_label : "",
            sizeof(s_confirm_layout.confirm_label) - 1u);
    s_confirm_layout.confirm_label[
        sizeof(s_confirm_layout.confirm_label) - 1u] = '\0';

    text_renderer_init(connection, config->theme.window.active.font);
    s_confirm_compute_layout(&s_confirm_layout);
    s_confirm_selected = 0;

    menu_dialog_center(surface, s_confirm_layout.w,
            s_confirm_layout.h, &x, &y);

    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = config->theme.window.inactive.background_color;
    values[1] = config->theme.window.active.border_color;
    values[2] = XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_BUTTON_PRESS     |
        XCB_EVENT_MASK_KEY_PRESS;

    s_confirm_window = xcb_generate_id(connection);
    xcb_create_window(connection,
            XCB_COPY_FROM_PARENT,
            s_confirm_window,
            surface->screen->root,
            x, y,
            s_confirm_layout.w, s_confirm_layout.h,
            1,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    xcb_map_window(connection, s_confirm_window);
    xcb_flush(connection);

    xcb_grab_keyboard(connection, 0, s_confirm_window,
            XCB_CURRENT_TIME,
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    xcb_set_input_focus(connection,
            XCB_INPUT_FOCUS_POINTER_ROOT,
            s_confirm_window, XCB_CURRENT_TIME);
    xcb_flush(connection);
}


/* Destroy the confirm dialog */
void menu_confirm_dialog_close(xcb_connection_t *connection)
{
    if (connection == NULL || s_confirm_window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_ungrab_keyboard(connection, XCB_CURRENT_TIME);
    xcb_destroy_window(connection, s_confirm_window);
    xcb_flush(connection);
    s_confirm_window   = XCB_WINDOW_NONE;
    s_confirm_selected = 0;
}


/* Repaint the confirm dialog */
void menu_confirm_dialog_repaint(xcb_connection_t *connection,
        const config_td *config)
{
    s_confirm_draw(connection, config);
}


/* Handle a mouse click in the confirm dialog */
bool menu_confirm_dialog_handle_click(xcb_connection_t *connection,
        int x, int y)
{
    const s_confirm_layout_td *lo = &s_confirm_layout;

    if (connection == NULL || s_confirm_window == XCB_WINDOW_NONE) {
        return false;
    }

    if (y >= (int) lo->btn_y &&
            y < (int) lo->btn_y + (int) lo->btn_h) {
        if (x >= (int) lo->cancel_x &&
                x < (int) lo->cancel_x + (int) lo->btn_w) {
            s_confirm_selected = 0;
            menu_confirm_dialog_close(connection);
            return true;
        }
        if (x >= (int) lo->confirm_x &&
                x < (int) lo->confirm_x + (int) lo->btn_w) {
            s_confirm_selected = 1;
            menu_confirm_dialog_accept(connection, NULL);
            return true;
        }
    }

    return false;
}


/* Cycle to next button */
void menu_confirm_dialog_toggle_selection(void)
{
    s_confirm_selected = (s_confirm_selected == 0) ? 1 : 0;
}


/* Activate current button */
void menu_confirm_dialog_accept(xcb_connection_t *connection,
        void (*on_confirm)(xcb_connection_t *))
{
    int selected = s_confirm_selected;
    menu_confirm_dialog_close(connection);
    if (selected == 1 && on_confirm != NULL) {
        on_confirm(connection);
    }
}


/* Is the confirm dialog visible? */
bool menu_confirm_dialog_is_open(void)
{
    return s_confirm_window != XCB_WINDOW_NONE;
}


/* Return the confirm dialog window */
xcb_window_t menu_confirm_dialog_window(void)
{
    return s_confirm_window;
}


/* Message dialog state and layout */

/** Internal layout record for the message dialog */
typedef struct {
    uint16_t w;
    uint16_t h;
    uint16_t btn_w;
    uint16_t btn_h;
    int16_t msg_x;
    int16_t msg_y;
    int16_t btn_x;
    int16_t btn_y;
    int16_t btn_label_x;
    int16_t btn_label_y;
    char message[DIALOG_TEXT_MAX_LEN];
} s_message_layout_td;


/** XCB window of the currently visible message dialog */
static xcb_window_t s_message_window = XCB_WINDOW_NONE;

/** Cached layout used for both creation and repaint */
static s_message_layout_td s_message_layout;


/**
 * @brief Compute layout geometry for the message dialog
 *
 * Uses the message text already stored in @p layout->message.
 */
static void s_message_compute_layout(s_message_layout_td *layout)
{
    uint16_t msg_w;
    uint16_t ok_w;
    uint16_t msg_span_w;
    uint16_t ok_span_w;

    if (layout == NULL) {
        return;
    }

    msg_w = menu_draw_measure(layout->message);
    ok_w  = menu_draw_measure(DIALOG_MSG_LABEL_OK);

    layout->btn_w = s_u16max(DIALOG_BTN_MIN_W,
            (uint16_t) (ok_w + (DIALOG_BTN_LABEL_PAD_X * 2u)));
    layout->btn_h = DIALOG_BTN_H;

    msg_span_w = (uint16_t) (msg_w + (DIALOG_PAD_X * 2u));
    ok_span_w  = (uint16_t) (layout->btn_w + (DIALOG_PAD_X * 2u));

    layout->w = s_u16max(DIALOG_MIN_W,
            s_u16max(msg_span_w, ok_span_w));
    layout->h = s_u16max(DIALOG_MIN_H,
            (uint16_t) (DIALOG_PROMPT_BASELINE_Y +
                DIALOG_PROMPT_TO_BTN_GAP +
                layout->btn_h +
                DIALOG_PAD_BOTTOM));

    layout->btn_x = (int16_t) ((layout->w - layout->btn_w) / 2u);
    layout->btn_y = (int16_t) ((int16_t) layout->h -
            (int16_t) DIALOG_PAD_BOTTOM -
            (int16_t) layout->btn_h);

    layout->msg_x = (int16_t) ((layout->w > msg_w)
            ? (layout->w - msg_w) / 2u : DIALOG_PAD_X);
    if (layout->msg_x < (int16_t) DIALOG_PAD_X) {
        layout->msg_x = (int16_t) DIALOG_PAD_X;
    }
    layout->msg_y = (int16_t) DIALOG_PROMPT_BASELINE_Y;

    layout->btn_label_x = (int16_t) (layout->btn_x +
            (int16_t) ((layout->btn_w - ok_w) / 2u));
    layout->btn_label_y = (int16_t) (layout->btn_y +
            (int16_t) DIALOG_BTN_LABEL_BASELINE_Y);
}


/**
 * @brief Render the message dialog (message text and OK button)
 */
static void s_message_draw(xcb_connection_t *connection,
        const config_td *config)
{
    xcb_gcontext_t gc;
    xcb_rectangle_t rect;
    uint32_t gc_vals[1];
    uint32_t bg_win;
    uint32_t fg_sel;
    uint32_t bg_sel;
    uint32_t fg_nor;
    const s_message_layout_td *lo = &s_message_layout;

    if (connection == NULL || config == NULL ||
            s_message_window == XCB_WINDOW_NONE) {
        return;
    }

    bg_win = config->theme.window.inactive.background_color;
    fg_sel = config->theme.window.active.foreground_color;
    bg_sel = config->theme.window.active.background_color;
    fg_nor = config->theme.window.inactive.foreground_color;

    gc = xcb_generate_id(connection);

    /* Clear background */
    gc_vals[0] = bg_win;
    xcb_create_gc(connection, gc, s_message_window,
            XCB_GC_FOREGROUND, gc_vals);
    rect.x = 0;
    rect.y = 0;
    rect.width = lo->w;
    rect.height = lo->h;
    xcb_poly_fill_rectangle(connection, s_message_window, gc, 1, &rect);

    /* OK button (always "selected" / highlighted) */
    gc_vals[0] = bg_sel;
    xcb_change_gc(connection, gc, XCB_GC_FOREGROUND, gc_vals);
    rect.x = lo->btn_x;
    rect.y = lo->btn_y;
    rect.width = lo->btn_w;
    rect.height = lo->btn_h;
    xcb_poly_fill_rectangle(connection, s_message_window, gc, 1, &rect);
    xcb_free_gc(connection, gc);

    /* Message text */
    text_renderer_set_color(fg_nor, bg_win);
    menu_draw_label(connection, s_message_window,
            lo->msg_x, lo->msg_y, lo->message);

    /* OK label */
    text_renderer_set_color(fg_sel, bg_sel);
    menu_draw_label(connection, s_message_window,
            lo->btn_label_x, lo->btn_label_y,
            DIALOG_MSG_LABEL_OK);

    xcb_flush(connection);
}


/* Open the message dialog */
void menu_message_dialog_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const char *message, menu_msg_level_e level)
{
    int16_t x;
    int16_t y;
    uint32_t mask;
    uint32_t values[3];
    const char *prefix;

    if (connection == NULL || surface == NULL || config == NULL ||
            surface->screen == NULL) {
        return;
    }

    if (s_message_window != XCB_WINDOW_NONE) {
        return;
    }

    switch (level) {
        case MENU_MSG_LEVEL_WARNING:
            prefix = DIALOG_MSG_PREFIX_WARNING;
            break;

        case MENU_MSG_LEVEL_ERROR:
            prefix = DIALOG_MSG_PREFIX_ERROR;
            break;

        case MENU_MSG_LEVEL_INFO:
            prefix = DIALOG_MSG_PREFIX_INFO;
            break;
    }

    (void) strncpy(s_message_layout.message, prefix,
            sizeof(s_message_layout.message) - 1u);
    s_message_layout.message[sizeof(s_message_layout.message) - 1u] = '\0';

    if (message != NULL) {
        size_t plen = strlen(s_message_layout.message);
        (void) strncpy(s_message_layout.message + plen, message,
                sizeof(s_message_layout.message) - 1u - plen);
        s_message_layout.message[
            sizeof(s_message_layout.message) - 1u] = '\0';
    }

    text_renderer_init(connection, config->theme.window.active.font);
    s_message_compute_layout(&s_message_layout);

    menu_dialog_center(surface, s_message_layout.w,
            s_message_layout.h, &x, &y);

    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = config->theme.window.inactive.background_color;
    values[1] = config->theme.window.active.border_color;
    values[2] = XCB_EVENT_MASK_EXPOSURE     |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_KEY_PRESS;

    s_message_window = xcb_generate_id(connection);
    xcb_create_window(connection,
            XCB_COPY_FROM_PARENT,
            s_message_window,
            surface->screen->root,
            x, y,
            s_message_layout.w, s_message_layout.h,
            1,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    xcb_map_window(connection, s_message_window);
    xcb_flush(connection);

    xcb_grab_keyboard(connection, 0, s_message_window,
            XCB_CURRENT_TIME,
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    xcb_set_input_focus(connection,
            XCB_INPUT_FOCUS_POINTER_ROOT,
            s_message_window, XCB_CURRENT_TIME);
    xcb_flush(connection);
}


/* Destroy the message dialog */
void menu_message_dialog_close(xcb_connection_t *connection)
{
    if (connection == NULL || s_message_window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_ungrab_keyboard(connection, XCB_CURRENT_TIME);
    xcb_destroy_window(connection, s_message_window);
    xcb_flush(connection);
    s_message_window = XCB_WINDOW_NONE;
}


/* Repaint the message dialog */
void menu_message_dialog_repaint(xcb_connection_t *connection,
        const config_td *config)
{
    s_message_draw(connection, config);
}


/* Handle a click in the message dialog */
void menu_message_dialog_handle_click(xcb_connection_t *connection,
        int x, int y)
{
    const s_message_layout_td *lo = &s_message_layout;

    if (connection == NULL || s_message_window == XCB_WINDOW_NONE) {
        return;
    }

    if (y >= (int) lo->btn_y &&
            y < (int) lo->btn_y + (int) lo->btn_h &&
            x >= (int) lo->btn_x &&
            x < (int) lo->btn_x + (int) lo->btn_w) {
        menu_message_dialog_close(connection);
    }
}


/* Check if the message dialog is visible */
bool menu_message_dialog_is_open(void)
{
    return s_message_window != XCB_WINDOW_NONE;
}


/* Return the message dialog window */
xcb_window_t menu_message_dialog_window(void)
{
    return s_message_window;
}


/* Centering helper */
/* Compute centered coordinates for a dialog on a surface */
void menu_dialog_center(const surface_td *surface,
        uint16_t width, uint16_t height, int16_t *out_x, int16_t *out_y)
{
    if (out_x == NULL || out_y == NULL) {
        return;
    }

    if (surface == NULL) {
        *out_x = 0;
        *out_y = 0;
        return;
    }

    *out_x = (int16_t) ((surface->properties.dim.w > width)
            ? (surface->properties.dim.w - width) / 2u : 0u);
    *out_y = (int16_t) ((surface->properties.dim.h > height)
            ? (surface->properties.dim.h - height) / 2u : 0u);
}
