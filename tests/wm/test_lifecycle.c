/**
 * @file tests/wm/test_lifecycle.c
 *
 * @brief Test battery for the window manager singleton's guard
 *        clauses, query accessors, and state-transition helpers
 *        (wm.c)
 *
 * wm.c's own two heaviest entry points, wm_start and the non-null
 * body of wm_stop's s_wm_cleanup, are not exercised here at all:
 * wm_start dials a real xcb_connect before anything else runs
 * (s_wm_connect), and s_wm_cleanup fans out into more than thirty
 * external calls across systray, XSETTINGS, IPC, the text and icon
 * renderers, session hooks, and the X connection itself.  Standing in
 * for that entire stage just to prove a pile of stubs got called in
 * some order would not exercise wm.c's own logic at all, so this file
 * instead covers exactly what is genuinely wm.c's own: the null-
 * singleton guard clause every public entry point begins with, the
 * pure state transitions wm_request_stop/wm_request_restart/
 * wm_restart_requested make on the running flag and the module-local
 * restart flag, wm_emergency_exit_enable's own flag flip, and the
 * read-only query accessors (wm_get_client_desktop,
 * wm_get_stage_by_id, wm_sync_is_available, wm_get_stages,
 * wm_get_desktop_stage, wm_get_config, wm_get_keysyms) together with
 * the three redraw-flagging functions (wm_request_client_redraw,
 * wm_request_client_reposition, wm_request_full_redraw), all of
 * which only ever read wm_td's fields and a real stages/desktops
 * list, never touch the X connection directly.
 *
 * wm.c's own translation unit is linked for real and reaches, through
 * the functions above, exactly three external symbols:
 * lookup_find_client (lookup.c), stage_desktop_walk_all
 * (stage/desktops.c), and desktop_mark_outdated (desktop.c); each is
 * a small, controllable stand-in below rather than the real
 * implementation, letting every scenario assert on wm.c's own
 * dispatch and field access directly instead of on some other
 * subsystem's traversal order.  Every other external symbol wm.c's
 * translation unit references (because wm_start, s_wm_cleanup, and
 * their own file-local helpers reference them, not because any
 * function this file actually calls ever does) is a harmless,
 * never-invoked link-only stand-in below, following
 * tests/wm/test_actions.c's own precedent for the same shape of
 * problem.
 *
 * wm.c declares its own singleton, 'wm_td *wm', at file scope, not
 * 'static', specifically so a test can reach around wm_start entirely
 * and assign a locally built 'struct wm_s' to it directly; this file
 * does exactly that; through the 'extern' declared below, the same
 * way tests/wm/test_clients.c already does for the very same global.
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
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <menu/dialog/message.h>
#include <rules.h>
#include <session.h>
#include <stage.h>
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <harness/tap.h>


/** The real singleton wm.c defines at file scope; every scenario
 *  below assigns and clears it directly instead of running wm_start */
extern wm_td *wm;


/* Recording state for the lookup_find_client stand-in */
static client_td *s_stub_found_client;
static stage_td *s_stub_found_stage;
static desktop_td *s_stub_found_desktop;
static list_td *s_lookup_last_stages;
static xcb_window_t s_lookup_last_window;

/** Link-only stand-in for lookup_find_client (lookup.c): hands back
 *  whatever a scenario set up beforehand, and records what it was
 *  asked to search for, rather than actually walking a real hash
 *  table */
client_td *lookup_find_client(list_td *stages, xcb_window_t window,
        stage_td **out_stage, desktop_td **out_desktop)
{
    s_lookup_last_stages = stages;
    s_lookup_last_window = window;
    if (out_stage != NULL) {
        *out_stage = s_stub_found_stage;
    }
    if (out_desktop != NULL) {
        *out_desktop = s_stub_found_desktop;
    }
    return s_stub_found_client;
}


/* Recording state for the stage_desktop_walk_all stand-in */
static int s_walk_call_count;
static const stage_td *s_walk_last_stage;
static stage_desktop_visitor_fn s_walk_last_visitor;
static void *s_walk_last_data;

/** Whether the stand-in below actually invokes the visitor it was
 *  given, once per desktop in 's_walk_desktops', instead of merely
 *  recording that it was reached */
static desktop_td **s_walk_desktops;
static uint32_t s_walk_desktop_count;

/** Link-only stand-in for stage_desktop_walk_all (stage/desktops.c):
 *  records every call, and, when a scenario populated
 *  's_walk_desktops', actually invokes the visitor on each of them,
 *  exactly the real function's own contract, without requiring a real
 *  cdlist */
void stage_desktop_walk_all(const stage_td *stage,
        stage_desktop_visitor_fn visit, void *data)
{
    s_walk_call_count++;
    s_walk_last_stage = stage;
    s_walk_last_visitor = visit;
    s_walk_last_data = data;

    if (visit == NULL) {
        return;
    }
    for (uint32_t i = 0u; i < s_walk_desktop_count; ++i) {
        if (s_walk_desktops[i] != NULL) {
            visit(s_walk_desktops[i], data);
        }
    }
}


/** Link-only stand-in for desktop_mark_outdated (desktop.c): flips
 *  the same flag the real one does, letting wm_request_full_redraw's
 *  own per-desktop pass be told apart from a no-op */
void desktop_mark_outdated(desktop_td *desktop)
{
    if (desktop != NULL) {
        desktop->is_outdated = true;
    }
}


/* Every stand-in below is reached only through wm_start/s_wm_cleanup
 * and their own file-local helpers, never through any function this
 * file actually calls; each is a harmless, never-invoked link-only
 * placeholder purely so wm.c's one translation unit links at all */

/** Link-only stand-in for atom_intern (utils/xcb/atom.c) */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    (void) connection;
    (void) name;
    (void) only_if_exists;
    return XCB_ATOM_NONE;
}


/** Link-only stand-in for cctl_sn_set_timeout_seconds (cctl/sn.c) */
void cctl_sn_set_timeout_seconds(uint32_t seconds)
{
    (void) seconds;
}


/** Link-only stand-in for config_destroy (config.c) */
void config_destroy(config_td *config)
{
    (void) config;
}


/** Link-only stand-in for config_init (config.c) */
config_td *config_init(void)
{
    return NULL;
}


/** Link-only stand-in for config_load (config.c) */
int config_load(config_td *config, const char *config_dir_prefix)
{
    (void) config;
    (void) config_dir_prefix;
    return 1;
}


/** Link-only stand-in for config_load_memguard (config/memguard.c) */
int config_load_memguard(config_td *config, const char *config_prefix)
{
    (void) config;
    (void) config_prefix;
    return 1;
}


/** Link-only stand-in for config_memguard_init (config/memguard.c) */
config_td *config_memguard_init(void)
{
    return NULL;
}


/** Link-only stand-in for config_missing_theme_get (config.c) */
const char *config_missing_theme_get(void)
{
    return NULL;
}


/** Link-only stand-in for config_missing_theme_reset (config.c) */
void config_missing_theme_reset(void)
{
}


/** Link-only stand-in for focus_order_destroy (policy/focus.c) */
void focus_order_destroy(void)
{
}


/** Link-only stand-in for ipc_destroy (ipc.c) */
void ipc_destroy(void)
{
}


/** Link-only stand-in for ipc_init (ipc.c) */
int ipc_init(void)
{
    return 1;
}


/** Link-only stand-in for json_syntax_errors_count
 *  (utils/config/json.c) */
uint32_t json_syntax_errors_count(void)
{
    return 0u;
}


/** Link-only stand-in for json_syntax_errors_get
 *  (utils/config/json.c) */
const char *json_syntax_errors_get(uint32_t index)
{
    (void) index;
    return NULL;
}


/** Link-only stand-in for json_syntax_errors_reset
 *  (utils/config/json.c) */
void json_syntax_errors_reset(void)
{
}


/** Link-only stand-in for logger_msg (logger.c): a silent no-op,
 *  matching the real logger's own behavior whenever logger_start has
 *  never run (its first check is 'logger == NULL'), the same
 *  reasoning tests/desktop/test_dclient.c already documents for
 *  never calling logger_start at all here either */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/** Link-only stand-in for loop_run (loop.c) */
void loop_run(wm_td *wm_instance)
{
    (void) wm_instance;
}


/** Link-only stand-in for memguard_init (memguard.c) */
void memguard_init(uint32_t ceiling_mib)
{
    (void) ceiling_mib;
}


/** Link-only stand-in for menu_message_dialog_show
 *  (menu/dialog/message.c) */
void menu_message_dialog_show(xcb_connection_t *connection,
        stage_td *stage, const config_td *config, const char *message,
        menu_msg_level_e level)
{
    (void) connection;
    (void) stage;
    (void) config;
    (void) message;
    (void) level;
}


/** Link-only stand-in for mouse_resize_cursors_destroy
 *  (input/mouse/cursor.c) */
void mouse_resize_cursors_destroy(xcb_connection_t *connection)
{
    (void) connection;
}


/** Link-only stand-in for mouse_resize_cursors_init
 *  (input/mouse/cursor.c) */
void mouse_resize_cursors_init(xcb_connection_t *connection)
{
    (void) connection;
}


/** Link-only stand-in for rootmenu_menu_json_free
 *  (menu/context/rootmenu.c) */
void rootmenu_menu_json_free(void)
{
}


/** Link-only stand-in for rootmenu_menu_json_load
 *  (menu/context/rootmenu.c) */
void rootmenu_menu_json_load(const char *config_dir)
{
    (void) config_dir;
}


/** Link-only stand-in for rules_destroy (rules.c) */
void rules_destroy(rules_td *rules)
{
    (void) rules;
}


/** Link-only stand-in for rules_init (rules.c) */
rules_td *rules_init(void)
{
    return NULL;
}


/** Link-only stand-in for rules_load (rules.c) */
int rules_load(rules_td *rules, const char *config_dir_prefix)
{
    (void) rules;
    (void) config_dir_prefix;
    return 1;
}


/** Link-only stand-in for session_destroy (session.c) */
void session_destroy(session_td *session)
{
    (void) session;
}


/** Link-only stand-in for session_init (session.c) */
session_td *session_init(void)
{
    return NULL;
}


/** Link-only stand-in for session_load (session.c) */
int session_load(session_td *session, const char *config_dir_prefix)
{
    (void) session;
    (void) config_dir_prefix;
    return 1;
}


/** Link-only stand-in for session_run_hook (session.c) */
void session_run_hook(const session_td *session, enum session_hook_e hook)
{
    (void) session;
    (void) hook;
}


/** Link-only stand-in for stage_destroy (stage.c) */
void stage_destroy(stage_td *stage)
{
    (void) stage;
}


/** Link-only stand-in for stage_init (stage.c) */
stage_td *stage_init(xcb_connection_t *connection, uint32_t screen_num,
        uint32_t desktop_count, config_td *config)
{
    (void) connection;
    (void) screen_num;
    (void) desktop_count;
    (void) config;
    return NULL;
}


/** Link-only stand-in for sysmem_available_mib (utils/sysmem.c) */
bool sysmem_available_mib(uint32_t *out_mib)
{
    (void) out_mib;
    return false;
}


/** Link-only stand-in for systray_init (systray.c) */
void systray_init(const wm_td *wm_instance)
{
    (void) wm_instance;
}


/** Link-only stand-in for systray_shutdown (systray.c) */
void systray_shutdown(wm_td *wm_instance)
{
    (void) wm_instance;
}


/** Link-only stand-in for text_renderer_destroy (render/text.c) */
void text_renderer_destroy(void)
{
}


/** Link-only stand-in for wmicon_renderer_destroy (render/wmicon.c) */
void wmicon_renderer_destroy(void)
{
}


/** Link-only stand-in for text_renderer_disable_glyph_backend
 *  (render/text.c) */
void text_renderer_disable_glyph_backend(void)
{
}


/** Link-only stand-in for text_renderer_init (render/text.c) */
int text_renderer_init(const xcb_connection_t *connection)
{
    (void) connection;
    return 1;
}


/** Link-only stand-in for winlist_close (menu/context/winlist.c) */
void winlist_close(void)
{
}


/** Link-only stand-in for wm_startup_acquire_selection
 *  (wm/startup/selection.c) */
int wm_startup_acquire_selection(wm_td *wm_instance, bool replace_requested)
{
    (void) wm_instance;
    (void) replace_requested;
    return -1;
}


/** Link-only stand-in for wm_startup_randr_init (wm/startup.c) */
int wm_startup_randr_init(wm_td *wm_instance)
{
    (void) wm_instance;
    return -1;
}


/** Link-only stand-in for wm_startup_subscribe_randr_events
 *  (wm/startup/subscribe.c) */
int wm_startup_subscribe_randr_events(const wm_td *wm_instance)
{
    (void) wm_instance;
    return -1;
}


/** Link-only stand-in for wm_startup_subscribe_root_events
 *  (wm/startup/subscribe.c) */
int wm_startup_subscribe_root_events(const wm_td *wm_instance)
{
    (void) wm_instance;
    return -1;
}


/** Link-only stand-in for wm_startup_sync_init (wm/startup.c) */
int wm_startup_sync_init(wm_td *wm_instance)
{
    (void) wm_instance;
    return -1;
}


/** Link-only stand-in for wm_ewmh_init (wm/ewmh.c) */
int wm_ewmh_init(const wm_td *wm_instance)
{
    (void) wm_instance;
    return 1;
}


/** Link-only stand-in for wm_ewmh_sync (wm/ewmh.c) */
void wm_ewmh_sync(wm_td *wm_instance)
{
    (void) wm_instance;
}


/** Link-only stand-in for wm_client_unmanage_all (wm/client.c) */
void wm_client_unmanage_all(const wm_td *wm_instance)
{
    (void) wm_instance;
}


/** Link-only stand-in for wm_shutdown_begin (wm/shutdown.c) */
void wm_shutdown_begin(const wm_td *wm_instance)
{
    (void) wm_instance;
}


/** Link-only stand-in for xsettings_init (xsettings.c) */
void xsettings_init(const wm_td *wm_instance)
{
    (void) wm_instance;
}


/** Link-only stand-in for xsettings_shutdown (xsettings.c) */
void xsettings_shutdown(wm_td *wm_instance)
{
    (void) wm_instance;
}


/** Link-only stand-in for xcb_connect (libxcb) */
xcb_connection_t *xcb_connect(const char *displayname, int *screenp)
{
    (void) displayname;
    (void) screenp;
    return NULL;
}


/** Link-only stand-in for xcb_connection_has_error (libxcb) */
int xcb_connection_has_error(xcb_connection_t *c)
{
    (void) c;
    return 1;
}


/** Link-only stand-in for xcb_connection_set (utils/xcb/connection.c) */
void xcb_connection_set(xcb_connection_t *connection)
{
    (void) connection;
}


/** Link-only stand-in for xcb_disconnect (libxcb) */
void xcb_disconnect(xcb_connection_t *c)
{
    (void) c;
}


/** Link-only stand-in for xcb_ewmh_connection_set
 *  (utils/xcb/connection.c) */
void xcb_ewmh_connection_set(xcb_ewmh_connection_t *ewmh)
{
    (void) ewmh;
}


/* xcb_ewmh_connection_wipe, xcb_ewmh_init_atoms, and
 * xcb_ewmh_init_atoms_replies are all real, genuinely linkable
 * libxcb-ewmh library functions, not project-internal symbols, so
 * none of the three is stood in for here: the real 'xcb_ewmh.h'
 * prototypes are pulled in unchanged through 'wm.h' above, and the
 * real library implementation is linked via '-lxcb-ewmh' */


/** Link-only stand-in for xcb_flush (libxcb) */
int xcb_flush(xcb_connection_t *c)
{
    (void) c;
    return 1;
}


/** Link-only stand-in for xcb_get_setup (libxcb) */
const xcb_setup_t *xcb_get_setup(xcb_connection_t *c)
{
    (void) c;
    return NULL;
}


/** Link-only stand-in for xcb_screen_next (libxcb) */
void xcb_screen_next(xcb_screen_iterator_t *i)
{
    (void) i;
}


/** Link-only stand-in for xcb_set_selection_owner (libxcb) */
xcb_void_cookie_t xcb_set_selection_owner(xcb_connection_t *c,
        xcb_window_t owner, xcb_atom_t selection, xcb_timestamp_t time)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) owner;
    (void) selection;
    (void) time;
    return cookie;
}


/** Link-only stand-in for xcb_setup_roots_iterator (libxcb) */
xcb_screen_iterator_t xcb_setup_roots_iterator(const xcb_setup_t *r)
{
    xcb_screen_iterator_t it;

    (void) r;
    memset(&it, 0, sizeof(it));
    return it;
}


/** Link-only stand-in for xcb_window_destroy (utils/xcb/window.c) */
void xcb_window_destroy(xcb_window_t window)
{
    (void) window;
}


/**
 * @brief Reset every recording global back to its starting state
 */
static void s_stub_reset(void)
{
    s_stub_found_client = NULL;
    s_stub_found_stage = NULL;
    s_stub_found_desktop = NULL;
    s_lookup_last_stages = NULL;
    s_lookup_last_window = XCB_NONE;
    s_walk_call_count = 0;
    s_walk_last_stage = NULL;
    s_walk_last_visitor = NULL;
    s_walk_last_data = NULL;
    s_walk_desktops = NULL;
    s_walk_desktop_count = 0u;
}


/**
 * @brief Build a minimal, real 'struct wm_s' on the stack
 */
static wm_td s_make_wm(void)
{
    wm_td local_wm;

    memset(&local_wm, 0, sizeof(local_wm));
    return local_wm;
}


/* Every public entry point that begins with a null-singleton guard
 * refuses cleanly, touching nothing else, when the singleton has
 * never been started */
static void s_test_null_singleton_guards(void)
{
    client_td client;
    desktop_td desktop;

    memset(&client, 0, sizeof(client));
    memset(&desktop, 0, sizeof(desktop));
    wm = NULL;

    TAP_EQ_INT(wm_stop(), 1,
            "wm_stop on a null singleton reports no-op");
    TAP_EQ_INT(wm_request_stop(), 1,
            "wm_request_stop on a null singleton reports failure");
    TAP_EQ_INT(wm_request_restart(), 1,
            "wm_request_restart on a null singleton reports failure");
    TAP_OK(wm_get_client_desktop(&client) == NULL,
            "wm_get_client_desktop on a null singleton returns NULL");
    TAP_OK(wm_get_stage_by_id(0u) == NULL,
            "wm_get_stage_by_id on a null singleton returns NULL");
    TAP_EQ_INT(wm_sync_is_available(), false,
            "wm_sync_is_available on a null singleton is false");
    TAP_OK(wm_get_stages() == NULL,
            "wm_get_stages on a null singleton returns NULL");
    TAP_OK(wm_get_desktop_stage(&desktop) == NULL,
            "wm_get_desktop_stage on a null singleton returns NULL");
    TAP_OK(wm_get_config() == NULL,
            "wm_get_config on a null singleton returns NULL");
    TAP_OK(wm_get_keysyms() == NULL,
            "wm_get_keysyms on a null singleton returns NULL");

    /* Neither of these has any observable state to check on a null
     * singleton beyond simply not crashing */
    wm_request_client_redraw(&client);
    wm_request_full_redraw();
    wm_emergency_exit_enable();
    TAP_OK(true, "wm_request_client_redraw/wm_request_full_redraw/" \
            "wm_emergency_exit_enable are safe no-ops on a null" \
            " singleton");
}


/* wm_request_stop flips is_running to false and reports success on an
 * initialized singleton, leaving the restart flag untouched */
static void s_test_request_stop_flips_running_flag(void)
{
    wm_td local_wm = s_make_wm();

    local_wm.is_running = true;
    wm = &local_wm;

    TAP_EQ_INT(wm_request_stop(), 0,
            "wm_request_stop on an initialized singleton succeeds");
    TAP_OK(!local_wm.is_running,
            "wm_request_stop clears the running flag");
    TAP_OK(!wm_restart_requested(),
            "wm_request_stop alone never sets the restart flag");

    wm = NULL;
}


/* wm_request_restart flips is_running to false exactly like
 * wm_request_stop, but also marks the module-local restart flag,
 * which wm_restart_requested reports back afterward, including once
 * the singleton itself is gone again */
static void s_test_request_restart_sets_restart_flag(void)
{
    wm_td local_wm = s_make_wm();

    local_wm.is_running = true;
    wm = &local_wm;

    TAP_EQ_INT(wm_request_restart(), 0,
            "wm_request_restart on an initialized singleton succeeds");
    TAP_OK(!local_wm.is_running,
            "wm_request_restart clears the running flag exactly like"
            " wm_request_stop");
    TAP_OK(wm_restart_requested(),
            "wm_request_restart sets the restart flag");

    wm = NULL;
    TAP_OK(wm_restart_requested(),
            "the restart flag survives after the singleton is torn"
            " down, since it lives independent of wm_td itself");
}


/* wm_emergency_exit_enable sets the emergency exit flag on an
 * initialized singleton, and only on that singleton, without touching
 * anything else */
static void s_test_emergency_exit_enable_sets_flag(void)
{
    wm_td local_wm = s_make_wm();

    local_wm.is_emergency_exit = false;
    wm = &local_wm;

    wm_emergency_exit_enable();

    TAP_OK(local_wm.is_emergency_exit,
            "wm_emergency_exit_enable sets the emergency exit flag");

    wm = NULL;
}


/* wm_get_client_desktop forwards to lookup_find_client with the
 * singleton's own stages list and the client's window, and returns
 * exactly whichever desktop it reports back */
static void s_test_get_client_desktop_forwards_lookup(void)
{
    wm_td local_wm = s_make_wm();
    list_td stages_list_storage;
    client_td client;
    desktop_td desktop;
    desktop_td *result;

    memset(&stages_list_storage, 0, sizeof(stages_list_storage));
    memset(&client, 0, sizeof(client));
    memset(&desktop, 0, sizeof(desktop));
    client.id = 0x1234u;

    s_stub_reset();
    s_stub_found_desktop = &desktop;
    local_wm.stages = &stages_list_storage;
    wm = &local_wm;

    result = wm_get_client_desktop(&client);

    TAP_OK(result == &desktop,
            "wm_get_client_desktop returns exactly the desktop"
            " lookup_find_client resolved");
    TAP_OK(s_lookup_last_stages == &stages_list_storage,
            "wm_get_client_desktop passes the singleton's own"
            " stages list through to lookup_find_client");
    TAP_EQ_INT((long) s_lookup_last_window, (long) client.id,
            "wm_get_client_desktop passes the client's window/id"
            " through to lookup_find_client unchanged");

    wm = NULL;
}


/* A null client argument is refused outright, never reaching
 * lookup_find_client at all */
static void s_test_get_client_desktop_null_client(void)
{
    wm_td local_wm = s_make_wm();

    s_stub_reset();
    wm = &local_wm;

    TAP_OK(wm_get_client_desktop(NULL) == NULL,
            "wm_get_client_desktop on a null client returns NULL");
    TAP_EQ_INT(s_lookup_last_window, XCB_NONE,
            "a null client never reaches lookup_find_client at all");

    wm = NULL;
}


/* wm_get_stage_by_id scans the singleton's stages and returns the
 * one whose id matches, skipping any null entries along the way, and
 * returns NULL when none matches or the stages list itself is
 * NULL */
static void s_test_get_stage_by_id(void)
{
    wm_td local_wm = s_make_wm();
    stage_td stage_a;
    stage_td stage_b;
    list_td *stages;

    memset(&stage_a, 0, sizeof(stage_a));
    memset(&stage_b, 0, sizeof(stage_b));
    stage_a.id = 0u;
    stage_b.id = 5u;

    stages = list_init(NULL);
    list_ins_next(stages, NULL, NULL);
    list_ins_next(stages, list_tail(stages), &stage_a);
    list_ins_next(stages, list_tail(stages), &stage_b);

    local_wm.stages = stages;
    wm = &local_wm;

    TAP_OK(wm_get_stage_by_id(5u) == &stage_b,
            "wm_get_stage_by_id finds the matching stage,"
            " skipping a null entry along the way");
    TAP_OK(wm_get_stage_by_id(99u) == NULL,
            "wm_get_stage_by_id returns NULL when no stage"
            " matches");

    local_wm.stages = NULL;
    TAP_OK(wm_get_stage_by_id(0u) == NULL,
            "wm_get_stage_by_id on a NULL stages list returns"
            " NULL");

    list_destroy(stages);
    wm = NULL;
}


/* wm_sync_is_available reports exactly the singleton's own
 * is_sync_available flag */
static void s_test_sync_is_available_reflects_flag(void)
{
    wm_td local_wm = s_make_wm();

    local_wm.is_sync_available = true;
    wm = &local_wm;
    TAP_EQ_INT(wm_sync_is_available(), true,
            "wm_sync_is_available reflects a true flag");

    local_wm.is_sync_available = false;
    TAP_EQ_INT(wm_sync_is_available(), false,
            "wm_sync_is_available reflects a false flag");

    wm = NULL;
}


/* wm_get_stages/wm_get_config/wm_get_keysyms are plain field
 * accessors on the singleton, answering exactly what was stored */
static void s_test_plain_field_accessors(void)
{
    wm_td local_wm = s_make_wm();
    list_td stages_storage;
    config_td config_storage;
    xcb_key_symbols_t *const fake_keysyms =
        (xcb_key_symbols_t *) &stages_storage;

    memset(&stages_storage, 0, sizeof(stages_storage));
    memset(&config_storage, 0, sizeof(config_storage));
    local_wm.stages = &stages_storage;
    local_wm.config = &config_storage;
    local_wm.keysyms = fake_keysyms;
    wm = &local_wm;

    TAP_OK(wm_get_stages() == &stages_storage,
            "wm_get_stages returns the singleton's own stages"
            " list");
    TAP_OK(wm_get_config() == &config_storage,
            "wm_get_config returns the singleton's own configuration");
    TAP_OK(wm_get_keysyms() == fake_keysyms,
            "wm_get_keysyms returns the singleton's own key symbols"
            " table");

    wm = NULL;
}


/* wm_get_desktop_stage walks every stage's desktops (through the
 * stage_desktop_walk_all stand-in) and returns whichever stage's own
 * walk actually contains the target desktop */
static void s_test_get_desktop_stage_finds_owner(void)
{
    wm_td local_wm = s_make_wm();
    stage_td stage_a;
    stage_td stage_b;
    desktop_td desktop_on_b;
    desktop_td *desktops_of_b[1];
    list_td *stages;

    memset(&stage_a, 0, sizeof(stage_a));
    memset(&stage_b, 0, sizeof(stage_b));
    memset(&desktop_on_b, 0, sizeof(desktop_on_b));
    stage_a.id = 0u;
    stage_b.id = 1u;

    stages = list_init(NULL);
    list_ins_next(stages, NULL, &stage_a);
    list_ins_next(stages, list_tail(stages), &stage_b);

    local_wm.stages = stages;
    wm = &local_wm;

    /* stage_a's own walk (invoked while wm_get_desktop_stage
     * checks it first) finds nothing, since 's_walk_desktops' is
     * still empty for that call; only once the loop reaches
     * stage_b does the stand-in below get told about
     * 'desktop_on_b' */
    s_stub_reset();
    desktops_of_b[0] = &desktop_on_b;

    /* The stand-in below cannot tell which stage is being walked
     * apart from another on its own, so this scenario instead proves
     * the overall contract end to end: with 's_walk_desktops' set for
     * every call, the desktop is found on whichever stage the walk
     * reports it on, here the very first one reached */
    s_walk_desktops = desktops_of_b;
    s_walk_desktop_count = 1u;

    TAP_OK(wm_get_desktop_stage(&desktop_on_b) == &stage_a,
            "wm_get_desktop_stage returns the first stage whose"
            " own walk reports the target desktop");
    TAP_OK(wm_get_desktop_stage(NULL) == NULL,
            "wm_get_desktop_stage on a null desktop returns NULL");

    local_wm.stages = NULL;
    TAP_OK(wm_get_desktop_stage(&desktop_on_b) == NULL,
            "wm_get_desktop_stage on a NULL stages list returns"
            " NULL");

    list_destroy(stages);
    wm = NULL;
}


/* wm_request_client_redraw marks the client itself outdated, marks
 * the desktop lookup_find_client resolves for it outdated, and marks
 * whichever stage's id matches the client's screen_id outdated too,
 * leaving every other stage untouched */
static void s_test_request_client_redraw_marks_owner_chain(void)
{
    wm_td local_wm = s_make_wm();
    stage_td stage_a;
    stage_td stage_b;
    desktop_td desktop;
    client_td client;
    list_td *stages;

    memset(&stage_a, 0, sizeof(stage_a));
    memset(&stage_b, 0, sizeof(stage_b));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    stage_a.id = 0u;
    stage_b.id = 1u;
    client.screen_id = 1u;
    client.is_outdated = false;
    desktop.is_outdated = false;

    stages = list_init(NULL);
    list_ins_next(stages, NULL, &stage_a);
    list_ins_next(stages, list_tail(stages), &stage_b);

    local_wm.stages = stages;
    wm = &local_wm;

    s_stub_reset();
    s_stub_found_desktop = &desktop;

    wm_request_client_redraw(&client);

    TAP_OK(client.is_outdated,
            "wm_request_client_redraw marks the client itself"
            " outdated");
    TAP_OK(desktop.is_outdated,
            "wm_request_client_redraw marks the client's own desktop"
            " outdated");
    TAP_OK(stage_b.is_outdated,
            "wm_request_client_redraw marks the stage whose id"
            " matches the client's screen_id outdated");
    TAP_OK(!stage_a.is_outdated,
            "wm_request_client_redraw leaves an unrelated stage"
            " untouched");
    TAP_OK(client.needs_decoration_repaint,
            "wm_request_client_redraw also asks for a decoration"
            " repaint, not just a reposition");

    list_destroy(stages);
    wm = NULL;
}


/* wm_request_client_reposition marks the same owner chain outdated
 * as wm_request_client_redraw, but leaves needs_decoration_repaint
 * false: a plain move never changes how the frame border or
 * titlebar look, only where the frame sits on screen */
static void s_test_request_client_reposition_skips_decoration(void)
{
    wm_td local_wm = s_make_wm();
    stage_td stage;
    desktop_td desktop;
    client_td client;
    list_td *stages;

    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    stage.id = 0u;
    client.screen_id = 0u;
    client.is_outdated = false;
    client.needs_decoration_repaint = false;
    desktop.is_outdated = false;

    stages = list_init(NULL);
    list_ins_next(stages, NULL, &stage);

    local_wm.stages = stages;
    wm = &local_wm;

    s_stub_reset();
    s_stub_found_desktop = &desktop;

    wm_request_client_reposition(&client);

    TAP_OK(client.is_outdated,
            "wm_request_client_reposition marks the client itself"
            " outdated, the same as wm_request_client_redraw");
    TAP_OK(desktop.is_outdated,
            "...and its owner desktop too");
    TAP_OK(stage.is_outdated,
            "...and its owner stage too");
    TAP_OK(!client.needs_decoration_repaint,
            "...but never asks for a decoration repaint on its own");

    list_destroy(stages);
    wm = NULL;
}


/* A decoration repaint already pending from an earlier call stays
 * pending through a later wm_request_client_reposition; one quiet
 * call must never cancel a repaint some other, real reason already
 * asked for */
static void s_test_request_client_reposition_keeps_pending_repaint(void)
{
    wm_td local_wm = s_make_wm();
    stage_td stage;
    desktop_td desktop;
    client_td client;
    list_td *stages;

    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    stage.id = 0u;
    client.screen_id = 0u;

    stages = list_init(NULL);
    list_ins_next(stages, NULL, &stage);

    local_wm.stages = stages;
    wm = &local_wm;

    s_stub_reset();
    s_stub_found_desktop = &desktop;

    wm_request_client_redraw(&client);
    wm_request_client_reposition(&client);

    TAP_OK(client.needs_decoration_repaint,
            "a decoration repaint already pending stays pending"
            " through a later, quiet reposition call");

    list_destroy(stages);
    wm = NULL;
}


/* wm_request_full_redraw marks every stage, and every desktop of
 * every stage (through the stage_desktop_walk_all stand-in actually
 * invoking desktop_mark_outdated), outdated */
static void s_test_request_full_redraw_marks_everything(void)
{
    wm_td local_wm = s_make_wm();
    stage_td stage_a;
    stage_td stage_b;
    desktop_td desktop_a;
    desktop_td desktop_b;
    desktop_td *desktops[2];
    list_td *stages;

    memset(&stage_a, 0, sizeof(stage_a));
    memset(&stage_b, 0, sizeof(stage_b));
    memset(&desktop_a, 0, sizeof(desktop_a));
    memset(&desktop_b, 0, sizeof(desktop_b));

    stages = list_init(NULL);
    list_ins_next(stages, NULL, &stage_a);
    list_ins_next(stages, list_tail(stages), &stage_b);

    local_wm.stages = stages;
    wm = &local_wm;

    s_stub_reset();
    desktops[0] = &desktop_a;
    desktops[1] = &desktop_b;
    s_walk_desktops = desktops;
    s_walk_desktop_count = 2u;

    wm_request_full_redraw();

    TAP_OK(stage_a.is_outdated && stage_b.is_outdated,
            "wm_request_full_redraw marks every stage outdated");
    TAP_OK(desktop_a.is_outdated && desktop_b.is_outdated,
            "wm_request_full_redraw's per-stage walk marks every"
            " desktop outdated through desktop_mark_outdated");
    TAP_EQ_INT(s_walk_call_count, 2,
            "stage_desktop_walk_all is invoked exactly once per"
            " stage");

    list_destroy(stages);
    wm = NULL;
}


int main(void)
{
    TAP_PLAN(48);

    s_test_null_singleton_guards();
    s_test_request_stop_flips_running_flag();
    s_test_request_restart_sets_restart_flag();
    s_test_emergency_exit_enable_sets_flag();
    s_test_get_client_desktop_forwards_lookup();
    s_test_get_client_desktop_null_client();
    s_test_get_stage_by_id();
    s_test_sync_is_available_reflects_flag();
    s_test_plain_field_accessors();
    s_test_get_desktop_stage_finds_owner();
    s_test_request_client_redraw_marks_owner_chain();
    s_test_request_client_reposition_skips_decoration();
    s_test_request_client_reposition_keeps_pending_repaint();
    s_test_request_full_redraw_marks_everything();

    return TAP_DONE();
}
