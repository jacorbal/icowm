/**
 * @file menu/dialog/confirm.c
 *
 * @brief Two-button confirm/cancel modal dialog
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
#include <menu/dialog/confirm.h>
#include <menu/draw.h>


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
    cancel_w = dlgutil_u16max(cancel_w,
            menu_draw_measure(layout->cancel_label));
    confirm_w = dlgutil_u16max(confirm_w,
            menu_draw_measure(layout->confirm_label));
    btn_text_h = dlgutil_u16max(btn_text_h,
            (uint16_t) (text_font_ascent() + text_font_descent()));

    btn_label_w = dlgutil_u16max(cancel_w, confirm_w);

    layout->btn_w = dlgutil_u16max(DIALOG_BTN_MIN_W,
            (uint16_t) (btn_label_w + (pad_x * 2u)));
    layout->btn_h = (uint16_t) (btn_text_h + (pad_y * 2u));

    btns_group_w = (uint16_t) ((layout->btn_w * 2u) + gap);
    btns_span_w = (uint16_t) (btns_group_w + (label_pad_x * 2u));

    prompt_span_w = (uint16_t) (prompt_w + (label_pad_x * 2u));

    layout->w = dlgutil_u16max(DIALOG_MIN_W,
            dlgutil_u16max(btns_span_w, prompt_span_w));
    layout->h = dlgutil_u16max(DIALOG_MIN_H,
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
    dlgutil_draw_button_border(connection, s_confirm_window,
            (s_confirm_selected == 0)
                ? config->theme.dialog.button.selected.border.color
                : config->theme.dialog.button.unselected.border.color,
            (s_confirm_selected == 0)
                ? config->theme.dialog.button.selected.border.width
                : config->theme.dialog.button.unselected.border.width,
            lo->cancel_x, lo->btn_y, lo->btn_w, lo->btn_h);
    dlgutil_draw_button_border(connection, s_confirm_window,
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

    menu_dialog_center(connection, surface, s_confirm_layout.w,
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
 * long.  'menu_confirm_dialog_tick', called every main-loop iteration,
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

