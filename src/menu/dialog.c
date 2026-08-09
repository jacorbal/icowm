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

#define _POSIX_C_SOURCE 200112L /* popen, pclose */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* popen, pclose */
#include <string.h>     /* memcpy, memset */
#include <time.h>       /* clock_gettime, struct timespec */

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
 * @brief Absolute time the deferred click action (see
 *        's_confirm_defer_action') becomes due, valid only while
 *        's_confirm_deferred_pending' is true
 */
static struct timespec s_confirm_deferred_due;

/**
 * @brief Whether a mouse click on the confirm dialog is waiting to
 *        actually close/accept it, deferred so the newly selected
 *        button is visible for a moment first; see
 *        's_confirm_defer_action' for the full reasoning
 */
static bool s_confirm_deferred_pending = false;

/** How long the newly selected button stays visible before a
 *  mouse-click-triggered close/accept actually happens */
#define DIALOG_CONFIRM_CLICK_DELAY_MS (150)


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
    /* Also cancels any click-triggered close/accept still scheduled
     * (see 's_confirm_defer_action'), so 'menu_confirm_dialog_tick'
     * has nothing left to do once this dialog is gone through some
     * other path (e.g., Escape) before that delay elapsed on its
     * own. */
    s_confirm_deferred_pending = false;
}


/* Repaint the confirm dialog */
void menu_confirm_dialog_repaint(xcb_connection_t *connection,
        const config_td *config)
{
    s_confirm_draw(connection, config);
}


/**
 * @brief Repaint the confirm dialog with its newly clicked selection,
 *        then schedule the actual close/accept for shortly after
 *
 * A mouse click on the button that was not already selected changes
 * 's_confirm_selected' and needs that to actually be visible before
 * the dialog goes away, or the click reads as though it did not
 * register at the right spot even though it did; closing on the very
 * same repaint that shows the new selection would not give a person
 * any real chance to perceive it (screen updates and human perception
 * both take a moment neither this function nor the repaint it just
 * issued can shortcut), and blocking here with a sleep to wait one
 * out would freeze the whole window manager's event loop for that
 * long. 'menu_confirm_dialog_tick', called every main-loop iteration,
 * performs the deferred close/accept once 's_confirm_deferred_due'
 * arrives instead.
 *
 * @param connection XCB connection
 * @param config     Active configuration, for the repaint; the
 *                    deferred action is scheduled even when this is
 *                    @c NULL, just without a repaint first
 *
 * @note Complexity: @e O(1)
 */
static void s_confirm_defer_action(xcb_connection_t *connection,
        const config_td *config)
{
    if (config != NULL) {
        s_confirm_draw(connection, config);
        xcb_flush(connection);
    }

    if (clock_gettime(CLOCK_MONOTONIC, &s_confirm_deferred_due) != 0) {
        /* Could not read the clock to schedule the delay; still
         * better to act immediately than to leave the dialog stuck
         * open with a pending action that can never become due. */
        s_confirm_deferred_pending = false;
        if (s_confirm_selected == 1) {
            menu_confirm_dialog_accept(connection, s_confirm_callback);
        } else {
            menu_confirm_dialog_close(connection);
        }
        return;
    }

    s_confirm_deferred_due.tv_nsec +=
        (long) DIALOG_CONFIRM_CLICK_DELAY_MS * 1000000L;
    if (s_confirm_deferred_due.tv_nsec >= 1000000000L) {
        s_confirm_deferred_due.tv_sec += 1;
        s_confirm_deferred_due.tv_nsec -= 1000000000L;
    }
    s_confirm_deferred_pending = true;
}


/* Milliseconds until the deferred confirm-dialog click action is due */
int menu_confirm_dialog_ms_remaining(void)
{
    struct timespec now;
    long remaining_ms;

    if (!s_confirm_deferred_pending) {
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }

    remaining_ms =
        (long) (s_confirm_deferred_due.tv_sec - now.tv_sec) * 1000L +
        (s_confirm_deferred_due.tv_nsec - now.tv_nsec) / 1000000L;

    return (remaining_ms < 0) ? 0 : (int) remaining_ms;
}


/* Perform the deferred confirm-dialog click action, if due */
void menu_confirm_dialog_tick(xcb_connection_t *connection)
{
    if (!s_confirm_deferred_pending ||
            menu_confirm_dialog_ms_remaining() > 0) {
        return;
    }

    s_confirm_deferred_pending = false;

    if (s_confirm_selected == 1) {
        menu_confirm_dialog_accept(connection, s_confirm_callback);
    } else {
        menu_confirm_dialog_close(connection);
    }
}


/* Handle a mouse click in the confirm dialog */
bool menu_confirm_dialog_handle_click(xcb_connection_t *connection,
        const config_td *config, int x, int y)
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
            s_confirm_defer_action(connection, config);
            return true;
        }
        if (x >= (int) lo->confirm_x &&
                x < (int) lo->confirm_x + (int) lo->btn_w) {
            s_confirm_selected = 1;
            s_confirm_defer_action(connection, config);
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
    int16_t line_height;    /**< Pixel height (ascent + descent) of
                                  one wrapped line in the label font */
    int16_t btn_x;
    int16_t btn_y;
    int16_t btn_label_x;
    int16_t btn_label_y;
    char raw_message[DIALOG_MSG_RAW_MAX_LEN]; /**< Prefix + caller's
                                                     text, before
                                                     wrapping */
    char lines[DIALOG_MSG_MAX_LINES][DIALOG_MSG_LINE_MAX_LEN];
    uint8_t line_count;
} s_message_layout_td;


/** XCB window of the currently visible message dialog */
static xcb_window_t s_message_window = XCB_WINDOW_NONE;

/** Cached layout used for both creation and repaint */
static s_message_layout_td s_message_layout;


/**
 * @brief Word-wrap @p raw into @p lines, each at most
 *        @c DIALOG_MSG_WRAP_WIDTH pixels wide in whichever font
 *        @c text_renderer_init last selected
 *
 * A general-purpose wrap usable for any message dialog text, not
 * specific to any one caller: explicit @c '\n' characters in @p raw
 * force a line break (so a caller that already knows its own
 * paragraph structure is respected exactly), and within each such
 * paragraph, words are packed onto a line up to the wrap width before
 * moving to the next one.  A single word wider than the wrap width on
 * its own is placed on its own line and allowed to overflow rather
 * than being split mid-word, since breaking a word arbitrarily reads
 * worse than a rare, slightly-too-wide line.  Stops after
 * @c DIALOG_MSG_MAX_LINES lines regardless of how much text remains,
 * silently dropping the rest, so a pathologically long message can
 * never grow the dialog (or the fixed-size @p lines array) without
 * bound.
 *
 * @param raw        Null-terminated text to wrap
 * @param lines       Array of @c DIALOG_MSG_MAX_LINES buffers, each
 *                    @c DIALOG_MSG_LINE_MAX_LEN bytes, to receive the
 *                    wrapped lines
 * @param out_count   Receives the number of lines actually produced
 *
 * @note Complexity: @e O(n), where @e n is the length of @p raw
 */
static void s_message_wrap_text(const char *raw,
        char lines[][DIALOG_MSG_LINE_MAX_LEN], uint8_t *out_count)
{
    size_t raw_len;
    size_t i = 0u;
    uint8_t count = 0u;

    if (out_count != NULL) {
        *out_count = 0u;
    }
    if (raw == NULL || lines == NULL || out_count == NULL) {
        return;
    }

    /* Defensively blank every line slot up front, not just the ones
     * this call ends up writing: 'lines' is the caller's own
     * persistent, static buffer (reused call to call, never
     * reallocated), so a slot a previous, longer call wrote into but
     * this call's own (possibly shorter) output never touches again
     * would otherwise still hold that stale content -- harmless as
     * long as every reader stops at 'line_count', but cheap enough to
     * rule out entirely rather than rely on that holding everywhere
     * text ever gets read from this array. */
    memset(lines, 0, (size_t) DIALOG_MSG_MAX_LINES *
            (size_t) DIALOG_MSG_LINE_MAX_LEN);

    raw_len = safe_strlen(raw);

    while (i < raw_len && count < (uint8_t) DIALOG_MSG_MAX_LINES) {
        char line[DIALOG_MSG_LINE_MAX_LEN];
        size_t line_len = 0u;

        line[0] = '\0';

        while (i < raw_len && raw[i] == ' ') {
            ++i;
        }

        while (i < raw_len && raw[i] != '\n') {
            size_t word_start = i;
            size_t word_len = 0u;
            char candidate[DIALOG_MSG_LINE_MAX_LEN];
            uint16_t candidate_w;
            size_t space_len;
            size_t used;
            size_t avail;
            size_t fit_len;
            size_t candidate_pos;

            while (i < raw_len && raw[i] != ' ' && raw[i] != '\n') {
                ++i;
                ++word_len;
            }

            /* 'fit_len' is provably bounded so that 'line' plus the
             * optional separating space plus 'fit_len' bytes of the
             * word always fits within 'candidate', with room left for
             * the terminating null ('used' capped at capacity first
             * avoids the subtraction underflowing if 'line' is
             * already at or past it).  Built here with explicit
             * 'memcpy' calls at that already-proven-safe length,
             * rather than 'snprintf' with a '%.*s' precision
             * argument: GCC's own '-Wformat-truncation' analysis is
             * not able to trace a bound proven this way (through
             * several local variables and a ternary) back to a
             * precision argument, and warns as if the call were
             * unbounded even though it provably is not; avoiding the
             * format string here entirely sidesteps that analysis
             * rather than silencing it. */
            space_len = (line_len > 0u) ? 1u : 0u;
            used = line_len + space_len;
            avail = (used < sizeof(candidate) - 1u)
                ? (sizeof(candidate) - 1u - used) : 0u;
            fit_len = (word_len > avail) ? avail : word_len;

            memcpy(candidate, line, line_len);
            candidate_pos = line_len;
            if (space_len > 0u) {
                candidate[candidate_pos] = ' ';
                candidate_pos += 1u;
            }
            memcpy(candidate + candidate_pos, &raw[word_start], fit_len);
            candidate_pos += fit_len;
            candidate[candidate_pos] = '\0';

            candidate_w = text_measure_string(candidate);

            if (candidate_w <= (uint16_t) DIALOG_MSG_WRAP_WIDTH ||
                    line_len == 0u) {
                (void) safe_strncpy(line, candidate, sizeof(line) - 1u);
                line[sizeof(line) - 1u] = '\0';
                line_len = safe_strlen(line);
            } else {
                /* Does not fit and the line already has something on
                 * it: rewind to re-process this same word as the
                 * start of the next line instead. */
                i = word_start;
                break;
            }

            while (i < raw_len && raw[i] == ' ') {
                ++i;
            }
        }

        (void) safe_strncpy(lines[count], line,
                DIALOG_MSG_LINE_MAX_LEN - 1u);
        lines[count][DIALOG_MSG_LINE_MAX_LEN - 1u] = '\0';
        ++count;

        if (i < raw_len && raw[i] == '\n') {
            ++i;
        }
    }

    *out_count = count;
}


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
    uint16_t msg_w = 0u;
    uint16_t ok_w;
    uint16_t msg_span_w;
    uint16_t ok_span_w;
    uint16_t pad_x;
    uint16_t pad_y;
    uint16_t btn_text_h;
    uint16_t label_pad_x;
    uint16_t extra_lines_h;

    if (connection == NULL || config == NULL || layout == NULL) {
        return;
    }

    label_pad_x = (uint16_t) config->theme.dialog.label.padding.horizontal;
    pad_x = (uint16_t) config->theme.dialog.button.padding.horizontal;
    pad_y = (uint16_t) config->theme.dialog.button.padding.vertical;

    text_renderer_init(connection, config->theme.dialog.label.font);
    s_message_wrap_text(layout->raw_message, layout->lines,
            &layout->line_count);
    layout->line_height = (int16_t) (text_font_ascent() +
            text_font_descent());
    for (uint8_t i = 0u; i < layout->line_count; ++i) {
        uint16_t w = menu_draw_measure(layout->lines[i]);

        if (w > msg_w) {
            msg_w = w;
        }
    }

    text_renderer_init(connection, config->theme.dialog.button.selected.font);
    ok_w = menu_draw_measure(DIALOG_MSG_LABEL_OK);
    btn_text_h = (uint16_t) (text_font_ascent() + text_font_descent());

    layout->btn_w = s_u16max(DIALOG_BTN_MIN_W,
            (uint16_t) (ok_w + (pad_x * 2u)));
    layout->btn_h = (uint16_t) (btn_text_h + (pad_y * 2u));

    msg_span_w = (uint16_t) (msg_w + (label_pad_x * 2u));
    ok_span_w = (uint16_t) (layout->btn_w + (label_pad_x * 2u));

    /* Every wrapped line past the first extends the dialog by one
     * more line height plus the inter-line gap; a single-line message
     * (the common case) adds nothing here, matching the previous
     * fixed layout exactly. */
    extra_lines_h = (layout->line_count > 1u)
        ? (uint16_t) ((layout->line_count - 1u) *
                ((uint16_t) layout->line_height + DIALOG_MSG_LINE_GAP))
        : 0u;

    layout->w = s_u16max(DIALOG_MIN_W,
            s_u16max(msg_span_w, ok_span_w));
    layout->h = s_u16max(DIALOG_MIN_H,
            (uint16_t) (DIALOG_PROMPT_BASELINE_Y +
                extra_lines_h +
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

    /* Message text, one call per wrapped line; each line uses the
     * same 'msg_x' (computed from the widest line) rather than being
     * individually re-centered, so the whole block reads as one
     * left-aligned paragraph rather than each line jittering
     * sideways relative to the others. */
    text_renderer_init(connection, config->theme.dialog.label.font);
    text_renderer_set_color(fg_nor, bg_win);
    for (uint8_t i = 0u; i < lo->line_count; ++i) {
        int16_t line_y = (int16_t) (lo->msg_y +
                (int16_t) i * (lo->line_height +
                    (int16_t) DIALOG_MSG_LINE_GAP));

        menu_draw_label(connection, s_message_window,
                lo->msg_x, line_y, lo->lines[i]);
    }

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
        case MENU_MSG_LEVEL_NONE:
            /* 'prefix' is already the empty string from its own
             * declaration above; nothing to do here */
            break;

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

    (void) safe_strncpy(s_message_layout.raw_message, prefix,
            sizeof(s_message_layout.raw_message) - 1u);
    s_message_layout.raw_message[
        sizeof(s_message_layout.raw_message) - 1u] = '\0';

    if (message != NULL) {
        size_t plen = safe_strlen(s_message_layout.raw_message);
        (void) safe_strncpy(s_message_layout.raw_message + plen, message,
                sizeof(s_message_layout.raw_message) - 1u - plen);
        s_message_layout.raw_message[
            sizeof(s_message_layout.raw_message) - 1u] = '\0';
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


/* Show the output of 'fortune', or an invitation to install it */
void dialog_fortune_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config)
{
    char buffer[DIALOG_FORTUNE_MAX_LEN];
    FILE *pipe;
    size_t len;
    const char *text;

    if (connection == NULL || surface == NULL || config == NULL) {
        return;
    }

    buffer[0] = '\0';

    /* Redirect stderr to /dev/null so a missing binary's shell
     * "command not found" complaint never ends up as this dialog's
     * text; an empty read is exactly what should fall through to the
     * fallback message below regardless of why it came up empty. */
    pipe = popen("fortune 2>/dev/null", "r");
    if (pipe != NULL) {
        size_t n = fread(buffer, 1u, sizeof(buffer) - 1u, pipe);

        buffer[n] = '\0';
        (void) pclose(pipe);
    }

    /* Trim the trailing newline(s) 'fortune' output typically ends
     * with, so wrapping does not leave a visibly blank final line at
     * the bottom of the dialog. */
    len = safe_strlen(buffer);
    while (len > 0u &&
            (buffer[len - 1u] == '\n' || buffer[len - 1u] == '\r' ||
             buffer[len - 1u] == ' ' || buffer[len - 1u] == '\t')) {
        buffer[--len] = '\0';
    }

    text = (len > 0u) ? buffer : DIALOG_FORTUNE_FALLBACK_MSG;

    menu_message_dialog_show(connection, surface, config,
            text, MENU_MSG_LEVEL_NONE);
}
