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
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* free */
#include <time.h>       /* clock_gettime, struct timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/dialog.h>
#include <defs/uistr.h>
#include <i18n.h>

/* Util includes */
#include <utils/safe/safestr.h>
#include <utils/xcb/atom.h>

/* Project includes */
#include <config.h>
#include <render/text.h>
#include <surface.h>

/* Local includes */
#include <menu/dialog.h>
#include <menu/dialog/confirm.h>
#include <menu/dialog/defer.h>
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
    int16_t timeout_y;
    char prompt[DIALOG_TEXT_MAX_LEN];
    char cancel_label[DIALOG_TEXT_MAX_LEN];
    char confirm_label[DIALOG_TEXT_MAX_LEN];
} s_confirm_layout_td;


/** XCB window of the currently visible confirm dialog */
static xcb_window_t s_confirm_window = XCB_WINDOW_NONE;

/** Real X11 input focus captured right before this dialog took it,
 *  so it can be restored on close; same pattern already used by
 *  'search.c' and 'cycle.c' */
static xcb_window_t s_confirm_prev_focus = XCB_WINDOW_NONE;

/** Currently highlighted button: 0 = cancel (default), 1 = confirm */
static int s_confirm_selected = 0;

/** Callback invoked when the confirm button is activated */
static void (*s_confirm_callback)(xcb_connection_t *) = NULL;

/** Callback invoked when the cancel button is activated, by a
 *  person, by Escape, or by 's_confirm_timeout_active' elapsing */
static void (*s_confirm_cancel_callback)(xcb_connection_t *) = NULL;

/** Cached layout used for both creation and repaint */
static s_confirm_layout_td s_confirm_layout;

/** Whether a countdown timeout is currently running (see
 *  'timeout_seconds' on 'menu_confirm_dialog_show') */
static bool s_confirm_timeout_active = false;

/** Absolute time the running countdown fully elapses, valid only
 *  while 's_confirm_timeout_active' is true */
static struct timespec s_confirm_timeout_due;

/** Whole seconds remaining last shown in the countdown line, so
 *  's_confirm_tick_timeout' only repaints when that number actually
 *  changes rather than on every main-loop iteration; -1 before the
 *  first paint so that one always happens */
static int s_confirm_timeout_last_shown = -1;

/** Vertical gap, in pixels, between the prompt and the countdown
 *  line beneath it, when a timeout is running */
#define DIALOG_CONFIRM_TIMEOUT_LINE_GAP (6)


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
    uint16_t timeout_line_h;
    int16_t timeout_ascent;

    if (connection == NULL || config == NULL || layout == NULL) {
        return;
    }

    pad_x = (uint16_t) config->theme.dialog.button.padding.horizontal;
    pad_y = (uint16_t) config->theme.dialog.button.padding.vertical;
    gap = (uint16_t) config->theme.dialog.button.gap;
    label_pad_x = (uint16_t) config->theme.dialog.label.padding.horizontal;

    text_renderer_init(connection, config->theme.dialog.label.font);
    prompt_w = menu_draw_measure(layout->prompt);

    /* Room for the countdown line, in the same font as the prompt
     * (measured here, while it is still the active font, rather than
     * re-selecting it later): only reserved while a timeout is
     * actually running, so a plain confirm dialog with none (every
     * existing caller, e.g., 'dialog_quit_show') is laid out exactly
     * as before this existed. */
    timeout_ascent = text_font_ascent();
    timeout_line_h = s_confirm_timeout_active
        ? (uint16_t) (timeout_ascent + text_font_descent() +
                DIALOG_CONFIRM_TIMEOUT_LINE_GAP)
        : 0u;

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
                timeout_line_h +
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
    layout->timeout_y = (int16_t) (DIALOG_PROMPT_BASELINE_Y +
            DIALOG_CONFIRM_TIMEOUT_LINE_GAP + (uint16_t) timeout_ascent);

    /* Each button label's own X/Y depends on which font actually ends
     * up drawing it (unselected or selected), which can change every
     * repaint as the user tabs between buttons; see 's_confirm_draw',
     * which computes both freshly right before drawing instead of
     * relying on a value fixed here. */
}


/**
 * @brief Milliseconds remaining until the running countdown timeout
 *        fully elapses; forward-declared here so 's_confirm_draw'
 *        (defined ahead of it, closer to the layout it renders) can
 *        show the live countdown number
 *
 * @return Milliseconds remaining (never negative), or -1 if no
 *         timeout is currently running
 *
 * @note Complexity: @e O(1)
 */
static int s_confirm_timeout_ms_remaining(void);


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
    /* Sized well beyond 'DIALOG_TEXT_MAX_LEN' rather than exactly
     * that: 'cancel_label' (itself up to that size) is only one part
     * of what this formats (see 'STR_DIALOG_CONFIRM_TIMEOUT_FMT'),
     * plus the fixed wording around it and the seconds count, so
     * matching that size exactly leaves GCC's own static bound
     * analysis unable to rule out '-Wformat-truncation' -- this
     * headroom, together with that format string's own explicit
     * '%.255s' precision (capping the part GCC cannot otherwise
     * prove is bounded to the cancel label's own declared array
     * size, rather than the rest of the struct after it), is what
     * lets it actually prove 'snprintf' below can never truncate,
     * not just widen the margin informally. */
    char timeout_text[DIALOG_TEXT_MAX_LEN + 96u];
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

    /* Countdown line, only while a timeout is actually running; same
     * font as the prompt, sharing its own horizontal centering
     * (recomputed here since the text itself changes every second,
     * unlike the prompt's fixed 'prompt_x'). */
    if (s_confirm_timeout_active) {
        int timeout_ms = s_confirm_timeout_ms_remaining();
        int seconds = (timeout_ms >= 0) ? (timeout_ms + 999) / 1000 : 0;
        int16_t timeout_x;
        uint16_t timeout_w;

        (void) snprintf(timeout_text, sizeof(timeout_text),
                _(STR_DIALOG_CONFIRM_TIMEOUT_FMT), lo->cancel_label,
                seconds);
        timeout_w = menu_draw_measure(timeout_text);
        timeout_x = (int16_t) ((lo->w > timeout_w)
                ? (lo->w - timeout_w) / 2u
                : config->theme.dialog.label.padding.horizontal);
        menu_draw_label(connection, s_confirm_window,
                timeout_x, lo->timeout_y, timeout_text);
    }

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
        void (*on_confirm)(xcb_connection_t *),
        void (*on_cancel)(xcb_connection_t *),
        uint32_t timeout_seconds)
{
    int16_t x;
    int16_t y;
    uint32_t mask;
    uint32_t values[4];
    xcb_get_input_focus_cookie_t foc_cookie;
    xcb_get_input_focus_reply_t *foc_reply;

    if (connection == NULL || surface == NULL || config == NULL ||
            surface->screen == NULL) {
        return;
    }

    if (s_confirm_window != XCB_WINDOW_NONE) {
        return;
    }

    foc_cookie = xcb_get_input_focus(connection);
    foc_reply = xcb_get_input_focus_reply(connection, foc_cookie, NULL);
    s_confirm_prev_focus = (foc_reply != NULL &&
            foc_reply->focus != XCB_WINDOW_NONE &&
            foc_reply->focus != XCB_INPUT_FOCUS_POINTER_ROOT &&
            foc_reply->focus != XCB_INPUT_FOCUS_NONE)
        ? foc_reply->focus : XCB_WINDOW_NONE;
    if (foc_reply != NULL) {
        free(foc_reply);
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

    s_confirm_selected = 0;
    s_confirm_callback = on_confirm;
    s_confirm_cancel_callback = on_cancel;
    s_confirm_timeout_last_shown = -1;

    s_confirm_timeout_active = timeout_seconds > 0u;
    if (s_confirm_timeout_active) {
        if (clock_gettime(CLOCK_MONOTONIC, &s_confirm_timeout_due) != 0) {
            /* Could not read the clock to schedule the countdown at
             * all: safer to run with no timeout (the dialog just
             * waits indefinitely, same as before this feature
             * existed) than to silently skip showing one while a
             * caller believes it is protected by an automatic
             * revert. */
            s_confirm_timeout_active = false;
        } else {
            s_confirm_timeout_due.tv_sec += (time_t) timeout_seconds;
        }
    }

    s_confirm_compute_layout(connection, config, &s_confirm_layout);

    menu_dialog_center(connection, surface, s_confirm_layout.w,
            s_confirm_layout.h, &x, &y);

    /* XCB requires attribute values to be listed in ascending bit order
     * of their mask.
     *
     * BACK_PIXEL(2) < BORDER_PIXEL(8) < OVERRIDE_REDIRECT(512) <
     * EVENT_MASK(2048)
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
    atom_set_window_opacity(connection, s_confirm_window,
            config_theme_opacity_to_raw(config->theme.dialog.opacity));

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
    s_confirm_cancel_callback = NULL;

    /* Restore whichever real X11 focus this dialog displaced when it
     * opened; without this, focus reverts to 'PointerRoot' instead
     * (per the revert_to mode 'menu_confirm_dialog_show' set it up
     * with), which may land on a different client than the one the
     * window manager's own bookkeeping still shows as active, or on
     * nothing at all. */
    if (s_confirm_prev_focus != XCB_WINDOW_NONE) {
        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_PARENT,
                s_confirm_prev_focus, XCB_CURRENT_TIME);
    }
    s_confirm_prev_focus = XCB_WINDOW_NONE;

    /* Also cancels any click-triggered close/accept still scheduled
     * (see 'menu_dialog_defer_schedule' in menu_confirm_dialog_
     * handle_click), so 'menu_dialog_defer_tick' has nothing left to
     * do once this dialog is gone through some other path (e.g.,
     * Escape) before that delay elapsed on its own. */
    menu_dialog_defer_cancel();
    s_confirm_timeout_active = false;
    s_confirm_timeout_last_shown = -1;
}


/* Repaint the confirm dialog */
void menu_confirm_dialog_repaint(xcb_connection_t *connection,
        const config_td *config)
{
    s_confirm_draw(connection, config);
}


/**
 * @brief Milliseconds remaining until an absolute deadline, floored
 *        at zero rather than going negative once past it
 *
 * The request/reply-free clock computation
 * 's_confirm_timeout_ms_remaining' needs against the running
 * countdown's own absolute deadline (see 'timeout_seconds' on
 * 'menu_confirm_dialog_show'); the equivalent computation for the
 * click-triggered close/accept lives in 'menu/dialog/defer.c' instead,
 * shared with every other dialog that defers one the same way.
 *
 * @param due Absolute deadline (@c CLOCK_MONOTONIC) to measure against
 *
 * @return Milliseconds remaining (never negative), or @c 0 if the
 *         clock itself could not be read
 *
 * @note Complexity: @e O(1)
 */
static int s_confirm_ms_until(const struct timespec *due)
{
    struct timespec now;
    long remaining_ms;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }

    remaining_ms =
        (long) (due->tv_sec - now.tv_sec) * 1000L +
        (due->tv_nsec - now.tv_nsec) / 1000000L;

    return (remaining_ms < 0) ? 0 : (int) remaining_ms;
}


/**
 * @brief Repaint the confirm dialog with its newly clicked selection,
 *        then defer the actual close/accept for shortly after
 *
 * A mouse click on the button that was not already selected changes
 * 's_confirm_selected' and needs that to actually be visible before
 * the dialog goes away, or the click reads as though it did not
 * register at the right spot even though it did.  The deferral itself
 * is 'menu_dialog_defer_schedule' (menu/dialog/defer.h), shared with
 * every other dialog that closes itself in response to a button
 * click; this just repaints first so what it defers has something new
 * to show.
 *
 * @param connection XCB connection
 * @param config     Active configuration, for the repaint; the
 *                    deferred action is scheduled even when this is
 *                    @c NULL, just without a repaint first
 *
 * @note Complexity: @e O(1)
 */
static void s_confirm_defer_click(xcb_connection_t *connection,
        const config_td *config)
{
    if (config != NULL) {
        s_confirm_draw(connection, config);
        xcb_flush(connection);
    }

    menu_dialog_defer_schedule(connection, DIALOG_CLICK_FEEDBACK_DELAY_MS,
            menu_confirm_dialog_accept);
}


/**
 * @brief Milliseconds remaining until the running countdown timeout
 *        fully elapses
 *
 * @return Milliseconds remaining (never negative), or -1 if no
 *         timeout is currently running
 *
 * @note Complexity: @e O(1)
 */
static int s_confirm_timeout_ms_remaining(void)
{
    if (!s_confirm_timeout_active) {
        return -1;
    }
    return s_confirm_ms_until(&s_confirm_timeout_due);
}


/* Milliseconds until the next thing this dialog needs to wake up for */
int menu_confirm_dialog_ms_remaining(void)
{
    int click_ms = menu_dialog_defer_ms_remaining();
    int timeout_ms = s_confirm_timeout_ms_remaining();
    int wake_ms;
    int shown_seconds;

    if (timeout_ms >= 0) {
        /* Wake at the countdown's own final expiry, or sooner still
         * at whenever the whole seconds shown next decreases by one
         * (so the visible number counts down instead of only
         * changing once, from its starting value straight to
         * vanishing), whichever comes first. */
        shown_seconds = (timeout_ms + 999) / 1000;
        wake_ms = timeout_ms - ((shown_seconds - 1) * 1000);
        if (wake_ms < 0) {
            wake_ms = 0;
        }
        timeout_ms = (wake_ms < timeout_ms) ? wake_ms : timeout_ms;
    }

    if (click_ms < 0) {
        return timeout_ms;
    }
    if (timeout_ms < 0) {
        return click_ms;
    }
    return (click_ms < timeout_ms) ? click_ms : timeout_ms;
}


/* Service whichever timer is due */
void menu_confirm_dialog_tick(xcb_connection_t *connection,
        const config_td *config)
{
    int timeout_ms;
    int shown_seconds;

    menu_dialog_defer_tick(connection);
    if (s_confirm_window == XCB_WINDOW_NONE) {
        /* A click-triggered close/accept just closed this dialog;
         * nothing left below to service. */
        return;
    }

    timeout_ms = s_confirm_timeout_ms_remaining();
    if (timeout_ms < 0) {
        return;
    }

    if (timeout_ms == 0) {
        menu_confirm_dialog_cancel(connection);
        return;
    }

    shown_seconds = (timeout_ms + 999) / 1000;
    if (shown_seconds != s_confirm_timeout_last_shown) {
        s_confirm_timeout_last_shown = shown_seconds;
        if (config != NULL) {
            s_confirm_draw(connection, config);
            xcb_flush(connection);
        }
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
            s_confirm_defer_click(connection, config);
            return true;
        }
        if (x >= (int) lo->confirm_x &&
                x < (int) lo->confirm_x + (int) lo->btn_w) {
            s_confirm_selected = 1;
            s_confirm_defer_click(connection, config);
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
void menu_confirm_dialog_accept(xcb_connection_t *connection)
{
    int selected = s_confirm_selected;
    void (*confirm_cb)(xcb_connection_t *) = s_confirm_callback;
    void (*cancel_cb)(xcb_connection_t *) = s_confirm_cancel_callback;

    menu_confirm_dialog_close(connection);
    if (selected == 1) {
        if (confirm_cb != NULL) {
            confirm_cb(connection);
        }
    } else if (cancel_cb != NULL) {
        cancel_cb(connection);
    }
}


/* Cancel the dialog regardless of which button is selected */
void menu_confirm_dialog_cancel(xcb_connection_t *connection)
{
    void (*cancel_cb)(xcb_connection_t *) = s_confirm_cancel_callback;

    menu_confirm_dialog_close(connection);
    if (cancel_cb != NULL) {
        cancel_cb(connection);
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

