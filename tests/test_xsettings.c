/**
 * @file tests/test_xsettings.c
 *
 * @brief Test battery for xsettings.c's public entry-point guard
 *        clauses (xsettings.c)
 *
 * xsettings.c's three public entry points, xsettings_init,
 * xsettings_shutdown, and xsettings_reload, each begin with a plain
 * guard clause reachable and assertable without any live X connection
 * at all: xsettings_init returns immediately on a NULL wm, a NULL
 * config, or 'config->theme.xsettings.is_enabled' being false, never
 * reaching its own file-static s_xs_ensure_window (a real
 * xcb_generate_id/xcb_create_window round trip) or
 * s_xs_acquire_selection (a real selection-ownership round trip)
 * helpers; xsettings_reload returns immediately on a NULL wm or a NULL
 * config, and, past that guard, its "was disabled, remains disabled"
 * branch (the fall-through case when @c is_selection_owned is false
 * and @c should_be_enabled is also false) is reachable and assertable
 * purely through the file-static @c s_xs module state, which this file
 * never disturbs since no scenario here ever makes xsettings_init
 * or xsettings_reload actually acquire a selection; xsettings_shutdown
 * has no guard clause of its own, but with the module-static @c s_xs
 * left at its all-zero initial state (as every scenario in this file
 * leaves it, never having called anything that flips
 * @c is_selection_owned or @c is_window_ready to true), both of its own
 * conditionals evaluate to "nothing to release, nothing to destroy",
 * so it, too, runs to completion without ever reaching a real X call.
 *
 * Every other branch inside xsettings_init/xsettings_reload
 * (s_xs_ensure_window's own xcb_generate_id/xcb_create_window,
 * s_xs_acquire_selection's util_xcb_acquire_manager_selection call,
 * s_xs_publish's xcb_change_property/xcb_flush, and
 * xsettings_shutdown's own xcb_window_destroy path once a window were
 * actually created) is unreachable without a live X connection to
 * hand xcb_generate_id a real resource ID range and to carry a real
 * selection-ownership round trip; none of that is exercised here, and
 * none of xsettings.c's own file-static helper functions
 * (s_xs_padded_len, s_xs_int_entry_size, s_xs_string_entry_size,
 * s_xs_put_u16, s_xs_put_u32, s_xs_put_padded_bytes,
 * s_xs_write_int_entry, s_xs_write_string_entry, s_xs_build_property,
 * s_xs_load_config, s_xs_config_changed, s_xs_ensure_window,
 * s_xs_acquire_selection, s_xs_release_selection, s_xs_publish) can be
 * called directly from outside xsettings.c's own translation unit, so
 * their real byte-level property-encoding logic, despite being pure
 * and genuinely unit-testable in isolation, is simply not reachable
 * from a separate test file without editing xsettings.c itself to
 * expose them, which is out of scope for this round.
 *
 * wm/instance.c is linked for real: it is a small, self-contained file
 * of narrow field accessors over the real 'struct wm_s' built directly
 * below (through 'wm/internal.h', the same private header
 * tests/wm/test_lifecycle.c and tests/wm/test_startup.c already reach
 * around wm_start with), and referencing it for real here lets every
 * scenario exercise xsettings.c's own wm_config(wm)/wm==NULL dispatch
 * exactly as the real build does, rather than through a stand-in that
 * would only prove this file's own assumptions about wm_config's
 * behavior.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <stage.h>
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <harness/tap.h>
#include <utils/xcb/atom.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/selection.h>
#include <utils/xcb/window.h>
#include <xsettings.h>


/** Link-only stand-in for logger_msg (logger.c): a silent no-op,
 *  matching the real logger's own behavior whenever logger_start has
 *  never run, the same reasoning tests/wm/test_lifecycle.c documents
 *  for never calling the real logger_start here either */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/** Link-only stand-in for atom_intern (utils/xcb/atom.c): never
 *  actually invoked by any scenario in this file, since every one of
 *  them stops at a guard clause before xsettings.c's own
 *  s_xs_ensure_window could call it; present only so the real
 *  xsettings.c translation unit links */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    (void) connection;
    (void) name;
    (void) only_if_exists;
    return XCB_ATOM_NONE;
}


/** Link-only stand-in for util_xcb_acquire_manager_selection
 *  (utils/xcb/selection.c): never actually invoked by any scenario in
 *  this file, since every one of them stops at a guard clause before
 *  xsettings.c's own s_xs_acquire_selection could call it; present
 *  only so the real xsettings.c translation unit links */
bool util_xcb_acquire_manager_selection(xcb_connection_t *connection,
        xcb_window_t window, xcb_atom_t selection_atom,
        xcb_atom_t manager_atom, xcb_window_t root)
{
    (void) connection;
    (void) window;
    (void) selection_atom;
    (void) manager_atom;
    (void) root;
    return false;
}


/** Link-only stand-in for xcb_window_destroy (utils/xcb/window.c):
 *  never actually invoked by any scenario in this file, since every
 *  xsettings_shutdown scenario here leaves 'is_window_ready' false;
 *  present only so the real xsettings.c translation unit links */
void xcb_window_destroy(xcb_window_t window)
{
    (void) window;
}


/** Link-only stand-in for xcb_connection_get (utils/xcb/connection.c):
 *  never actually invoked by any scenario in this file, since every
 *  path reaching it in xsettings.c sits past a guard clause none of
 *  them cross; present only so the real xsettings.c translation unit
 *  links */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/** Build a zeroed, otherwise-valid wm_td/config_td pair with
 *  'is_enabled' set as requested, leaving every other field at a
 *  harmless all-zero default no scenario here ever reads */
static void s_build_wm(wm_td *wm, config_td *config, bool xs_enabled)
{
    memset(wm, 0, sizeof(*wm));
    memset(config, 0, sizeof(*config));
    config->theme.xsettings.is_enabled = xs_enabled;
    wm->config = config;
}


/* xsettings_init on a NULL wm returns without touching anything */
static void s_test_init_null_wm(void)
{
    xsettings_init(NULL);

    TAP_OK(true, "xsettings_init(NULL) does not crash");
}


/* xsettings_init on a wm with a NULL config returns without touching
 * anything, since wm_config(wm) resolves to NULL */
static void s_test_init_null_config(void)
{
    wm_td wm;

    memset(&wm, 0, sizeof(wm));
    wm.config = NULL;

    xsettings_init(&wm);

    TAP_OK(true, "xsettings_init with a NULL config does not crash");
}


/* xsettings_init on a wm whose configuration has XSETTINGS disabled
 * returns immediately, never reaching s_xs_ensure_window's real XCB
 * calls */
static void s_test_init_disabled(void)
{
    wm_td wm;
    config_td config;

    s_build_wm(&wm, &config, false);

    xsettings_init(&wm);

    TAP_OK(true,
            "xsettings_init with xsettings.is_enabled=false does not"
            " crash or hang waiting on a live X connection");
}


/* xsettings_shutdown tolerates a wm whose config was never even
 * touched, since it never dereferences its argument at all */
static void s_test_shutdown_untouched_state(void)
{
    wm_td wm;
    config_td config;

    s_build_wm(&wm, &config, false);

    xsettings_shutdown(&wm);

    TAP_OK(true,
            "xsettings_shutdown on module state that was never"
            " initialized does not crash");
}


/* xsettings_reload on a NULL wm returns without touching anything */
static void s_test_reload_null_wm(void)
{
    xsettings_reload(NULL);

    TAP_OK(true, "xsettings_reload(NULL) does not crash");
}


/* xsettings_reload on a wm with a NULL config returns without
 * touching anything, since wm_config(wm) resolves to NULL */
static void s_test_reload_null_config(void)
{
    wm_td wm;

    memset(&wm, 0, sizeof(wm));
    wm.config = NULL;

    xsettings_reload(&wm);

    TAP_OK(true, "xsettings_reload with a NULL config does not crash");
}


/* xsettings_reload when the module was never enabled and remains
 * disabled in the reloaded configuration falls through every branch
 * as a no-op, never reaching a real XCB call */
static void s_test_reload_stays_disabled(void)
{
    wm_td wm;
    config_td config;

    s_build_wm(&wm, &config, false);

    xsettings_reload(&wm);

    TAP_OK(true,
            "xsettings_reload staying disabled across a reload does"
            " not crash or hang waiting on a live X connection");
}


int main(void)
{
    TAP_PLAN(7);

    s_test_init_null_wm();
    s_test_init_null_config();
    s_test_init_disabled();
    s_test_shutdown_untouched_state();
    s_test_reload_null_wm();
    s_test_reload_null_config();
    s_test_reload_stays_disabled();

    return TAP_DONE();
}
