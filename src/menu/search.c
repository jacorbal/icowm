/**
 * @file menu/search.c
 *
 * @brief Fuzzy window-search widget implementation
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

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/icon.h>
#include <defs/search.h>
#include <defs/uistr.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <i18n.h>
#include <lookup.h>
#include <render/text.h>
#include <render/wmicon.h>
#include <surface.h>

/* Policy includes */
#include <policy/focus.h>

/* Menu includes */
#include <menu/dialog/info.h>
#include <menu/draw.h>

/* Local includes */
#include <menu/search.h>


/**
 * @brief Pixel Y where the first result row starts: the top padding,
 *        the query bar, and a second padding strip reserved for the
 *        up-scroll indicator, the same reasoning @c WM_CYCLE_MENU_
 *        PAD_Y reserves around @c cycledraw.c's own menu
 */
#define S_SEARCH_ROWS_TOP \
    (WM_SEARCH_PAD_Y + WM_SEARCH_BAR_HEIGHT + WM_SEARCH_PAD_Y)


/**
 * @brief One matched entry ready to be drawn: its client, the
 *        desktop it lives on, and its pre-rendered name/hints text
 */
typedef struct {
    client_td *client;
    desktop_td *desktop;
    char name[WM_SEARCH_ENTRY_LENGTH];
    char hints[16];
    int score;
} s_search_result_td;


/** Private widget state singleton */
static struct {
    xcb_window_t window;
    surface_td *surface;
    char query[WM_SEARCH_QUERY_MAX_LENGTH];
    size_t query_len;

    /** Every focusable candidate collected at open time, alongside
     *  the desktop it lives on, before any query has filtered it */
    client_td *candidates[WM_SEARCH_MAX_ENTRIES];
    desktop_td *candidate_desktops[WM_SEARCH_MAX_ENTRIES];
    int candidate_count;

    s_search_result_td results[WM_SEARCH_MAX_ENTRIES];
    int result_count;
    int selected;
    int scroll_offset;
    int viewport_rows;

    uint16_t height;
    xcb_window_t prev_focus;
    const config_td *config;
} s_search;


/**
 * @brief Score how well a typed query matches a candidate string as
 *        a fuzzy subsequence
 *
 * Every character of @p query must appear in @p text in the same
 * order, case-insensitively, but not necessarily contiguous, the
 * same style of match tools like @e fzf use.  Consecutive matched
 * characters and a match starting right at the beginning of @p text
 * both score higher, so tighter and earlier matches sort first.
 *
 * @param query Typed query, already known non-empty
 * @param text  Candidate string to test
 *
 * @return A non-negative score when every character of @p query was
 *         found in order, or -1 when @p text does not match at all
 *
 * @note Complexity: @e O(n), where @e n is the length of @p text
 */
static int s_search_fuzzy_score(const char *restrict query,
        const char *restrict text)
{
    size_t qi = 0;
    size_t ti = 0;
    size_t qlen = safe_strlen(query);
    size_t tlen = safe_strlen(text);
    int score = 0;
    bool prev_matched = false;

    if (qlen == 0 || tlen == 0) {
        return -1;
    }

    for (ti = 0; ti < tlen && qi < qlen; ++ti) {
        if (tolower((unsigned char) text[ti]) ==
                tolower((unsigned char) query[qi])) {
            score += (ti == 0) ? 3 : 1;
            if (prev_matched) {
                score += 2;
            }
            prev_matched = true;
            ++qi;
        } else {
            prev_matched = false;
        }
    }

    if (qi < qlen) {
        return -1;
    }

    /* A shorter candidate that still matches is a tighter match than
     * a longer one scoring the same on the criteria above */
    if (tlen < 1000u) {
        score += (int) (1000u - tlen) / 100;
    }

    return score;
}


/**
 * @brief Build the bracketed state-hint text for one client
 *
 * Combines the client's mutually exclusive geometry state (fullscreen
 * or one of the maximized variants) with its independent shaded,
 * sticky, and urgent flags into a single comma-separated list, e.g.,
 * @c "[p,m,!]".  Writes nothing (an empty string) when no hint
 * applies.
 *
 * @param client Client to inspect
 * @param out    Destination buffer
 * @param size   Size of @p out
 *
 * @note Complexity: @e O(1)
 */
static void s_search_build_hints(const client_td *client, char *out,
        size_t size)
{
    char letters[8];
    size_t n = 0;
    size_t pos;

    if (client_is_fullscreen(client)) {
        letters[n++] = WM_ICON_HINT_FULLSCREEN;
    } else if (client->properties.state ==
            (uint16_t) CLIENT_STATE_MAXIMIZED) {
        letters[n++] = WM_ICON_HINT_MAXIMIZED;
    } else if (client->properties.state ==
            (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ) {
        letters[n++] = WM_ICON_HINT_MAXIMIZED_HORZ;
    } else if (client->properties.state ==
            (uint16_t) CLIENT_STATE_MAXIMIZED_VERT) {
        letters[n++] = WM_ICON_HINT_MAXIMIZED_VERT;
    } else if (client_is_iconified(client)) {
        letters[n++] = WM_ICON_HINT_ICONIFIED;
    }

    if (client_is_shaded(client) && n < sizeof(letters)) {
        letters[n++] = WM_ICON_HINT_SHADED;
    }
    if (client_is_hidden(client) && !client_is_iconified(client) &&
            n < sizeof(letters)) {
        letters[n++] = WM_ICON_HINT_HIDDEN;
    }
    if (client_is_pinned(client) && n < sizeof(letters)) {
        letters[n++] = WM_ICON_HINT_PINNED;
    }
    if (client_is_urgent(client) && n < sizeof(letters)) {
        letters[n++] = WM_ICON_HINT_URGENT;
    }

    if (n == 0) {
        out[0] = '\0';
        return;
    }

    out[0] = '[';
    pos = 1;
    for (size_t i = 0; i < n && pos + 2 < size; ++i) {
        if (i > 0) {
            out[pos++] = ',';
        }
        out[pos++] = letters[i];
    }
    out[pos++] = ']';
    out[pos] = '\0';
}


/**
 * @brief Collect every focusable, non-skip-taskbar client across
 *        every desktop of @c s_search.surface into @c s_search.
 *        candidates
 *
 * Same eligibility filter @c cycle_init uses for its own window list;
 * unrelated to the currently active desktop, so a client on a desktop
 * other than the one showing right now is still collected.
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       clients across every desktop of @c s_search.surface
 */
static void s_search_collect_candidates(void)
{
    s_search.candidate_count = 0;

    for (uint32_t di = 0; di < s_search.surface->desktop_count &&
            s_search.candidate_count < WM_SEARCH_MAX_ENTRIES; ++di) {
        desktop_td *const desktop = surface_desktop_get(s_search.surface, di);
        cdlist_item_td *node;
        const cdlist_item_td *initial;

        if (desktop == NULL || desktop->stacking == NULL) {
            continue;
        }

        node = cdlist_tail(desktop->stacking);
        initial = node;
        if (node == NULL) {
            continue;
        }

        do {
            client_td *const c = (client_td *) cdlist_data(node);

            if (c != NULL && client_is_focusable(c) &&
                    !(c->properties.flags & CLIENT_FLAG_SKIP_TASKBAR) &&
                    s_search.candidate_count < WM_SEARCH_MAX_ENTRIES) {
                int idx = s_search.candidate_count;

                s_search.candidates[idx] = c;
                s_search.candidate_desktops[idx] = desktop;
                s_search.candidate_count++;
            }
            node = cdlist_prev(node);
        } while (node != NULL && node != initial);
    }
}


/**
 * @brief Re-filter and re-rank @c s_search.results from @c s_search.
 *        candidates against the current @c s_search.query
 *
 * An empty query matches every candidate, in collection order (most
 * recently focused first, since candidates were collected tail to
 * head).  Resets the selection and scroll position to the top.
 *
 * @note Complexity: @e O(n log n), where @e n is @c s_search.
 *       candidate_count
 */
static void s_search_refilter(void)
{
    s_search.result_count = 0;

    for (int i = 0; i < s_search.candidate_count; ++i) {
        client_td *const c = s_search.candidates[i];
        const char *name = (c->info.name != NULL &&
                c->info.name[0] != '\0') ? c->info.name : "(unnamed)";
        int score = 0;
        int idx;
        s_search_result_td *r;

        if (s_search.query_len > 0) {
            score = s_search_fuzzy_score(s_search.query, name);
            if (score < 0) {
                continue;
            }
        }

        idx = s_search.result_count;
        r = &s_search.results[idx];

        r->client = c;
        r->desktop = s_search.candidate_desktops[i];
        r->score = score;
        snprintf(r->name, sizeof(r->name), "%s", name);
        s_search_build_hints(c, r->hints, sizeof(r->hints));
        s_search.result_count++;
    }

    /* Small insertion sort by descending score; the candidate count
     * this widget deals with (open windows) never justifies anything
     * fancier */
    for (int i = 1; i < s_search.result_count; ++i) {
        s_search_result_td key = s_search.results[i];
        int j = i - 1;

        while (j >= 0 && s_search.results[j].score < key.score) {
            s_search.results[j + 1] = s_search.results[j];
            --j;
        }
        s_search.results[j + 1] = key;
    }

    s_search.selected = (s_search.result_count > 0) ? 0 : -1;
    s_search.scroll_offset = 0;
}


/**
 * @brief Recompute the widget's total window height and viewport row
 *        count from the current result count
 *
 * Caps visible height at @c WM_SEARCH_MAX_HEIGHT_PERCENT of the
 * surface's own height, the same reasoning @c cycle_init uses for
 * its own menu.
 *
 * @note Complexity: @e O(1)
 */
static void s_search_compute_geometry(void)
{
    uint32_t screen_h_pct = s_search.surface->properties.dim.h *
        (uint32_t) WM_SEARCH_MAX_HEIGHT_PERCENT / 100u;
    uint32_t avail = (screen_h_pct > (uint32_t) WM_SEARCH_BAR_HEIGHT)
        ? screen_h_pct - (uint32_t) WM_SEARCH_BAR_HEIGHT : 0u;
    int vp_rows = (int) (avail / (uint32_t) WM_SEARCH_ROW_HEIGHT);

    if (vp_rows < 1) {
        vp_rows = 1;
    }
    if (vp_rows > s_search.result_count) {
        vp_rows = s_search.result_count;
    }
    s_search.viewport_rows = vp_rows;

    s_search.height = (uint16_t) (S_SEARCH_ROWS_TOP +
            s_search.viewport_rows * WM_SEARCH_ROW_HEIGHT +
            WM_SEARCH_PAD_Y);
}


/**
 * @brief Keep the current selection inside the visible viewport,
 *        scrolling the minimum amount needed
 *
 * @note Complexity: @e O(1)
 */
static void s_search_scroll_to_selection(void)
{
    if (s_search.selected < 0) {
        return;
    }
    if (s_search.selected < s_search.scroll_offset) {
        s_search.scroll_offset = s_search.selected;
    } else if (s_search.selected >=
            s_search.scroll_offset + s_search.viewport_rows) {
        s_search.scroll_offset =
            s_search.selected - s_search.viewport_rows + 1;
    }
}


/**
 * @brief Return the result index at a given pixel Y inside the
 *        widget, or -1 if @p y falls outside every visible row
 *
 * @param y Pixel Y relative to the widget window
 *
 * @return Absolute result index (not viewport-relative), or -1
 *
 * @note Complexity: @e O(1)
 */
static int s_search_row_at_y(int16_t y)
{
    int rel_row;
    int idx;

    if (y < S_SEARCH_ROWS_TOP) {
        return -1;
    }

    rel_row = (y - S_SEARCH_ROWS_TOP) / WM_SEARCH_ROW_HEIGHT;
    if (rel_row < 0 || rel_row >= s_search.viewport_rows) {
        return -1;
    }

    idx = s_search.scroll_offset + rel_row;
    if (idx < 0 || idx >= s_search.result_count) {
        return -1;
    }
    return idx;
}


/* Query whether the window-search widget is currently open */
bool search_is_open(void)
{
    return s_search.window != XCB_WINDOW_NONE;
}


/* Return the window-search widget's own X window identifier */
xcb_window_t search_window(void)
{
    return s_search.window;
}


/* Initialize the window-search widget */
void search_init(list_td *surfaces, xcb_connection_t *connection,
        surface_td *surface, const config_td *cfg)
{
    xcb_get_input_focus_cookie_t foc_cookie;
    xcb_get_input_focus_reply_t *foc_reply;
    uint32_t mask;
    uint32_t values[4];
    int16_t widget_x;
    int16_t widget_y;

    (void) surfaces;

    if (connection == NULL || surface == NULL || cfg == NULL) {
        return;
    }

    if (search_is_open()) {
        search_destroy(connection);
    }

    memset(&s_search, 0, sizeof(s_search));
    s_search.window = XCB_WINDOW_NONE;
    s_search.surface = surface;
    s_search.config = cfg;

    foc_cookie = xcb_get_input_focus(connection);
    foc_reply = xcb_get_input_focus_reply(connection, foc_cookie, NULL);
    s_search.prev_focus = (foc_reply != NULL &&
            foc_reply->focus != XCB_WINDOW_NONE &&
            foc_reply->focus != XCB_INPUT_FOCUS_POINTER_ROOT &&
            foc_reply->focus != XCB_INPUT_FOCUS_NONE)
        ? foc_reply->focus : XCB_WINDOW_NONE;
    if (foc_reply != NULL) {
        free(foc_reply);
    }

    s_search_collect_candidates();

    if (s_search.candidate_count == 0) {
        /* Nothing to search for at all: showing the widget empty,
         * with no way to ever produce a result no matter what is
         * typed, would only look broken rather than actually
         * informative.  's_search.window' is still 'XCB_WINDOW_NONE'
         * here (set right after the 'memset' above), so
         * 'search_is_open' already correctly reports the widget as
         * never having opened. */
        dialog_info_show(connection, surface, cfg,
                _(STR_SEARCH_NO_WINDOWS), MENU_MSG_LEVEL_INFO);
        return;
    }

    s_search_refilter();
    s_search_compute_geometry();

    widget_x = (int16_t) (((int32_t) surface->properties.dim.w -
                (int32_t) WM_SEARCH_WIDTH) / 2);
    widget_y = (int16_t) (((int32_t) surface->properties.dim.h -
                (int32_t) s_search.height) / 3);
    if (widget_x < 0) { widget_x = 0; }
    if (widget_y < 0) { widget_y = 0; }

    s_search.window = xcb_generate_id(connection);

    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
        XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    values[0] = cfg->theme.search.unselected.color.background;
    values[1] = cfg->theme.search.border.color;
    values[2] = 1;  /* override_redirect: prevent WM from managing it */
    values[3] = XCB_EVENT_MASK_EXPOSURE     |
        XCB_EVENT_MASK_KEY_PRESS    |
        XCB_EVENT_MASK_KEY_RELEASE  |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_POINTER_MOTION;

    xcb_create_window(connection,
            XCB_COPY_FROM_PARENT,
            s_search.window,
            surface->screen->root,
            widget_x, widget_y,
            (uint16_t) WM_SEARCH_WIDTH, s_search.height,
            (uint16_t) cfg->theme.search.border.width,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    xcb_map_window(connection, s_search.window);
    xcb_grab_keyboard(connection,
            0,
            surface->screen->root,
            XCB_CURRENT_TIME,
            XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC);
    xcb_set_input_focus(connection,
            XCB_INPUT_FOCUS_POINTER_ROOT,
            s_search.window,
            XCB_CURRENT_TIME);

    /* Paint immediately: every candidate is shown right away with an
     * empty query (see 's_search_refilter''s comment), so the widget
     * must never open blank and wait for the first keystroke to show
     * anything */
    search_draw(connection, cfg);
}


/* Destroy the window-search widget and restore previous focus */
void search_destroy(xcb_connection_t *connection)
{
    if (connection == NULL || s_search.window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_ungrab_keyboard(connection, XCB_CURRENT_TIME);
    xcb_destroy_window(connection, s_search.window);

    if (s_search.prev_focus != XCB_WINDOW_NONE) {
        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_PARENT,
                s_search.prev_focus, XCB_CURRENT_TIME);
    }

    xcb_flush(connection);

    s_search.window = XCB_WINDOW_NONE;
    s_search.surface = NULL;
    s_search.config = NULL;
    s_search.candidate_count = 0;
    s_search.result_count = 0;
    s_search.selected = -1;
}


/**
 * @brief Confirm the currently selected result: switch to its
 *        desktop, restore it if needed, and focus and raise it
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces, passed through to
 *                   @c focus_apply
 *
 * @note Complexity: @e O(1)
 */
static void s_search_confirm(xcb_connection_t *connection,
        list_td *surfaces)
{
    client_td *client;
    desktop_td *desktop;
    surface_td *surface;

    if (s_search.selected < 0 ||
            s_search.selected >= s_search.result_count) {
        search_destroy(connection);
        return;
    }

    client = s_search.results[s_search.selected].client;
    desktop = s_search.results[s_search.selected].desktop;
    surface = s_search.surface;

    search_destroy(connection);

    if (client == NULL || desktop == NULL || surface == NULL) {
        return;
    }

    if (client_is_iconified(client) ||
            (client->properties.flags & CLIENT_FLAG_HIDDEN)) {
        enact_client_restore(client);
    }
    if (client_is_shaded(client)) {
        enact_client_unshade(client);
    }
    if (desktop->id != surface->desktop_cur) {
        enact_surface_desktop_switch(surface, desktop->id);
    }

    focus_apply(surfaces, surface, desktop, client, true,
            s_search.config);
}


/* Handle a key press while the search widget is open */
void search_handle_keypress(xcb_connection_t *connection,
        list_td *surfaces, xcb_keysym_t keysym, uint16_t state,
        const config_td *cfg)
{
    bool is_tab = (keysym == 0xff09u);   /* Tab */
    bool has_shift = (state & (uint16_t) XCB_MOD_MASK_SHIFT) != 0u;

    (void) cfg;

    if (!search_is_open()) {
        return;
    }

    if (keysym == 0xff1bu) {   /* Escape */
        search_destroy(connection);
        return;
    }

    if (keysym == 0xff0du || keysym == 0xff8du) {   /* Return / KP_Enter */
        s_search_confirm(connection, surfaces);
        return;
    }

    if (keysym == 0xff08u) {   /* Backspace */
        if (s_search.query_len > 0) {
            s_search.query[--s_search.query_len] = '\0';
            s_search_refilter();
            s_search_compute_geometry();
        }
        search_draw(connection, s_search.config);
        return;
    }

    /* Up, or Shift+Tab acting the same way */
    if (keysym == 0xff52u || (is_tab && has_shift)) {
        if (s_search.result_count > 0) {
            s_search.selected = (s_search.selected <= 0)
                ? s_search.result_count - 1 : s_search.selected - 1;
            s_search_scroll_to_selection();
        }
        search_draw(connection, s_search.config);
        return;
    }

    /* Down, or a plain Tab acting the same way */
    if (keysym == 0xff54u || (is_tab && !has_shift)) {
        if (s_search.result_count > 0) {
            s_search.selected =
                (s_search.selected + 1) % s_search.result_count;
            s_search_scroll_to_selection();
        }
        search_draw(connection, s_search.config);
        return;
    }

    if (keysym <= 0xFFu && isprint((int) keysym) &&
            s_search.query_len + 1 < WM_SEARCH_QUERY_MAX_LENGTH) {
        s_search.query[s_search.query_len++] = (char) keysym;
        s_search.query[s_search.query_len] = '\0';
        s_search_refilter();
        s_search_compute_geometry();
        search_draw(connection, s_search.config);
    }
}


/* Handle a button-press event inside the search widget */
void search_handle_click(xcb_connection_t *connection,
        list_td *surfaces, int16_t x, int16_t y, const config_td *cfg)
{
    int idx;

    (void) x;
    (void) cfg;

    if (!search_is_open()) {
        return;
    }

    idx = s_search_row_at_y(y);
    if (idx < 0) {
        return;
    }

    s_search.selected = idx;
    s_search_confirm(connection, surfaces);
}


/* Handle a pointer-motion event inside the search widget */
void search_handle_motion(int16_t x, int16_t y)
{
    int idx;

    (void) x;

    if (!search_is_open()) {
        return;
    }

    idx = s_search_row_at_y(y);
    if (idx < 0 || idx == s_search.selected) {
        return;
    }

    s_search.selected = idx;

    if (s_search.surface != NULL) {
        search_draw(s_search.surface->connection, s_search.config);
    }
}


/**
 * @brief Draw the query bar at the top of the widget, including a
 *        trailing block cursor
 *
 * @param connection XCB connection
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(1)
 */
static void s_search_draw_bar(xcb_connection_t *connection,
        const config_td *cfg)
{
    char shown[WM_SEARCH_QUERY_MAX_LENGTH + 2];

    menu_draw_row_bg(connection, s_search.window,
            cfg->theme.search.input.color.background,
            (int16_t) WM_SEARCH_PAD_Y, (uint16_t) WM_SEARCH_BAR_HEIGHT,
            (uint16_t) WM_SEARCH_WIDTH);

    snprintf(shown, sizeof(shown), "%s_", s_search.query);

    text_renderer_init(connection, cfg->theme.search.input.font);
    text_renderer_set_color(cfg->theme.search.input.color.foreground,
            cfg->theme.search.input.color.background);
    menu_draw_label(connection, s_search.window,
            (struct position_s) { WM_SEARCH_PAD_X,
                WM_SEARCH_PAD_Y + WM_SEARCH_BAR_HEIGHT - 7 },
            shown);
}


/**
 * @brief Paint one result row: background, optional icon, name,
 *        desktop name, and bracketed hints
 *
 * @param connection XCB connection
 * @param cfg        Active configuration
 * @param i          Absolute result index to draw
 *
 * @note Complexity: @e O(1)
 */
static void s_search_draw_row(xcb_connection_t *connection,
        const config_td *cfg, int i)
{
    const s_search_result_td *r = &s_search.results[i];
    int16_t row_y = (int16_t) (S_SEARCH_ROWS_TOP +
            (i - s_search.scroll_offset) * WM_SEARCH_ROW_HEIGHT);
    int16_t text_x = (int16_t) WM_SEARCH_PAD_X;
    int16_t safe_right = (int16_t) (WM_SEARCH_WIDTH - WM_SEARCH_PAD_X -
            WM_SEARCH_HINTS_RESERVED_WIDTH);
    char name_buf[WM_SEARCH_ENTRY_LENGTH];
    uint32_t fg;
    uint32_t bg;
    bool is_sel = (i == s_search.selected);

    fg = (is_sel) ? cfg->theme.search.selected.color.foreground
        : cfg->theme.search.unselected.color.foreground;
    bg = (is_sel) ? cfg->theme.search.selected.color.background
        : cfg->theme.search.unselected.color.background;

    menu_draw_row_bg(connection, s_search.window, bg, row_y,
            (uint16_t) WM_SEARCH_ROW_HEIGHT, (uint16_t) WM_SEARCH_WIDTH);

    if (cfg->theme.menu.show_pixmaps && r->client != NULL &&
            s_search.surface != NULL) {
        uint16_t icon_size = (uint16_t) (WM_SEARCH_ROW_HEIGHT - 4);
        struct position_s icon_pos;

        icon_pos.x = text_x;
        icon_pos.y = row_y +
                (WM_SEARCH_ROW_HEIGHT - (int) icon_size) / 2;

        wmicon_draw_at(connection, s_search.surface->ewmh,
                r->client->window, s_search.window,
                icon_pos, icon_size, fg, bg,
                &r->client->icon_pixmap_cache);
        text_x = (int16_t) (text_x + icon_size + WM_SEARCH_PAD_X);
    }

    /* 'text_renderer_init' destroys and recreates the shared GC
     * (with neutral, unthemed colors) whenever the requested font
     * differs from whichever one is currently loaded (see its own
     * doc comment, render/text.c), so 'text_renderer_set_color' must
     * always run after it, never before: this row's own real colors
     * would otherwise survive only until the next row happens to
     * request a different font than this one, right up until then
     * looking like nothing was ever wrong at all. */
    text_renderer_init(connection, is_sel
            ? cfg->theme.search.selected.font
            : cfg->theme.search.unselected.font);
    text_renderer_set_color(fg, bg);

    snprintf(name_buf, sizeof(name_buf), "%s", r->name);
    if (safe_right > text_x) {
        uint16_t name_max = (uint16_t) (safe_right - text_x);

        if (name_max > (uint16_t) WM_SEARCH_NAME_MAX_WIDTH) {
            name_max = (uint16_t) WM_SEARCH_NAME_MAX_WIDTH;
        }
        menu_draw_truncate(name_buf, name_max);
    }
    menu_draw_label(connection, s_search.window,
            (struct position_s) { text_x,
                row_y + WM_SEARCH_ROW_HEIGHT - 4 }, name_buf);

    if (s_search.surface->desktop_count > 1u && r->desktop != NULL) {
        int16_t desk_x = (int16_t) (text_x +
                menu_draw_measure(name_buf) + WM_SEARCH_COLUMN_GAP);

        if (desk_x < safe_right) {
            char desk_buf[WM_SEARCH_ENTRY_LENGTH];

            if (r->desktop->name[0] != '\0') {
                snprintf(desk_buf, sizeof(desk_buf), "[%u] -- %s",
                        r->desktop->id, r->desktop->name);
            } else {
                snprintf(desk_buf, sizeof(desk_buf), "[%u]",
                        r->desktop->id);
            }
            menu_draw_truncate(desk_buf,
                    (uint16_t) (safe_right - desk_x));
            menu_draw_label(connection, s_search.window,
                    (struct position_s) { desk_x,
                        row_y + WM_SEARCH_ROW_HEIGHT - 4 },
                    desk_buf);
        }
    }

    if (r->hints[0] != '\0') {
        uint16_t hint_w = menu_draw_measure(r->hints);
        int16_t hint_x = (int16_t) (WM_SEARCH_WIDTH - WM_SEARCH_PAD_X -
                hint_w);

        menu_draw_label(connection, s_search.window,
                (struct position_s) { hint_x,
                    row_y + WM_SEARCH_ROW_HEIGHT - 4 }, r->hints);
    }
}


/* Repaint the search widget */
void search_draw(xcb_connection_t *connection, const config_td *cfg)
{
    if (!search_is_open() || cfg == NULL) {
        return;
    }

    /* Kept in sync with 's_search.height' here, the one place every
     * caller that might have just changed it (a new query result
     * count changing how many rows there are to show, via
     * 's_search_compute_geometry') already converges on before ever
     * repainting, rather than needing each of them to remember their
     * own 'xcb_configure_window' too: left undone, the physical
     * window kept whatever taller height an earlier, larger result
     * set had already sized it to, so a repaint after the count
     * shrank only ever painted over its own new, shorter area,
     * leaving the previous (now stale) rows still visible below it,
     * looking like the new list runs into leftover entries from the
     * old one. */
    xcb_configure_window(connection, s_search.window,
            XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) { s_search.height });

    menu_draw_row_bg(connection, s_search.window,
            cfg->theme.search.unselected.color.background,
            0, s_search.height, (uint16_t) WM_SEARCH_WIDTH);

    s_search_draw_bar(connection, cfg);

    for (int i = s_search.scroll_offset;
            i < s_search.scroll_offset + s_search.viewport_rows &&
                i < s_search.result_count; ++i) {
        s_search_draw_row(connection, cfg, i);
    }

    /* Scroll-indicator arrows, same reasoning as 'cycle_draw''s own
     * (menu/cycledraw.c): the up arrow lives in the padding strip
     * right below the query bar, the down arrow in the padding strip
     * right above the window's bottom edge, each only drawn when
     * entries exist beyond the visible viewport on that side */
    if (s_search.result_count > s_search.viewport_rows) {
        int16_t up_y = (int16_t) (WM_SEARCH_PAD_Y + WM_SEARCH_BAR_HEIGHT);
        int16_t down_y = (int16_t) (S_SEARCH_ROWS_TOP +
                s_search.viewport_rows * WM_SEARCH_ROW_HEIGHT);

        text_renderer_init(connection, cfg->theme.search.selected.font);

        if (s_search.scroll_offset > 0) {
            text_renderer_set_color(
                    cfg->theme.search.selected.color.foreground,
                    cfg->theme.search.unselected.color.background);
            menu_draw_label(connection, s_search.window,
                    (struct position_s) { WM_SEARCH_WIDTH / 2 - 4,
                        up_y + text_font_ascent() },
                    WM_SEARCH_MENU_SCROLL_UP_INDICATOR);
        }

        if (s_search.scroll_offset + s_search.viewport_rows <
                s_search.result_count) {
            text_renderer_set_color(
                    cfg->theme.search.selected.color.foreground,
                    cfg->theme.search.unselected.color.background);
            menu_draw_label(connection, s_search.window,
                    (struct position_s) { WM_SEARCH_WIDTH / 2 - 4,
                        down_y + WM_SEARCH_PAD_Y - text_font_descent() },
                    WM_SEARCH_MENU_SCROLL_DOWN_INDICATOR);
        }
    }

    xcb_flush(connection);
}
