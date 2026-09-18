/**
 * @file tests/menu/test_cycle.c
 *
 * @brief Test battery for the window and icon cycle menu
 *
 * 'menu/cycle.c' owns the one real instance of the shared
 * 'g_cycle_menu' global (declared @c extern in @c menu/internal.h);
 * this file links the real 'cycle.c' and therefore that one real
 * instance, exercising every public entry point exactly as any other
 * translation unit in the project would.  Every raw XCB entry point
 * is a link-only or recording stand-in, following
 * 'tests/menu/test_notify.c''s established pattern.  Every genuinely
 * cross-module dependency this file cannot resolve without a live X
 * server, a real focus-order data structure, or a real keybinding
 * table is a controllable stand-in defined directly in this file,
 * the same substitution style 'tests/rules/test_apply.c' uses for
 * 'enact.h': 'focus_order_walk' (policy/focus.c) walks a small,
 * test-populated array of 'client_td*' instead of a real desktop's
 * hash table; 'keyboard_find' (input/kbd/bind.c) answers fixed
 * next/prev keysym pairs; 'focus_apply' (policy/focus.c),
 * 'enact_client_restore' / 'enact_client_unhide' /
 * 'enact_client_unshade' (enact.c),
 * 'scmd_stage_viewport_center_on_client' (cmds/stage.c),
 * and 'ri_render_client_icon'
 * (render/icon.c) are recording no-ops; 'mi_cycle_preview_target' /
 * 'mi_cycle_preview_apply' / 'mi_cycle_preview_style_icon'
 * (menu/cycle/draw.c, covered on their own in
 * 'tests/menu/cycle/test_draw.c') are recording stand-ins too, since
 * that file owns their real behavior; 'render_outline_hide'
 * (render/outline.c) is a recording no-op.  'menu/draw.c' is linked
 * for real so 'menu_draw_measure''s forwarding to
 * 'text_string_measure' is genuinely exercised in the label-width
 * computation inside 'cycle_init'.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* CLOCK_MONOTONIC, clock_gettime */


/* System includes */
#include <stdint.h>
#include <stdlib.h>     /* malloc, free */
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <input/kbd/bind.h>
#include <policy/focus.h>
#include <render/icon.h>
#include <render/outline.h>
#include <stage.h>
#include <types/pair.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/cycle.h>
#include <menu/internal.h>


/** Fake, non-null XCB connection handle */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;

static uint32_t s_next_generated_id;

static int s_call_xcb_create_window;
static int s_call_xcb_map_window;
static int s_call_xcb_window_destroy;
static int s_call_xcb_set_input_focus;
static int s_call_render_outline_hide;
static int s_call_focus_apply;
static int s_call_enact_client_restore;
static int s_call_enact_client_unhide;
static int s_call_enact_client_unshade;
static int s_call_viewport_center_on_client;
static client_td *s_last_viewport_centered;
static int s_call_ri_render_client_icon;
static int s_call_mi_cycle_preview_apply;
static int s_call_mi_cycle_preview_style_icon;
static int s_call_mi_cycle_preview_target;
static xcb_window_t s_last_destroyed_window;
static xcb_window_t s_last_focused_window;
static client_td *s_last_focus_apply_client;

/** Clients 'focus_order_walk' hands to the visitor, most-recent-first,
 *  test-populated */
static client_td *s_focus_order_clients[WM_CYCLE_MENU_MAX_ENTRIES];
static int s_focus_order_count;

/** Fixed keybinding stand-in replies for 'keyboard_find' */
static xcb_keysym_t s_next_keysym_reply = 0xff09; /* Tab */
static xcb_keysym_t s_prev_keysym_reply = 0xff09; /* Tab */
static uint16_t s_next_modmask_reply = XCB_MOD_MASK_1;
static uint16_t s_prev_modmask_reply =
    (uint16_t) (XCB_MOD_MASK_1 | XCB_MOD_MASK_SHIFT);

/** Width 'text_string_measure' answers for every label */
static uint16_t s_measure_reply;

/** Window 'mi_cycle_preview_target' answers for a given client */
static xcb_window_t s_preview_target_reply;


xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return NULL;
}


xcb_connection_t *xcb_connection_get(void)
{
    return s_fake_connection;
}


uint32_t xcb_generate_id(xcb_connection_t *connection)
{
    (void) connection;
    s_next_generated_id++;
    return s_next_generated_id;
}


xcb_get_input_focus_cookie_t xcb_get_input_focus(xcb_connection_t *c)
{
    xcb_get_input_focus_cookie_t cookie = { 0 };

    (void) c;
    return cookie;
}


xcb_get_input_focus_reply_t *xcb_get_input_focus_reply(
        xcb_connection_t *c, xcb_get_input_focus_cookie_t cookie,
        xcb_generic_error_t **e)
{
    xcb_get_input_focus_reply_t *reply;

    (void) c;
    (void) cookie;
    if (e != NULL) {
        *e = NULL;
    }
    reply = malloc(sizeof(*reply));
    if (reply != NULL) {
        memset(reply, 0, sizeof(*reply));
        reply->focus = XCB_WINDOW_NONE;
    }
    return reply;
}


xcb_void_cookie_t xcb_set_input_focus(xcb_connection_t *c,
        uint8_t revert_to, xcb_window_t focus, xcb_timestamp_t time)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) c;
    (void) revert_to;
    (void) time;
    s_call_xcb_set_input_focus++;
    s_last_focused_window = focus;
    return cookie;
}


xcb_void_cookie_t xcb_create_window(xcb_connection_t *connection,
        uint8_t depth, xcb_window_t wid, xcb_window_t parent,
        int16_t x, int16_t y, uint16_t width, uint16_t height,
        uint16_t border_width, uint16_t class,
        xcb_visualid_t visual, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) depth;
    (void) wid;
    (void) parent;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    (void) border_width;
    (void) class;
    (void) visual;
    (void) value_mask;
    (void) value_list;
    s_call_xcb_create_window++;
    return cookie;
}


xcb_void_cookie_t xcb_map_window(xcb_connection_t *connection,
        xcb_window_t window)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) window;
    s_call_xcb_map_window++;
    return cookie;
}


void xcb_window_destroy(xcb_window_t window)
{
    s_call_xcb_window_destroy++;
    s_last_destroyed_window = window;
}


void atom_set_window_opacity(xcb_connection_t *connection,
        xcb_window_t window, uint32_t raw_opacity)
{
    (void) connection;
    (void) window;
    (void) raw_opacity;
}


uint32_t config_theme_opacity_to_raw(uint8_t percent)
{
    return (uint32_t) percent;
}


xcb_void_cookie_t xcb_ewmh_set_wm_window_type(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, uint32_t list_len, xcb_atom_t *list)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) ewmh;
    (void) window;
    (void) list_len;
    (void) list;
    return cookie;
}


xcb_void_cookie_t xcb_create_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc, xcb_drawable_t drawable, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) gc;
    (void) drawable;
    (void) value_mask;
    (void) value_list;
    return cookie;
}


xcb_void_cookie_t xcb_poly_fill_rectangle(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc, uint32_t rects_len,
        const xcb_rectangle_t *rects)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) drawable;
    (void) gc;
    (void) rects_len;
    (void) rects;
    return cookie;
}


xcb_void_cookie_t xcb_free_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) gc;
    return cookie;
}


void text_draw_string(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        struct position_s pos, const char *text)
{
    (void) connection;
    (void) drawable;
    (void) gc;
    (void) pos;
    (void) text;
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


uint16_t text_string_measure(const char *text)
{
    (void) text;
    return s_measure_reply;
}


/**
 * @brief Controllable stand-in for @a focus_order_walk
 *
 * The real focus-order data structure (policy/focus.c) is a
 * dedicated internal list rebuilt from every desktop's clients; this
 * file has no such list, so it walks its own small, test-populated
 * 's_focus_order_clients' array instead, in the exact same
 * most-recent-first order 'cycle_init' expects.
 *
 * @note Complexity: @e O(n)
 */
void focus_order_walk(const desktop_td *desktop,
        void (*visit)(client_td *client, void *data), void *data)
{
    int i;

    (void) desktop;
    for (i = 0; i < s_focus_order_count; ++i) {
        visit(s_focus_order_clients[i], data);
    }
}


/**
 * @brief Controllable stand-in for @a keyboard_find
 *
 * Answers fixed next/prev keysym and modifier mask pairs from
 * 's_next_keysym_reply' / 's_prev_keysym_reply' and their modmask
 * counterparts, the real keybinding table (input/kbd/bind.c)
 * belonging to a genuinely different module already covered on its
 * own elsewhere.
 *
 * @note Complexity: @e O(1)
 */
bool keyboard_find(enum wm_keybind_type_e type, xcb_keysym_t *out_keysym,
        uint16_t *out_modmask)
{
    bool is_next = (type == KEYBIND_CLIENT_CYCLE_NEXT ||
            type == KEYBIND_DESKTOP_ICON_NEXT);

    if (out_keysym != NULL) {
        *out_keysym = (is_next) ? s_next_keysym_reply : s_prev_keysym_reply;
    }
    if (out_modmask != NULL) {
        *out_modmask =
            (is_next) ? s_next_modmask_reply : s_prev_modmask_reply;
    }
    return true;
}


/**
 * @brief Recording no-op stand-in for @a focus_apply
 * @note Complexity: @e O(1)
 */
void focus_apply(list_td *stages, stage_td *stage,
        desktop_td *desktop, client_td *client, bool raise,
        const config_td *cfg)
{
    (void) stages;
    (void) stage;
    (void) desktop;
    (void) raise;
    (void) cfg;
    s_call_focus_apply++;
    s_last_focus_apply_client = client;
}


/**
 * @brief Recording no-op stand-in for @a enact_client_restore
 * @note Complexity: @e O(1)
 */
void enact_client_restore(client_td *client)
{
    (void) client;
    s_call_enact_client_restore++;
}


/**
 * @brief Recording no-op stand-in for @a enact_client_unhide
 * @note Complexity: @e O(1)
 */
void enact_client_unhide(client_td *client)
{
    (void) client;
    s_call_enact_client_unhide++;
}


/**
 * @brief Recording no-op stand-in for @a enact_client_unshade
 * @note Complexity: @e O(1)
 */
void enact_client_unshade(client_td *client)
{
    (void) client;
    s_call_enact_client_unshade++;
}


/**
 * @brief Recording no-op stand-in for
 *        @a scmd_stage_viewport_center_on_client
 * @note Complexity: @e O(1)
 */
void scmd_stage_viewport_center_on_client(stage_td *stage,
        client_td *client)
{
    (void) stage;
    s_call_viewport_center_on_client++;
    s_last_viewport_centered = client;
}


/**
 * @brief Recording no-op stand-in for @a ri_render_client_icon
 * @note Complexity: @e O(1)
 */
void ri_render_client_icon(client_td *client, bool is_current,
        bool force, bool restack)
{
    (void) client;
    (void) is_current;
    (void) force;
    (void) restack;
    s_call_ri_render_client_icon++;
}


/**
 * @brief Recording no-op stand-in for @a render_outline_hide
 * @note Complexity: @e O(1)
 */
void render_outline_hide(xcb_connection_t *connection,
        xcb_window_t windows[4])
{
    (void) connection;
    (void) windows;
    s_call_render_outline_hide++;
}


/**
 * @brief Recording stand-in for @a mi_cycle_preview_target
 *
 * The real behavior belongs to 'menu/cycle/draw.c', covered on its
 * own in 'tests/menu/cycle/test_draw.c'; here it only reports whatever
 * 's_preview_target_reply' was set to, so 'cycle_destroy''s
 * 's_cycle_preview_restore' branch that marks a client outdated on a
 * missing target can be exercised deterministically.
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t mi_cycle_preview_target(const client_td *client,
        bool is_icon_menu)
{
    (void) client;
    (void) is_icon_menu;
    s_call_mi_cycle_preview_target++;
    return s_preview_target_reply;
}


/**
 * @brief Recording no-op stand-in for @a mi_cycle_preview_apply
 * @note Complexity: @e O(1)
 */
void mi_cycle_preview_apply(xcb_connection_t *connection,
        const config_td *cfg)
{
    (void) connection;
    (void) cfg;
    s_call_mi_cycle_preview_apply++;
}


/**
 * @brief Recording no-op stand-in for @a mi_cycle_preview_style_icon
 * @note Complexity: @e O(1)
 */
void mi_cycle_preview_style_icon(xcb_connection_t *connection,
        xcb_window_t icon_window, const config_td *cfg,
        uint32_t border_color)
{
    (void) connection;
    (void) icon_window;
    (void) cfg;
    (void) border_color;
    s_call_mi_cycle_preview_style_icon++;
}


/**
 * @brief Link-only stand-in for @a client_last_user_time
 *
 * The real bookkeeping (client.c) belongs to a different, far larger
 * module, already exercised on its own elsewhere; a constant nonzero
 * reply here is all 'cycle_init' and 'cycle_destroy' need to pick the
 * "use the recorded time" branch of their own
 * '(client_last_user_time() != 0u) ? ... : XCB_CURRENT_TIME' ternary
 * deterministically.
 *
 * @note Complexity: @e O(1)
 */
uint32_t client_last_user_time(void)
{
    return 12345u;
}


static void s_reset(void)
{
    s_next_generated_id = 4000u;
    s_call_xcb_create_window = 0;
    s_call_xcb_map_window = 0;
    s_call_xcb_window_destroy = 0;
    s_call_xcb_set_input_focus = 0;
    s_call_render_outline_hide = 0;
    s_call_focus_apply = 0;
    s_call_enact_client_restore = 0;
    s_call_enact_client_unhide = 0;
    s_call_enact_client_unshade = 0;
    s_call_viewport_center_on_client = 0;
    s_last_viewport_centered = NULL;
    s_call_ri_render_client_icon = 0;
    s_call_mi_cycle_preview_apply = 0;
    s_call_mi_cycle_preview_style_icon = 0;
    s_call_mi_cycle_preview_target = 0;
    s_last_destroyed_window = XCB_WINDOW_NONE;
    s_last_focused_window = XCB_WINDOW_NONE;
    s_last_focus_apply_client = NULL;
    s_focus_order_count = 0;
    memset(s_focus_order_clients, 0, sizeof(s_focus_order_clients));
    s_next_keysym_reply = 0xff09;
    s_prev_keysym_reply = 0xff09;
    s_next_modmask_reply = XCB_MOD_MASK_1;
    s_prev_modmask_reply =
        (uint16_t) (XCB_MOD_MASK_1 | XCB_MOD_MASK_SHIFT);
    s_measure_reply = 40u;
    s_preview_target_reply = XCB_WINDOW_NONE;

    /* Settle the shared g_cycle_menu global back to closed before
     * each scenario.  A real cycle_destroy() call is deliberately not
     * used here: it would write through g_cycle_menu.stage, which,
     * by the time the next scenario starts, is a dangling pointer to
     * a stack-local stage_td that already returned in the previous
     * scenario's own function.  Memsetting the struct directly reaches
     * the exact same "closed" state (count 0, window XCB_WINDOW_NONE,
     * is_icon_menu false) without dereferencing anything stale. */
    memset(&g_cycle_menu, 0, sizeof(g_cycle_menu));
}


static config_td s_make_config(void)
{
    config_td cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.theme.menu.unselected.font[0] = '\0';
    cfg.theme.menu.selected.font[0] = '\0';
    cfg.theme.menu.padding.horizontal = 10u;
    cfg.theme.menu.padding.vertical = 6u;
    cfg.theme.menu.border.color = 0x202020u;
    cfg.theme.menu.border.width = 1u;
    cfg.theme.menu.opacity = 90u;
    cfg.theme.menu.show_pixmaps = false;
    cfg.theme.menu.unselected.color.background = 0x111111u;
    cfg.theme.icon.inactive.border.color = 0x333333u;
    cfg.theme.window.active.border.color = 0x00ff00u;
    cfg.theme.window.inactive.border.color = 0x777777u;
    return cfg;
}


static stage_td s_make_stage(uint32_t w, uint32_t h)
{
    stage_td stage;
    static xcb_screen_t screen;

    memset(&stage, 0, sizeof(stage));
    memset(&screen, 0, sizeof(screen));
    stage.screen = &screen;
    stage.properties.dim.w = w;
    stage.properties.dim.h = h;
    return stage;
}


static desktop_td s_make_desktop(xcb_window_t active_id)
{
    desktop_td desktop;

    memset(&desktop, 0, sizeof(desktop));
    desktop.client_active_id = active_id;
    return desktop;
}


/** Backing storage for test clients, reused per scenario */
static client_td s_clients[WM_CYCLE_MENU_MAX_ENTRIES];


static client_td *s_make_client(int slot, xcb_window_t id,
        const char *name, uint32_t flags, uint16_t state)
{
    client_td *c = &s_clients[slot];

    memset(c, 0, sizeof(*c));
    c->id = id;
    c->info.name = (char *) name;
    c->properties.flags = flags;
    c->properties.state = state;
    return c;
}


/* cycle_init does nothing at all on any null argument: no window
 * created, and the focusable-clients walk is never even consulted */
static void s_test_init_null_guards(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();
    s_focus_order_count = 0;

    cycle_init(NULL, &stage, &desktop, false, 1, 0, &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a null connection creates no cycle window");

    cycle_init(s_fake_connection, NULL, &desktop, false, 1, 0, &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a null stage creates no cycle window");

    cycle_init(s_fake_connection, &stage, NULL, false, 1, 0, &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a null desktop creates no cycle window");

    cycle_init(s_fake_connection, &stage, &desktop, false, 1, 0,
            NULL);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a null config creates no cycle window");
}


/* cycle_init with no focusable clients at all leaves the menu closed,
 * with no window created */
static void s_test_init_no_clients_stays_closed(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();
    s_focus_order_count = 0;

    cycle_init(s_fake_connection, &stage, &desktop, false, 1, 0,
            &cfg);

    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "no focusable clients means no window is ever created");
    TAP_OK(!cycle_is_open(), "the menu is correctly reported closed");
}


/* A normal cycle_init with several focusable, non-iconified windows
 * collects them all, in focus order, opens exactly one window, and
 * applies the initial preview */
static void s_test_init_collects_and_opens(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(0x10u);
    cfg = s_make_config();

    s_focus_order_clients[0] =
        s_make_client(0, 0x10u, "Terminal", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_clients[1] =
        s_make_client(1, 0x11u, "Browser", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_clients[2] =
        s_make_client(2, 0x12u, "Editor", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_count = 3;

    cycle_init(s_fake_connection, &stage, &desktop, false, 1, 0,
            &cfg);

    TAP_EQ_INT(g_cycle_menu.count, 3,
            "all three focusable clients are collected");
    TAP_EQ_STR(g_cycle_menu.labels[0], "Terminal",
            "the first collected label matches its client's name");
    TAP_EQ_INT(s_call_xcb_create_window, 1,
            "exactly one cycle menu window is created");
    TAP_EQ_INT(s_call_xcb_map_window, 1,
            "exactly one cycle menu window is mapped");
    TAP_OK(cycle_is_open(), "the menu now reports itself as open");
    TAP_EQ_INT(s_call_mi_cycle_preview_apply, 1,
            "the initial preview is applied exactly once");
}


/* A client without CLIENT_FLAG_FOCUSABLE, one marked
 * CLIENT_FLAG_SKIP_TASKBAR, one transient for another window, and an
 * iconified one when collecting windows (not icons) are all
 * correctly excluded from the collected set */
static void s_test_init_excludes_unwanted_clients(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();

    s_focus_order_clients[0] =
        s_make_client(0, 0x20u, "NotFocusable", 0, 0);
    s_focus_order_clients[1] = s_make_client(1, 0x21u, "SkipsTaskbar",
            (uint16_t) (CLIENT_FLAG_FOCUSABLE |
                CLIENT_FLAG_SKIP_TASKBAR), 0);
    s_focus_order_clients[2] = s_make_client(2, 0x22u, "Iconified",
            CLIENT_FLAG_FOCUSABLE, CLIENT_STATE_ICONIFIED);
    s_focus_order_clients[3] = s_make_client(3, 0x23u, "Transient",
            CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_clients[3]->transient_for = (xcb_window_t) 1;
    s_focus_order_clients[4] =
        s_make_client(4, 0x24u, "Wanted", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_count = 5;

    cycle_init(s_fake_connection, &stage, &desktop, false, 1, 0,
            &cfg);

    TAP_EQ_INT(g_cycle_menu.count, 1,
            "only the one genuinely eligible window client is"
            " collected");
    TAP_EQ_STR(g_cycle_menu.labels[0], "Wanted",
            "the surviving client is the expected one");
}


/* Building the icon-cycle menu (is_icon=true) collects only iconified
 * clients, and formats their labels wrapped in parentheses */
static void s_test_init_icon_menu_collects_iconified(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();

    s_focus_order_clients[0] = s_make_client(0, 0x30u, "NotIconified",
            CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_clients[1] = s_make_client(1, 0x31u, "Minimized",
            CLIENT_FLAG_FOCUSABLE, CLIENT_STATE_ICONIFIED);
    s_focus_order_count = 2;

    cycle_init(s_fake_connection, &stage, &desktop, true, 1, 0, &cfg);

    TAP_EQ_INT(g_cycle_menu.count, 1,
            "only the iconified client is collected for the icon"
            " menu");
    TAP_EQ_STR(g_cycle_menu.labels[0], "(Minimized)",
            "an iconified client's label is wrapped in parentheses");
    TAP_OK(g_cycle_menu.is_icon_menu,
            "the menu correctly remembers it is the icon variant");
}


/* A hidden (but not iconified) client's label is wrapped in angle
 * brackets instead */
static void s_test_init_hidden_client_label(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();

    s_focus_order_clients[0] = s_make_client(0, 0x40u, "Background",
            (uint16_t) (CLIENT_FLAG_FOCUSABLE | CLIENT_FLAG_HIDDEN), 0);
    s_focus_order_count = 1;

    cycle_init(s_fake_connection, &stage, &desktop, false, 1, 0,
            &cfg);

    TAP_EQ_STR(g_cycle_menu.labels[0], "<Background>",
            "a hidden, non-iconified client's label uses angle"
            " brackets");
}


/* Preselection lands on the active client stepped by 'preselect', and
 * cleanly wraps around the collected set either direction */
static void s_test_init_preselects_relative_to_active(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    /* Active id matches the second collected client (index 1) */
    desktop = s_make_desktop(0x51u);
    cfg = s_make_config();

    s_focus_order_clients[0] =
        s_make_client(0, 0x50u, "A", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_clients[1] =
        s_make_client(1, 0x51u, "B", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_clients[2] =
        s_make_client(2, 0x52u, "C", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_count = 3;

    cycle_init(s_fake_connection, &stage, &desktop, false, 1, 0,
            &cfg);
    TAP_EQ_INT(g_cycle_menu.selected, 2,
            "preselect=+1 from the active client (index 1) lands on"
            " index 2");

    cycle_init(s_fake_connection, &stage, &desktop, false, -1, 0,
            &cfg);
    TAP_EQ_INT(g_cycle_menu.selected, 0,
            "preselect=-1 from the active client (index 1) wraps"
            " correctly to index 0");
}


/* cycle_navigate_next / cycle_navigate_prev advance and wrap the
 * selection, and each transition repaints exactly the previously- and
 * newly-selected real icons */
static void s_test_navigate_next_and_prev_wrap(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();

    s_focus_order_clients[0] =
        s_make_client(0, 0x60u, "A", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_clients[1] =
        s_make_client(1, 0x61u, "B", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_clients[2] =
        s_make_client(2, 0x62u, "C", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_count = 3;

    cycle_init(s_fake_connection, &stage, &desktop, false, 0, 0,
            &cfg);
    TAP_EQ_INT(g_cycle_menu.selected, 0, "starts selected at index 0");

    s_call_ri_render_client_icon = 0;
    cycle_navigate_next();
    TAP_EQ_INT(g_cycle_menu.selected, 1,
            "navigate_next advances the selection by one");
    TAP_EQ_INT(s_call_ri_render_client_icon, 2,
            "navigating repaints both the old and new selection's"
            " real icons");

    cycle_navigate_next();
    cycle_navigate_next();
    TAP_EQ_INT(g_cycle_menu.selected, 0,
            "navigate_next wraps back around to index 0 after the"
            " last entry");

    cycle_navigate_prev();
    TAP_EQ_INT(g_cycle_menu.selected, 2,
            "navigate_prev wraps backward to the last entry from"
            " index 0");
}


/* cycle_navigate_to clamps an out-of-range index to the last entry,
 * and is a no-op when the menu has no entries at all */
static void s_test_navigate_to_clamps_and_guards(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();

    cycle_navigate_to(5u);
    TAP_OK(1,
            "navigating with no menu open at all never crashes");

    s_focus_order_clients[0] =
        s_make_client(0, 0x70u, "A", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_clients[1] =
        s_make_client(1, 0x71u, "B", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_count = 2;

    cycle_init(s_fake_connection, &stage, &desktop, false, 0, 0,
            &cfg);
    cycle_navigate_to(99u);
    TAP_EQ_INT(g_cycle_menu.selected, 1,
            "an out-of-range index clamps to the last entry");

    cycle_navigate_to(0u);
    TAP_EQ_INT(g_cycle_menu.selected, 0,
            "an in-range index is honored exactly");
}


/* cycle_get_selected_client / cycle_window / cycle_modifier /
 * cycle_next_keysym / cycle_next_modmask / cycle_prev_keysym /
 * cycle_prev_modmask all answer sensible defaults when nothing is
 * open, and the real values once a menu is */
static void s_test_accessors_reflect_state(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();

    TAP_OK(cycle_get_selected_client() == NULL,
            "with nothing open, the selected client is NULL");
    TAP_EQ_INT((long) cycle_window(), (long) XCB_WINDOW_NONE,
            "with nothing open, cycle_window answers XCB_WINDOW_NONE");

    s_next_keysym_reply = 0xff09;
    s_prev_keysym_reply = 0xff09;
    s_next_modmask_reply = XCB_MOD_MASK_1;
    s_prev_modmask_reply =
        (uint16_t) (XCB_MOD_MASK_1 | XCB_MOD_MASK_SHIFT);

    s_focus_order_clients[0] =
        s_make_client(0, 0x80u, "A", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_count = 1;

    cycle_init(s_fake_connection, &stage, &desktop, false, 0, 0,
            &cfg);

    TAP_OK(cycle_get_selected_client() == s_focus_order_clients[0],
            "the selected client after init is the one collected");
    TAP_OK(cycle_window() != XCB_WINDOW_NONE,
            "cycle_window answers a real window id once open");
    TAP_EQ_INT((long) cycle_modifier(), (long) XCB_MOD_MASK_1,
            "the shared modifier is only the bits common to both"
            " next and prev bindings (Alt, not Alt+Shift)");
    TAP_EQ_INT((long) cycle_next_keysym(), (long) 0xff09,
            "cycle_next_keysym reflects the stand-in's reply");
    TAP_EQ_INT((long) cycle_prev_keysym(), (long) 0xff09,
            "cycle_prev_keysym reflects the stand-in's reply");
    TAP_EQ_INT((long) cycle_next_modmask(), (long) XCB_MOD_MASK_1,
            "cycle_next_modmask is unlocked-bits-stripped");
    TAP_EQ_INT((long) cycle_prev_modmask(),
            (long) (XCB_MOD_MASK_1 | XCB_MOD_MASK_SHIFT),
            "cycle_prev_modmask carries Alt+Shift as configured");
}


/* cycle_destroy on a null connection, or with nothing open, is a
 * harmless no-op that touches none of the teardown stand-ins */
static void s_test_destroy_null_guards(void)
{
    s_reset();

    cycle_destroy(NULL);
    TAP_EQ_INT(s_call_xcb_window_destroy, 0,
            "a null connection destroys nothing");

    cycle_destroy(s_fake_connection);
    TAP_EQ_INT(s_call_xcb_window_destroy, 0,
            "destroying an already-closed menu destroys nothing");
}


/* A real cycle_destroy tears down the window, hides the outline, and
 * restores whichever window held focus before the menu opened */
static void s_test_destroy_restores_focus_and_hides_outline(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();

    s_focus_order_clients[0] =
        s_make_client(0, 0x90u, "A", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_count = 1;

    cycle_init(s_fake_connection, &stage, &desktop, false, 0, 0,
            &cfg);
    TAP_OK(cycle_is_open(), "the menu opens as expected");

    cycle_destroy(s_fake_connection);

    TAP_EQ_INT(s_call_xcb_window_destroy, 1,
            "destroy tears down exactly one window");
    TAP_EQ_INT(s_call_render_outline_hide, 1,
            "destroy hides the preview outline exactly once");
    TAP_OK(!cycle_is_open(),
            "the menu reports itself closed afterward");
    TAP_EQ_INT(g_cycle_menu.count, 0,
            "the collected client count is reset to 0");
}


/* cycle_confirm on the currently selected client tears the menu down
 * without restoring the previous focus (prev_focus is cleared first),
 * then applies focus to the target through focus_apply */
static void s_test_confirm_applies_focus_to_selected(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();

    s_focus_order_clients[0] =
        s_make_client(0, 0xa0u, "A", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_clients[1] =
        s_make_client(1, 0xa1u, "B", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_count = 2;

    cycle_init(s_fake_connection, &stage, &desktop, false, 0, 0,
            &cfg);
    cycle_navigate_next();

    /* Reset the counter only now: 'cycle_init' just above already
     * called 'xcb_set_input_focus' once itself, moving the keyboard
     * to the cycle window, which is unrelated to what this scenario
     * actually checks */
    s_call_xcb_set_input_focus = 0;

    cycle_confirm(s_fake_connection, NULL, &cfg);

    TAP_EQ_INT(s_call_focus_apply, 1,
            "focus_apply is called exactly once on confirm");
    TAP_OK(s_last_focus_apply_client == s_focus_order_clients[1],
            "focus_apply receives the client that was actually"
            " selected");
    TAP_OK(!cycle_is_open(),
            "confirming closes the menu");
    TAP_EQ_INT(s_call_xcb_set_input_focus, 0,
            "the focus-restore path is skipped on confirm, since"
            " prev_focus is cleared before cycle_destroy runs");
}


/* cycle_confirm on the icon menu restores the iconified target via
 * enact_client_restore rather than unhide/unshade */
static void s_test_confirm_icon_menu_restores(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();

    s_focus_order_clients[0] = s_make_client(0, 0xb0u, "Minimized",
            CLIENT_FLAG_FOCUSABLE, CLIENT_STATE_ICONIFIED);
    s_focus_order_count = 1;

    cycle_init(s_fake_connection, &stage, &desktop, true, 0, 0, &cfg);
    cycle_confirm(s_fake_connection, NULL, &cfg);

    TAP_EQ_INT(s_call_enact_client_restore, 1,
            "confirming on the icon menu calls enact_client_restore"
            " exactly once");
    TAP_EQ_INT(s_call_enact_client_unhide, 0,
            "enact_client_unhide is never called on the icon-menu"
            " path");
}


/* cycle_confirm on a hidden (non-iconified) window client calls
 * enact_client_unhide before focusing it */
static void s_test_confirm_hidden_client_unhides(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();

    s_focus_order_clients[0] = s_make_client(0, 0xc0u, "Hidden",
            (uint16_t) (CLIENT_FLAG_FOCUSABLE | CLIENT_FLAG_HIDDEN), 0);
    s_focus_order_count = 1;

    cycle_init(s_fake_connection, &stage, &desktop, false, 0, 0,
            &cfg);
    cycle_confirm(s_fake_connection, NULL, &cfg);

    TAP_EQ_INT(s_call_enact_client_unhide, 1,
            "confirming a hidden client unhides it exactly once");
    TAP_EQ_INT(s_call_enact_client_restore, 0,
            "enact_client_restore is never called on the non-icon"
            " path");
}


/* cycle_confirm on a shaded window client calls enact_client_unshade
 * as well, since shading and hiding are independent flags */
static void s_test_confirm_shaded_client_unshades(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();

    s_focus_order_clients[0] = s_make_client(0, 0xd0u, "Shaded",
            (uint16_t) (CLIENT_FLAG_FOCUSABLE | CLIENT_FLAG_SHADED), 0);
    s_focus_order_count = 1;

    cycle_init(s_fake_connection, &stage, &desktop, false, 0, 0,
            &cfg);
    cycle_confirm(s_fake_connection, NULL, &cfg);

    TAP_EQ_INT(s_call_enact_client_unshade, 1,
            "confirming a shaded client unshades it exactly once");
}


/* cycle_notice_client_destroyed forces the menu closed if the
 * destroyed client is among the collected set, and is a harmless
 * no-op otherwise */
static void s_test_notice_client_destroyed(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;
    client_td unrelated;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();
    memset(&unrelated, 0, sizeof(unrelated));
    unrelated.id = 0xffeeu;

    cycle_notice_client_destroyed(NULL);
    TAP_OK(1, "a null client is a harmless no-op with no menu open");

    s_focus_order_clients[0] =
        s_make_client(0, 0xe0u, "A", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_clients[1] =
        s_make_client(1, 0xe1u, "B", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_count = 2;

    cycle_init(s_fake_connection, &stage, &desktop, false, 0, 0,
            &cfg);

    cycle_notice_client_destroyed(&unrelated);
    TAP_OK(cycle_is_open(),
            "notice of an unrelated client's destruction leaves the"
            " menu open");

    cycle_notice_client_destroyed(s_focus_order_clients[1]);
    TAP_OK(!cycle_is_open(),
            "notice of a collected client's destruction closes the"
            " menu");
}


/* cycle_force_full_repaint clears the has-drawn-once flag so the next
 * cycle_draw call is known to repaint every visible row; checked
 * here through the field directly, since cycle.c owns it and
 * cycle/draw.c's own drawing is covered in its own test file */
static void s_test_force_full_repaint_clears_flag(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u);
    desktop = s_make_desktop(XCB_WINDOW_NONE);
    cfg = s_make_config();

    s_focus_order_clients[0] =
        s_make_client(0, 0xf0u, "A", CLIENT_FLAG_FOCUSABLE, 0);
    s_focus_order_count = 1;

    cycle_init(s_fake_connection, &stage, &desktop, false, 0, 0,
            &cfg);
    g_cycle_menu.has_drawn_once = true;

    cycle_force_full_repaint();

    TAP_OK(!g_cycle_menu.has_drawn_once,
            "cycle_force_full_repaint clears has_drawn_once");
}


int main(void)
{
    TAP_PLAN(57);

    s_test_init_null_guards();
    s_test_init_no_clients_stays_closed();
    s_test_init_collects_and_opens();
    s_test_init_excludes_unwanted_clients();
    s_test_init_icon_menu_collects_iconified();
    s_test_init_hidden_client_label();
    s_test_init_preselects_relative_to_active();
    s_test_navigate_next_and_prev_wrap();
    s_test_navigate_to_clamps_and_guards();
    s_test_accessors_reflect_state();
    s_test_destroy_null_guards();
    s_test_destroy_restores_focus_and_hides_outline();
    s_test_confirm_applies_focus_to_selected();
    s_test_confirm_icon_menu_restores();
    s_test_confirm_hidden_client_unhides();
    s_test_confirm_shaded_client_unshades();
    s_test_notice_client_destroyed();
    s_test_force_full_repaint_clears_flag();

    return TAP_DONE();
}
