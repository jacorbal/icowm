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

/* XCB includes */
#include <xcb/xcb.h>

/* Util includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <config.h>
#include <render/text.h>
#include <surface.h>

/* Local includes */
#include <menu/dialog.h>
#include <menu/draw.h>


/* Shared layout constants */
/** Bottom padding below buttons (pixels) */
#define DIALOG_PAD_BOTTOM (8u)

/** Minimum dialog width (pixels) */
#define DIALOG_MIN_W (220u)

/** Minimum dialog height (pixels) */
#define DIALOG_MIN_H (90u)

/** Minimum button width (pixels) */
#define DIALOG_BTN_MIN_W (60u)

/** Prompt baseline position from dialog top (pixels) */
#define DIALOG_PROMPT_BASELINE_Y (22u)

/** Vertical gap between prompt baseline and button top (pixels) */
#define DIALOG_PROMPT_TO_BTN_GAP (34u)

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
    char prompt[DIALOG_TEXT_MAX_LEN];
    char cancel_label[DIALOG_TEXT_MAX_LEN];
    char confirm_label[DIALOG_TEXT_MAX_LEN];
} s_confirm_layout_td;


/** XCB window of the currently visible confirm dialog */
static xcb_window_t s_confirm_window = XCB_WINDOW_NONE;

/** Currently highlighted button: 0 = cancel (default), 1 = confirm */
static int s_confirm_selected = 0;

/** Callback invoked when the confirm button is activated */
static void (*s_confirm_callback)(xcb_connection_t *) = NULL;

/** Cached layout used for both creation and repaint */
static s_confirm_layout_td s_confirm_layout;


/**
 * @brief Compute layout geometry for the confirm dialog
 *
 * Fills @p layout from the prompt and button label text already stored
 * in @c layout->prompt, @c layout->cancel_label, and
 * @c layout->confirm_label.
 *
 * Button sizing measures every label in both @c button.unselected and
 * @c button.selected fonts and keeps the wider/taller of the two, so
 * the button is always big enough for whichever one actually ends up
 * selected; the exact label position within that button is computed
 * separately at draw time (see @c s_confirm_draw), using whichever
 * font is actually being drawn, so the text stays centered even when
 * @c selected is bold and therefore wider than @c unselected.
 *
 * @param connection XCB connection, needed to measure text in each
 *                   candidate font
 * @param config     Theme providing button fonts and padding
 * @param layout     Layout structure containing input text and
 *                   receiving the computed dialog geometry
 */
static void s_confirm_compute_layout(xcb_connection_t *connection,
        const config_td *config, s_confirm_layout_td *layout)
{
    uint16_t prompt_w;
    uint16_t cancel_w;
    uint16_t confirm_w;
    uint16_t btn_label_w;
    uint16_t btns_group_w;
    uint16_t btns_span_w;
    uint16_t prompt_span_w;
    uint16_t btn_text_h;
    uint16_t pad_x;
    uint16_t pad_y;
    uint16_t gap;
    uint16_t label_pad_x;

    if (connection == NULL || config == NULL || layout == NULL) {
        return;
    }

    pad_x = (uint16_t) config->theme.dialog.button.padding.horizontal;
    pad_y = (uint16_t) config->theme.dialog.button.padding.vertical;
    gap = (uint16_t) config->theme.dialog.button.gap;
    label_pad_x = (uint16_t) config->theme.dialog.label.padding.horizontal;

    text_renderer_init(connection, config->theme.dialog.label.font);
    prompt_w = menu_draw_measure(layout->prompt);

    /* Measure both labels in both fonts and keep the widest/tallest
     * result: whichever button ends up selected renders in
     * 'button.selected.font' (bold by default), and sizing off only
     * 'unselected' would leave no room for that, causing the
     * off-center look this whole function exists to avoid. */
    text_renderer_init(connection, config->theme.dialog.button.unselected.font);
    cancel_w = menu_draw_measure(layout->cancel_label);
    confirm_w = menu_draw_measure(layout->confirm_label);
    btn_text_h = (uint16_t) (text_font_ascent() + text_font_descent());

    text_renderer_init(connection, config->theme.dialog.button.selected.font);
    cancel_w = s_u16max(cancel_w, menu_draw_measure(layout->cancel_label));
    confirm_w = s_u16max(confirm_w, menu_draw_measure(layout->confirm_label));
    btn_text_h = s_u16max(btn_text_h,
            (uint16_t) (text_font_ascent() + text_font_descent()));

    btn_label_w = s_u16max(cancel_w, confirm_w);

    layout->btn_w = s_u16max(DIALOG_BTN_MIN_W,
            (uint16_t) (btn_label_w + (pad_x * 2u)));
    layout->btn_h = (uint16_t) (btn_text_h + (pad_y * 2u));

    btns_group_w = (uint16_t) ((layout->btn_w * 2u) + gap);
    btns_span_w = (uint16_t) (btns_group_w + (label_pad_x * 2u));

    prompt_span_w = (uint16_t) (prompt_w + (label_pad_x * 2u));

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
            (int16_t) gap);
    layout->btn_y = (int16_t) ((int16_t) layout->h -
            (int16_t) DIALOG_PAD_BOTTOM -
            (int16_t) layout->btn_h);
    layout->prompt_x = (int16_t) ((layout->w > prompt_w)
            ? (layout->w - prompt_w) / 2u : label_pad_x);

    if (layout->prompt_x < (int16_t) label_pad_x) {
        layout->prompt_x = (int16_t) label_pad_x;
    }

    layout->prompt_y = (int16_t) DIALOG_PROMPT_BASELINE_Y;

    /* Each button label's own X/Y depends on which font actually ends
     * up drawing it (unselected or selected), which can change every
     * repaint as the user tabs between buttons; see 's_confirm_draw',
     * which computes both freshly right before drawing instead of
     * relying on a value fixed here. */
}


/**
 * @brief Draw a solid border outline around a button, if its theme
 *        style gives it a non-zero border width
 *
 * @param connection XCB connection
 * @param window     Target drawable window
 * @param color      Border color
 * @param width      Border width in pixels; a no-op when 0
 * @param x          Left edge of the button fill rectangle
 * @param y          Top edge of the button fill rectangle
 * @param w          Width of the button fill rectangle (pixels)
 * @param h          Height of the button fill rectangle (pixels)
 */
static void s_draw_button_border(xcb_connection_t *connection,
        xcb_window_t window, uint32_t color, uint32_t width,
        int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    xcb_gcontext_t gc;
    xcb_rectangle_t rect;

    if (connection == NULL || window == XCB_WINDOW_NONE ||
            w == 0u || h == 0u || width == 0u) {
        return;
    }

    gc = xcb_generate_id(connection);
    xcb_create_gc(connection, gc, window,
            XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH,
            (const uint32_t[]) { color, width });

    rect.x = (int16_t) (x + (int16_t) (width / 2u));
    rect.y = (int16_t) (y + (int16_t) (width / 2u));
    rect.width = (uint16_t) (w - width);
    rect.height = (uint16_t) (h - width);
    xcb_poly_rectangle(connection, window, gc, 1, &rect);
    xcb_free_gc(connection, gc);
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
    uint16_t label_w;
    int16_t label_x;
    int16_t label_y;
    const s_confirm_layout_td *lo = &s_confirm_layout;

    if (connection == NULL || config == NULL ||
            s_confirm_window == XCB_WINDOW_NONE) {
        return;
    }

    bg_win = config->theme.dialog.background;
    fg_sel = config->theme.dialog.button.selected.color.foreground;
    bg_sel = config->theme.dialog.button.selected.color.background;
    fg_nor = config->theme.dialog.button.unselected.color.foreground;
    bg_nor = config->theme.dialog.button.unselected.color.background;

    /* 'text_renderer_init' sets shared, module-level font state used
     * by every 'text_draw_string' caller, not something private to
     * this dialog.  Re-asserting it right before each piece of text
     * below, not just once when the dialog first opens, is what keeps
     * every label on the correct font: something else repainting text
     * in between two key presses here (a titlebar, a menu, the systray
     * clock) would otherwise leave its own font selected the next
     * time this function runs, and the prompt and the two buttons can
     * each have their own font besides. */

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

    /* Border around each button, from its own theme style */
    s_draw_button_border(connection, s_confirm_window,
            (s_confirm_selected == 0)
                ? config->theme.dialog.button.selected.border.color
                : config->theme.dialog.button.unselected.border.color,
            (s_confirm_selected == 0)
                ? config->theme.dialog.button.selected.border.width
                : config->theme.dialog.button.unselected.border.width,
            lo->cancel_x, lo->btn_y, lo->btn_w, lo->btn_h);
    s_draw_button_border(connection, s_confirm_window,
            (s_confirm_selected == 1)
                ? config->theme.dialog.button.selected.border.color
                : config->theme.dialog.button.unselected.border.color,
            (s_confirm_selected == 1)
                ? config->theme.dialog.button.selected.border.width
                : config->theme.dialog.button.unselected.border.width,
            lo->confirm_x, lo->btn_y, lo->btn_w, lo->btn_h);

    /* Prompt text */
    text_renderer_init(connection, config->theme.dialog.label.font);
    text_renderer_set_color(config->theme.dialog.label.foreground, bg_win);
    menu_draw_label(connection, s_confirm_window,
            lo->prompt_x, lo->prompt_y, lo->prompt);

    /* Cancel label: font, and therefore width, depends on whether
     * this button is the current selection, so both are recomputed
     * fresh on every repaint (see the doc comment on
     * 's_confirm_compute_layout') rather than using a fixed position;
     * the vertical centering the same way, using the active font's
     * own ascent/descent against 'btn_h' so it stays centered
     * regardless of which font is taller. */
    text_renderer_init(connection, (s_confirm_selected == 0)
            ? config->theme.dialog.button.selected.font
            : config->theme.dialog.button.unselected.font);
    label_w = menu_draw_measure(lo->cancel_label);
    label_x = (int16_t) (lo->cancel_x +
            (int16_t) ((lo->btn_w - label_w) / 2u));
    label_y = (int16_t) (lo->btn_y +
            (int16_t) ((lo->btn_h -
                    (uint16_t) (text_font_ascent() +
                        text_font_descent())) / 2u) +
            text_font_ascent());
    text_renderer_set_color(
            (s_confirm_selected == 0) ? fg_sel : fg_nor,
            (s_confirm_selected == 0) ? bg_sel : bg_nor);
    menu_draw_label(connection, s_confirm_window,
            label_x, label_y, lo->cancel_label);

    /* Confirm label: same reasoning as the cancel label above */
    text_renderer_init(connection, (s_confirm_selected == 1)
            ? config->theme.dialog.button.selected.font
            : config->theme.dialog.button.unselected.font);
    label_w = menu_draw_measure(lo->confirm_label);
    label_x = (int16_t) (lo->confirm_x +
            (int16_t) ((lo->btn_w - label_w) / 2u));
    label_y = (int16_t) (lo->btn_y +
            (int16_t) ((lo->btn_h -
                    (uint16_t) (text_font_ascent() +
                        text_font_descent())) / 2u) +
            text_font_ascent());
    text_renderer_set_color(
            (s_confirm_selected == 1) ? fg_sel : fg_nor,
            (s_confirm_selected == 1) ? bg_sel : bg_nor);
    menu_draw_label(connection, s_confirm_window,
            label_x, label_y, lo->confirm_label);

    xcb_flush(connection);
}


/* Open the confirm dialog */
void menu_confirm_dialog_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const char *prompt,
        const char *cancel_label, const char *confirm_label,
        void (*on_confirm)(xcb_connection_t *))
{
    int16_t x;
    int16_t y;
    uint32_t mask;
    uint32_t values[4];

    if (connection == NULL || surface == NULL || config == NULL ||
            surface->screen == NULL) {
        return;
    }

    if (s_confirm_window != XCB_WINDOW_NONE) {
        return;
    }

    (void) safe_strncpy(s_confirm_layout.prompt,
            (prompt != NULL) ? prompt : "",
            sizeof(s_confirm_layout.prompt) - 1u);
    s_confirm_layout.prompt[sizeof(s_confirm_layout.prompt) - 1u] = '\0';

    (void) safe_strncpy(s_confirm_layout.cancel_label,
            (cancel_label != NULL) ? cancel_label : "",
            sizeof(s_confirm_layout.cancel_label) - 1u);
    s_confirm_layout.cancel_label[
        sizeof(s_confirm_layout.cancel_label) - 1u] = '\0';

    (void) safe_strncpy(s_confirm_layout.confirm_label,
            (confirm_label != NULL) ? confirm_label : "",
            sizeof(s_confirm_layout.confirm_label) - 1u);
    s_confirm_layout.confirm_label[
        sizeof(s_confirm_layout.confirm_label) - 1u] = '\0';

    s_confirm_compute_layout(connection, config, &s_confirm_layout);
    s_confirm_selected = 0;
    s_confirm_callback = on_confirm;

    menu_dialog_center(surface, s_confirm_layout.w,
            s_confirm_layout.h, &x, &y);

    /* XCB requires attribute values to be listed in ascending bit order
     * of their mask.
     *
     * BACK_PIXEL(2) < BORDER_PIXEL(8) < OVERRIDE_REDIRECT(512) < EVENT_MASK(2048)
     * */
    mask = XCB_CW_BACK_PIXEL        |
        XCB_CW_BORDER_PIXEL         |
        XCB_CW_OVERRIDE_REDIRECT    |
        XCB_CW_EVENT_MASK;
    values[0] = config->theme.dialog.background;
    values[1] = config->theme.dialog.border.color;
    values[2] = 1;  /* override_redirect: keep WM from managing it */
    values[3] = XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_BUTTON_PRESS     |
        XCB_EVENT_MASK_KEY_PRESS;

    s_confirm_window = xcb_generate_id(connection);
    xcb_create_window(connection,
            XCB_COPY_FROM_PARENT,
            s_confirm_window,
            surface->screen->root,
            x, y,
            s_confirm_layout.w, s_confirm_layout.h,
            (uint16_t) config->theme.dialog.border.width,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    xcb_map_window(connection, s_confirm_window);
    xcb_configure_window(connection, s_confirm_window,
            XCB_CONFIG_WINDOW_STACK_MODE,
            (const uint32_t[]) { XCB_STACK_MODE_ABOVE });
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

    s_confirm_window= XCB_WINDOW_NONE;
    s_confirm_selected = 0;
    s_confirm_callback = NULL;
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
            menu_confirm_dialog_accept(connection, s_confirm_callback);
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
 * Uses the message text already stored in @p layout->message.  The
 * "OK" button always renders in @c button.selected.font (it has no
 * unselected state to switch to), so its width and label position are
 * measured directly in that font, avoiding the same off-center risk
 * @c s_confirm_compute_layout guards against for the two-button
 * confirm dialog.
 *
 * @param connection XCB connection, needed to measure the label text
 * @param config     Theme providing button font and padding
 * @param layout     Layout structure containing input text and
 *                   receiving the computed dialog geometry
 */
static void s_message_compute_layout(xcb_connection_t *connection,
        const config_td *config, s_message_layout_td *layout)
{
    uint16_t msg_w;
    uint16_t ok_w;
    uint16_t msg_span_w;
    uint16_t ok_span_w;
    uint16_t pad_x;
    uint16_t pad_y;
    uint16_t btn_text_h;
    uint16_t label_pad_x;

    if (connection == NULL || config == NULL || layout == NULL) {
        return;
    }

    label_pad_x = (uint16_t) config->theme.dialog.label.padding.horizontal;
    pad_x = (uint16_t) config->theme.dialog.button.padding.horizontal;
    pad_y = (uint16_t) config->theme.dialog.button.padding.vertical;

    text_renderer_init(connection, config->theme.dialog.label.font);
    msg_w = menu_draw_measure(layout->message);

    text_renderer_init(connection, config->theme.dialog.button.selected.font);
    ok_w = menu_draw_measure(DIALOG_MSG_LABEL_OK);
    btn_text_h = (uint16_t) (text_font_ascent() + text_font_descent());

    layout->btn_w = s_u16max(DIALOG_BTN_MIN_W,
            (uint16_t) (ok_w + (pad_x * 2u)));
    layout->btn_h = (uint16_t) (btn_text_h + (pad_y * 2u));

    msg_span_w = (uint16_t) (msg_w + (label_pad_x * 2u));
    ok_span_w  = (uint16_t) (layout->btn_w + (label_pad_x * 2u));

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
            ? (layout->w - msg_w) / 2u : label_pad_x);
    if (layout->msg_x < (int16_t) label_pad_x) {
        layout->msg_x = (int16_t) label_pad_x;
    }
    layout->msg_y = (int16_t) DIALOG_PROMPT_BASELINE_Y;

    layout->btn_label_x = (int16_t) (layout->btn_x +
            (int16_t) ((layout->btn_w - ok_w) / 2u));
    layout->btn_label_y = (int16_t) (layout->btn_y +
            (int16_t) ((layout->btn_h - btn_text_h) / 2u) +
            text_font_ascent());
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

    bg_win = config->theme.dialog.background;
    fg_sel = config->theme.dialog.button.selected.color.foreground;
    bg_sel = config->theme.dialog.button.selected.color.background;
    fg_nor = config->theme.dialog.label.foreground;

    /* See the matching comment in 's_confirm_draw' above: font has to
     * be re-asserted right before each piece of text, not just once
     * when the dialog first opens. */

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

    s_draw_button_border(connection, s_message_window,
            config->theme.dialog.button.selected.border.color,
            config->theme.dialog.button.selected.border.width,
            lo->btn_x, lo->btn_y, lo->btn_w, lo->btn_h);

    /* Message text */
    text_renderer_init(connection, config->theme.dialog.label.font);
    text_renderer_set_color(fg_nor, bg_win);
    menu_draw_label(connection, s_message_window,
            lo->msg_x, lo->msg_y, lo->message);

    /* OK label */
    text_renderer_init(connection, config->theme.dialog.button.selected.font);
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
    uint32_t values[4];
    const char *prefix = "\0";

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

    (void) safe_strncpy(s_message_layout.message, prefix,
            sizeof(s_message_layout.message) - 1u);
    s_message_layout.message[sizeof(s_message_layout.message) - 1u] = '\0';

    if (message != NULL) {
        size_t plen = safe_strlen(s_message_layout.message);
        (void) safe_strncpy(s_message_layout.message + plen, message,
                sizeof(s_message_layout.message) - 1u - plen);
        s_message_layout.message[
            sizeof(s_message_layout.message) - 1u] = '\0';
    }

    s_message_compute_layout(connection, config, &s_message_layout);

    menu_dialog_center(surface, s_message_layout.w,
            s_message_layout.h, &x, &y);

    /* XCB requires attribute values to be listed in ascending bit order
     * of their mask.
     *
     * BACK_PIXEL(2) < BORDER_PIXEL(8) < OVERRIDE_REDIRECT(512) < EVENT_MASK(2048)
     * */
    mask = XCB_CW_BACK_PIXEL        |
        XCB_CW_BORDER_PIXEL         |
        XCB_CW_OVERRIDE_REDIRECT    |
        XCB_CW_EVENT_MASK;
    values[0] = config->theme.dialog.background;
    values[1] = config->theme.dialog.border.color;
    values[2] = 1;  /* override_redirect: keep WM from managing it */
    values[3] = XCB_EVENT_MASK_EXPOSURE     |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_KEY_PRESS;

    s_message_window = xcb_generate_id(connection);
    xcb_create_window(connection,
            XCB_COPY_FROM_PARENT,
            s_message_window,
            surface->screen->root,
            x, y,
            s_message_layout.w, s_message_layout.h,
            (uint16_t) config->theme.dialog.border.width,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    xcb_map_window(connection, s_message_window);
    xcb_configure_window(connection, s_message_window,
            XCB_CONFIG_WINDOW_STACK_MODE,
            (const uint32_t[]) { XCB_STACK_MODE_ABOVE });
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
