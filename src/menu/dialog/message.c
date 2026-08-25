/**
 * @file menu/dialog/message.c
 *
 * @brief Read-only message modal dialog with a scrollable body and a
 *        single dismiss button
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
#include <stdlib.h>     /* free, malloc */
#include <string.h>     /* memcpy */

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* Default initial values */
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
#include <menu/dialog/defer.h>
#include <menu/dialog/message.h>
#include <menu/draw.h>


/* Message dialog state and layout */

/** One already-wrapped message line, at most
 *  @c DIALOG_MSG_LINE_MAX_LENGTH bytes wide */
typedef char s_message_line_td[DIALOG_MSG_LINE_MAX_LENGTH];

/** Internal layout record for the message dialog */
typedef struct {
    char *raw_message;      /**< Prefix + caller's text, before
                                 wrapping; allocated to exactly what
                                 this message needs, see
                                 'menu_message_dialog_show' */

    s_message_line_td *lines; /**< Wrapped lines; allocated to exactly
                                   'line_count' of them, see
                                   's_message_wrap_text' */

    menu_msg_level_e level; /**< Alert level this dialog was shown at */
    struct geometry_s btn;
    uint16_t w;
    uint16_t h;
    int16_t msg_x;
    int16_t msg_y;
    int16_t line_height;    /**< Pixel height (ascent + descent) of
                                 one wrapped line in the label font */
    uint8_t line_count;
    uint8_t visible_lines;  /**< How many of 'lines' fit within 'h' at
                                 once; the rest scroll */
    uint8_t scroll_offset;  /**< Index into 'lines' of the first
                                 currently visible line */

    bool ok_selected;       /**< Whether the "OK" button is currently
                                 selected; see 'menu_message_dialog_
                                 show' for why this starts false for
                                 warning/error dialogs instead of
                                 always true */
} s_message_layout_td;


/** XCB window of the currently visible message dialog */
static xcb_window_t s_message_window = XCB_WINDOW_NONE;

/** Real X11 input focus captured right before this dialog took it,
 *  so it can be restored on close; same pattern already used by
 *  'search.c' and 'cycle.c' */
static xcb_window_t s_message_prev_focus = XCB_WINDOW_NONE;

/** Cached layout used for both creation and repaint */
static s_message_layout_td s_message_layout;


/**
 * @brief Word-wrap @p raw into a freshly allocated array of lines,
 *        each at most @c DIALOG_MSG_LINE_MAX_LENGTH bytes wide
 *
 * A general-purpose wrap usable for any message dialog text, not
 * specific to any one caller.  Explicit @c '\n' characters in @p raw
 * force a line break, so a caller that already knows its own
 * paragraph structure is respected exactly, and within each such
 * paragraph, words are packed onto a line up to the wrap width before
 * moving to the next one.  A single word wider than the wrap width on
 * its own is placed on its own line and allowed to overflow rather
 * than being split mid-word, since breaking a word arbitrarily reads
 * worse than a rare, slightly-too-wide line.  Stops after
 * @c DIALOG_MSG_MAX_LINES lines regardless of how much text remains,
 * silently dropping the rest, so a pathologically long message can
 * never grow the dialog, or this function's own allocation, without
 * bound.  A @c '\r' is treated exactly like a space (dropped as a
 * word separator, never copied into a line): callers on a platform
 * that terminates lines with @c "\r\n" would otherwise leave that
 * @c '\r' attached to the end of a word, where an X core (non-Xft)
 * bitmap font typically has a visible glyph for it instead of
 * treating it as whitespace.
 *
 * Wraps into a fixed-size scratch buffer on this function's own
 * stack first, sized to the @c DIALOG_MSG_MAX_LINES safety ceiling,
 * then allocates and returns only the @c *out_count lines that
 * actually got produced.  Reusing the same wrapping logic against a
 * stack scratch buffer, rather than wrapping twice (once to count
 * lines, once to fill an exactly-sized allocation), avoids
 * duplicating it; the stack buffer itself costs nothing once this
 * call returns, unlike a @c static one that stayed reserved for the
 * life of the process regardless of whether a dialog was even open.
 *
 * @param raw        Null-terminated text to wrap
 * @param out_count  Receives the number of lines actually produced,
 *                   always set even on failure
 *
 * @return A @c malloc'd array of @c *out_count lines, for the caller
 *         to @c free once done with it, or @c NULL if @p raw or
 *         @p out_count is @c NULL, @p raw is empty, or the allocation
 *         itself fails
 *
 * @note Complexity: @e O(n), where @e n is the length of @p raw
 */
static s_message_line_td *s_message_wrap_text(const char *raw,
        uint8_t *out_count)
{
    s_message_line_td scratch[DIALOG_MSG_MAX_LINES];
    size_t raw_len;
    size_t i = 0u;
    uint8_t count = 0u;
    s_message_line_td *out;

    if (out_count != NULL) {
        *out_count = 0u;
    }
    if (raw == NULL || out_count == NULL) {
        return NULL;
    }

    raw_len = safe_strlen(raw);

    while (i < raw_len && count < (uint8_t) DIALOG_MSG_MAX_LINES) {
        char line[DIALOG_MSG_LINE_MAX_LENGTH];
        size_t line_len = 0u;

        line[0] = '\0';

        while (i < raw_len && (raw[i] == ' ' || raw[i] == '\r')) {
            ++i;
        }

        while (i < raw_len && raw[i] != '\n') {
            size_t word_start = i;
            size_t word_len = 0u;
            char candidate[DIALOG_MSG_LINE_MAX_LENGTH];
            size_t space_len;
            size_t used;
            size_t avail;
            size_t fit_len;
            size_t candidate_pos;

            while (i < raw_len && raw[i] != ' ' && raw[i] != '\n' &&
                    raw[i] != '\r') {
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

            if (candidate_pos <= (size_t) DIALOG_MSG_WRAP_LENGTH ||
                    line_len == 0u) {
                (void) safe_strncpy(line, candidate, sizeof(line) - 1u);
                line[sizeof(line) - 1u] = '\0';
                line_len = safe_strlen(line);

                if (fit_len < word_len) {
                    /* The word itself does not fit within 'candidate'
                     * own raw buffer capacity at all (a far more
                     * extreme case than merely overflowing the
                     * visual wrap target above), so only its own
                     * first 'fit_len' bytes actually made it onto
                     * this line.  Rewound here to right after
                     * whatever was actually consumed, rather than
                     * past the word's own real end, so its own
                     * remaining bytes are not silently dropped:
                     * picked back up as the start of the very next
                     * line instead, the same as any other word that
                     * does not fit on the current one. */
                    i = word_start + fit_len;
                    break;
                }
            } else {
                /* Does not fit and the line already has something on
                 * it: rewind to re-process this same word as the
                 * start of the next line instead. */
                i = word_start;
                break;
            }

            while (i < raw_len && (raw[i] == ' ' || raw[i] == '\r')) {
                ++i;
            }
        }

        (void) safe_strncpy(scratch[count], line,
                DIALOG_MSG_LINE_MAX_LENGTH - 1u);
        scratch[count][DIALOG_MSG_LINE_MAX_LENGTH - 1u] = '\0';
        ++count;

        if (i < raw_len && raw[i] == '\n') {
            ++i;
        }
    }

    if (count == 0u) {
        return NULL;
    }

    out = malloc((size_t) count * sizeof(s_message_line_td));
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, scratch, (size_t) count * sizeof(s_message_line_td));
    *out_count = count;
    return out;
}


/**
 * @brief Compute layout geometry for the message dialog
 *
 * Uses the message text already stored in @p layout->raw_message.
 * The "OK" button always renders in @c button.selected.font (it has
 * no unselected state to switch to), so its width and label position
 * are measured directly in that font, avoiding the same off-center
 * risk @c s_confirm_compute_layout (menu/dialog/confirm.c) guards
 * against for the two-button confirm dialog.  Caps @p layout->h to
 * 70% of @p surface's resolved target monitor (see
 * @c dlgutil_resolve_monitor) and computes how many message lines fit
 * within that cap into @p layout->visible_lines, scrolling the rest
 * instead of growing past it; see @c s_message_draw for how that
 * scrolling is actually drawn.
 *
 * @param connection XCB connection, needed to measure the label text
 *                   and to resolve the target monitor
 * @param surface    Surface the dialog will show on, to resolve the
 *                   target monitor against
 * @param config     Theme providing button font and padding
 * @param layout     Layout structure containing input text and
 *                   receiving the computed dialog geometry
 *
 * @note Complexity: @e O(n), where @e n is @p layout->line_count
 */
static void s_message_compute_layout(xcb_connection_t *connection,
        const surface_td *surface,
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
    uint16_t reserved_h;
    uint16_t max_h;
    monitor_td monitor;

    if (connection == NULL || config == NULL || layout == NULL) {
        return;
    }

    label_pad_x = (uint16_t) config->theme.dialog.label.padding.horizontal;
    pad_x = (uint16_t) config->theme.dialog.button.padding.horizontal;
    pad_y = (uint16_t) config->theme.dialog.button.padding.vertical;

    (void) text_renderer_use_font(connection,
            config->theme.dialog.label.font);

    /* Freed defensively before this call's own assignment below, the
     * same as 'menu_message_dialog_show' already does for
     * 'raw_message': a no-op in the normal one-open-dialog-at-a-time
     * flow, but cheap enough to rule out a leak here too rather than
     * rely on that flow never changing. */
    free(layout->lines);
    layout->lines = s_message_wrap_text(layout->raw_message,
            &layout->line_count);
    layout->line_height = (int16_t) (text_font_ascent() +
            text_font_descent());
    for (uint8_t i = 0u; i < layout->line_count; ++i) {
        uint16_t w = menu_draw_measure(layout->lines[i]);

        if (w > msg_w) {
            msg_w = w;
        }
    }

    /* Measures both 'button.unselected.font' and 'button.selected.
     * font' and keeps the wider/taller of the two, exactly like
     * 's_confirm_compute_layout' does for its own two buttons: this
     * button can render in either state now (unselected by default
     * for warning/error levels; see 'menu_message_dialog_show'), and
     * sizing off only one font risks an off-center label once the
     * other one is actually the one drawn. */
    (void) text_renderer_use_font(connection,
            config->theme.dialog.button.unselected.font);
    ok_w = menu_draw_measure(_(STR_DIALOG_MSG_LABEL_OK));
    btn_text_h = (uint16_t) (text_font_ascent() + text_font_descent());

    (void) text_renderer_use_font(connection,
            config->theme.dialog.button.selected.font);
    ok_w = dlgutil_u16max(ok_w, menu_draw_measure(_(STR_DIALOG_MSG_LABEL_OK)));
    btn_text_h = dlgutil_u16max(btn_text_h,
            (uint16_t) (text_font_ascent() + text_font_descent()));

    layout->btn.dim.w = dlgutil_u16max(DIALOG_BTN_MIN_W,
            (uint16_t) (ok_w + (pad_x * 2u)));
    layout->btn.dim.h = (uint16_t) (btn_text_h + (pad_y * 2u));

    msg_span_w = (uint16_t) (msg_w + (label_pad_x * 2u));
    ok_span_w = (uint16_t) (layout->btn.dim.w + (label_pad_x * 2u));

    /* Every wrapped line past the first extends the dialog by one
     * more line height plus the inter-line gap; a single-line message
     * (the common case) adds nothing here, matching the previous
     * fixed layout exactly. */
    extra_lines_h = (layout->line_count > 1u)
        ? (uint16_t) ((layout->line_count - 1u) *
                ((uint16_t) layout->line_height + DIALOG_MSG_LINE_GAP))
        : 0u;

    /* Everything the message area's own height competes with: the
     * padding above it, the gap and button below it, and the bottom
     * padding.  Used both to size the unclamped 'natural' height below
     * and, if that would be too tall, to work out how much of it is
     * actually left over for message lines once the monitor cap is
     * applied. */
    reserved_h = (uint16_t) (DIALOG_PROMPT_BASELINE_Y +
            DIALOG_PROMPT_TO_BTN_GAP +
            layout->btn.dim.h +
            DIALOG_PAD_BOTTOM);

    layout->w = dlgutil_u16max(DIALOG_MIN_W,
            dlgutil_u16max(msg_span_w, ok_span_w));
    layout->h = dlgutil_u16max(DIALOG_MIN_H,
            (uint16_t) (reserved_h + extra_lines_h));

    /* Cap the dialog to a fraction of its target monitor's own
     * height, well short of covering it edge to edge, and scroll
     * whatever does not fit instead of ever growing past that; see
     * 's_message_draw' for how 'scroll_offset' and the three-row
     * footer (a blank spacer, a separator rule, and the status/
     * scroll-hint line itself) it reserves when active are used. */
    monitor = dlgutil_resolve_monitor(connection, surface);
    max_h = (uint16_t) ((monitor.h * 70u) / 100u);
    if (max_h > 0u && layout->h > max_h) {
        uint16_t avail_lines_h;
        uint16_t avail_lines;
        const uint16_t footer_rows = 3u;

        layout->h = dlgutil_u16max(DIALOG_MIN_H, max_h);
        avail_lines_h = (layout->h > reserved_h)
            ? (uint16_t) (layout->h - reserved_h) : 0u;
        avail_lines = (uint16_t) (avail_lines_h /
                ((uint16_t) layout->line_height + DIALOG_MSG_LINE_GAP));
        /* Reserve the last three visible rows for the footer (blank
         * spacer, separator rule, "more above/below" status line)
         * whenever scrolling is actually needed (i.e., whenever this
         * branch is reached at all), so it never displaces a row of
         * real content instead of sitting below it. */
        layout->visible_lines = (avail_lines > footer_rows)
            ? (uint8_t) (avail_lines - footer_rows) : 1u;
    } else {
        layout->visible_lines = layout->line_count;
    }
    layout->scroll_offset = 0u;

    layout->btn.pos.x = (int16_t)
        ((layout->w - (int32_t) layout->btn.dim.w) / 2);
    layout->btn.pos.y = (int16_t) ((int16_t) layout->h -
            (int16_t) DIALOG_PAD_BOTTOM -
            (int16_t) layout->btn.dim.h);

    layout->msg_x = (int16_t) ((layout->w > msg_w)
            ? (layout->w - msg_w) / 2u : label_pad_x);
    if (layout->msg_x < (int16_t) label_pad_x) {
        layout->msg_x = (int16_t) label_pad_x;
    }
    layout->msg_y = (int16_t) DIALOG_PROMPT_BASELINE_Y;
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
    uint32_t fg_btn_nor;
    uint32_t bg_btn_nor;
    uint32_t fg_nor;
    uint16_t label_w;
    int16_t label_x;
    int16_t label_y;
    const s_message_layout_td *lo = &s_message_layout;
    uint8_t shown;

    if (connection == NULL || config == NULL ||
            s_message_window == XCB_WINDOW_NONE) {
        return;
    }

    bg_win = config->theme.dialog.background;
    fg_sel = config->theme.dialog.button.selected.color.foreground;
    bg_sel = config->theme.dialog.button.selected.color.background;
    fg_btn_nor = config->theme.dialog.button.unselected.color.foreground;
    bg_btn_nor = config->theme.dialog.button.unselected.color.background;
    fg_nor = config->theme.dialog.label.foreground;

    /* See the matching comment in 's_confirm_draw'
     * (menu/dialog/confirm.c): font has to be re-asserted right
     * before each piece of text, not just once when the dialog first
     * opens. */

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

    /* OK button: highlighted only once 'lo->ok_selected' is true (see
     * 'menu_message_dialog_show' for when that starts false instead
     * of the previous, always-true behavior), the same selected/
     * unselected distinction 's_confirm_draw' already draws between
     * its own two buttons. */
    gc_vals[0] = (lo->ok_selected) ? bg_sel : bg_btn_nor;
    xcb_change_gc(connection, gc, XCB_GC_FOREGROUND, gc_vals);
    rect.x = (int16_t) lo->btn.pos.x;
    rect.y = (int16_t) lo->btn.pos.y;
    rect.width = (uint16_t) lo->btn.dim.w;
    rect.height = (uint16_t) lo->btn.dim.h;
    xcb_poly_fill_rectangle(connection, s_message_window, gc, 1, &rect);
    xcb_free_gc(connection, gc);

    dlgutil_button_border_draw(connection, s_message_window,
            (lo->ok_selected)
                ? config->theme.dialog.button.selected.border.color
                : config->theme.dialog.button.unselected.border.color,
            (lo->ok_selected)
                ? config->theme.dialog.button.selected.border.width
                : config->theme.dialog.button.unselected.border.width,
            lo->btn);

    /* Message text, one call per wrapped line; each line uses the
     * same 'msg_x' (computed from the widest line) rather than being
     * individually re-centered, so the whole block reads as one
     * left-aligned paragraph rather than each line jittering
     * sideways relative to the others.  Only 'visible_lines' worth of
     * 'lines', starting at 'scroll_offset', are ever drawn: the rest
     * exist off-screen in the buffer and are reached by scrolling. */
    (void) text_renderer_use_font(connection,
            config->theme.dialog.label.font);
    text_renderer_set_color(fg_nor, bg_win);
    shown = (uint8_t) (lo->line_count - lo->scroll_offset);

    if (shown > lo->visible_lines) {
        shown = lo->visible_lines;
    }
    for (uint8_t i = 0u; i < shown; ++i) {
        int16_t line_y = (int16_t) (lo->msg_y +
                (int16_t) i * (lo->line_height +
                    (int16_t) DIALOG_MSG_LINE_GAP));

        menu_draw_label(connection, s_message_window,
                (struct position_s) { lo->msg_x, line_y },
                lo->lines[lo->scroll_offset + i]);
    }

    /* Footer, right below the last content row shown above: only
     * drawn when there is more of the message than fits at once,
     * i.e., exactly when 'visible_lines' was computed with room
     * for it reserved in the first place (see
     * 's_message_compute_layout').  Three rows: a blank spacer so
     * the footer reads as clearly separate from the message
     * above it, a drawn horizontal rule for the same reason
     * (matching how 'ctxmenu/redraw.c' draws a context-menu
     * separator,
     * not a row of dashed text), and the "more above/below"
     * status/scroll-hint line itself. */
    if (lo->line_count > lo->visible_lines) {
        char status[DIALOG_MSG_LINE_MAX_LENGTH];
        int16_t row_step = (int16_t) (lo->line_height +
                (int16_t) DIALOG_MSG_LINE_GAP);
        int16_t separator_row_y = (int16_t) (lo->msg_y +
                (int16_t) (shown + 1u) * row_step);
        int16_t status_y = (int16_t) (separator_row_y + row_step);
        xcb_gcontext_t sep_gc;
        uint32_t sep_gc_vals[1];
        xcb_rectangle_t sep_rect;

        sep_gc = xcb_generate_id(connection);
        sep_gc_vals[0] = config->theme.dialog.border.color;
        xcb_create_gc(connection, sep_gc, s_message_window,
                XCB_GC_FOREGROUND, sep_gc_vals);
        sep_rect.x = lo->msg_x;
        sep_rect.y = (int16_t) (separator_row_y -
                (lo->line_height / 2));
        sep_rect.width = (lo->w > (uint16_t) (lo->msg_x * 2))
            ? (uint16_t) (lo->w - (uint16_t) (lo->msg_x * 2))
            : 0u;
        sep_rect.height = 1u;
        xcb_poly_fill_rectangle(connection, s_message_window,
                sep_gc, 1, &sep_rect);
        xcb_free_gc(connection, sep_gc);

        (void) snprintf(status, sizeof(status),
                "%u-%u/%u: Up/Down, PgUp/PgDn, wheel",
                (unsigned int) lo->scroll_offset + 1u,
                (unsigned int) lo->scroll_offset + shown,
                (unsigned int) lo->line_count);
        menu_draw_label(connection, s_message_window,
                (struct position_s) { lo->msg_x, status_y }, status);
    }

    /* OK label: font, and therefore width, depends on whether the
     * button is currently selected, so both are recomputed fresh on
     * every repaint, as 's_message_compute_layout''s own comment
     * describes, rather than using a fixed position; the vertical
     * centering the same way, using the active font's own ascent/
     * descent against 'btn.dim.h' so it stays centered regardless of
     * which font is taller.  Same reasoning as the cancel/confirm
     * labels in 's_confirm_draw' (menu/dialog/confirm.c). */
    (void) text_renderer_use_font(connection, (lo->ok_selected)
            ? config->theme.dialog.button.selected.font
            : config->theme.dialog.button.unselected.font);
    label_w = menu_draw_measure(_(STR_DIALOG_MSG_LABEL_OK));
    label_x = (int16_t) (lo->btn.pos.x +
            (int16_t) (((int32_t) lo->btn.dim.w - label_w) / 2));
    label_y = (int16_t) (lo->btn.pos.y +
            (int16_t) (((int32_t) lo->btn.dim.h -
                    (uint16_t) (text_font_ascent() +
                        text_font_descent())) / 2) +
            text_font_ascent());
    text_renderer_set_color(
            (lo->ok_selected) ? fg_sel : fg_btn_nor,
            (lo->ok_selected) ? bg_sel : bg_btn_nor);
    menu_draw_label(connection, s_message_window,
            (struct position_s) { label_x, label_y },
            _(STR_DIALOG_MSG_LABEL_OK));

    xcb_flush(connection);
}


/* Open the message dialog */
void menu_message_dialog_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const char *message, menu_msg_level_e level)
{
    xcb_get_input_focus_cookie_t foc_cookie;
    xcb_get_input_focus_reply_t *foc_reply;
    int16_t x;
    int16_t y;
    uint32_t mask;
    uint32_t values[4];
    const char *prefix = "\0";
    size_t prefix_len;
    size_t message_len;
    size_t needed;

    if (connection == NULL || surface == NULL || config == NULL ||
            surface->screen == NULL) {
        return;
    }

    if (s_message_window != XCB_WINDOW_NONE) {
        return;
    }

    foc_cookie = xcb_get_input_focus(connection);
    foc_reply = xcb_get_input_focus_reply(connection, foc_cookie, NULL);
    s_message_prev_focus = (foc_reply != NULL &&
            foc_reply->focus != XCB_WINDOW_NONE &&
            foc_reply->focus != XCB_INPUT_FOCUS_POINTER_ROOT &&
            foc_reply->focus != XCB_INPUT_FOCUS_NONE)
        ? foc_reply->focus : XCB_WINDOW_NONE;
    if (foc_reply != NULL) {
        free(foc_reply);
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

    s_message_layout.level = level;

    /* Warning and error dialogs require the "OK" button to be
     * explicitly selected (click it directly, or Tab to it then
     * Enter/Space; see the keyboard handling in input/kbd/event.c)
     * before it can be activated, and Escape does not dismiss them at
     * all: both exist so that a message serious enough to warrant
     * one of these two levels cannot be dismissed by reflex, the way
     * repeatedly hitting Escape or Enter/Space to close whatever
     * dialog currently has focus easily could otherwise.  Every other
     * level keeps the previous, quicker-to-dismiss behavior: the
     * button starts selected, and Escape works normally. */
    s_message_layout.ok_selected =
        (level != MENU_MSG_LEVEL_WARNING &&
         level != MENU_MSG_LEVEL_ERROR);

    /* Freed defensively here even though 'menu_message_dialog_close'
     * already frees and nulls it, and the early return above already
     * refuses a second 'show' while one dialog is still open: 'free'
     * on a null pointer is a valid no-op, so this costs nothing when
     * everything else already behaved, while still ruling out a leak
     * if that ever stops being true. */
    free(s_message_layout.raw_message);
    s_message_layout.raw_message = NULL;

    /* Allocated to exactly what this message needs, capped at
     * 'DIALOG_MSG_RAW_MAX_LENGTH' as a safety ceiling against a
     * pathologically long caller-supplied 'message' rather than as
     * this allocation's default size. */
    prefix_len = safe_strlen(prefix);
    message_len = (message != NULL) ? safe_strlen(message) : 0u;
    needed = prefix_len + message_len + 1u;
    if (needed > (size_t) DIALOG_MSG_RAW_MAX_LENGTH) {
        needed = (size_t) DIALOG_MSG_RAW_MAX_LENGTH;
    }

    s_message_layout.raw_message = malloc(needed);
    if (s_message_layout.raw_message != NULL) {
        (void) safe_strncpy(s_message_layout.raw_message, prefix,
                needed - 1u);
        s_message_layout.raw_message[needed - 1u] = '\0';

        if (message != NULL) {
            size_t plen = safe_strlen(s_message_layout.raw_message);

            (void) safe_strncpy(s_message_layout.raw_message + plen,
                    message, needed - plen);
            s_message_layout.raw_message[needed - 1u] = '\0';
        }
    }

    s_message_compute_layout(connection, surface, config,
            &s_message_layout);

    menu_dialog_center(connection, surface, s_message_layout.w,
            s_message_layout.h, &x, &y);

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
    atom_set_window_opacity(connection, s_message_window,
            config_theme_opacity_to_raw(config->theme.dialog.opacity));

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

    /* Restore whichever real X11 focus this dialog displaced when it
     * opened; without this, focus reverts to 'PointerRoot' instead
     * (per the revert_to mode 'menu_message_dialog_show' set it up
     * with), which may land on a different client than the one the
     * window manager's own bookkeeping still shows as active, or on
     * nothing at all. */
    if (s_message_prev_focus != XCB_WINDOW_NONE) {
        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_PARENT,
                s_message_prev_focus, XCB_CURRENT_TIME);
    }
    s_message_prev_focus = XCB_WINDOW_NONE;

    /* Also cancels any click-triggered close still scheduled (see
     * 'menu_dialog_defer_schedule' in
     * 'menu_message_dialog_handle_click'), so
     * 'menu_dialog_defer_tick' has nothing left to do once
     * this dialog is gone through some other path (e.g., Escape)
     * before that delay elapsed on its own. */
    menu_dialog_defer_cancel();

    /* Given back immediately on close, rather than held until the
     * next 'menu_message_dialog_show' reuses or replaces it: nothing
     * stays reserved for this dialog's own text while no dialog is
     * even open. */
    free(s_message_layout.raw_message);
    s_message_layout.raw_message = NULL;
    free(s_message_layout.lines);
    s_message_layout.lines = NULL;
}


/* Repaint the message dialog */
void menu_message_dialog_repaint(xcb_connection_t *connection,
        const config_td *config)
{
    if (connection == NULL || config == NULL ||
            s_message_window == XCB_WINDOW_NONE) {
        return;
    }
    s_message_draw(connection, config);
}


/* Handle a click in the message dialog */
void menu_message_dialog_handle_click(xcb_connection_t *connection,
        const config_td *config, int x, int y)
{
    const s_message_layout_td *lo = &s_message_layout;

    if (connection == NULL || s_message_window == XCB_WINDOW_NONE) {
        return;
    }

    if (y >= (int) lo->btn.pos.y &&
            y < (int) lo->btn.pos.y + (int) lo->btn.dim.h &&
            x >= (int) lo->btn.pos.x &&
            x < (int) lo->btn.pos.x + (int) lo->btn.dim.w) {
        /* Selected and repainted first, the same as
         * 'menu_confirm_dialog_handle_click' already does for its own
         * two buttons, so a person actually sees the click land on
         * the "OK" button before the deferred close below makes the
         * dialog go away. */
        s_message_layout.ok_selected = true;
        if (config != NULL) {
            s_message_draw(connection, config);
        }
        menu_dialog_defer_schedule(connection,
                DIALOG_CLICK_FEEDBACK_DELAY_MS,
                menu_message_dialog_close);
    }
}


/* Milliseconds until a pending click-triggered close becomes due */
int menu_message_dialog_ms_remaining(void)
{
    return menu_dialog_defer_ms_remaining();
}


/* Close the dialog if a click-triggered close is due */
void menu_message_dialog_tick(xcb_connection_t *connection)
{
    menu_dialog_defer_tick(connection);
}


/* Query whether the currently visible message dialog requires the
 * "OK" button to be explicitly selected first */
bool menu_message_dialog_requires_selection(void)
{
    return s_message_window != XCB_WINDOW_NONE &&
        (s_message_layout.level == MENU_MSG_LEVEL_WARNING ||
         s_message_layout.level == MENU_MSG_LEVEL_ERROR);
}


/* Query whether the "OK" button is currently selected */
bool menu_message_dialog_ok_selected(void)
{
    return s_message_layout.ok_selected;
}


/* Select the "OK" button and repaint */
void menu_message_dialog_select_ok(xcb_connection_t *connection,
        const config_td *config)
{
    if (s_message_window == XCB_WINDOW_NONE ||
            s_message_layout.ok_selected) {
        return;
    }
    s_message_layout.ok_selected = true;
    s_message_draw(connection, config);
}


/* Scroll the message dialog's text by the given number of lines */
void menu_message_dialog_scroll(xcb_connection_t *connection,
        const config_td *config, int32_t delta)
{
    s_message_layout_td *const lo = &s_message_layout;
    int32_t max_offset;
    int32_t new_offset;

    if (connection == NULL || config == NULL ||
            s_message_window == XCB_WINDOW_NONE ||
            lo->line_count <= lo->visible_lines) {
        return;
    }

    max_offset = (int32_t) lo->line_count - (int32_t) lo->visible_lines;
    new_offset = (int32_t) lo->scroll_offset + delta;
    if (new_offset < 0) {
        new_offset = 0;
    } else if (new_offset > max_offset) {
        new_offset = max_offset;
    }

    if ((uint8_t) new_offset == lo->scroll_offset) {
        return;
    }
    lo->scroll_offset = (uint8_t) new_offset;

    s_message_draw(connection, config);
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
