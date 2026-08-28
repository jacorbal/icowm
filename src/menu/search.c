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
#include <policy/stacking.h>
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
#include <utils/xcb/connection.h>


/**
 * @brief Pixel Y where the first result row starts: the top padding,
 *        the query bar, and a second padding strip reserved for the
 *        up-scroll indicator, the same reasoning
 *        @c WM_CYCLE_MENU_PAD_Y reserves around @c cycledraw.c's
 *        own menu
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
    surface_td *surface;
    size_t query_len;
    const config_td *config;

    /** Every focusable candidate collected at open time, alongside
     *  the desktop it lives on, before any query has filtered it */
    client_td *candidates[WM_SEARCH_MAX_ENTRIES];

    desktop_td *candidate_desktops[WM_SEARCH_MAX_ENTRIES];
    s_search_result_td results[WM_SEARCH_MAX_ENTRIES];
    xcb_window_t window;
    int candidate_count;
    int result_count;
    int selected;
    int scroll_offset;
    int viewport_rows;
    xcb_window_t prev_focus;
    uint16_t height;
    char query[WM_SEARCH_QUERY_MAX_LENGTH];
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
 * A client may hold several geometry states at once, full screen over
 * a maximized window being the ordinary case, so the chain below
 * reports the outermost one alone, the same one the window is
 * actually drawn as.  Everything after it is independent, of that
 * letter and of the others: iconified, shaded, hidden, sticky and
 * urgent each report on their own, so a maximized window sitting as
 * an icon says so twice over.  The result is one comma-separated
 * list such as @c "[p,m,!]", and nothing at all when no hint
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
    } else if (client_is_maximized(client)) {
        letters[n++] = WM_ICON_HINT_MAXIMIZED;
    } else if (client_is_maximized_horz(client)) {
        letters[n++] = WM_ICON_HINT_MAXIMIZED_HORZ;
    } else if (client_is_maximized_vert(client)) {
        letters[n++] = WM_ICON_HINT_MAXIMIZED_VERT;
    }

    /* On its own rather than in the chain above: being an icon is not
     * one of the geometry states, it is a thing that happens to a
     * window whatever geometry state it holds, so a maximized window
     * sitting as an icon has to report both. */
    if (client_is_iconified(client) && n < sizeof(letters)) {
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
 * @brief Note one client as a search candidate
 *
 * @param client Client reached by the walk
 * @param data   The desktop it is on, as a @c desktop_td pointer
 *
 * @note Complexity: @e O(1)
 */
static void s_search_candidate_visit(client_td *client, void *data)
{
    desktop_td *const desktop = data;
    int index;

    if (client == NULL || desktop == NULL ||
            !client_is_focusable(client) ||
            (client->properties.flags & CLIENT_FLAG_SKIP_TASKBAR) ||
            s_search.candidate_count >= WM_SEARCH_MAX_ENTRIES) {
        return;
    }

    index = s_search.candidate_count;
    s_search.candidates[index] = client;
    s_search.candidate_desktops[index] = desktop;
    s_search.candidate_count++;
}


/**
 * @brief Collect every focusable, non-skip-taskbar client across
 *        every desktop of @c s_search.surface into @c s_search.
 *        candidates
 *
 * Same eligibility filter @a cycle_init uses for its own window list;
 * unrelated to the currently active desktop, so a client on a desktop
 * other than the one showing right now is still collected.
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       clients across every desktop of @c s_search.surface (a
 *       single walk of the surface's own circular desktop list,
 *       not one lookup per index, plus one walk of each desktop's
 *       own stacking list)
 */
static void s_search_collect_candidates(void)
{
    cdlist_item_td *dnode;

    s_search.candidate_count = 0;

    cdlist_foreach(s_search.surface->desktops, dnode) {
        desktop_td *const desktop = (desktop_td *) cdlist_data(dnode);

        if (s_search.candidate_count >= WM_SEARCH_MAX_ENTRIES) {
            break;
        }
        /* Walked from the top of the stack down, so the windows most
         * likely to be wanted are offered first */
        stacking_walk_down(desktop, s_search_candidate_visit, desktop);
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
        (void) snprintf(r->name, sizeof(r->name), "%s", name);
        s_search_build_hints(c, r->hints, sizeof(r->hints));
        s_search.result_count++;
    }

    /* Small insertion sort by descending score; the candidate count
     * this widget deals with (open windows) never justifies anything
     * fancier */
    /* Indexed without a sign, so that the descending walk stops at
     * zero rather than at minus one: a signed index there lets the
     * optimizer assume its own arithmetic never overflows, which is
     * what '-Wstrict-overflow' reports on */
    for (unsigned int i = 1u; i < (unsigned int) s_search.result_count;
            ++i) {
        s_search_result_td key = s_search.results[i];
        unsigned int j = i;

        while (j > 0u && s_search.results[j - 1u].score < key.score) {
            s_search.results[j] = s_search.results[j - 1u];
            --j;
        }
        s_search.results[j] = key;
    }

    s_search.selected = (s_search.result_count > 0) ? 0 : -1;
    s_search.scroll_offset = 0;
}


/**
 * @brief Recompute the widget's total window height and viewport row
 *        count from the current result count
 *
 * Caps visible height at @c WM_SEARCH_MAX_HEIGHT_PERCENT of the
 * surface's own height, the same reasoning @a cycle_init uses for
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
    unsigned int rel_row;
    int idx;

    if (y < S_SEARCH_ROWS_TOP) {
        return -1;
    }

    /* Counted without a sign once @p y is known to sit at or below
     * the first row: the subtraction cannot go negative from here,
     * and a signed one would let the optimizer assume as much on its
     * own, which is what '-Wstrict-overflow' reports on */
    rel_row = ((unsigned int) y - (unsigned int) S_SEARCH_ROWS_TOP) /
        (unsigned int) WM_SEARCH_ROW_HEIGHT;
    if (s_search.viewport_rows < 0 ||
            rel_row >= (unsigned int) s_search.viewport_rows) {
        return -1;
    }

    idx = s_search.scroll_offset + (int) rel_row;
    if (idx < 0 || idx >= s_search.result_count) {
        return -1;
    }
    return idx;
}


/**
 * @brief Confirm the currently selected result: switch to its
 *        desktop, restore it if needed, and focus and raise it
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces, passed through to
 *                   @a focus_apply
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

    if (client_is_iconified(client)) {
        /* Takes priority over the plain-hidden branch below: its own
         * restore path (see 's_ccmd_client_restore_one', cmds/
         * client/focus.c) already clears 'CLIENT_FLAG_HIDDEN' too
         * along the way, in the unlikely case both ever happened to
         * be set on the very same client at once. */
        enact_client_restore(client);
    } else if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
        /* Deliberately 'unhide', not 'restore': a plain hide never
         * touches 'layout.geometry.old' the way iconifying does (see
         * 'client_geometry_save''s own call sites), so restoring
         * from it here would apply whatever that field last held for
         * an entirely different reason (stale, or never set at all)
         * instead of leaving this client's own current geometry
         * alone, the correct behavior 'ccmd_client_unhide' itself
         * already provides. */
        enact_client_unhide(client);
    }
    if (client_is_shaded(client)) {
        enact_client_unshade(client);
    }
    if (client_is_pinned(client)) {
        /* A pinned client is already visible on whichever desktop is
         * currently shown: pinning never actually moves a client
         * between desktops, it stays registered under whichever one
         * it was originally on forever; see
         * 'ccmd_client_bring_family''s comment in
         * 'cmds/client/transient.c' for
         * the fuller reasoning; so there is nothing to switch to
         * here.  Using its own recorded 'desktop' below instead
         * (wherever it still happens to be registered) would switch
         * away from right where the user already is, to bring up a
         * window already sitting in front of them; 'focus_apply'
         * itself already correctly brings any of its own un-pinned
         * transient descendants onto this same current desktop via
         * its own 'ccmd_client_bring_family' call, using 'surface->
         * desktop_cur' exactly as this does. */
        desktop = surface_desktop_get(surface, surface->desktop_cur);
        if (desktop == NULL) {
            return;
        }
    } else if (desktop->id != surface->desktop_cur) {
        enact_surface_desktop_switch(surface, desktop->id);
    }

    focus_apply(surfaces, surface, desktop, client, true,
            s_search.config);
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

        wmicon_draw_at(connection, xcb_ewmh_connection_get(),
                r->client->window, s_search.window,
                icon_pos, icon_size, fg, bg,
                &r->client->icon_pixmap_cache);
        text_x = (int16_t) (text_x + icon_size + WM_SEARCH_PAD_X);
    }

    /* Each cached font carries a graphics context of its own, so
     * 'text_renderer_set_color' always applies to whichever font is
     * selected at that moment.  It must therefore run after
     * 'text_renderer_use_font', never before, or this row's colors
     * would land on the font the previous row happened to leave
     * selected. */
    (void) text_renderer_use_font(connection, is_sel
            ? cfg->theme.search.selected.font
            : cfg->theme.search.unselected.font);
    text_renderer_set_color(fg, bg);

    /* The desktop label comes first and the window title after it,
     * rather than the other way round.  A title is as long as the
     * application cares to make it, so with the title leading, the
     * label lands at a different offset on every row and cannot be
     * read down the list; at the front the labels line up in a column.
     *
     * That is also why the label leaves out the desktop's own name
     * here: what identifies an entry in a list of windows is the
     * window's own title, and the name would take width from it. */
    if (s_search.surface->desktop_count > 1u && r->desktop != NULL) {
        char desk_buf[WM_SEARCH_ENTRY_LENGTH];

        surface_desktop_label(s_search.surface, r->desktop->id,
                r->desktop->name,
                r->client != NULL && client_is_pinned(r->client),
                false, desk_buf, sizeof(desk_buf));

        if (desk_buf[0] != '\0' && text_x < safe_right) {
            menu_draw_truncate(desk_buf,
                    (uint16_t) (safe_right - text_x));
            menu_draw_label(connection, s_search.window,
                    (struct position_s) { text_x,
                        row_y + WM_SEARCH_ROW_HEIGHT - 4 }, desk_buf);
            text_x = (int16_t) (text_x + menu_draw_measure(desk_buf) +
                    WM_SEARCH_COLUMN_GAP);
        }
    }

    (void) snprintf(name_buf, sizeof(name_buf), "%s", r->name);
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

    if (r->hints[0] != '\0') {
        uint16_t hint_w = menu_draw_measure(r->hints);
        int16_t hint_x = (int16_t) (WM_SEARCH_WIDTH - WM_SEARCH_PAD_X -
                hint_w);

        menu_draw_label(connection, s_search.window,
                (struct position_s) { hint_x,
                    row_y + WM_SEARCH_ROW_HEIGHT - 4 }, r->hints);
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

    (void) snprintf(shown, sizeof(shown), "%s_", s_search.query);

    (void) text_renderer_use_font(connection,
            cfg->theme.search.input.font);
    text_renderer_set_color(cfg->theme.search.input.color.foreground,
            cfg->theme.search.input.color.background);
    menu_draw_label(connection, s_search.window,
            (struct position_s) { WM_SEARCH_PAD_X,
                WM_SEARCH_PAD_Y + WM_SEARCH_BAR_HEIGHT - 7 },
            shown);
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
            (client_last_user_time() != 0u)
                ? client_last_user_time()
                : (uint32_t) XCB_CURRENT_TIME);

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
                s_search.prev_focus, (client_last_user_time() != 0u)
                ? client_last_user_time()
                : (uint32_t) XCB_CURRENT_TIME);
    }

    xcb_flush(connection);

    s_search.window = XCB_WINDOW_NONE;
    s_search.surface = NULL;
    s_search.config = NULL;
    s_search.candidate_count = 0;
    s_search.result_count = 0;
    s_search.selected = -1;
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

    /* Return or KP_Enter */
    if (keysym == 0xff0du || keysym == 0xff8du) {
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
        search_draw(xcb_connection_get(), s_search.config);
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

        (void) text_renderer_use_font(connection,
                cfg->theme.search.selected.font);

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
