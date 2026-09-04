/**
 * @file tests/loop/test_signals.c
 *
 * @brief Test battery for the main loop's deferred signal-flag
 *        handling (loop/signals.c)
 *
 * loop_signals_process's own logic, past a null-ctx guard, is exactly
 * four independent "was this flag raised since last checked" queries,
 * each gating a corresponding action; none of wm_startup_requested_*'s
 * own flag storage, wm_action_config_reload's own configuration
 * reload, keyboard_load/mouse_load's own grab machinery, or
 * session_reap_children's own process reaping belongs to loop/
 * signals.c itself, so every one is a link-only stand-in below whose
 * return value (for the four query functions) a scenario controls
 * directly, and whose call is simply counted (for the four actions),
 * the same shape of substitution tests/loop/test_dispatch.c already
 * uses for handler.h's own targets.
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
#include <string.h>

/* Local includes */
#include <harness/tap.h>
#include <logger.h>
#include <loop/context.h>
#include <loop/signals.h>


/** Link-only stand-in for logger_msg (logger.c): a silent no-op,
 *  matching the real logger's own behavior whenever logger_start has
 *  never run (its first check is 'logger == NULL'), the same
 *  reasoning tests/wm/test_lifecycle.c already documents for never
 *  calling logger_start at all here either */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/* Controllable results for the four query stand-ins below */
static bool s_requested_stop;
static bool s_requested_reload;
static bool s_requested_resume;
static bool s_requested_child_reap;

/* Call counters/recording for the four action stand-ins below */
static int s_wm_request_stop_calls;
static int s_config_reload_calls;
static const wm_td *s_config_reload_last_wm;
static int s_keyboard_load_calls;
static int s_mouse_load_calls;
static int s_session_reap_children_calls;


static void s_reset(void)
{
    s_requested_stop = false;
    s_requested_reload = false;
    s_requested_resume = false;
    s_requested_child_reap = false;
    s_wm_request_stop_calls = 0;
    s_config_reload_calls = 0;
    s_config_reload_last_wm = NULL;
    s_keyboard_load_calls = 0;
    s_mouse_load_calls = 0;
    s_session_reap_children_calls = 0;
}


/** Link-only stand-in for wm_startup_requested_stop
 *  (wm/startup/handle.c) */
bool wm_startup_requested_stop(void)
{
    return s_requested_stop;
}


/** Link-only stand-in for wm_startup_requested_reload
 *  (wm/startup/handle.c) */
bool wm_startup_requested_reload(void)
{
    return s_requested_reload;
}


/** Link-only stand-in for wm_startup_requested_resume
 *  (wm/startup/handle.c) */
bool wm_startup_requested_resume(void)
{
    return s_requested_resume;
}


/** Link-only stand-in for wm_startup_requested_child_reap
 *  (wm/startup/handle.c) */
bool wm_startup_requested_child_reap(void)
{
    return s_requested_child_reap;
}


/** Link-only stand-in for wm_request_stop (wm.c): wm_request_stop's
 *  own singleton-mutating logic is covered on its own terms in
 *  tests/wm/test_lifecycle.c, not here */
int wm_request_stop(void)
{
    s_wm_request_stop_calls++;
    return 0;
}


/** Link-only stand-in for wm_action_config_reload (wm/action.c):
 *  a real reload re-parses configuration files off disk and re-runs
 *  keyboard_load/mouse_load itself, none of which is loop/signals.c's
 *  own logic to exercise */
int wm_action_config_reload(const wm_td *wm)
{
    s_config_reload_calls++;
    s_config_reload_last_wm = wm;
    return 0;
}


/** Link-only stand-in for keyboard_load (input/kbd/bind.c) */
void keyboard_load(list_td *surfaces, xcb_key_symbols_t *keysyms,
        const config_td *config)
{
    (void) surfaces;
    (void) keysyms;
    (void) config;
    s_keyboard_load_calls++;
}


/** Link-only stand-in for mouse_load (input/mouse/bind.c) */
void mouse_load(list_td *surfaces, const config_td *config)
{
    (void) surfaces;
    (void) config;
    s_mouse_load_calls++;
}


/** Link-only stand-in for session_reap_children (session.c) */
void session_reap_children(void)
{
    s_session_reap_children_calls++;
}


/**
 * @brief Build a loop context with every signal flag left unset
 */
static loop_ctx_td s_make_ctx(void)
{
    loop_ctx_td ctx;

    memset(&ctx, 0, sizeof(ctx));
    return ctx;
}


/* A null ctx returns false without touching any of the four queries */
static void s_test_null_ctx(void)
{
    s_reset();
    TAP_OK(!loop_signals_process(NULL),
            "loop_signals_process on a null ctx returns false");
    TAP_EQ_INT(s_wm_request_stop_calls, 0,
            "a null ctx never reaches wm_startup_requested_stop");
}


/* With every flag left unset, the loop reports true (keep running)
 * and calls none of the four actions */
static void s_test_no_flags_set(void)
{
    loop_ctx_td ctx = s_make_ctx();

    s_reset();
    TAP_OK(loop_signals_process(&ctx),
            "loop_signals_process with no flag raised returns true");
    TAP_EQ_INT(s_wm_request_stop_calls, 0,
            "no flag raised calls no wm_request_stop");
    TAP_EQ_INT(s_config_reload_calls, 0,
            "no flag raised calls no config reload");
    TAP_EQ_INT(s_keyboard_load_calls, 0,
            "no flag raised calls no keyboard_load");
    TAP_EQ_INT(s_mouse_load_calls, 0,
            "no flag raised calls no mouse_load");
    TAP_EQ_INT(s_session_reap_children_calls, 0,
            "no flag raised calls no session_reap_children");
}


/* A termination request reports false (stop the loop) and requests
 * a shutdown, returning immediately without checking any of the
 * other three flags at all */
static void s_test_stop_requested_short_circuits(void)
{
    loop_ctx_td ctx = s_make_ctx();

    s_reset();
    s_requested_stop = true;
    s_requested_reload = true;
    s_requested_resume = true;
    s_requested_child_reap = true;

    TAP_OK(!loop_signals_process(&ctx),
            "loop_signals_process with a termination request returns"
            " false");
    TAP_EQ_INT(s_wm_request_stop_calls, 1,
            "a termination request calls wm_request_stop exactly"
            " once");
    TAP_EQ_INT(s_config_reload_calls, 0,
            "a termination request returns before ever checking the"
            " reload flag, even though it too was set");
    TAP_EQ_INT(s_keyboard_load_calls, 0,
            "a termination request returns before ever checking the"
            " resume flag");
    TAP_EQ_INT(s_session_reap_children_calls, 0,
            "a termination request returns before ever checking the"
            " child-reap flag");
}


/* A reload request alone reloads the configuration through the ctx's
 * own wm pointer and still reports true (keep running) */
static void s_test_reload_requested(void)
{
    wm_td *const fake_wm = (wm_td *) (void *) 0x1234;
    loop_ctx_td ctx = s_make_ctx();

    ctx.wm = fake_wm;

    s_reset();
    s_requested_reload = true;

    TAP_OK(loop_signals_process(&ctx),
            "loop_signals_process with only a reload request returns"
            " true");
    TAP_EQ_INT(s_config_reload_calls, 1,
            "a reload request reloads the configuration exactly once");
    TAP_OK(s_config_reload_last_wm == fake_wm,
            "the reload is passed the ctx's own wm pointer unchanged");
    TAP_EQ_INT(s_wm_request_stop_calls, 0,
            "a reload request alone never requests a shutdown");
}


/* A resume request alone re-establishes both keyboard and mouse
 * grabs, and still reports true */
static void s_test_resume_requested(void)
{
    loop_ctx_td ctx = s_make_ctx();

    s_reset();
    s_requested_resume = true;

    TAP_OK(loop_signals_process(&ctx),
            "loop_signals_process with only a resume request returns"
            " true");
    TAP_EQ_INT(s_keyboard_load_calls, 1,
            "a resume request re-loads the keyboard bindings exactly"
            " once");
    TAP_EQ_INT(s_mouse_load_calls, 1,
            "a resume request re-loads the mouse bindings exactly"
            " once");
    TAP_EQ_INT(s_config_reload_calls, 0,
            "a resume request alone never reloads the configuration");
}


/* A child-reap request alone reaps children, and still reports true
 */
static void s_test_child_reap_requested(void)
{
    loop_ctx_td ctx = s_make_ctx();

    s_reset();
    s_requested_child_reap = true;

    TAP_OK(loop_signals_process(&ctx),
            "loop_signals_process with only a child-reap request"
            " returns true");
    TAP_EQ_INT(s_session_reap_children_calls, 1,
            "a child-reap request reaps children exactly once");
    TAP_EQ_INT(s_keyboard_load_calls, 0,
            "a child-reap request alone never re-loads the keyboard"
            " bindings");
}


/* Reload, resume, and child-reap can all fire together in the same
 * pass, each acting independently of the other two */
static void s_test_multiple_flags_all_act(void)
{
    loop_ctx_td ctx = s_make_ctx();

    s_reset();
    s_requested_reload = true;
    s_requested_resume = true;
    s_requested_child_reap = true;

    TAP_OK(loop_signals_process(&ctx),
            "loop_signals_process with reload, resume, and"
            " child-reap all set together still returns true");
    TAP_EQ_INT(s_config_reload_calls, 1,
            "the reload action fires");
    TAP_EQ_INT(s_keyboard_load_calls, 1,
            "the resume action fires its own keyboard_load");
    TAP_EQ_INT(s_mouse_load_calls, 1,
            "the resume action fires its own mouse_load");
    TAP_EQ_INT(s_session_reap_children_calls, 1,
            "the child-reap action fires");
}


int main(void)
{
    TAP_PLAN(29);

    s_test_null_ctx();
    s_test_no_flags_set();
    s_test_stop_requested_short_circuits();
    s_test_reload_requested();
    s_test_resume_requested();
    s_test_child_reap_requested();
    s_test_multiple_flags_all_act();

    return TAP_DONE();
}
