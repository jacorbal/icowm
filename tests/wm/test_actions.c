/**
 * @file tests/wm/test_actions.c
 *
 * @brief Test battery for wm_action_rearrange (wm/actions.c)
 *
 * wm/actions.c compiles as a single translation unit holding two
 * public functions: wm_action_rearrange, a thin three-line dispatcher,
 * and wm_action_config_reload, which reaches into configuration
 * loading, keyboard/mouse grab re-establishment, the root menu, the
 * systray, XSETTINGS, session hooks, and 's_resync_after_reload' own
 * per-client theme resync and systray-overlap icon repositioning, an
 * entire startup-adjacent subsystem's worth of real X server state
 * this file has no live connection to drive.  Only wm_action_rearrange
 * is exercised for real here; every other external symbol
 * wm/actions.c's own translation unit references (because
 * wm_action_config_reload and its file-local helpers reference them,
 * not because wm_action_rearrange itself ever does) is a link-only
 * stand-in below, never actually reached by any scenario in this file,
 * exactly the same shape test_desktop_add_remove.c already uses for
 * the handful of symbols stage/switch.c pulls in that its own tests
 * never reach either.
 *
 * wm_action_rearrange itself calls exactly two functions:
 * lookup_current_desktop (lookup.c) and enact_desktop_client_rearrange_all
 * (enact.c); both are recording stand-ins here, letting every scenario
 * assert directly on whether, and with which desktop, the rearrange
 * actually happened, rather than reimplementing placement policy by
 * hand to check its output.
 *
 * wm/internal.h's real 'struct wm_s' is used directly (this file
 * builds one on the stack, never through wm_start), rather than an
 * opaque handle stood in for through the wm_* accessors used in
 * tests/rules/test_apply.c: wm_action_rearrange never touches any of
 * 'wm_s''s own fields (it only ever forwards the pointer, unread, to
 * enact_desktop_client_rearrange_all, itself a stand-in here), so any
 * distinguishable non-null pointer would do equally well; a real
 * 'struct wm_s' is used anyway since wm/internal.h is already a
 * legitimate include for anything under tests/wm/, matching
 * tests/wm/test_clients.c's own approach.
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

/* Local includes */
#include <desktop.h>
#include <enact.h>
#include <harness/tap.h>
#include <lookup.h>
#include <session.h>
#include <stage.h>
#include <wm.h>
#include <wm/internal.h>


/** Desktop lookup_current_desktop hands back on its next call, set by
 *  each scenario before calling wm_action_rearrange */
static desktop_td *s_stub_current_desktop = NULL;

/** Number of times enact_desktop_client_rearrange_all was actually
 *  reached, and the arguments of its most recent call; what every
 *  scenario below asserts on */
static int s_rearrange_call_count = 0;
static const wm_td *s_rearrange_last_wm = NULL;
static stage_td *s_rearrange_last_stage = NULL;
static const desktop_td *s_rearrange_last_desktop = NULL;


/**
 * @brief Reset every recording global back to its starting state,
 *        ready for the next scenario
 */
static void s_stub_reset(void)
{
    s_stub_current_desktop = NULL;
    s_rearrange_call_count = 0;
    s_rearrange_last_wm = NULL;
    s_rearrange_last_stage = NULL;
    s_rearrange_last_desktop = NULL;
}


/** Link-only stand-in for lookup_current_desktop (lookup.c): hands
 *  back whichever desktop the scenario set up beforehand, ignoring
 *  'stage' itself, since nothing here builds a real stage/desktop
 *  cdlist relationship for it to search */
desktop_td *lookup_current_desktop(stage_td *stage)
{
    (void) stage;
    return s_stub_current_desktop;
}


/** Link-only stand-in for enact_desktop_client_rearrange_all (enact.c):
 *  records that it was reached, and with what, instead of actually
 *  touching any client */
void enact_desktop_client_rearrange_all(const wm_td *wm, stage_td *stage,
        const desktop_td *desktop)
{
    ++s_rearrange_call_count;
    s_rearrange_last_wm = wm;
    s_rearrange_last_stage = stage;
    s_rearrange_last_desktop = desktop;
}


/* Every stand-in below is reached only through wm/actions.c's other
 * public function, wm_action_config_reload, and its own file-local
 * helpers, never through wm_action_rearrange itself, which no
 * scenario in this file calls; each is a harmless, never-actually-
 * invoked link-only placeholder purely so this one translation unit
 * links at all */

/** Link-only stand-in for cctl_sn_set_timeout_seconds (cctl/sn.c) */
/** Link-only stand-in for @a viewport_mesh_cache_invalidate: the mesh
 *  is dropped whenever the desktop background may have changed under
 *  it, which this file has no mesh to drop */
void viewport_mesh_cache_invalidate(void)
{
}


void cctl_sn_set_timeout_seconds(uint32_t seconds)
{
    (void) seconds;
}


/** Link-only stand-in for client_theme_layout_resync (client.c) */
void client_theme_layout_resync(client_td *client, bool is_active)
{
    (void) client;
    (void) is_active;
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


/** Link-only stand-in for config_missing_theme_reset (config.c) */
void config_missing_theme_reset(void)
{
}


/** Link-only stand-in for dialog_rrsafe_show (menu/dialog/rrsafe.c) */
void dialog_rrsafe_show(xcb_connection_t *connection, stage_td *stage,
        config_td *config)
{
    (void) connection;
    (void) stage;
    (void) config;
}


/** Link-only stand-in for json_syntax_errors_reset
 *  (utils/config/json.c) */
void json_syntax_errors_reset(void)
{
}


/** Link-only stand-in for keyboard_load (input/kbd/bind.c) */
void keyboard_load(list_td *stages, xcb_key_symbols_t *keysyms,
        config_td *config)
{
    (void) stages;
    (void) keysyms;
    (void) config;
}


/** Link-only stand-in for mouse_load (input/mouse/bind.c) */
void mouse_load(list_td *stages, const config_td *config)
{
    (void) stages;
    (void) config;
}


/** Link-only stand-in for place_icon_avoid_systray_overlap
 *  (policy/placement/icon.c) */
bool place_icon_avoid_systray_overlap(
        const int16_t *restrict io_x, int16_t *restrict io_y,
        struct dimensions_s icon_dim, struct geometry_s tray,
        const struct geometry_s *workarea)
{
    (void) io_x;
    (void) io_y;
    (void) icon_dim;
    (void) tray;
    (void) workarea;
    return false;
}


/** Link-only stand-in for rootmenu_close (menu/context/rootmenu.c) */
void rootmenu_close(void)
{
}


/** Link-only stand-in for rootmenu_menu_json_load
 *  (menu/context/rootmenu.c) */
void rootmenu_menu_json_load(const char *config_dir)
{
    (void) config_dir;
}


/** Link-only stand-in for rules_load (rules.c) */
int rules_load(rules_td *rules, const char *config_dir_prefix)
{
    (void) rules;
    (void) config_dir_prefix;
    return 1;
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


/** Link-only stand-in for stage_action_randr_snapshot_begin
 *  (stage/actions/randr.c) */
void stage_action_randr_snapshot_begin(void)
{
}


/** Link-only stand-in for stage_action_randr_apply_profiles
 *  (stage/actions/randr.c) */
bool stage_action_randr_apply_profiles(stage_td *stage,
        bool take_snapshot)
{
    (void) stage;
    (void) take_snapshot;
    return false;
}


/**
 * @brief Link-only stand-in for scmd_stage_viewport_reclamp
 *
 * Reached only from 's_desktop_reload_visit' (wm/actions.c, static,
 * unreachable from here except through 'wm_action_config_reload'
 * itself), which no test in this file drives far enough to call it:
 * 'config_load' below always fails, and 'stage_desktop_walk_all' just
 * below visits nothing, so nothing in the reload path past that point
 * is exercised here at all, this one line no differently than every
 * other one already in it.
 */
void scmd_stage_viewport_reclamp(stage_td *stage,
        desktop_td *desktop)
{
    (void) stage;
    (void) desktop;
}


/** Link-only stand-in for stage_desktop_walk_all (stage/desktops.c) */
void stage_desktop_walk_all(const stage_td *stage,
        stage_desktop_visitor_fn visit, void *data)
{
    (void) stage;
    (void) visit;
    (void) data;
}


/** Link-only stand-in for stage_workarea_refresh_all
 *  (stage/workareas.c) */
void stage_workarea_refresh_all(stage_td *stage)
{
    (void) stage;
}


/** Link-only stand-in for systray_get_geometry (systray.c) */
bool systray_get_geometry(const stage_td *stage,
        struct geometry_s *restrict out_tray)
{
    (void) stage;
    (void) out_tray;
    return false;
}


/** Link-only stand-in for systray_reload (systray.c) */
void systray_reload(const wm_td *wm)
{
    (void) wm;
}


/** Link-only stand-in for wm_request_client_redraw (wm.c) */
void wm_request_client_redraw(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for xsettings_reload (xsettings.c) */
void xsettings_reload(const wm_td *wm)
{
    (void) wm;
}


/** Link-only stand-in for wm_connection (wm/instance.c): only ever
 *  reached from wm_action_config_reload's own translation unit, never
 *  by wm_action_rearrange */
xcb_connection_t *wm_connection(const wm_td *wm)
{
    (void) wm;
    return NULL;
}


/** Link-only stand-in for wm_config (wm/instance.c) */
config_td *wm_config(const wm_td *wm)
{
    (void) wm;
    return NULL;
}


/** Link-only stand-in for wm_config_dir_prefix (wm/instance.c) */
const char *wm_config_dir_prefix(const wm_td *wm)
{
    (void) wm;
    return NULL;
}


/** Link-only stand-in for wm_get_config (wm.c) */
config_td *wm_get_config(void)
{
    return NULL;
}


/** Link-only stand-in for wm_json_syntax_errors_warn (wm.c) */
void wm_json_syntax_errors_warn(void)
{
}


/** Link-only stand-in for wm_keysyms (wm/instance.c) */
xcb_key_symbols_t *wm_keysyms(const wm_td *wm)
{
    (void) wm;
    return NULL;
}


/** Link-only stand-in for wm_restricted_memory_mib (wm/instance.c) */
uint32_t wm_restricted_memory_mib(const wm_td *wm)
{
    (void) wm;
    return 0u;
}


/** Link-only stand-in for wm_rules (wm/instance.c) */
rules_td *wm_rules(const wm_td *wm)
{
    (void) wm;
    return NULL;
}


/** Link-only stand-in for wm_session (wm/instance.c) */
session_td *wm_session(const wm_td *wm)
{
    (void) wm;
    return NULL;
}


/** Link-only stand-in for wm_stages (wm/instance.c) */
list_td *wm_stages(const wm_td *wm)
{
    (void) wm;
    return NULL;
}


/** Link-only stand-in for xcb_configure_window (libxcb) */
xcb_void_cookie_t xcb_configure_window(xcb_connection_t *c,
        xcb_window_t window, uint16_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) window;
    (void) value_mask;
    (void) value_list;
    return cookie;
}


/**
 * @brief Build a minimal, real 'struct wm_s' on the stack
 *
 * wm_action_rearrange never reads any of its fields, only forwards the
 * pointer itself straight through to enact_desktop_client_rearrange_all
 * (a stand-in here), so leaving every field zeroed is enough
 */
static wm_td s_make_wm(void)
{
    wm_td wm;

    memset(&wm, 0, sizeof(wm));
    return wm;
}


/**
 * @brief Build a minimal stage_td, distinguishable from another by
 *        its 'id' alone
 */
static stage_td s_make_stage(uint32_t id)
{
    stage_td stage;

    memset(&stage, 0, sizeof(stage));
    stage.id = id;
    return stage;
}


/* A null stage is refused outright: neither lookup_current_desktop
 * nor enact_desktop_client_rearrange_all is ever reached */
static void s_test_null_stage_is_refused(void)
{
    wm_td wm = s_make_wm();

    s_stub_reset();
    wm_action_rearrange(&wm, NULL);

    TAP_EQ_INT(s_rearrange_call_count, 0,
            "a null stage never reaches"
            " enact_desktop_client_rearrange_all");
}


/* A stage with no current desktop (lookup_current_desktop returns
 * NULL, e.g., an uninitialized or torn-down stage) is also a safe
 * no-op */
static void s_test_stage_with_no_current_desktop_is_a_no_op(void)
{
    wm_td wm = s_make_wm();
    stage_td stage = s_make_stage(0u);

    s_stub_reset();
    s_stub_current_desktop = NULL;
    wm_action_rearrange(&wm, &stage);

    TAP_EQ_INT(s_rearrange_call_count, 0,
            "no current desktop never reaches"
            " enact_desktop_client_rearrange_all");
}


/* The ordinary case: a stage with a real current desktop reaches
 * enact_desktop_client_rearrange_all exactly once, with the exact same
 * wm, stage, and desktop pointers wm_action_rearrange itself
 * received or looked up */
static void s_test_rearrange_dispatches_with_exact_arguments(void)
{
    wm_td wm = s_make_wm();
    stage_td stage = s_make_stage(7u);
    desktop_td desktop;

    memset(&desktop, 0, sizeof(desktop));
    desktop.id = 3u;
    s_stub_reset();
    s_stub_current_desktop = &desktop;

    wm_action_rearrange(&wm, &stage);

    TAP_EQ_INT(s_rearrange_call_count, 1,
            "a stage with a current desktop reaches"
            " enact_desktop_client_rearrange_all exactly once");
    TAP_OK(s_rearrange_last_wm == &wm,
            "the exact wm pointer wm_action_rearrange received is"
            " forwarded unchanged");
    TAP_OK(s_rearrange_last_stage == &stage,
            "the exact stage pointer wm_action_rearrange received"
            " is forwarded unchanged");
    TAP_OK(s_rearrange_last_desktop == &desktop,
            "the exact desktop lookup_current_desktop resolved is"
            " forwarded unchanged");
}


/* Calling wm_action_rearrange again on a second, distinct stage (a
 * second monitor's own stage, say) dispatches independently: the
 * most recent call's arguments reflect that second stage and
 * desktop, not the first */
static void s_test_repeated_calls_reflect_the_latest_stage(void)
{
    wm_td wm = s_make_wm();
    stage_td stage_a = s_make_stage(0u);
    stage_td stage_b = s_make_stage(1u);
    desktop_td desktop_a;
    desktop_td desktop_b;

    memset(&desktop_a, 0, sizeof(desktop_a));
    memset(&desktop_b, 0, sizeof(desktop_b));
    desktop_a.id = 0u;
    desktop_b.id = 1u;
    s_stub_reset();

    s_stub_current_desktop = &desktop_a;
    wm_action_rearrange(&wm, &stage_a);
    s_stub_current_desktop = &desktop_b;
    wm_action_rearrange(&wm, &stage_b);

    TAP_EQ_INT(s_rearrange_call_count, 2,
            "two rearrange calls on two distinct stages both reach"
            " enact_desktop_client_rearrange_all");
    TAP_OK(s_rearrange_last_stage == &stage_b,
            "the most recent call's stage is the second stage,"
            " not the first");
    TAP_OK(s_rearrange_last_desktop == &desktop_b,
            "the most recent call's desktop is the second desktop,"
            " not the first");
}


int main(void)
{
    TAP_PLAN(9);

    s_test_null_stage_is_refused();
    s_test_stage_with_no_current_desktop_is_a_no_op();
    s_test_rearrange_dispatches_with_exact_arguments();
    s_test_repeated_calls_reflect_the_latest_stage();

    return TAP_DONE();
}
