/**
 * @file tests/menu/cycle/test_draw.c
 *
 * @brief Test battery for cycle menu visual rendering
 *
 * @c g_cycle_menu (menu/internal.h) is the shared, module-level state
 * this whole file operates on; this test file owns the one real
 * instance directly, the same way 'tests/systray/test_layout.c' owns
 * its one real 's_tray', rather than linking real 'menu/cycle.c' (that
 * file, and 'g_cycle_menu''s state-management side, is already
 * covered on its own in 'tests/menu/test_cycle.c').  Every XCB entry
 * point 'cycle/draw.c' calls, both raw 'xcb_*' requests and this
 * project's own thin wrappers around them ('xcb_window_set_border',
 * 'xcb_window_stack_below'), plus 'render/outline.h',
 * 'render/wmicon.h', 'render/icon.h', and 'render/text.h''s renderer
 * entry points, are stubbed below as controllable, call-recording
 * stand-ins, so every branch can be driven and checked without a real
 * X server around it.  'client_is_decorated', 'client_is_fullscreen',
 * and 'client_border_width' are real, header-inline macros/functions
 * (predicates.h, client.h), never stubbed.  'mi_cycle_preview_target'
 * is this same file's own real function under test, exercised for
 * real by driving each fixture client's own 'window'/'frame'/
 * 'icon_window' fields rather than by a stand-in.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L


/* System includes */
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <stage.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/cycle.h>
#include <menu/internal.h>


/** The one real shared cycle-menu state instance this file owns */
struct cycle_menu_state_s g_cycle_menu;

static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;

static int s_call_menu_draw_row_bg;
static int s_call_menu_draw_label;
static int s_call_wmicon_draw_at;
static int s_call_ri_render_client_icon;
static int s_call_render_outline_show;
static int s_call_render_outline_move;
static int s_call_render_outline_hide;
static int s_call_xcb_window_set_border;
static int s_call_xcb_window_stack_below;
static int s_call_xcb_change_window_attributes;
static int s_call_xcb_clear_area;
static uint32_t s_last_border_width;
static uint32_t s_last_change_attr_value;
static xcb_window_t s_last_stack_below_window;
static xcb_window_t s_last_stack_below_sibling;

/** Recorded label buffer of the most recent 'menu_draw_label' call */
static char s_last_label[WM_CYCLE_MENU_ENTRY_LENGTH];


void menu_draw_row_bg(xcb_connection_t *connection, xcb_window_t window,
        uint32_t color, int16_t y, uint16_t height, uint16_t width)
{
    (void) connection;
    (void) window;
    (void) color;
    (void) y;
    (void) height;
    (void) width;
    s_call_menu_draw_row_bg++;
}


void menu_draw_label(xcb_connection_t *connection, xcb_window_t window,
        struct position_s pos, const char *text)
{
    (void) connection;
    (void) window;
    (void) pos;
    s_call_menu_draw_label++;
    (void) snprintf(s_last_label, sizeof(s_last_label), "%s", text);
}


uint16_t menu_draw_measure(const char *text)
{
    (void) text;
    return 40u;
}


void menu_draw_truncate(char *buf, uint16_t max_width)
{
    (void) max_width;
    (void) buf;
}


int text_renderer_use_font(xcb_connection_t *connection,
        const char *font_name)
{
    (void) connection;
    (void) font_name;
    return 0;
}


void text_renderer_set_color(uint32_t fg, uint32_t bg)
{
    (void) fg;
    (void) bg;
}


int16_t text_font_ascent(void)
{
    return 10;
}


int16_t text_font_descent(void)
{
    return 3;
}


xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return NULL;
}


void wmicon_draw_at(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        xcb_drawable_t drawable, struct position_s pos,
        uint16_t area_size, uint32_t frame_color, uint32_t bg_color,
        wmicon_cache_td *cache)
{
    (void) connection;
    (void) ewmh;
    (void) window;
    (void) drawable;
    (void) pos;
    (void) area_size;
    (void) frame_color;
    (void) bg_color;
    (void) cache;
    s_call_wmicon_draw_at++;
}


void ri_render_client_icon(client_td *client, bool is_current,
        bool force)
{
    (void) client;
    (void) is_current;
    (void) force;
    s_call_ri_render_client_icon++;
}


void render_outline_show(xcb_connection_t *connection, xcb_window_t root,
        struct geometry_s geom, uint32_t border_width, uint32_t color,
        xcb_window_t stack_below, xcb_window_t windows[4])
{
    int i;

    (void) connection;
    (void) root;
    (void) geom;
    (void) border_width;
    (void) color;
    (void) stack_below;
    s_call_render_outline_show++;
    for (i = 0; i < 4; ++i) {
        windows[i] = (xcb_window_t) (900 + i);
    }
}


void render_outline_move(xcb_connection_t *connection,
        struct geometry_s geom, uint32_t border_width,
        xcb_window_t stack_below, xcb_window_t windows[4])
{
    (void) connection;
    (void) geom;
    (void) border_width;
    (void) stack_below;
    (void) windows;
    s_call_render_outline_move++;
}


void render_outline_hide(xcb_connection_t *connection,
        xcb_window_t windows[4])
{
    int i;

    (void) connection;
    s_call_render_outline_hide++;
    for (i = 0; i < 4; ++i) {
        windows[i] = XCB_WINDOW_NONE;
    }
}


void xcb_window_set_border(xcb_window_t window, uint32_t width)
{
    (void) window;
    s_call_xcb_window_set_border++;
    s_last_border_width = width;
}


void xcb_window_stack_below(xcb_window_t window, xcb_window_t sibling)
{
    s_call_xcb_window_stack_below++;
    s_last_stack_below_window = window;
    s_last_stack_below_sibling = sibling;
}


xcb_void_cookie_t xcb_change_window_attributes(xcb_connection_t *c,
        xcb_window_t window, uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) c;
    (void) window;
    (void) value_mask;
    s_call_xcb_change_window_attributes++;
    s_last_change_attr_value = *(const uint32_t *) value_list;
    return cookie;
}


xcb_void_cookie_t xcb_clear_area(xcb_connection_t *c, uint8_t exposures,
        xcb_window_t window, int16_t x, int16_t y, uint16_t width,
        uint16_t height)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) c;
    (void) exposures;
    (void) window;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    s_call_xcb_clear_area++;
    return cookie;
}



static void s_reset(void)
{
    memset(&g_cycle_menu, 0, sizeof(g_cycle_menu));
    s_call_menu_draw_row_bg = 0;
    s_call_menu_draw_label = 0;
    s_call_wmicon_draw_at = 0;
    s_call_ri_render_client_icon = 0;
    s_call_render_outline_show = 0;
    s_call_render_outline_move = 0;
    s_call_render_outline_hide = 0;
    s_call_xcb_window_set_border = 0;
    s_call_xcb_window_stack_below = 0;
    s_call_xcb_change_window_attributes = 0;
    s_call_xcb_clear_area = 0;
    s_last_border_width = 0u;
    s_last_change_attr_value = 0u;
    s_last_stack_below_window = XCB_WINDOW_NONE;
    s_last_stack_below_sibling = XCB_WINDOW_NONE;
    s_last_label[0] = '\0';
}


static config_td s_make_config(void)
{
    config_td cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.theme.menu.unselected.font[0] = '\0';
    cfg.theme.menu.selected.color.foreground = 0xffffffu;
    cfg.theme.menu.selected.color.background = 0x0000ffu;
    cfg.theme.menu.unselected.color.foreground = 0xccccccu;
    cfg.theme.menu.unselected.color.background = 0x101010u;
    cfg.theme.menu.padding.horizontal = 8u;
    cfg.theme.menu.padding.vertical = 4u;
    cfg.theme.menu.show_pixmaps = false;
    cfg.theme.cycle.border.color = 0x00ff00u;
    cfg.theme.cycle.border.width = 2u;
    cfg.theme.icon.is_captioned = false;
    cfg.theme.icon.active.border.width = 1u;
    cfg.theme.icon.active.border.color = 0x00aa00u;
    cfg.theme.icon.inactive.border.color = 0x555555u;
    cfg.theme.window.active.border.width = 1u;
    cfg.theme.window.active.border.color = 0x00aa00u;
    cfg.theme.window.inactive.border.color = 0x777777u;
    return cfg;
}


static stage_td s_make_stage(void)
{
    stage_td stage;
    static xcb_screen_t screen;
    static xcb_screen_t *screen_ptr = &screen;

    memset(&stage, 0, sizeof(stage));
    memset(&screen, 0, sizeof(screen));
    screen.root = 1u;
    stage.screen = screen_ptr;
    return stage;
}


static desktop_td s_make_desktop(xcb_window_t active_id)
{
    desktop_td desktop;

    memset(&desktop, 0, sizeof(desktop));
    desktop.client_active_id = active_id;
    return desktop;
}


static client_td s_clients[8];


static client_td *s_make_client(int slot, xcb_window_t id,
        const char *name, uint16_t flags)
{
    client_td *c = &s_clients[slot];

    memset(c, 0, sizeof(*c));
    c->id = id;
    c->window = id;
    c->info.name = (char *) name;
    c->properties.flags = flags;
    return c;
}


/* cycle_draw on a null connection, null config, or with no menu
 * window open at all is a harmless no-op */
static void s_test_draw_null_guards(void)
{
    config_td cfg;

    s_reset();
    cfg = s_make_config();

    cycle_draw(NULL, &cfg);
    TAP_EQ_INT(s_call_menu_draw_row_bg, 0,
            "a null connection paints nothing at all");

    cycle_draw(s_fake_connection, NULL);
    TAP_EQ_INT(s_call_menu_draw_row_bg, 0,
            "a null config paints nothing at all");

    g_cycle_menu.window = XCB_WINDOW_NONE;
    cycle_draw(s_fake_connection, &cfg);
    TAP_EQ_INT(s_call_menu_draw_row_bg, 0,
            "no cycle window open paints nothing at all");
}


/* The first cycle_draw call after a fresh menu (has_drawn_once still
 * false) always does a full repaint: one background plus one label
 * per visible row */
static void s_test_draw_full_repaint_first_call(void)
{
    config_td cfg;
    stage_td stage;
    desktop_td desktop;

    s_reset();
    cfg = s_make_config();
    stage = s_make_stage();
    desktop = s_make_desktop(XCB_WINDOW_NONE);

    g_cycle_menu.window = 42u;
    g_cycle_menu.stage = &stage;
    g_cycle_menu.desktop = &desktop;
    g_cycle_menu.count = 3;
    g_cycle_menu.selected = 0;
    g_cycle_menu.visible_rows = 3;
    g_cycle_menu.scroll_offset = 0;
    g_cycle_menu.width = 200u;
    g_cycle_menu.has_drawn_once = false;
    g_cycle_menu.clients[0] = s_make_client(0, 0x10u, "A",
            CLIENT_FLAG_FOCUSABLE);
    g_cycle_menu.clients[1] = s_make_client(1, 0x11u, "B",
            CLIENT_FLAG_FOCUSABLE);
    g_cycle_menu.clients[2] = s_make_client(2, 0x12u, "C",
            CLIENT_FLAG_FOCUSABLE);
    (void) snprintf(g_cycle_menu.labels[0],
            sizeof(g_cycle_menu.labels[0]), "A");
    (void) snprintf(g_cycle_menu.labels[1],
            sizeof(g_cycle_menu.labels[1]), "B");
    (void) snprintf(g_cycle_menu.labels[2],
            sizeof(g_cycle_menu.labels[2]), "C");

    cycle_draw(s_fake_connection, &cfg);

    TAP_EQ_INT(s_call_menu_draw_row_bg, 3,
            "a full first repaint paints one background per visible"
            " row");
    TAP_EQ_INT(s_call_menu_draw_label, 3,
            "a full first repaint paints one label per visible row");
    TAP_OK(g_cycle_menu.has_drawn_once,
            "has_drawn_once is set once a repaint has actually"
            " happened");
    TAP_EQ_INT(g_cycle_menu.last_drawn_selected, 0,
            "last_drawn_selected is recorded after the repaint");
    TAP_EQ_INT(g_cycle_menu.last_drawn_scroll_offset, 0,
            "last_drawn_scroll_offset is recorded after the repaint");
}


/* Once has_drawn_once is true and the scroll offset has not changed,
 * a plain selection move repaints only the two affected rows, not
 * every visible row */
static void s_test_draw_selection_move_repaints_two_rows(void)
{
    config_td cfg;
    stage_td stage;
    desktop_td desktop;

    s_reset();
    cfg = s_make_config();
    stage = s_make_stage();
    desktop = s_make_desktop(XCB_WINDOW_NONE);

    g_cycle_menu.window = 42u;
    g_cycle_menu.stage = &stage;
    g_cycle_menu.desktop = &desktop;
    g_cycle_menu.count = 3;
    g_cycle_menu.selected = 1;
    g_cycle_menu.visible_rows = 3;
    g_cycle_menu.scroll_offset = 0;
    g_cycle_menu.width = 200u;
    g_cycle_menu.has_drawn_once = true;
    g_cycle_menu.last_drawn_selected = 0;
    g_cycle_menu.last_drawn_scroll_offset = 0;
    g_cycle_menu.clients[0] = s_make_client(0, 0x20u, "A",
            CLIENT_FLAG_FOCUSABLE);
    g_cycle_menu.clients[1] = s_make_client(1, 0x21u, "B",
            CLIENT_FLAG_FOCUSABLE);
    g_cycle_menu.clients[2] = s_make_client(2, 0x22u, "C",
            CLIENT_FLAG_FOCUSABLE);

    cycle_draw(s_fake_connection, &cfg);

    TAP_EQ_INT(s_call_menu_draw_row_bg, 2,
            "a same-visible-range selection move repaints exactly two"
            " row backgrounds");
    TAP_EQ_INT(s_call_menu_draw_label, 2,
            "a same-visible-range selection move repaints exactly two"
            " labels");
    TAP_EQ_INT(g_cycle_menu.last_drawn_selected, 1,
            "last_drawn_selected tracks the new selection afterward");
}


/* A scroll-offset change forces the full-repaint path even though
 * has_drawn_once is already true */
static void s_test_draw_scroll_change_forces_full_repaint(void)
{
    config_td cfg;
    stage_td stage;
    desktop_td desktop;

    s_reset();
    cfg = s_make_config();
    stage = s_make_stage();
    desktop = s_make_desktop(XCB_WINDOW_NONE);

    g_cycle_menu.window = 42u;
    g_cycle_menu.stage = &stage;
    g_cycle_menu.desktop = &desktop;
    g_cycle_menu.count = 5;
    g_cycle_menu.selected = 2;
    g_cycle_menu.visible_rows = 2;
    g_cycle_menu.scroll_offset = 1;
    g_cycle_menu.width = 200u;
    g_cycle_menu.has_drawn_once = true;
    g_cycle_menu.last_drawn_selected = 2;
    g_cycle_menu.last_drawn_scroll_offset = 0;
    g_cycle_menu.clients[0] = s_make_client(0, 0x30u, "A",
            CLIENT_FLAG_FOCUSABLE);
    g_cycle_menu.clients[1] = s_make_client(1, 0x31u, "B",
            CLIENT_FLAG_FOCUSABLE);
    g_cycle_menu.clients[2] = s_make_client(2, 0x32u, "C",
            CLIENT_FLAG_FOCUSABLE);
    g_cycle_menu.clients[3] = s_make_client(3, 0x33u, "D",
            CLIENT_FLAG_FOCUSABLE);
    g_cycle_menu.clients[4] = s_make_client(4, 0x34u, "E",
            CLIENT_FLAG_FOCUSABLE);

    cycle_draw(s_fake_connection, &cfg);

    /* 2 real rows plus 2 padding-strip backgrounds, since count (5)
     * exceeds visible_rows (2) here too */
    TAP_EQ_INT(s_call_menu_draw_row_bg, 4,
            "a scroll change repaints exactly the rows now visible"
            " in the shifted range, plus both scroll-indicator"
            " padding strips");
    TAP_EQ_INT(g_cycle_menu.last_drawn_scroll_offset, 1,
            "last_drawn_scroll_offset tracks the new offset"
            " afterward");
}


/* When more entries exist than fit the visible rows, a full repaint
 * also draws the scroll-indicator strips; the up arrow only once
 * scroll_offset is past 0, and the down arrow only while entries
 * remain below the visible rows */
static void s_test_draw_scroll_indicators(void)
{
    config_td cfg;
    stage_td stage;
    desktop_td desktop;
    int extra_bg_calls;

    s_reset();
    cfg = s_make_config();
    stage = s_make_stage();
    desktop = s_make_desktop(XCB_WINDOW_NONE);

    g_cycle_menu.window = 42u;
    g_cycle_menu.stage = &stage;
    g_cycle_menu.desktop = &desktop;
    g_cycle_menu.count = 5;
    g_cycle_menu.selected = 2;
    g_cycle_menu.visible_rows = 2;
    g_cycle_menu.scroll_offset = 1;
    g_cycle_menu.width = 200u;
    g_cycle_menu.has_drawn_once = false;
    g_cycle_menu.clients[0] = s_make_client(0, 0x40u, "A",
            CLIENT_FLAG_FOCUSABLE);
    g_cycle_menu.clients[1] = s_make_client(1, 0x41u, "B",
            CLIENT_FLAG_FOCUSABLE);
    g_cycle_menu.clients[2] = s_make_client(2, 0x42u, "C",
            CLIENT_FLAG_FOCUSABLE);
    g_cycle_menu.clients[3] = s_make_client(3, 0x43u, "D",
            CLIENT_FLAG_FOCUSABLE);
    g_cycle_menu.clients[4] = s_make_client(4, 0x44u, "E",
            CLIENT_FLAG_FOCUSABLE);

    cycle_draw(s_fake_connection, &cfg);

    /* 2 real rows plus 2 padding-strip backgrounds (one for the up
     * arrow area, one for the down arrow area) */
    extra_bg_calls = s_call_menu_draw_row_bg - 2;
    TAP_EQ_INT(extra_bg_calls, 2,
            "both the top and bottom padding strips are painted"
            " when entries exist on both sides of the visible rows");
    TAP_EQ_INT(s_call_menu_draw_label, 2 + 2,
            "both a scroll-up and a scroll-down indicator label are"
            " drawn alongside the 2 real row labels");
}


/* mi_cycle_preview_apply, called at the tail end of cycle_draw,
 * applies the border style and window-menu stacking to the newly
 * selected client's preview target, and creates the outline strips
 * the first time */
static void s_test_draw_applies_preview_to_selection(void)
{
    config_td cfg;
    stage_td stage;
    desktop_td desktop;
    client_td *selected;

    s_reset();
    cfg = s_make_config();
    stage = s_make_stage();
    desktop = s_make_desktop(0x50u);

    g_cycle_menu.window = 42u;
    g_cycle_menu.stage = &stage;
    g_cycle_menu.desktop = &desktop;
    g_cycle_menu.count = 1;
    g_cycle_menu.selected = 0;
    g_cycle_menu.visible_rows = 1;
    g_cycle_menu.scroll_offset = 0;
    g_cycle_menu.width = 200u;
    g_cycle_menu.has_drawn_once = false;
    g_cycle_menu.is_icon_menu = false;
    selected = s_make_client(0, 0x50u, "Active",
            (uint16_t) (CLIENT_FLAG_FOCUSABLE | CLIENT_FLAG_DECORATED));
    selected->frame = 77u;
    g_cycle_menu.clients[0] = selected;
    (void) snprintf(g_cycle_menu.labels[0],
            sizeof(g_cycle_menu.labels[0]), "Active");

    cycle_draw(s_fake_connection, &cfg);

    TAP_EQ_INT(s_call_xcb_window_stack_below, 1,
            "the newly selected target is stacked below the cycle"
            " menu window exactly once");
    TAP_EQ_INT((long) s_last_stack_below_window, (long) 77u,
            "the target actually stacked below is the resolved"
            " preview target (the client's own frame, since it is"
            " decorated)");
    TAP_EQ_INT((long) s_last_stack_below_sibling, (long) 42u,
            "it is stacked directly below the cycle menu window"
            " itself");
    TAP_EQ_INT(s_call_render_outline_show, 1,
            "the outline strips are created the first time a"
            " selection is previewed");
    TAP_OK(g_cycle_menu.outline_windows[0] != XCB_WINDOW_NONE,
            "the outline windows array is filled in after creation");
    TAP_OK(g_cycle_menu.preview_client == selected,
            "preview_client is updated to the newly selected client");
    TAP_EQ_INT(s_call_xcb_window_set_border, 0,
            "the selected window's own border width is never touched:"
            " the cycle outline alone marks the selection");
    TAP_EQ_INT(s_call_xcb_change_window_attributes, 0,
            "...nor its frame's or border's colors");
}


/* Once a selection is already previewed, moving to a different
 * selection restyles the previous target back to its own inactive
 * color and moves (rather than recreates) the outline strips */
static void s_test_draw_preview_transitions_between_targets(void)
{
    config_td cfg;
    stage_td stage;
    desktop_td desktop;
    client_td *first;
    client_td *second;

    s_reset();
    cfg = s_make_config();
    stage = s_make_stage();
    desktop = s_make_desktop(0x60u);

    first = s_make_client(0, 0x60u, "First", CLIENT_FLAG_FOCUSABLE);
    second = s_make_client(1, 0x61u, "Second", CLIENT_FLAG_FOCUSABLE);

    g_cycle_menu.window = 42u;
    g_cycle_menu.stage = &stage;
    g_cycle_menu.desktop = &desktop;
    g_cycle_menu.count = 2;
    g_cycle_menu.selected = 0;
    g_cycle_menu.visible_rows = 2;
    g_cycle_menu.scroll_offset = 0;
    g_cycle_menu.width = 200u;
    g_cycle_menu.has_drawn_once = false;
    g_cycle_menu.is_icon_menu = false;
    g_cycle_menu.clients[0] = first;
    g_cycle_menu.clients[1] = second;

    cycle_draw(s_fake_connection, &cfg);
    TAP_EQ_INT(s_call_render_outline_show, 1,
            "the first draw creates the outline strips exactly once");
    TAP_OK(g_cycle_menu.preview_client == first,
            "the first draw previews the first client");

    g_cycle_menu.selected = 1;
    g_cycle_menu.last_drawn_selected = 0;
    s_call_xcb_change_window_attributes = 0;

    cycle_draw(s_fake_connection, &cfg);

    TAP_EQ_INT(s_call_render_outline_move, 1,
            "a later draw with an existing outline moves it rather"
            " than recreating it");
    TAP_EQ_INT(s_call_render_outline_show, 1,
            "render_outline_show is never called a second time once"
            " the strips already exist");
    TAP_OK(g_cycle_menu.preview_client == second,
            "preview_client now tracks the second, newly selected"
            " client");
    TAP_EQ_INT(s_call_xcb_window_set_border, 0,
            "neither the deselected nor the newly selected window has"
            " its own border restyled");
}


/* When the cycle menu is showing icon previews (is_icon_menu true),
 * mi_cycle_preview_apply additionally repaints the selected and
 * deselected clients' real rendered icons through ri_render_client_icon */
static void s_test_draw_icon_menu_preview_repaints_icons(void)
{
    config_td cfg;
    stage_td stage;
    desktop_td desktop;
    client_td *first;
    client_td *second;

    s_reset();
    cfg = s_make_config();
    stage = s_make_stage();
    desktop = s_make_desktop(XCB_WINDOW_NONE);

    first = s_make_client(0, 0x70u, "First", CLIENT_FLAG_FOCUSABLE);
    first->icon_window = 0x9300u;
    second = s_make_client(1, 0x71u, "Second", CLIENT_FLAG_FOCUSABLE);
    second->icon_window = 0x9400u;

    g_cycle_menu.window = 42u;
    g_cycle_menu.stage = &stage;
    g_cycle_menu.desktop = &desktop;
    g_cycle_menu.count = 2;
    g_cycle_menu.selected = 0;
    g_cycle_menu.visible_rows = 2;
    g_cycle_menu.scroll_offset = 0;
    g_cycle_menu.width = 200u;
    g_cycle_menu.has_drawn_once = false;
    g_cycle_menu.is_icon_menu = true;
    g_cycle_menu.clients[0] = first;
    g_cycle_menu.clients[1] = second;

    cycle_draw(s_fake_connection, &cfg);
    TAP_EQ_INT(s_call_ri_render_client_icon, 1,
            "selecting the first icon for the first time repaints"
            " exactly its own real icon");

    g_cycle_menu.selected = 1;
    g_cycle_menu.last_drawn_selected = 0;

    cycle_draw(s_fake_connection, &cfg);
    TAP_EQ_INT(s_call_ri_render_client_icon, 3,
            "moving the icon selection repaints both the newly"
            " deselected and newly selected icons for real (1 more"
            " from the first draw, 2 more from this one)");
    TAP_EQ_INT(s_call_xcb_window_set_border, 3,
            "icon borders are still restyled: the selected one on the"
            " first draw, both on the second");
}


/* Re-selecting the exact same client already being previewed is a
 * no-op for mi_cycle_preview_apply's styling and stacking work */
static void s_test_draw_preview_noop_on_same_selection(void)
{
    config_td cfg;
    stage_td stage;
    desktop_td desktop;
    client_td *only;

    s_reset();
    cfg = s_make_config();
    stage = s_make_stage();
    desktop = s_make_desktop(XCB_WINDOW_NONE);

    only = s_make_client(0, 0x80u, "Only", CLIENT_FLAG_FOCUSABLE);

    g_cycle_menu.window = 42u;
    g_cycle_menu.stage = &stage;
    g_cycle_menu.desktop = &desktop;
    g_cycle_menu.count = 1;
    g_cycle_menu.selected = 0;
    g_cycle_menu.visible_rows = 1;
    g_cycle_menu.scroll_offset = 0;
    g_cycle_menu.width = 200u;
    g_cycle_menu.has_drawn_once = false;
    g_cycle_menu.clients[0] = only;

    cycle_draw(s_fake_connection, &cfg);
    TAP_EQ_INT(s_call_xcb_window_stack_below, 1,
            "stacking the target happens exactly once on the first"
            " draw");

    g_cycle_menu.has_drawn_once = true;
    g_cycle_menu.last_drawn_scroll_offset = 0;
    /* selected did not change, so cycle_draw's own repaint dispatch
     * paints nothing further, but mi_cycle_preview_apply is still
     * called unconditionally every time */
    cycle_draw(s_fake_connection, &cfg);

    TAP_EQ_INT(s_call_xcb_window_stack_below, 1,
            "re-previewing the exact same already-selected client a"
            " second time restacks nothing further");
}


/* mi_cycle_preview_apply, called with no menu open (window ==
 * XCB_WINDOW_NONE) or with an invalid selection index, is a harmless
 * no-op; exercised directly since cycle_draw's own top guard would
 * otherwise always shield it */
static void s_test_preview_apply_direct_guards(void)
{
    config_td cfg;
    stage_td stage;
    desktop_td desktop;

    s_reset();
    cfg = s_make_config();
    stage = s_make_stage();
    desktop = s_make_desktop(XCB_WINDOW_NONE);

    mi_cycle_preview_apply(NULL, &cfg);
    TAP_EQ_INT(s_call_xcb_window_stack_below, 0,
            "a null connection is a harmless no-op");

    mi_cycle_preview_apply(s_fake_connection, NULL);
    TAP_EQ_INT(s_call_xcb_window_stack_below, 0,
            "a null config is a harmless no-op");

    g_cycle_menu.window = XCB_WINDOW_NONE;
    mi_cycle_preview_apply(s_fake_connection, &cfg);
    TAP_EQ_INT(s_call_xcb_window_stack_below, 0,
            "no cycle window open is a harmless no-op");

    g_cycle_menu.window = 42u;
    g_cycle_menu.stage = &stage;
    g_cycle_menu.desktop = &desktop;
    g_cycle_menu.count = 1;
    g_cycle_menu.selected = 5;
    mi_cycle_preview_apply(s_fake_connection, &cfg);
    TAP_EQ_INT(s_call_xcb_window_stack_below, 0,
            "an out-of-range selected index is a harmless no-op");
}


/* mi_cycle_preview_style_icon's own guards, exercised directly
 * since every path through mi_cycle_preview_apply above already
 * resolves a valid icon window before calling it */
static void s_test_style_icon_null_guards(void)
{
    config_td cfg;

    s_reset();
    cfg = s_make_config();

    mi_cycle_preview_style_icon(NULL, 0x9000u, &cfg, 0x123456u);
    TAP_EQ_INT(s_call_xcb_window_set_border, 0,
            "a null connection styles nothing");

    mi_cycle_preview_style_icon(s_fake_connection, XCB_WINDOW_NONE, &cfg,
            0x123456u);
    TAP_EQ_INT(s_call_xcb_window_set_border, 0,
            "an XCB_WINDOW_NONE icon window styles nothing");

    mi_cycle_preview_style_icon(s_fake_connection, 0x9000u, NULL,
            0x123456u);
    TAP_EQ_INT(s_call_xcb_window_set_border, 0,
            "a null config styles nothing");
}


/* An icon window gets the icon theme's active border width, selected
 * or not, and the color it is given */
static void s_test_style_icon_width_and_color(void)
{
    config_td cfg;

    s_reset();
    cfg = s_make_config();

    mi_cycle_preview_style_icon(s_fake_connection, 0x9000u, &cfg,
            0x111111u);
    TAP_EQ_INT((long) s_last_border_width,
            (long) cfg.theme.icon.active.border.width,
            "an icon's border width comes from the icon theme's active"
            " border width");
    TAP_EQ_INT((long) s_last_change_attr_value, (long) 0x111111u,
            "...and its color is the one given");
}


int main(void)
{
    TAP_PLAN(43);

    s_test_draw_null_guards();
    s_test_draw_full_repaint_first_call();
    s_test_draw_selection_move_repaints_two_rows();
    s_test_draw_scroll_change_forces_full_repaint();
    s_test_draw_scroll_indicators();
    s_test_draw_applies_preview_to_selection();
    s_test_draw_preview_transitions_between_targets();
    s_test_draw_icon_menu_preview_repaints_icons();
    s_test_draw_preview_noop_on_same_selection();
    s_test_preview_apply_direct_guards();
    s_test_style_icon_null_guards();
    s_test_style_icon_width_and_color();

    return TAP_DONE();
}
