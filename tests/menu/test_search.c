/**
 * @file tests/menu/test_search.c
 *
 * @brief Test battery for the fuzzy window-search widget
 *
 * s_search (declared file-static inside menu/search.c) is reached only
 * through the public API this file exercises directly: search_init,
 * search_destroy, search_is_open, search_window, search_handle_keypress,
 * search_handle_click, search_handle_motion and search_draw.  Every raw
 * XCB entry point search.c calls, every project-level XCB wrapper, the
 * text renderer, the menu drawing primitives, the stacking walk, and
 * the enact/focus/desktop helpers it reaches on confirm are all stubbed
 * below as controllable, call-recording stand-ins, so every branch runs
 * without a real X server, a real window manager, or a real font.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/ohtbl.h>

/* Local includes */
#include <client.h>
#include <config.h>
#include <defs/search.h>
#include <desktop.h>
#include <harness/tap.h>
#include <menu/dialog/message.h>
#include <menu/search.h>
#include <policy/stacking.h>
#include <surface.h>


/** Shared open-addressed-hash-table callbacks for every fixture
 *  desktop's 'clients' table, keying purely off 'client_td.id', the
 *  same pattern 'tests/menu/context/test_winlist.c' and
 *  'tests/systray/test_layout.c' both already established */
static size_t s_id_hash1(const void *key)
{
    return (size_t) ((const client_td *) key)->id;
}

static size_t s_id_hash2(const void *key)
{
    (void) key;
    return 1u;
}

static bool s_id_match(const void *key1, const void *key2)
{
    return ((const client_td *) key1)->id ==
        ((const client_td *) key2)->id;
}


/* Controllable stand-in state */

static xcb_connection_t *s_connection_stub = (xcb_connection_t *) 1;
static xcb_ewmh_connection_t *s_ewmh_stub = NULL;

static uint32_t s_next_id = 1000u;
static xcb_window_t s_created_window = XCB_WINDOW_NONE;
static xcb_window_t s_created_parent = XCB_WINDOW_NONE;
static int s_create_window_calls = 0;
static int s_map_window_calls = 0;
static int s_grab_keyboard_calls = 0;
static xcb_window_t s_grab_keyboard_window = XCB_WINDOW_NONE;
static int s_set_input_focus_calls = 0;
static xcb_window_t s_set_input_focus_window = XCB_WINDOW_NONE;
static uint8_t s_set_input_focus_revert_to = 0u;
static int s_ungrab_keyboard_calls = 0;
static int s_window_destroy_calls = 0;
static xcb_window_t s_window_destroyed = XCB_WINDOW_NONE;
static int s_configure_window_calls = 0;

/** What 'xcb_get_input_focus_reply' hands back to 'search_init'; a test
 *  sets this before calling search_init to control 's_search.prev_focus'
 */
static xcb_window_t s_focus_reply_focus = XCB_WINDOW_NONE;
static bool s_focus_reply_is_null = false;

static uint32_t s_last_user_time_stub = 0u;

static int s_dialog_info_show_calls = 0;
static menu_msg_level_e s_dialog_info_show_level = MENU_MSG_LEVEL_NONE;

static int s_client_restore_calls = 0;
static client_td *s_client_restore_last = NULL;
static int s_client_unhide_calls = 0;
static client_td *s_client_unhide_last = NULL;
static int s_client_unshade_calls = 0;
static client_td *s_client_unshade_last = NULL;
static int s_desktop_switch_calls = 0;
static uint32_t s_desktop_switch_last_id = 0u;
static int s_focus_apply_calls = 0;
static client_td *s_focus_apply_last_client = NULL;
static desktop_td *s_focus_apply_last_desktop = NULL;

/** Desktops this file's own 'surface_desktop_get' stand-in answers
 *  from, registered by 's_make_desktop', the same pattern
 *  'test_winlist.c' uses for its own stand-in of the same function */
#define MAX_TEST_DESKTOPS (4)
static desktop_td *s_desktops_by_id[MAX_TEST_DESKTOPS];
static int s_desktops_registered;

static int s_menu_draw_row_bg_calls = 0;
static int s_menu_draw_label_calls = 0;
static int s_menu_draw_truncate_calls = 0;
static int s_text_use_font_calls = 0;
static int s_text_set_color_calls = 0;
static int s_wmicon_draw_at_calls = 0;


/* Raw XCB stand-ins */

uint32_t xcb_generate_id(xcb_connection_t *c)
{
    (void) c;
    return s_next_id++;
}

xcb_void_cookie_t xcb_create_window(xcb_connection_t *c, uint8_t depth,
        xcb_window_t wid, xcb_window_t parent, int16_t x, int16_t y,
        uint16_t width, uint16_t height, uint16_t border_width,
        uint16_t class, xcb_visualid_t visual, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) c;
    (void) depth;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    (void) border_width;
    (void) class;
    (void) visual;
    (void) value_mask;
    (void) value_list;

    s_create_window_calls++;
    s_created_window = wid;
    s_created_parent = parent;
    return cookie;
}

xcb_void_cookie_t xcb_map_window(xcb_connection_t *c, xcb_window_t window)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) c;
    (void) window;
    s_map_window_calls++;
    return cookie;
}

xcb_grab_keyboard_cookie_t xcb_grab_keyboard(xcb_connection_t *c,
        uint8_t owner_events, xcb_window_t grab_window,
        xcb_timestamp_t time, uint8_t pointer_mode,
        uint8_t keyboard_mode)
{
    xcb_grab_keyboard_cookie_t cookie = { 0u };

    (void) c;
    (void) owner_events;
    (void) time;
    (void) pointer_mode;
    (void) keyboard_mode;

    s_grab_keyboard_calls++;
    s_grab_keyboard_window = grab_window;
    return cookie;
}

xcb_void_cookie_t xcb_set_input_focus(xcb_connection_t *c,
        uint8_t revert_to, xcb_window_t focus, xcb_timestamp_t time)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) c;
    (void) time;

    s_set_input_focus_calls++;
    s_set_input_focus_revert_to = revert_to;
    s_set_input_focus_window = focus;
    return cookie;
}

xcb_get_input_focus_cookie_t xcb_get_input_focus(xcb_connection_t *c)
{
    xcb_get_input_focus_cookie_t cookie = { 0u };

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

    if (s_focus_reply_is_null) {
        return NULL;
    }

    reply = calloc(1, sizeof(*reply));
    reply->focus = s_focus_reply_focus;
    return reply;
}

xcb_void_cookie_t xcb_ungrab_keyboard(xcb_connection_t *c,
        xcb_timestamp_t time)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) c;
    (void) time;
    s_ungrab_keyboard_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_configure_window(xcb_connection_t *c,
        xcb_window_t window, uint16_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) c;
    (void) window;
    (void) value_mask;
    (void) value_list;
    s_configure_window_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_ewmh_set_wm_window_type(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, uint32_t list_len, xcb_atom_t *list)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) ewmh;
    (void) window;
    (void) list_len;
    (void) list;
    return cookie;
}


/* 'utils/xcb/connection.h' stand-ins */

xcb_connection_t *xcb_connection_get(void)
{
    return s_connection_stub;
}

xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return s_ewmh_stub;
}


/* 'utils/xcb/window.h' stand-in */

void xcb_window_destroy(xcb_window_t window)
{
    s_window_destroy_calls++;
    s_window_destroyed = window;
}


/* 'client.h' stand-in */

uint32_t client_last_user_time(void)
{
    return s_last_user_time_stub;
}


/* 'menu/dialog/info.h' stand-in */

void dialog_info_show(xcb_connection_t *connection, surface_td *surface,
        const config_td *config, const char *message,
        menu_msg_level_e level)
{
    (void) connection;
    (void) surface;
    (void) config;
    (void) message;

    s_dialog_info_show_calls++;
    s_dialog_info_show_level = level;
}


/* 'policy/stacking.h' stand-in */

/** Test-controlled visitor invocation: walks whichever client array was
 *  registered for 'desktop' via 's_set_stacking' from its first entry
 *  to its last, since callers of 's_set_stacking' in this file always
 *  list their clients in top-of-stack-first order already, matching
 *  stacking_walk_down's own documented top-of-stack-first order
 *  directly */
#define MAX_STACKING_CLIENTS (WM_SEARCH_MAX_ENTRIES + 8)
static desktop_td *s_stacking_desktop[MAX_TEST_DESKTOPS];
static client_td *s_stacking_clients[MAX_TEST_DESKTOPS][MAX_STACKING_CLIENTS];
static int s_stacking_count[MAX_TEST_DESKTOPS];
static int s_stacking_registered;

static void s_set_stacking(desktop_td *desktop, client_td **clients,
        int count)
{
    int slot = s_stacking_registered;

    s_stacking_desktop[slot] = desktop;
    for (int i = 0; i < count; ++i) {
        s_stacking_clients[slot][i] = clients[i];
    }
    s_stacking_count[slot] = count;
    s_stacking_registered++;
}

void stacking_walk_down(const desktop_td *desktop,
        stacking_visitor_fn visit, void *data)
{
    for (int s = 0; s < s_stacking_registered; ++s) {
        if (s_stacking_desktop[s] != desktop) {
            continue;
        }
        for (int i = 0; i < s_stacking_count[s]; ++i) {
            visit(s_stacking_clients[s][i], data);
        }
        return;
    }
}


/* 'policy/focus.h' stand-in */

void focus_apply(list_td *surfaces, surface_td *surface,
        desktop_td *desktop, client_td *client, bool raise,
        const config_td *cfg)
{
    (void) surfaces;
    (void) surface;
    (void) raise;
    (void) cfg;

    s_focus_apply_calls++;
    s_focus_apply_last_client = client;
    s_focus_apply_last_desktop = desktop;
}


/* 'enact.h' stand-ins */

void enact_client_restore(client_td *client)
{
    s_client_restore_calls++;
    s_client_restore_last = client;
}

void enact_client_unhide(client_td *client)
{
    s_client_unhide_calls++;
    s_client_unhide_last = client;
}

void enact_client_unshade(client_td *client)
{
    s_client_unshade_calls++;
    s_client_unshade_last = client;
}

void enact_surface_desktop_switch(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;

    s_desktop_switch_calls++;
    s_desktop_switch_last_id = desktop_id;
}


/* 'surface.h' stand-ins */

desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;

    for (int i = 0; i < s_desktops_registered; ++i) {
        if (s_desktops_by_id[i] != NULL &&
                s_desktops_by_id[i]->id == (xcb_window_t) desktop_id) {
            return s_desktops_by_id[i];
        }
    }
    return NULL;
}

void surface_desktop_label(const surface_td *surface, uint32_t desktop_id,
        const char *desktop_name, bool is_pinned, bool shows_name,
        char *out_label, size_t length)
{
    (void) surface;
    (void) desktop_id;
    (void) is_pinned;
    (void) shows_name;

    if (out_label != NULL && length > 0u) {
        (void) snprintf(out_label, length, "%s",
                (desktop_name != NULL) ? desktop_name : "");
    }
}


/* 'menu/draw.h' stand-ins */

void menu_draw_row_bg(xcb_connection_t *connection, xcb_window_t window,
        uint32_t color, int16_t row_y, uint16_t row_h, uint16_t w)
{
    (void) connection;
    (void) window;
    (void) color;
    (void) row_y;
    (void) row_h;
    (void) w;
    s_menu_draw_row_bg_calls++;
}

void menu_draw_label(xcb_connection_t *connection, xcb_window_t window,
        struct position_s pos, const char *text)
{
    (void) connection;
    (void) window;
    (void) pos;
    (void) text;
    s_menu_draw_label_calls++;
}

uint16_t menu_draw_measure(const char *text)
{
    /* One pixel per character is enough for every geometry decision
     * these tests make (safe_right comparisons, hint placement); the
     * real glyph metrics are never what is under test here */
    return (uint16_t) ((text != NULL) ? strlen(text) : 0u);
}

void menu_draw_truncate(char *buf, uint16_t max_w)
{
    (void) max_w;
    (void) buf;
    s_menu_draw_truncate_calls++;
}


/* 'render/text.h' stand-ins */

int text_renderer_use_font(xcb_connection_t *connection,
        const char *font_name)
{
    (void) connection;
    (void) font_name;
    s_text_use_font_calls++;
    return 0;
}

void text_renderer_set_color(uint32_t fg, uint32_t bg)
{
    (void) fg;
    (void) bg;
    s_text_set_color_calls++;
}

int16_t text_font_ascent(void)
{
    return 10;
}

int16_t text_font_descent(void)
{
    return 3;
}


/* 'render/wmicon.h' stand-in */

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
    s_wmicon_draw_at_calls++;
}


/* Fixture builders */

/** Every client and desktop this file calloc's, freed in one place by
 *  's_teardown' rather than at each test's own end, the same ownership
 *  pattern 'test_winlist.c' uses */
#define MAX_TEST_CLIENTS (200)
static client_td *s_owned_clients[MAX_TEST_CLIENTS];
static int s_owned_clients_used;
static desktop_td *s_owned_desktops[MAX_TEST_DESKTOPS];
static int s_owned_desktops_used;

static xcb_screen_t s_fake_screen;

static void s_reset(void)
{
    s_next_id = 1000u;
    s_created_window = XCB_WINDOW_NONE;
    s_created_parent = XCB_WINDOW_NONE;
    s_create_window_calls = 0;
    s_map_window_calls = 0;
    s_grab_keyboard_calls = 0;
    s_grab_keyboard_window = XCB_WINDOW_NONE;
    s_set_input_focus_calls = 0;
    s_set_input_focus_window = XCB_WINDOW_NONE;
    s_set_input_focus_revert_to = 0u;
    s_ungrab_keyboard_calls = 0;
    s_window_destroy_calls = 0;
    s_window_destroyed = XCB_WINDOW_NONE;
    s_configure_window_calls = 0;
    s_focus_reply_focus = XCB_WINDOW_NONE;
    s_focus_reply_is_null = false;
    s_last_user_time_stub = 0u;
    s_dialog_info_show_calls = 0;
    s_dialog_info_show_level = MENU_MSG_LEVEL_NONE;
    s_client_restore_calls = 0;
    s_client_restore_last = NULL;
    s_client_unhide_calls = 0;
    s_client_unhide_last = NULL;
    s_client_unshade_calls = 0;
    s_client_unshade_last = NULL;
    s_desktop_switch_calls = 0;
    s_desktop_switch_last_id = 0u;
    s_focus_apply_calls = 0;
    s_focus_apply_last_client = NULL;
    s_focus_apply_last_desktop = NULL;
    s_desktops_registered = 0;
    memset(s_desktops_by_id, 0, sizeof(s_desktops_by_id));
    s_stacking_registered = 0;
    memset(s_stacking_desktop, 0, sizeof(s_stacking_desktop));
    memset(s_stacking_count, 0, sizeof(s_stacking_count));
    s_menu_draw_row_bg_calls = 0;
    s_menu_draw_label_calls = 0;
    s_menu_draw_truncate_calls = 0;
    s_text_use_font_calls = 0;
    s_text_set_color_calls = 0;
    s_wmicon_draw_at_calls = 0;

    memset(&s_fake_screen, 0, sizeof(s_fake_screen));
    s_fake_screen.root = (xcb_window_t) 1u;
}

static void s_teardown(void)
{
    for (int i = 0; i < s_owned_desktops_used; ++i) {
        ohtbl_destroy(s_owned_desktops[i]->clients);
        free(s_owned_desktops[i]);
    }
    s_owned_desktops_used = 0;

    for (int i = 0; i < s_owned_clients_used; ++i) {
        free(s_owned_clients[i]);
    }
    s_owned_clients_used = 0;
}

static desktop_td *s_make_desktop(uint32_t id, const char *name)
{
    desktop_td *desktop = calloc(1, sizeof(*desktop));

    desktop->id = (xcb_window_t) id;
    (void) snprintf(desktop->name, sizeof(desktop->name), "%s",
            (name != NULL) ? name : "");
    desktop->clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    s_owned_desktops[s_owned_desktops_used] = desktop;
    s_owned_desktops_used++;
    s_desktops_by_id[s_desktops_registered] = desktop;
    s_desktops_registered++;

    return desktop;
}

/** @p flags and @p state are ORed directly onto the new client's own
 *  'properties.flags'/'properties.state'; a plain, focusable,
 *  taskbar-visible client with no extra state needs neither, only its
 *  @p name */
static client_td *s_make_client(uint32_t id, const char *name,
        uint16_t flags, uint16_t state)
{
    client_td *client = calloc(1, sizeof(*client));

    client->id = (xcb_window_t) id;
    client->window = (xcb_window_t) id;
    client->info.name = (char *) name;
    client->properties.flags =
        (uint16_t) (CLIENT_FLAG_FOCUSABLE | flags);
    client->properties.state = state;
    s_owned_clients[s_owned_clients_used] = client;
    s_owned_clients_used++;

    return client;
}

/** Builds a one-desktop, one-monitor-sized surface with @p clients (in
 *  top-of-stack-first order, i.e., the order 'stacking_walk_down' will
 *  hand them out) registered on that single desktop, ready for
 *  'search_init' */
static void s_make_surface_one_desktop(surface_td *surface,
        desktop_td *desktop, client_td **clients, int client_count,
        uint32_t width, uint32_t height)
{
    memset(surface, 0, sizeof(*surface));
    surface->screen = &s_fake_screen;
    surface->properties.dim.w = width;
    surface->properties.dim.h = height;
    surface->desktops = cdlist_init(NULL);
    cdlist_ins_next(surface->desktops, NULL, desktop);
    surface->desktop_count = 1u;
    surface->desktop_cur = desktop->id;

    for (int i = 0; i < client_count; ++i) {
        ohtbl_insert(desktop->clients, clients[i]);
    }
    s_set_stacking(desktop, clients, client_count);
}

static void s_make_config(config_td *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    (void) snprintf(cfg->theme.search.input.font,
            sizeof(cfg->theme.search.input.font), "%s", "fixed");
    (void) snprintf(cfg->theme.search.unselected.font,
            sizeof(cfg->theme.search.unselected.font), "%s", "fixed");
    (void) snprintf(cfg->theme.search.selected.font,
            sizeof(cfg->theme.search.selected.font), "%s", "fixed");
    cfg->theme.search.unselected.color.background = 0x111111u;
    cfg->theme.search.unselected.color.foreground = 0xeeeeeeu;
    cfg->theme.search.selected.color.background = 0x2222aau;
    cfg->theme.search.selected.color.foreground = 0xffffffu;
    cfg->theme.search.input.color.background = 0x000000u;
    cfg->theme.search.input.color.foreground = 0xffffffu;
    cfg->theme.search.border.color = 0x333333u;
    cfg->theme.search.border.width = 1u;
    cfg->theme.menu.show_pixmaps = false;
}


/* search_init / search_is_open / search_window */

/* NULL connection, surface or config are all no-ops: the widget must
 * never half-open on invalid input */
static void s_test_init_rejects_null_args(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, NULL, &surface, &cfg);
    TAP_OK(!search_is_open(), "search_init with a null connection"
            " never opens the widget");

    search_init((list_td *) NULL, s_connection_stub, NULL, &cfg);
    TAP_OK(!search_is_open(), "search_init with a null surface never"
            " opens the widget");

    search_init((list_td *) NULL, s_connection_stub, &surface, NULL);
    TAP_OK(!search_is_open(), "search_init with a null config never"
            " opens the widget");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* No candidates at all: the widget never opens, and shows the
 * "no windows" informational dialog instead */
static void s_test_init_empty_candidates_shows_dialog(void)
{
    surface_td surface;
    desktop_td *desktop;
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    s_make_surface_one_desktop(&surface, desktop, NULL, 0, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);

    TAP_OK(!search_is_open(),
            "an empty candidate list leaves the widget unopened");
    TAP_EQ_INT(s_dialog_info_show_calls, 1,
            "dialog_info_show is called exactly once");
    TAP_EQ_INT((int) s_dialog_info_show_level, (int) MENU_MSG_LEVEL_INFO,
            "the dialog is shown at the informational level");
    TAP_EQ_INT(s_create_window_calls, 0,
            "no widget window is ever created for an empty result set");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* One eligible candidate: the widget opens, creates and maps its
 * window on the surface's own root, grabs the keyboard on that same
 * root, sets input focus to the new window, and paints immediately */
static void s_test_init_opens_and_paints_with_candidates(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);

    TAP_OK(search_is_open(), "a non-empty candidate list opens the"
            " widget");
    TAP_EQ_INT(s_create_window_calls, 1,
            "exactly one widget window is created");
    TAP_OK(s_created_parent == s_fake_screen.root,
            "the widget window is created as a child of the surface's"
            " own screen root");
    TAP_EQ_INT(s_map_window_calls, 1, "the widget window is mapped");
    TAP_EQ_INT(s_grab_keyboard_calls, 1,
            "the keyboard is grabbed exactly once");
    TAP_OK(s_grab_keyboard_window == s_fake_screen.root,
            "the keyboard grab targets the surface's screen root, not"
            " the widget window");
    TAP_EQ_INT(s_set_input_focus_calls, 1,
            "input focus is set exactly once");
    TAP_OK(s_set_input_focus_window == search_window(),
            "input focus is set to the newly created widget window");
    TAP_OK(search_window() != XCB_WINDOW_NONE,
            "search_window reports the created window, not"
            " XCB_WINDOW_NONE");
    TAP_OK(s_menu_draw_row_bg_calls > 0,
            "search_init paints immediately rather than waiting for"
            " the first keystroke");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Calling search_init while already open first tears down the
 * previous instance (ungrabbing/destroying its window) before
 * building the new one */
static void s_test_init_while_open_destroys_previous(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;
    xcb_window_t first_window;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    first_window = search_window();

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);

    TAP_EQ_INT(s_window_destroy_calls, 1,
            "re-opening an already open widget destroys the previous"
            " window exactly once");
    TAP_OK(s_window_destroyed == first_window,
            "the window torn down is the one that was open before");
    TAP_OK(search_window() != first_window,
            "the re-opened widget got a brand new window identifier");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A previous real input focus (neither PointerRoot nor None) is
 * recorded and restored, via XCB_INPUT_FOCUS_PARENT, on destroy */
static void s_test_destroy_restores_previous_focus(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);
    s_focus_reply_focus = (xcb_window_t) 555u;

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    s_set_input_focus_calls = 0;
    s_set_input_focus_window = XCB_WINDOW_NONE;

    search_destroy(s_connection_stub);

    TAP_EQ_INT(s_ungrab_keyboard_calls, 1,
            "destroy ungrabs the keyboard exactly once");
    TAP_EQ_INT(s_window_destroy_calls, 1,
            "destroy destroys the widget window exactly once");
    TAP_EQ_INT(s_set_input_focus_calls, 1,
            "a real previous focus is restored with one more"
            " xcb_set_input_focus call");
    TAP_OK(s_set_input_focus_window == (xcb_window_t) 555u,
            "focus is restored to exactly the window that held it"
            " before the widget opened");
    TAP_EQ_INT((int) s_set_input_focus_revert_to,
            (int) XCB_INPUT_FOCUS_PARENT,
            "the restore reverts to XCB_INPUT_FOCUS_PARENT, never"
            " XCB_INPUT_FOCUS_POINTER_ROOT");
    TAP_OK(!search_is_open(),
            "the widget reports closed once destroyed");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A prior focus of PointerRoot or None is never restored: both are
 * pseudo-windows XCB_INPUT_FOCUS_PARENT cannot sensibly target */
static void s_test_destroy_skips_restoring_pseudo_focus(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);
    s_focus_reply_focus = (xcb_window_t) XCB_INPUT_FOCUS_POINTER_ROOT;

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    s_set_input_focus_calls = 0;

    search_destroy(s_connection_stub);

    TAP_EQ_INT(s_set_input_focus_calls, 0,
            "PointerRoot as the previous focus is never restored");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A null xcb_get_input_focus_reply (as if the reply were lost) leaves
 * 'prev_focus' at XCB_WINDOW_NONE, so destroy skips restoring focus
 * rather than dereferencing a null reply */
static void s_test_init_null_focus_reply_is_safe(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);
    s_focus_reply_is_null = true;

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    TAP_OK(search_is_open(),
            "a null xcb_get_input_focus_reply does not stop the"
            " widget from opening");

    s_set_input_focus_calls = 0;
    search_destroy(s_connection_stub);
    TAP_EQ_INT(s_set_input_focus_calls, 0,
            "with no previous focus recorded, destroy never calls"
            " xcb_set_input_focus a second time");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Calling search_destroy on an already-closed widget, or with a null
 * connection, is a safe no-op */
static void s_test_destroy_idempotent_and_null_safe(void)
{
    TAP_OK(!search_is_open(),
            "the widget starts closed before this test's own calls");

    search_destroy(NULL);
    TAP_OK(!search_is_open(),
            "destroy with a null connection on an already-closed"
            " widget does not crash and changes nothing");

    search_destroy(s_connection_stub);
    TAP_OK(!search_is_open(),
            "destroying an already-closed widget is a safe no-op");
}


/* Candidate collection / eligibility filter */

/* A non-focusable client, and one flagged CLIENT_FLAG_SKIP_TASKBAR,
 * are both left out of the collected candidates; only the ordinary
 * one is offered as a result */
static void s_test_collect_filters_ineligible_clients(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[3];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "ordinary", 0, 0);
    clients[1] = calloc(1, sizeof(client_td));
    clients[1]->id = (xcb_window_t) 2u;
    clients[1]->window = (xcb_window_t) 2u;
    clients[1]->info.name = (char *) "not-focusable";
    /* Deliberately no CLIENT_FLAG_FOCUSABLE set at all */
    s_owned_clients[s_owned_clients_used++] = clients[1];
    clients[2] = s_make_client(3u, "skip-taskbar",
            CLIENT_FLAG_SKIP_TASKBAR, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 3, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);

    TAP_OK(search_is_open(),
            "at least one eligible candidate still opens the widget");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Every eligible client across every desktop of the surface is
 * collected, not only those on the surface's currently active one */
static void s_test_collect_spans_every_desktop(void)
{
    surface_td surface;
    desktop_td *desktop_a;
    desktop_td *desktop_b;
    client_td *clients_a[1];
    client_td *clients_b[1];
    config_td cfg;

    s_reset();
    desktop_a = s_make_desktop(0u, "a");
    desktop_b = s_make_desktop(1u, "b");
    clients_a[0] = s_make_client(1u, "on-a", 0, 0);
    clients_b[0] = s_make_client(2u, "on-b", 0, 0);

    memset(&surface, 0, sizeof(surface));
    surface.screen = &s_fake_screen;
    surface.properties.dim.w = 1024u;
    surface.properties.dim.h = 768u;
    surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(surface.desktops, NULL, desktop_a);
    cdlist_ins_next(surface.desktops, NULL, desktop_b);
    surface.desktop_count = 2u;
    surface.desktop_cur = 0u;
    ohtbl_insert(desktop_a->clients, clients_a[0]);
    ohtbl_insert(desktop_b->clients, clients_b[0]);
    s_set_stacking(desktop_a, clients_a, 1);
    s_set_stacking(desktop_b, clients_b, 1);

    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    TAP_OK(search_is_open(),
            "candidates from a desktop other than the surface's"
            " current one are still collected, opening the widget");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}


/* Fuzzy filtering via search_handle_keypress */

/** Feeds one ASCII character through search_handle_keypress as the
 *  widget itself receives it: as its own xcb_keysym_t value, valid for
 *  every printable ASCII character since Latin-1/ASCII keysyms equal
 *  their character code one for one */
static void s_type(xcb_connection_t *connection, const char *text,
        const config_td *cfg)
{
    for (size_t i = 0; text[i] != '\0'; ++i) {
        search_handle_keypress(connection, (list_td *) NULL,
                (xcb_keysym_t) (unsigned char) text[i], 0u, cfg);
    }
}

/* An empty query matches every candidate, in collection order (top of
 * stack first, i.e., most recently focused first) */
static void s_test_empty_query_matches_all_in_stack_order(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[3];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    /* Registered top-of-stack-first: 'top' before 'middle' before
     * 'bottom', the order stacking_walk_down (and this file's own
     * stand-in) hands them out in */
    clients[0] = s_make_client(1u, "top", 0, 0);
    clients[1] = s_make_client(2u, "middle", 0, 0);
    clients[2] = s_make_client(3u, "bottom", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 3, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);

    /* No public getter exposes the result list directly; clicking each
     * row in turn and checking which client search_handle_click's
     * eventual focus_apply call names is how this file inspects
     * ordering without touching s_search directly */
    search_handle_click(s_connection_stub, (list_td *) NULL, 0,
            (int16_t) (26 + 10 + 10 + 0 * 20 + 1), &cfg);
    TAP_OK(s_focus_apply_last_client == clients[0],
            "with an empty query, the first row is the most recently"
            " focused (top-of-stack) candidate");

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_click(s_connection_stub, (list_td *) NULL, 0,
            (int16_t) (26 + 10 + 10 + 1 * 20 + 1), &cfg);
    TAP_OK(s_focus_apply_last_client == clients[1],
            "the second row is the next-most-recently focused"
            " candidate");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* No candidate's name contains every query character in order: the
 * result list ends up empty, so clicking any row does nothing at all */
static void s_test_query_no_matches(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "firefox", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    s_type(s_connection_stub, "zzz", &cfg);

    s_focus_apply_calls = 0;
    search_handle_click(s_connection_stub, (list_td *) NULL, 0,
            (int16_t) (26 + 10 + 10 + 0 * 20 + 1), &cfg);
    TAP_EQ_INT(s_focus_apply_calls, 0,
            "a query matching nothing leaves every row unclickable,"
            " so no confirm ever fires");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* An exact, full-name query still matches (a subsequence is, trivially,
 * satisfied by an identical string) and selects that candidate */
static void s_test_query_exact_match(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[2];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "firefox", 0, 0);
    clients[1] = s_make_client(2u, "xterm", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 2, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    s_type(s_connection_stub, "firefox", &cfg);

    search_handle_click(s_connection_stub, (list_td *) NULL, 0,
            (int16_t) (26 + 10 + 10 + 0 * 20 + 1), &cfg);
    TAP_OK(s_focus_apply_last_client == clients[0],
            "typing a candidate's exact full name matches and selects"
            " exactly that candidate");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Matching is case-insensitive, and need not be contiguous: "ffx" as a
 * subsequence of "FireFox" still matches */
static void s_test_query_case_insensitive_partial_match(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "FireFox", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    s_type(s_connection_stub, "ffx", &cfg);

    search_handle_click(s_connection_stub, (list_td *) NULL, 0,
            (int16_t) (26 + 10 + 10 + 0 * 20 + 1), &cfg);
    TAP_OK(s_focus_apply_last_client == clients[0],
            "a lowercase, non-contiguous subsequence query matches a"
            " differently-cased candidate name");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A tighter (more contiguous, or starting right at the beginning)
 * match scores higher and therefore sorts first, ahead of a looser
 * match that still satisfies the same query */
static void s_test_query_scores_tighter_match_first(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[2];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    /* Registered so the loose match would win on stack order alone if
     * scoring were not actually reordering the results: 'loose' is
     * collected before 'ab-tight' (top of stack first), yet 'ab' as a
     * query should still promote 'ab-tight' to row 0 */
    clients[0] = s_make_client(1u, "a-x-x-x-x-x-x-x-x-x-b", 0, 0);
    clients[1] = s_make_client(2u, "ab-tight", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 2, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    s_type(s_connection_stub, "ab", &cfg);

    search_handle_click(s_connection_stub, (list_td *) NULL, 0,
            (int16_t) (26 + 10 + 10 + 0 * 20 + 1), &cfg);
    TAP_OK(s_focus_apply_last_client == clients[1],
            "a tight, front-loaded subsequence match outranks a loose,"
            " scattered one for the very same query, regardless of"
            " which was collected first");

    cdlist_destroy(surface.desktops);
    s_teardown();
}


/* search_handle_keypress: navigation and editing */

/* Escape closes the widget and restores previous focus, confirming
 * nothing */
static void s_test_keypress_escape_closes(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff1bu, 0u, &cfg);

    TAP_OK(!search_is_open(), "Escape closes the widget");
    TAP_EQ_INT(s_focus_apply_calls, 0,
            "Escape never confirms a selection");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Backspace removes the last query character and re-filters; erasing
 * back to empty restores every candidate as a result */
static void s_test_keypress_backspace_erases_and_refilters(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[2];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    clients[1] = s_make_client(2u, "beta", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 2, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    s_type(s_connection_stub, "beta", &cfg);
    search_handle_click(s_connection_stub, (list_td *) NULL, 0,
            (int16_t) (26 + 10 + 10 + 0 * 20 + 1), &cfg);
    TAP_OK(s_focus_apply_last_client == clients[1],
            "after typing an exact query, only its matching candidate"
            " is selectable at row 0");

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    s_type(s_connection_stub, "beta", &cfg);
    for (int i = 0; i < 4; ++i) {
        search_handle_keypress(s_connection_stub, (list_td *) NULL,
                (xcb_keysym_t) 0xff08u, 0u, &cfg);
    }
    search_handle_click(s_connection_stub, (list_td *) NULL, 0,
            (int16_t) (26 + 10 + 10 + 0 * 20 + 1), &cfg);
    TAP_OK(s_focus_apply_last_client == clients[0],
            "backspacing the whole query back to empty restores every"
            " candidate, with the top-of-stack one first again");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Backspace on an already-empty query is a harmless no-op, not an
 * out-of-bounds decrement */
static void s_test_keypress_backspace_on_empty_query_is_safe(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff08u, 0u, &cfg);

    TAP_OK(search_is_open(),
            "backspacing an empty query neither crashes nor closes"
            " the widget");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Down (and a plain Tab acting the same way) cycles the selection
 * forward, wrapping from the last result back to the first */
static void s_test_keypress_down_and_tab_cycle_forward(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[2];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    clients[1] = s_make_client(2u, "beta", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 2, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff54u, 0u, &cfg);   /* Down */

    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);   /* Return */
    TAP_OK(s_focus_apply_last_client == clients[1],
            "one Down from the top selects the second result");

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff09u, 0u, &cfg);   /* plain Tab */
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);
    TAP_OK(s_focus_apply_last_client == clients[1],
            "a plain Tab moves the selection exactly like Down");

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff54u, 0u, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff54u, 0u, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);
    TAP_OK(s_focus_apply_last_client == clients[0],
            "Down past the last result wraps back around to the"
            " first");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Up (and Shift+Tab, and the dedicated ISO_Left_Tab keysym, acting the
 * same way) cycles the selection backward, wrapping from the first
 * result to the last */
static void s_test_keypress_up_and_shift_tab_cycle_backward(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[2];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    clients[1] = s_make_client(2u, "beta", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 2, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff52u, 0u, &cfg);   /* Up, wraps from 0 */
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);
    TAP_OK(s_focus_apply_last_client == clients[1],
            "Up from the first result wraps around to the last");

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff09u,
            (uint16_t) XCB_MOD_MASK_SHIFT, &cfg);  /* Shift+Tab */
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);
    TAP_OK(s_focus_apply_last_client == clients[1],
            "Shift+Tab (via the modifier mask) moves the selection"
            " exactly like Up");

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xfe20u, 0u, &cfg);   /* ISO_Left_Tab */
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);
    TAP_OK(s_focus_apply_last_client == clients[1],
            "the dedicated ISO_Left_Tab keysym moves the selection"
            " exactly like Up too, even with no shift bit set");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Up/Down with zero results is a safe no-op rather than touching an
 * out-of-range selection index */
static void s_test_keypress_navigation_with_no_results_is_safe(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    s_type(s_connection_stub, "zzz", &cfg);  /* empties the result set */

    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff54u, 0u, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff52u, 0u, &cfg);
    TAP_OK(search_is_open(),
            "navigating with zero results neither crashes nor closes"
            " the widget");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* KP_Enter (the numeric-keypad Enter keysym) confirms exactly like
 * Return does */
static void s_test_keypress_kp_enter_confirms(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff8du, 0u, &cfg);   /* KP_Enter */

    TAP_OK(s_focus_apply_last_client == clients[0],
            "KP_Enter confirms the current selection exactly like"
            " Return");
    TAP_OK(!search_is_open(),
            "confirming closes the widget");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A non-printable, unrecognized key (well above the Latin-1 range) is
 * silently ignored: no query change, no navigation, no redraw side
 * effect beyond what was already there */
static void s_test_keypress_unrecognized_key_ignored(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;
    int draw_calls_before;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    draw_calls_before = s_menu_draw_row_bg_calls;

    /* 0xffbe is F1, well above the 0xFF printable-keysym ceiling
     * search.c itself checks */
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xffbeu, 0u, &cfg);

    TAP_EQ_INT(s_menu_draw_row_bg_calls, draw_calls_before,
            "an unrecognized function key triggers no redraw at all");
    TAP_OK(search_is_open(),
            "an unrecognized key leaves the widget open and"
            " untouched");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A printable character is appended to the query and immediately
 * re-filters the results */
static void s_test_keypress_printable_appends_and_refilters(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[2];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    clients[1] = s_make_client(2u, "beta", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 2, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 'b', 0u, &cfg);

    search_handle_click(s_connection_stub, (list_td *) NULL, 0,
            (int16_t) (26 + 10 + 10 + 0 * 20 + 1), &cfg);
    TAP_OK(s_focus_apply_last_client == clients[1],
            "typing 'b' filters the result list down to the"
            " candidate whose name contains it");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Typing stops accepting further characters once the query buffer
 * would fill to its maximum length, never overflowing it */
static void s_test_keypress_query_length_is_capped(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;
    char long_name[300];

    s_reset();
    desktop = s_make_desktop(0u, "one");
    for (size_t i = 0; i < sizeof(long_name) - 1u; ++i) {
        long_name[i] = 'a';
    }
    long_name[sizeof(long_name) - 1u] = '\0';
    clients[0] = s_make_client(1u, long_name, 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);

    for (int i = 0; i < 200; ++i) {
        search_handle_keypress(s_connection_stub, (list_td *) NULL,
                (xcb_keysym_t) 'a', 0u, &cfg);
    }

    /* Still open, and the one candidate (an all-'a' name) still
     * matches every 'a' typed so far, up to the cap: reaching this
     * point at all without an ASan buffer overflow is what this test
     * is really checking */
    TAP_OK(search_is_open(),
            "typing far more characters than the query buffer holds"
            " never overflows it, and the widget stays open");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Keypresses are ignored entirely while the widget is not open */
static void s_test_keypress_ignored_when_closed(void)
{
    config_td cfg;

    s_reset();
    s_make_config(&cfg);

    TAP_OK(!search_is_open(), "widget starts closed for this test");
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 'a', 0u, &cfg);
    TAP_OK(!search_is_open(),
            "a keypress while closed does not open the widget or"
            " otherwise crash");
}


/* search_handle_click / search_handle_motion */

/* A click outside every visible row (e.g., on the query bar itself) is
 * consumed without confirming anything */
static void s_test_click_outside_rows_does_nothing(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_click(s_connection_stub, (list_td *) NULL, 0, 5, &cfg);

    TAP_EQ_INT(s_focus_apply_calls, 0,
            "clicking inside the query bar (above every result row)"
            " confirms nothing");
    TAP_OK(search_is_open(),
            "and the widget stays open, since nothing was confirmed");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A click is ignored entirely while the widget is not open */
static void s_test_click_ignored_when_closed(void)
{
    config_td cfg;

    s_reset();
    s_make_config(&cfg);

    search_handle_click(s_connection_stub, (list_td *) NULL, 0, 40, &cfg);
    TAP_EQ_INT(s_focus_apply_calls, 0,
            "a click while the widget is closed does nothing at all");
}

/* Hovering over a different row than the current selection moves the
 * selection there and repaints immediately */
static void s_test_motion_moves_selection_and_repaints(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[2];
    config_td cfg;
    int draw_calls_before;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    clients[1] = s_make_client(2u, "beta", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 2, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    draw_calls_before = s_menu_draw_row_bg_calls;

    search_handle_motion(0, (int16_t) (26 + 10 + 10 + 1 * 20 + 1));
    TAP_OK(s_menu_draw_row_bg_calls > draw_calls_before,
            "moving the pointer onto a different row triggers an"
            " immediate repaint");

    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);
    TAP_OK(s_focus_apply_last_client == clients[1],
            "the hovered row became the selection, exactly as if it"
            " had been reached by keyboard navigation");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Hovering over the row that is already selected does not repaint
 * again (the early-return "idx == s_search.selected" branch) */
static void s_test_motion_same_row_skips_repaint(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;
    int draw_calls_before;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    draw_calls_before = s_menu_draw_row_bg_calls;

    search_handle_motion(0, (int16_t) (26 + 10 + 10 + 0 * 20 + 1));
    TAP_EQ_INT(s_menu_draw_row_bg_calls, draw_calls_before,
            "hovering the row that is already selected triggers no"
            " extra repaint");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Motion is ignored entirely while the widget is not open */
static void s_test_motion_ignored_when_closed(void)
{
    s_reset();
    search_handle_motion(0, 40);
    TAP_OK(!search_is_open(),
            "pointer motion while closed does not open the widget or"
            " otherwise crash");
}


/* s_search_confirm branches, reached through Return */

/* An iconified client is restored (not merely unhidden) on confirm */
static void s_test_confirm_iconified_is_restored(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, CLIENT_STATE_ICONIFIED);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);

    TAP_EQ_INT(s_client_restore_calls, 1,
            "an iconified client is restored exactly once on confirm");
    TAP_OK(s_client_restore_last == clients[0],
            "the restored client is the one that was selected");
    TAP_EQ_INT(s_client_unhide_calls, 0,
            "an iconified client is never separately unhidden too");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A plain hidden (not iconified) client is unhidden, never restored */
static void s_test_confirm_hidden_is_unhidden(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", CLIENT_FLAG_HIDDEN, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);

    TAP_EQ_INT(s_client_unhide_calls, 1,
            "a plain hidden client is unhidden exactly once on"
            " confirm");
    TAP_OK(s_client_unhide_last == clients[0],
            "the unhidden client is the one that was selected");
    TAP_EQ_INT(s_client_restore_calls, 0,
            "a plain hidden (not iconified) client is never restored");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A shaded client is unshaded on confirm, independently of and in
 * addition to any iconified/hidden handling */
static void s_test_confirm_shaded_is_unshaded(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", CLIENT_FLAG_SHADED, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);

    TAP_EQ_INT(s_client_unshade_calls, 1,
            "a shaded client is unshaded exactly once on confirm");
    TAP_OK(s_client_unshade_last == clients[0],
            "the unshaded client is the one that was selected");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A pinned client is never switched-to via enact_surface_desktop_switch,
 * since pinning already keeps it visible on whichever desktop the
 * surface currently shows */
static void s_test_confirm_pinned_never_switches_desktop(void)
{
    surface_td surface;
    desktop_td *desktop_a;
    desktop_td *desktop_b;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop_a = s_make_desktop(0u, "a");
    desktop_b = s_make_desktop(1u, "b");
    clients[0] = s_make_client(1u, "alpha", CLIENT_FLAG_PIN, 0);

    memset(&surface, 0, sizeof(surface));
    surface.screen = &s_fake_screen;
    surface.properties.dim.w = 1024u;
    surface.properties.dim.h = 768u;
    surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(surface.desktops, NULL, desktop_a);
    cdlist_ins_next(surface.desktops, NULL, desktop_b);
    surface.desktop_count = 2u;
    /* The pinned client is registered as living on desktop 1, while
     * the surface currently shows desktop 0, so a non-pinned client
     * in the same spot would trigger a desktop switch */
    surface.desktop_cur = 0u;
    ohtbl_insert(desktop_b->clients, clients[0]);
    s_set_stacking(desktop_b, clients, 1);
    s_set_stacking(desktop_a, NULL, 0);

    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);

    TAP_EQ_INT(s_desktop_switch_calls, 0,
            "a pinned client's confirm never calls"
            " enact_surface_desktop_switch");
    TAP_OK(s_focus_apply_last_desktop == desktop_a,
            "focus_apply is instead handed the surface's own current"
            " desktop, exactly where the user already is");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A non-pinned client whose desktop differs from the surface's
 * current one triggers a desktop switch before focusing it */
static void s_test_confirm_non_pinned_other_desktop_switches(void)
{
    surface_td surface;
    desktop_td *desktop_a;
    desktop_td *desktop_b;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop_a = s_make_desktop(0u, "a");
    desktop_b = s_make_desktop(1u, "b");
    clients[0] = s_make_client(1u, "alpha", 0, 0);

    memset(&surface, 0, sizeof(surface));
    surface.screen = &s_fake_screen;
    surface.properties.dim.w = 1024u;
    surface.properties.dim.h = 768u;
    surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(surface.desktops, NULL, desktop_a);
    cdlist_ins_next(surface.desktops, NULL, desktop_b);
    surface.desktop_count = 2u;
    surface.desktop_cur = 0u;
    ohtbl_insert(desktop_b->clients, clients[0]);
    s_set_stacking(desktop_b, clients, 1);
    s_set_stacking(desktop_a, NULL, 0);

    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);

    TAP_EQ_INT(s_desktop_switch_calls, 1,
            "confirming a non-pinned client on another desktop"
            " switches to it exactly once");
    TAP_EQ_INT((int) s_desktop_switch_last_id, (int) desktop_b->id,
            "the switch targets the client's own desktop");
    TAP_OK(s_focus_apply_last_client == clients[0],
            "focus_apply is still called with the confirmed client"
            " afterward");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A non-pinned client already on the surface's current desktop
 * triggers no desktop switch at all */
static void s_test_confirm_same_desktop_never_switches(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);

    TAP_EQ_INT(s_desktop_switch_calls, 0,
            "a client already on the current desktop never triggers a"
            " switch");
    TAP_EQ_INT(s_focus_apply_calls, 1,
            "focus_apply still runs exactly once to focus and raise"
            " it");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Confirming with no selection at all (an empty result set) simply
 * destroys the widget without touching any enact_ or focus_apply path */
static void s_test_confirm_no_selection_just_destroys(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    s_type(s_connection_stub, "zzz", &cfg);  /* empties the result set */

    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);

    TAP_OK(!search_is_open(),
            "confirming with no selection still closes the widget");
    TAP_EQ_INT(s_focus_apply_calls, 0,
            "and never calls focus_apply, since nothing was selected");
    TAP_EQ_INT(s_client_restore_calls + s_client_unhide_calls +
            s_client_unshade_calls + s_desktop_switch_calls, 0,
            "nor any of the enact_*/desktop-switch helpers");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A pinned client whose recorded desktop id no longer resolves via
 * surface_desktop_get (e.g., that desktop has since been torn down)
 * makes s_search_confirm return early, calling neither
 * enact_surface_desktop_switch nor focus_apply */
static void s_test_confirm_pinned_unresolvable_desktop_is_safe(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", CLIENT_FLAG_PIN, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    /* Unregister every desktop from this file's own
     * surface_desktop_get stand-in right before confirming, so the
     * pinned branch's lookup of 'surface->desktop_cur' resolves to
     * NULL, exactly as it would for a desktop torn down between the
     * widget opening and the user confirming */
    s_desktops_registered = 0;

    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);

    TAP_EQ_INT(s_focus_apply_calls, 0,
            "an unresolvable pinned-client desktop stops confirm"
            " before ever reaching focus_apply");

    s_desktops_registered = 1;
    s_desktops_by_id[0] = desktop;
    cdlist_destroy(surface.desktops);
    s_teardown();
}


/* Geometry: viewport rows, scrolling, row hit-testing */

/* With few enough results to fit without scrolling, every result is
 * one viewport row and no scroll indicators are drawn */
static void s_test_geometry_small_result_set_no_scroll(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[3];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "a", 0, 0);
    clients[1] = s_make_client(2u, "b", 0, 0);
    clients[2] = s_make_client(3u, "c", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 3, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);

    /* Clicking exactly at the third (last) row's own top-left corner
     * must still resolve to that row: if viewport_rows had been
     * wrongly capped below the true result count, this would miss */
    search_handle_click(s_connection_stub, (list_td *) NULL, 0,
            (int16_t) (26 + 10 + 10 + 2 * 20 + 1), &cfg);
    TAP_OK(s_focus_apply_last_client == clients[2],
            "with only three results, the third row is still directly"
            " reachable, all three fit in the viewport without"
            " scrolling");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* On a very short surface, the viewport is capped at a minimum of one
 * row rather than reaching zero (which would divide the widget by an
 * empty viewport and make every row unreachable) */
static void s_test_geometry_tiny_surface_keeps_one_row_minimum(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[5];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    for (int i = 0; i < 5; ++i) {
        char name[8];

        (void) snprintf(name, sizeof(name), "c%d", i);
        clients[i] = s_make_client((uint32_t) (1 + i),
                (i == 0) ? "c0" : (i == 1) ? "c1" : (i == 2) ? "c2" :
                (i == 3) ? "c3" : "c4", 0, 0);
        (void) name;
    }
    /* A surface only tall enough for the bar itself, well under one
     * more row's worth of height: 'avail' clamps to zero, and
     * 's_search_compute_geometry' must still floor 'viewport_rows' at
     * one rather than at zero */
    s_make_surface_one_desktop(&surface, desktop, clients, 5, 1024u, 40u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);

    search_handle_click(s_connection_stub, (list_td *) NULL, 0,
            (int16_t) (26 + 10 + 10 + 0 * 20 + 1), &cfg);
    TAP_OK(s_focus_apply_last_client == clients[0],
            "even on a surface too short to fit a full row's worth of"
            " extra height, the first result row is still reachable"
            " (viewport_rows floors at one, never zero)");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A click above the first row (inside the top padding/query bar) and a
 * click below the last visible row both miss every row, returning no
 * selection */
static void s_test_row_at_y_out_of_bounds_above_and_below(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[2];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "a", 0, 0);
    clients[1] = s_make_client(2u, "b", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 2, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);

    search_handle_click(s_connection_stub, (list_td *) NULL, 0, 0, &cfg);
    TAP_EQ_INT(s_focus_apply_calls, 0,
            "a click at y=0, above the first row entirely, selects"
            " nothing");

    search_handle_click(s_connection_stub, (list_td *) NULL, 0,
            (int16_t) (26 + 10 + 10 + 2 * 20 + 50), &cfg);
    TAP_EQ_INT(s_focus_apply_calls, 0,
            "a click far below the last visible row, past the"
            " viewport entirely, also selects nothing");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A scrolled-down selection (more results than fit in the viewport)
 * still resolves the right absolute candidate for a click on a
 * visible row, proving scroll_offset is folded into the hit test */
static void s_test_scroll_offset_folds_into_row_hit_test(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[40];
    char names[40][8];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    for (int i = 0; i < 40; ++i) {
        (void) snprintf(names[i], sizeof(names[i]), "c%02d", i);
        clients[i] = s_make_client((uint32_t) (1 + i), names[i], 0, 0);
    }
    /* Tall enough to give a viewport of several rows, but with 40
     * results still far more than fit, so scrolling is exercised */
    s_make_surface_one_desktop(&surface, desktop, clients, 40, 1024u,
            300u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);

    /* Move selection down far enough that scroll_to_selection must
     * scroll the viewport to keep it visible */
    for (int i = 0; i < 20; ++i) {
        search_handle_keypress(s_connection_stub, (list_td *) NULL,
                (xcb_keysym_t) 0xff54u, 0u, &cfg);
    }
    /* Now click on whichever row is drawn last (bottom of viewport):
     * with scrolling active, that must resolve to the client at
     * absolute index 'scroll_offset + viewport_rows - 1', not simply
     * 'viewport_rows - 1' as it would with no scroll_offset folded
     * in.  The selection is already at index 20 and clamped inside
     * the viewport by scroll_to_selection, so clicking that same
     * bottom-most drawn row must reconfirm client index 20. */
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff0du, 0u, &cfg);
    TAP_OK(s_focus_apply_last_client == clients[20],
            "after scrolling the selection down to the 21st result"
            " (index 20) and confirming, exactly that client is"
            " focused, proving scroll_offset was correctly folded"
            " into which candidate ended up selected");

    cdlist_destroy(surface.desktops);
    s_teardown();
}


/* search_draw: scroll indicators and icon drawing */

/* With more results than fit in the viewport, search_draw paints extra
 * scroll-indicator labels beyond one per visible row */
static void s_test_draw_shows_scroll_indicators_when_overflowing(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[40];
    char names[40][8];
    config_td cfg;
    int label_calls_before;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    for (int i = 0; i < 40; ++i) {
        (void) snprintf(names[i], sizeof(names[i]), "c%02d", i);
        clients[i] = s_make_client((uint32_t) (1 + i), names[i], 0, 0);
    }
    s_make_surface_one_desktop(&surface, desktop, clients, 40, 1024u,
            300u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    label_calls_before = s_menu_draw_label_calls;

    /* Scroll down once so both the up and down indicators are due:
     * some entries are hidden above the viewport now, and far more
     * remain hidden below it */
    search_handle_keypress(s_connection_stub, (list_td *) NULL,
            (xcb_keysym_t) 0xff54u, 0u, &cfg);
    for (int i = 0; i < 10; ++i) {
        search_handle_keypress(s_connection_stub, (list_td *) NULL,
                (xcb_keysym_t) 0xff54u, 0u, &cfg);
    }

    label_calls_before = s_menu_draw_label_calls;
    search_draw(s_connection_stub, &cfg);
    TAP_OK(s_menu_draw_label_calls > label_calls_before,
            "search_draw paints at least the scroll-indicator labels"
            " (beyond one per visible row) once scrolled past the"
            " top");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* With every result fitting in the viewport, no extra scroll
 * indicators are drawn */
static void s_test_draw_no_scroll_indicators_when_all_fit(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[2];
    config_td cfg;
    int label_calls_before;
    int label_calls_after;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "a", 0, 0);
    clients[1] = s_make_client(2u, "b", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 2, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);

    label_calls_before = s_menu_draw_label_calls;
    search_draw(s_connection_stub, &cfg);
    label_calls_after = s_menu_draw_label_calls;

    /* Exactly one label per visible row plus the query bar's own
     * label: two rows and one bar means exactly three label calls,
     * never more from a spurious scroll indicator */
    TAP_EQ_INT(label_calls_after - label_calls_before, 3,
            "with both results fitting in the viewport, search_draw"
            " paints exactly one label per row plus the query bar,"
            " no scroll-indicator extras");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* search_draw with a null config is a safe no-op */
static void s_test_draw_null_config_is_safe(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;
    int draw_calls_before;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    draw_calls_before = s_menu_draw_row_bg_calls;

    search_draw(s_connection_stub, NULL);
    TAP_EQ_INT(s_menu_draw_row_bg_calls, draw_calls_before,
            "search_draw with a null config paints nothing and does"
            " not crash");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* search_draw while closed is a safe no-op */
static void s_test_draw_while_closed_is_safe(void)
{
    config_td cfg;

    s_reset();
    s_make_config(&cfg);

    search_draw(s_connection_stub, &cfg);
    TAP_OK(!search_is_open(),
            "search_draw while the widget is closed does not open it"
            " or crash");
}

/* With theme.menu.show_pixmaps enabled, drawing a row for a result
 * that does carry a client icon calls wmicon_draw_at */
static void s_test_draw_with_show_pixmaps_draws_icon(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);
    cfg.theme.menu.show_pixmaps = true;

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    TAP_OK(s_wmicon_draw_at_calls > 0,
            "with show_pixmaps enabled, painting a client result row"
            " draws its icon via wmicon_draw_at");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* With theme.menu.show_pixmaps disabled (the default), no icon is ever
 * drawn for any row */
static void s_test_draw_without_show_pixmaps_skips_icon(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    TAP_EQ_INT(s_wmicon_draw_at_calls, 0,
            "with show_pixmaps left at its default (false), no icon is"
            " ever drawn for any row");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A candidate with a null (or empty) name falls back to the literal
 * placeholder "(unnamed)" rather than crashing on a null format
 * argument */
static void s_test_unnamed_client_uses_placeholder(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, NULL, 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    TAP_OK(search_is_open(),
            "a client with a null name is still collected and does"
            " not crash the widget");

    search_handle_click(s_connection_stub, (list_td *) NULL, 0,
            (int16_t) (26 + 10 + 10 + 0 * 20 + 1), &cfg);
    TAP_OK(s_focus_apply_last_client == clients[0],
            "and it still confirms exactly like any other candidate");

    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* Bracketed hint text: fullscreen takes priority over maximized, and
 * independent flags (pinned, urgent) both still show up alongside it */
static void s_test_hints_fullscreen_priority_and_independent_flags(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "one");
    clients[0] = s_make_client(1u, "alpha",
            (uint16_t) (CLIENT_FLAG_PIN | CLIENT_FLAG_URGENT),
            (uint16_t) (CLIENT_STATE_FULLSCREEN |
                CLIENT_STATE_MAXIMIZED));
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    /* No public accessor exposes the built hint string directly; this
     * test instead confirms the widget opens and paints without
     * incident with every exclusive-plus-independent hint bit set at
     * once, the combination 's_search_build_hints' has to fit within
     * its small fixed 'letters[8]' buffer without overflowing it */
    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    TAP_OK(search_is_open(),
            "a client with every hint-worthy bit set at once (an"
            " exclusive geometry state plus two independent flags)"
            " still opens and draws without an ASan overflow in the"
            " small fixed hint-letter buffer");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}

/* A surface with only one desktop never draws a desktop-name column at
 * all (the "desktop_count > 1" guard); one with several does */
static void s_test_single_desktop_surface_paints_without_crash(void)
{
    surface_td surface;
    desktop_td *desktop;
    client_td *clients[1];
    config_td cfg;

    s_reset();
    desktop = s_make_desktop(0u, "solo");
    clients[0] = s_make_client(1u, "alpha", 0, 0);
    s_make_surface_one_desktop(&surface, desktop, clients, 1, 1024u, 768u);
    s_make_config(&cfg);

    search_init((list_td *) NULL, s_connection_stub, &surface, &cfg);
    TAP_OK(search_is_open(),
            "a single-desktop surface paints its one result row"
            " without drawing (or crashing on) a desktop-name column");

    search_destroy(s_connection_stub);
    cdlist_destroy(surface.desktops);
    s_teardown();
}


int main(void)
{
    TAP_PLAN(101);

    s_test_init_rejects_null_args();
    s_test_init_empty_candidates_shows_dialog();
    s_test_init_opens_and_paints_with_candidates();
    s_test_init_while_open_destroys_previous();
    s_test_destroy_restores_previous_focus();
    s_test_destroy_skips_restoring_pseudo_focus();
    s_test_init_null_focus_reply_is_safe();
    s_test_destroy_idempotent_and_null_safe();

    s_test_collect_filters_ineligible_clients();
    s_test_collect_spans_every_desktop();

    s_test_empty_query_matches_all_in_stack_order();
    s_test_query_no_matches();
    s_test_query_exact_match();
    s_test_query_case_insensitive_partial_match();
    s_test_query_scores_tighter_match_first();

    s_test_keypress_escape_closes();
    s_test_keypress_backspace_erases_and_refilters();
    s_test_keypress_backspace_on_empty_query_is_safe();
    s_test_keypress_down_and_tab_cycle_forward();
    s_test_keypress_up_and_shift_tab_cycle_backward();
    s_test_keypress_navigation_with_no_results_is_safe();
    s_test_keypress_kp_enter_confirms();
    s_test_keypress_unrecognized_key_ignored();
    s_test_keypress_printable_appends_and_refilters();
    s_test_keypress_query_length_is_capped();
    s_test_keypress_ignored_when_closed();

    s_test_click_outside_rows_does_nothing();
    s_test_click_ignored_when_closed();
    s_test_motion_moves_selection_and_repaints();
    s_test_motion_same_row_skips_repaint();
    s_test_motion_ignored_when_closed();

    s_test_confirm_iconified_is_restored();
    s_test_confirm_hidden_is_unhidden();
    s_test_confirm_shaded_is_unshaded();
    s_test_confirm_pinned_never_switches_desktop();
    s_test_confirm_non_pinned_other_desktop_switches();
    s_test_confirm_same_desktop_never_switches();
    s_test_confirm_no_selection_just_destroys();
    s_test_confirm_pinned_unresolvable_desktop_is_safe();

    s_test_geometry_small_result_set_no_scroll();
    s_test_geometry_tiny_surface_keeps_one_row_minimum();
    s_test_row_at_y_out_of_bounds_above_and_below();
    s_test_scroll_offset_folds_into_row_hit_test();

    s_test_draw_shows_scroll_indicators_when_overflowing();
    s_test_draw_no_scroll_indicators_when_all_fit();
    s_test_draw_null_config_is_safe();
    s_test_draw_while_closed_is_safe();
    s_test_draw_with_show_pixmaps_draws_icon();
    s_test_draw_without_show_pixmaps_skips_icon();

    s_test_unnamed_client_uses_placeholder();
    s_test_hints_fullscreen_priority_and_independent_flags();
    s_test_single_desktop_surface_paints_without_crash();

    return TAP_DONE();
}
