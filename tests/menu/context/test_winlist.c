/**
 * @file tests/menu/context/test_winlist.c
 *
 * @brief Test battery for the window list menu's client grouping
 *
 * 's_build_desktop_entries' (menu/context/winlist.c), the function
 * whose grouping this exercises, is file-static, so it is reached
 * only through the real, public 'winlist_show' entry point, exactly
 * as the window manager itself reaches it.  A recording stand-in for
 * 'ctxmenu_show' captures the 'ctxmenu_state_td' it receives, which
 * this file then inspects directly once 'winlist_show' returns:
 * everything the real menu code decided to build is right there in
 * 'entries'/'entry_count' (and, for an application-group submenu,
 * in that entry's own 'items'/'item_count'), with nothing about how
 * it got assembled needing to be exposed on purpose just for this.
 *
 * Every test here runs a stage with exactly one desktop, so
 * 'winlist_show' takes its flattened, single-desktop path: no root
 * "one submenu per desktop" layer sits above the window list itself,
 * which keeps each captured entry array a direct, easy-to-check
 * picture of one desktop's clients.
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
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/ohtbl.h>

/* Local includes */
#include <client.h>
#include <client/icccm.h>
#include <client/state.h>
#include <desktop.h>
#include <enact.h>
#include <harness/tap.h>
#include <logger.h>
#include <menu/context/ctxmenu.h>
#include <menu/context/ctxmenu/tree.h>
#include <menu/context/winlist.h>
#include <stage.h>
#include <wm.h>


/**
 * @brief Recording stand-in for @a ctxmenu_show
 *
 * Captures @p state instead of ever actually mapping a window, so a
 * test can inspect whatever @c winlist_show built directly, the
 * moment it hands that off to be shown.
 *
 * @note Complexity: @e O(1)
 */
static ctxmenu_state_td *s_captured_state;

void ctxmenu_show(xcb_connection_t *connection, stage_td *stage,
        ctxmenu_state_td *state, struct position_s pos,
        const config_td *config)
{
    (void) connection;
    (void) stage;
    (void) pos;
    (void) config;

    s_captured_state = state;
}


/**
 * @brief Link-only stand-in for @a logger_msg
 *
 * Reached only on the defensive @c WINLIST_MAX_ENTRY_DATA exhaustion
 * path, which no scenario in this file drives the pool small enough
 * to hit.
 *
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;

    return 0;
}


/**
 * @brief Link-only stand-in for @a ctxmenu_close
 *
 * Reached at the top of every 'winlist_show' call (through
 * 'winlist_close'), and by 'winlist_close' on its own; a real menu
 * window is never actually created by anything in this file, so
 * there is nothing for it to tear down.
 *
 * @note Complexity: @e O(1)
 */
void ctxmenu_close(ctxmenu_state_td *state)
{
    (void) state;
}


/**
 * @brief Link-only stand-in for @a ctxmenu_is_open
 *
 * Reached only by 'winlist_is_open', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
bool ctxmenu_is_open(const ctxmenu_state_td *state)
{
    (void) state;
    return false;
}


/**
 * @brief Link-only stand-in for @a ctxmenu_tree_handle_click_window
 *
 * Reached only by 'winlist_handle_click', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
bool ctxmenu_tree_handle_click_window(xcb_connection_t *connection,
        stage_td *stage, ctxmenu_state_td *root, xcb_window_t win,
        int y, const config_td *config)
{
    (void) connection;
    (void) stage;
    (void) root;
    (void) win;
    (void) y;
    (void) config;
    return false;
}


/**
 * @brief Link-only stand-in for @a ctxmenu_tree_handle_keypress_deepest
 *
 * Reached only by 'winlist_handle_keypress', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
bool ctxmenu_tree_handle_keypress_deepest(xcb_connection_t *connection,
        stage_td *stage, ctxmenu_state_td *root,
        xcb_keysym_t keysym, const config_td *config)
{
    (void) connection;
    (void) stage;
    (void) root;
    (void) keysym;
    (void) config;
    return false;
}


/**
 * @brief Link-only stand-in for @a ctxmenu_tree_handle_motion_window
 *
 * Reached only by 'winlist_handle_motion', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void ctxmenu_tree_handle_motion_window(ctxmenu_state_td *root,
        xcb_window_t win, int x, int y)
{
    (void) root;
    (void) win;
    (void) x;
    (void) y;
}


/**
 * @brief Link-only stand-in for @a ctxmenu_tree_redraw_window
 *
 * Reached only by 'winlist_repaint', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void ctxmenu_tree_redraw_window(ctxmenu_state_td *root, xcb_window_t win)
{
    (void) root;
    (void) win;
}


/**
 * @brief Link-only stand-in for @a ctxmenu_tree_state_find_for_window
 *
 * Reached only by 'winlist_owns_window', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
ctxmenu_state_td *ctxmenu_tree_state_find_for_window(
        ctxmenu_state_td *state, xcb_window_t win)
{
    (void) state;
    (void) win;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a enact_client_restore
 *
 * Reached only through a click on an already-built entry, which
 * nothing here does.
 *
 * @note Complexity: @e O(1)
 */
void enact_client_restore(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a enact_client_unhide
 * @note Complexity: @e O(1)
 */
void enact_client_unhide(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a enact_client_unshade
 * @note Complexity: @e O(1)
 */
void enact_client_unshade(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a enact_stage_desktop_add
 * @note Complexity: @e O(1)
 */
void enact_stage_desktop_add(stage_td *stage)
{
    (void) stage;
}


/**
 * @brief Link-only stand-in for @a enact_stage_desktop_remove
 * @note Complexity: @e O(1)
 */
void enact_stage_desktop_remove(stage_td *stage)
{
    (void) stage;
}


/**
 * @brief Link-only stand-in for @a enact_stage_desktop_switch
 * @note Complexity: @e O(1)
 */
void enact_stage_desktop_switch(stage_td *stage,
        uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;
}


/**
 * @brief Link-only stand-in for @a focus_apply
 *
 * Reached only through a click on an already-built entry, which
 * nothing here does.
 *
 * @note Complexity: @e O(1)
 */
void focus_apply(list_td *stages, stage_td *stage,
        desktop_td *desktop, client_td *client, bool raise,
        const config_td *cfg)
{
    (void) stages;
    (void) stage;
    (void) desktop;
    (void) client;
    (void) raise;
    (void) cfg;
}


/**
 * @brief Link-only stand-in for @a memguard_max_clients
 *
 * Answers zero (not restricted-memory mode), the same as an ordinary
 * session: the trailing "add/remove desktop" entries this file's
 * assertions must filter out by @c icon_window are appended, exactly
 * as they would be in the real program.
 *
 * @note Complexity: @e O(1)
 */
uint32_t memguard_max_clients(void)
{
    return 0u;
}


/**
 * @brief Link-only stand-in for @a menu_draw_truncate
 *
 * The real implementation needs a live XCB font connection to
 * measure pixel width; what this file's tests check is which
 * entries get built and how they are grouped, never the exact pixel
 * width a title is truncated to, so leaving the label untouched
 * here costs nothing.
 *
 * @note Complexity: @e O(1)
 */
void menu_draw_truncate(char *label, uint16_t max_width)
{
    (void) label;
    (void) max_width;
}


/** Desktops this file's own 'stage_desktop_get' stand-in answers
 *  from, registered by @a s_make_desktop */
#define MAX_TEST_DESKTOPS (4)
static desktop_td *s_desktops_by_id[MAX_TEST_DESKTOPS];
static int s_desktops_registered;


/**
 * @brief Test-controlled stand-in for @a stage_desktop_get
 * @note Complexity: @e O(n), where @e n is the number of desktops
 *       registered
 */
desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;

    for (int i = 0; i < s_desktops_registered; ++i) {
        if (s_desktops_by_id[i] != NULL &&
                s_desktops_by_id[i]->id == (xcb_window_t) desktop_id) {
            return s_desktops_by_id[i];
        }
    }

    return NULL;
}


/**
 * @brief Link-only stand-in for @a stage_desktop_label
 *
 * Reached only by the multi-desktop submenu path, which a
 * single-desktop stage never takes.
 *
 * @note Complexity: @e O(1)
 */
void stage_desktop_label(const stage_td *stage,
        uint32_t desktop_id, const char *desktop_name, bool is_pinned,
        bool shows_name, char *out_label, size_t length)
{
    (void) stage;
    (void) desktop_id;
    (void) desktop_name;
    (void) is_pinned;
    (void) shows_name;

    if (out_label != NULL && length > 0u) {
        out_label[0] = '\0';
    }
}


/**
 * @brief Link-only stand-in for @a stage_desktop_walk_all
 *
 * Reached only by the multi-desktop submenu path, which a
 * single-desktop stage never takes.
 *
 * @note Complexity: @e O(1)
 */
void stage_desktop_walk_all(const stage_td *stage,
        stage_desktop_visitor_fn visit, void *data)
{
    (void) stage;
    (void) visit;
    (void) data;
}


/**
 * @brief Link-only stand-in for @a text_renderer_use_font
 *
 * Called unconditionally near the top of every 'winlist_show' call,
 * ahead of anything this file cares about checking.
 *
 * @note Complexity: @e O(1)
 */
int text_renderer_use_font(xcb_connection_t *connection,
        const char *font_name)
{
    (void) connection;
    (void) font_name;
    return 0;
}


/**
 * @brief Link-only stand-in for @a wm_get_stages
 *
 * Reached only through a click on an already-built entry, which
 * nothing here does.
 *
 * @note Complexity: @e O(1)
 */
list_td *wm_get_stages(void)
{
    return NULL;
}


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
    return ((const client_td *) key1)->id == ((const client_td *) key2)->id;
}


/** Every client and desktop this file calloc's, freed in one place by
 *  @a s_teardown rather than at each test's own end */
#define MAX_TEST_CLIENTS (80)
static client_td *s_owned_clients[MAX_TEST_CLIENTS];
static int s_owned_clients_used;
static desktop_td *s_owned_desktops[MAX_TEST_DESKTOPS];
static int s_owned_desktops_used;


static desktop_td *s_make_desktop(uint32_t id)
{
    desktop_td *desktop = calloc(1, sizeof(*desktop));

    desktop->id = (xcb_window_t) id;
    desktop->clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    s_owned_desktops[s_owned_desktops_used] = desktop;
    s_owned_desktops_used++;
    s_desktops_by_id[s_desktops_registered] = desktop;
    s_desktops_registered++;

    return desktop;
}


/* A window ID doubles as its client's leader when 'leader' is
 * nonzero, so windows sharing one 'leader' value group together the
 * same way real sibling windows of one application do */
static client_td *s_make_client(uint32_t id, uint32_t desktop_id,
        uint32_t leader)
{
    client_td *client = calloc(1, sizeof(*client));

    client->id = (xcb_window_t) id;
    client->window = (xcb_window_t) id;
    client->desktop_id = desktop_id;
    client->hints_icccm.hints.client_leader = (xcb_window_t) leader;
    client->hints_icccm.hints.group_leader = XCB_WINDOW_NONE;
    s_owned_clients[s_owned_clients_used] = client;
    s_owned_clients_used++;

    return client;
}


static void s_reset(void)
{
    s_captured_state = NULL;
    s_desktops_registered = 0;
    memset(s_desktops_by_id, 0, sizeof(s_desktops_by_id));
}


static void s_teardown(void)
{
    winlist_close();

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


/* An application with more member windows on one desktop than the
 * old fixed 32-slot group buffer held used to leave the extras
 * unplaced forever, invisible in the menu; every one of them must
 * now show up somewhere, whether folded into the group or listed on
 * its own once the group itself is full */
static void s_test_no_drop_past_appgroup_cap(void)
{
    stage_td stage;
    desktop_td *desktop;
    int total_icon_windows;

    s_reset();
    memset(&stage, 0, sizeof(stage));

    desktop = s_make_desktop(0u);
    for (int i = 0; i < 40; ++i) {
        client_td *client = s_make_client(100u + (uint32_t) i, 0u,
                999u);
        ohtbl_insert(desktop->clients, client);
    }

    stage.desktops = cdlist_init(NULL);
    cdlist_ins_next(stage.desktops, NULL, desktop);
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;

    winlist_show((xcb_connection_t *) 1, &stage,
            (struct position_s) { 0, 0 }, &(config_td) { 0 });

    TAP_OK(s_captured_state != NULL, "winlist_show hands its state to"
            " ctxmenu_show");

    total_icon_windows = 0;
    for (int i = 0; i < s_captured_state->entry_count; ++i) {
        const ctxmenu_entry_td *entry = &s_captured_state->entries[i];

        /* The single-desktop path builds no submenu layer of its own
         * kind other than application groups, so every 'CTXMENU_
         * SUBMENU' entry reached here is one of those; its own
         * 'icon_window' just mirrors its first member's for display
         * on the group marker itself, not a distinct window, so only
         * its 'items' are counted, never the marker entry too */
        if (entry->type == CTXMENU_SUBMENU) {
            for (int j = 0; j < entry->item_count; ++j) {
                if (entry->items[j].icon_window != XCB_WINDOW_NONE) {
                    total_icon_windows++;
                }
            }
        } else if (entry->icon_window != XCB_WINDOW_NONE) {
            total_icon_windows++;
        }
    }
    TAP_EQ_INT(total_icon_windows, 40,
            "all 40 windows of one over-sized application group are"
            " listed somewhere, none dropped past the old 32 cap");

    cdlist_destroy(stage.desktops);
    s_teardown();
}


/* A handful of windows sharing one leader collapse into one
 * application-group submenu instead of being listed one row each */
static void s_test_small_group_collapses_to_one_submenu(void)
{
    stage_td stage;
    desktop_td *desktop;
    client_td *members[3];
    bool found_submenu;
    int submenu_item_count;

    s_reset();
    memset(&stage, 0, sizeof(stage));

    desktop = s_make_desktop(0u);
    for (int i = 0; i < 3; ++i) {
        members[i] = s_make_client(200u + (uint32_t) i, 0u, 500u);
        ohtbl_insert(desktop->clients, members[i]);
    }

    stage.desktops = cdlist_init(NULL);
    cdlist_ins_next(stage.desktops, NULL, desktop);
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;

    winlist_show((xcb_connection_t *) 1, &stage,
            (struct position_s) { 0, 0 }, &(config_td) { 0 });

    found_submenu = false;
    submenu_item_count = 0;
    for (int i = 0; i < s_captured_state->entry_count; ++i) {
        if (s_captured_state->entries[i].type == CTXMENU_SUBMENU) {
            found_submenu = true;
            submenu_item_count = s_captured_state->entries[i].item_count;
        }
    }
    TAP_OK(found_submenu,
            "three windows sharing a leader form one submenu entry");
    TAP_EQ_INT(submenu_item_count, 3,
            "and that submenu holds all three of them");

    cdlist_destroy(stage.desktops);
    s_teardown();
}


/* Windows with no shared leader are listed one row each, never
 * folded into a group */
static void s_test_ungrouped_clients_listed_singly(void)
{
    stage_td stage;
    desktop_td *desktop;
    int command_count;

    s_reset();
    memset(&stage, 0, sizeof(stage));

    desktop = s_make_desktop(0u);
    for (int i = 0; i < 3; ++i) {
        client_td *client = s_make_client(300u + (uint32_t) i, 0u,
                XCB_WINDOW_NONE);
        ohtbl_insert(desktop->clients, client);
    }

    stage.desktops = cdlist_init(NULL);
    cdlist_ins_next(stage.desktops, NULL, desktop);
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;

    winlist_show((xcb_connection_t *) 1, &stage,
            (struct position_s) { 0, 0 }, &(config_td) { 0 });

    command_count = 0;
    for (int i = 0; i < s_captured_state->entry_count; ++i) {
        if (s_captured_state->entries[i].type == CTXMENU_COMMAND &&
                s_captured_state->entries[i].icon_window !=
                    XCB_WINDOW_NONE) {
            command_count++;
        }
    }
    TAP_EQ_INT(command_count, 3,
            "three leaderless windows appear as three separate rows,"
            " never grouped together");

    cdlist_destroy(stage.desktops);
    s_teardown();
}


/* A client flagged to stay off any taskbar-like listing is left out
 * of the window list menu entirely */
static void s_test_skip_taskbar_client_is_omitted(void)
{
    stage_td stage;
    desktop_td *desktop;
    client_td *ordinary;
    client_td *skipped;
    int command_count;
    bool skipped_seen;

    s_reset();
    memset(&stage, 0, sizeof(stage));

    desktop = s_make_desktop(0u);
    ordinary = s_make_client(400u, 0u, XCB_WINDOW_NONE);
    skipped = s_make_client(401u, 0u, XCB_WINDOW_NONE);
    skipped->properties.flags |= CLIENT_FLAG_SKIP_TASKBAR;
    ohtbl_insert(desktop->clients, ordinary);
    ohtbl_insert(desktop->clients, skipped);

    stage.desktops = cdlist_init(NULL);
    cdlist_ins_next(stage.desktops, NULL, desktop);
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;

    winlist_show((xcb_connection_t *) 1, &stage,
            (struct position_s) { 0, 0 }, &(config_td) { 0 });

    command_count = 0;
    skipped_seen = false;
    for (int i = 0; i < s_captured_state->entry_count; ++i) {
        if (s_captured_state->entries[i].icon_window == skipped->window) {
            skipped_seen = true;
        }
        if (s_captured_state->entries[i].type == CTXMENU_COMMAND &&
                s_captured_state->entries[i].icon_window !=
                    XCB_WINDOW_NONE) {
            command_count++;
        }
    }
    TAP_EQ_INT(command_count, 1,
            "only the ordinary client is listed");
    TAP_OK(!skipped_seen,
            "the taskbar-skipping client never appears at all");

    cdlist_destroy(stage.desktops);
    s_teardown();
}


/* A client that is transient for another window is left out of
 * the window list menu entirely, the same as one flagged
 * CLIENT_FLAG_SKIP_TASKBAR */
static void s_test_transient_client_is_omitted(void)
{
    stage_td stage;
    desktop_td *desktop;
    client_td *ordinary;
    client_td *transient;
    int command_count;
    bool transient_seen;

    s_reset();
    memset(&stage, 0, sizeof(stage));

    desktop = s_make_desktop(0u);
    ordinary = s_make_client(400u, 0u, XCB_WINDOW_NONE);
    transient = s_make_client(401u, 0u, XCB_WINDOW_NONE);
    transient->transient_for = (xcb_window_t) 1;
    ohtbl_insert(desktop->clients, ordinary);
    ohtbl_insert(desktop->clients, transient);

    stage.desktops = cdlist_init(NULL);
    cdlist_ins_next(stage.desktops, NULL, desktop);
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;

    winlist_show((xcb_connection_t *) 1, &stage,
            (struct position_s) { 0, 0 }, &(config_td) { 0 });

    command_count = 0;
    transient_seen = false;
    for (int i = 0; i < s_captured_state->entry_count; ++i) {
        if (s_captured_state->entries[i].icon_window ==
                transient->window) {
            transient_seen = true;
        }
        if (s_captured_state->entries[i].type == CTXMENU_COMMAND &&
                s_captured_state->entries[i].icon_window !=
                    XCB_WINDOW_NONE) {
            command_count++;
        }
    }
    TAP_EQ_INT(command_count, 1,
            "only the ordinary client is listed");
    TAP_OK(!transient_seen,
            "the transient client never appears at all");

    cdlist_destroy(stage.desktops);
    s_teardown();
}


/* A pinned client physically stored on another desktop is still
 * pulled into this one's listing, the same way it visually follows
 * every desktop switch */
static void s_test_pinned_client_pulled_from_other_desktop(void)
{
    stage_td stage;
    desktop_td *shown_desktop;
    desktop_td *other_desktop;
    client_td *pinned;
    bool pinned_seen;

    s_reset();
    memset(&stage, 0, sizeof(stage));

    shown_desktop = s_make_desktop(0u);
    other_desktop = s_make_desktop(1u);
    pinned = s_make_client(500u, 1u, XCB_WINDOW_NONE);
    pinned->properties.flags |= CLIENT_FLAG_PIN;
    ohtbl_insert(other_desktop->clients, pinned);

    stage.desktops = cdlist_init(NULL);
    cdlist_ins_next(stage.desktops, NULL, shown_desktop);
    cdlist_ins_next(stage.desktops, NULL, other_desktop);
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;

    winlist_show((xcb_connection_t *) 1, &stage,
            (struct position_s) { 0, 0 }, &(config_td) { 0 });

    pinned_seen = false;
    for (int i = 0; i < s_captured_state->entry_count; ++i) {
        if (s_captured_state->entries[i].icon_window == pinned->window) {
            pinned_seen = true;
        }
    }
    TAP_OK(pinned_seen,
            "a pinned client stored on another desktop still shows up"
            " in this desktop's listing");

    cdlist_destroy(stage.desktops);
    s_teardown();
}


/* A pinned client stored on another desktop that is also transient
 * for another window is left out of this desktop's listing too,
 * pinned or not */
static void s_test_transient_pinned_client_from_other_desktop_omitted(
        void)
{
    stage_td stage;
    desktop_td *shown_desktop;
    desktop_td *other_desktop;
    client_td *pinned;
    bool pinned_seen;

    s_reset();
    memset(&stage, 0, sizeof(stage));

    shown_desktop = s_make_desktop(0u);
    other_desktop = s_make_desktop(1u);
    pinned = s_make_client(500u, 1u, XCB_WINDOW_NONE);
    pinned->properties.flags |= CLIENT_FLAG_PIN;
    pinned->transient_for = (xcb_window_t) 1;
    ohtbl_insert(other_desktop->clients, pinned);

    stage.desktops = cdlist_init(NULL);
    cdlist_ins_next(stage.desktops, NULL, shown_desktop);
    cdlist_ins_next(stage.desktops, NULL, other_desktop);
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;

    winlist_show((xcb_connection_t *) 1, &stage,
            (struct position_s) { 0, 0 }, &(config_td) { 0 });

    pinned_seen = false;
    for (int i = 0; i < s_captured_state->entry_count; ++i) {
        if (s_captured_state->entries[i].icon_window == pinned->window) {
            pinned_seen = true;
        }
    }
    TAP_OK(!pinned_seen,
            "a transient client stored on another desktop never shows"
            " up here, even while pinned");

    cdlist_destroy(stage.desktops);
    s_teardown();
}


/* An empty desktop still opens the menu, with a single non-clickable
 * placeholder entry rather than an empty or malformed one */
static void s_test_empty_desktop_shows_placeholder(void)
{
    stage_td stage;
    desktop_td *desktop;

    s_reset();
    memset(&stage, 0, sizeof(stage));

    desktop = s_make_desktop(0u);

    stage.desktops = cdlist_init(NULL);
    cdlist_ins_next(stage.desktops, NULL, desktop);
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;

    winlist_show((xcb_connection_t *) 1, &stage,
            (struct position_s) { 0, 0 }, &(config_td) { 0 });

    TAP_OK(s_captured_state != NULL && s_captured_state->entry_count >= 1,
            "an empty desktop still produces a menu, not an empty one");
    TAP_OK(s_captured_state != NULL &&
            s_captured_state->entries[0].type == CTXMENU_LABEL,
            "its first entry is the non-clickable placeholder, not a"
            " stray command");

    cdlist_destroy(stage.desktops);
    s_teardown();
}


int main(void)
{
    TAP_PLAN(13);

    s_test_no_drop_past_appgroup_cap();
    s_test_small_group_collapses_to_one_submenu();
    s_test_ungrouped_clients_listed_singly();
    s_test_skip_taskbar_client_is_omitted();
    s_test_transient_client_is_omitted();
    s_test_pinned_client_pulled_from_other_desktop();
    s_test_transient_pinned_client_from_other_desktop_omitted();
    s_test_empty_desktop_shows_placeholder();

    return TAP_DONE();
}
