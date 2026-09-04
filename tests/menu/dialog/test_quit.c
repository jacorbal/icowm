/**
 * @file tests/menu/dialog/test_quit.c
 *
 * @brief Test battery for the quit-confirmation dialog thin wrapper
 *
 * 'dialog_quit_show' builds a prompt then delegates every layout and
 * interaction concern to the generic confirm dialog
 * (menu/dialog/confirm.h), so 'menu_confirm_dialog_show' is a
 * recording stand-in below rather than the real confirm.c, which has
 * its own test battery (tests/menu/dialog/test_confirm.c) already
 * exercising that machinery directly.  'wm_request_graceful_stop' is
 * likewise a recording stand-in: it lives in wm.c, a whole other
 * module this file has no reason to link just to observe that quit.c's
 * own confirm callback reaches it.  Never calling gettext's own
 * 'setlocale'/'bindtextdomain' setup (i18n_init) is deliberate: every
 * '_(STR_FOO)' call below still resolves through the real libc
 * 'gettext', which silently falls back to its original argument
 * without a bound catalog, so quit.c's own prompt text comes through
 * exactly as 'defs/uistr.h' declares it either way.
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

/* Project includes */
#include <config.h>
#include <surface.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/dialog/confirm.h>
#include <menu/dialog/quit.h>


/** Fake, non-null XCB connection/surface/config handles, standing in
 *  for live ones wherever quit.c merely forwards them onward without
 *  ever dereferencing them itself */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;
static surface_td s_fake_surface;
static config_td s_fake_config;

/** Recording stand-in's own call counter and captured arguments,
 *  reset by s_reset between scenarios */
static int s_call_confirm_show;
static xcb_connection_t *s_confirm_connection;
static surface_td *s_confirm_surface;
static const config_td *s_confirm_config;
static char s_confirm_prompt[256];
static char s_confirm_cancel_label[64];
static char s_confirm_confirm_label[64];
static void (*s_confirm_on_confirm)(xcb_connection_t *);
static void (*s_confirm_on_cancel)(xcb_connection_t *);
static uint32_t s_confirm_timeout_seconds;

/** How many times this file's own s_on_quit_confirm has run behind
 *  wm_request_graceful_stop's stand-in, reset by s_reset */
static int s_call_graceful_stop;


/**
 * @brief Reset every stand-in's recorded call state between scenarios
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    s_call_confirm_show = 0;
    s_confirm_connection = NULL;
    s_confirm_surface = NULL;
    s_confirm_config = NULL;
    s_confirm_prompt[0] = '\0';
    s_confirm_cancel_label[0] = '\0';
    s_confirm_confirm_label[0] = '\0';
    s_confirm_on_confirm = NULL;
    s_confirm_on_cancel = NULL;
    s_confirm_timeout_seconds = 0u;
    s_call_graceful_stop = 0;
}


/**
 * @brief Recording stand-in for @a menu_confirm_dialog_show
 * @note Complexity: @e O(1)
 */
void menu_confirm_dialog_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const char *prompt, const char *cancel_label,
        const char *confirm_label,
        void (*on_confirm)(xcb_connection_t *),
        void (*on_cancel)(xcb_connection_t *),
        uint32_t timeout_seconds)
{
    s_call_confirm_show++;
    s_confirm_connection = connection;
    s_confirm_surface = surface;
    s_confirm_config = config;
    (void) strncpy(s_confirm_prompt, (prompt != NULL) ? prompt : "",
            sizeof(s_confirm_prompt) - 1u);
    s_confirm_prompt[sizeof(s_confirm_prompt) - 1u] = '\0';
    (void) strncpy(s_confirm_cancel_label,
            (cancel_label != NULL) ? cancel_label : "",
            sizeof(s_confirm_cancel_label) - 1u);
    s_confirm_cancel_label[sizeof(s_confirm_cancel_label) - 1u] = '\0';
    (void) strncpy(s_confirm_confirm_label,
            (confirm_label != NULL) ? confirm_label : "",
            sizeof(s_confirm_confirm_label) - 1u);
    s_confirm_confirm_label[sizeof(s_confirm_confirm_label) - 1u] = '\0';
    s_confirm_on_confirm = on_confirm;
    s_confirm_on_cancel = on_cancel;
    s_confirm_timeout_seconds = timeout_seconds;
}


/**
 * @brief Link-only stand-in for @a wm_request_graceful_stop
 * @note Complexity: @e O(1)
 */
void wm_request_graceful_stop(void)
{
    s_call_graceful_stop++;
}


/* dialog_quit_show delegates to menu_confirm_dialog_show exactly once,
 * forwarding the connection, surface, and config unchanged */
static void s_test_show_delegates_to_confirm(void)
{
    s_reset();
    dialog_quit_show(s_fake_connection, &s_fake_surface, &s_fake_config);

    TAP_EQ_INT(s_call_confirm_show, 1,
            "quit_show: delegates to menu_confirm_dialog_show exactly"
            " once");
    TAP_OK(s_confirm_connection == s_fake_connection,
            "quit_show: forwards the connection unchanged");
    TAP_OK(s_confirm_surface == &s_fake_surface,
            "quit_show: forwards the surface unchanged");
    TAP_OK(s_confirm_config == &s_fake_config,
            "quit_show: forwards the config unchanged");
}


/* dialog_quit_show builds its prompt with the window-manager's own
 * EWMH name spliced into the format string */
static void s_test_show_builds_prompt_with_wm_name(void)
{
    s_reset();
    dialog_quit_show(s_fake_connection, &s_fake_surface, &s_fake_config);

    TAP_OK(strstr(s_confirm_prompt, "IcoWM") != NULL,
            "quit_show: prompt mentions the window manager's EWMH name");
    TAP_OK(strstr(s_confirm_prompt, "exit") != NULL,
            "quit_show: prompt asks about exiting");
}


/* dialog_quit_show passes non-empty cancel and confirm button labels,
 * and no timeout (an unattended quit dialog waits indefinitely) */
static void s_test_show_passes_labels_and_no_timeout(void)
{
    s_reset();
    dialog_quit_show(s_fake_connection, &s_fake_surface, &s_fake_config);

    TAP_OK(s_confirm_cancel_label[0] != '\0',
            "quit_show: passes a non-empty cancel label");
    TAP_OK(s_confirm_confirm_label[0] != '\0',
            "quit_show: passes a non-empty exit/confirm label");
    TAP_EQ_INT((int) s_confirm_timeout_seconds, 0,
            "quit_show: passes zero timeout seconds (waits indefinitely)");
    TAP_NOT_NULL(s_confirm_on_confirm,
            "quit_show: passes a non-NULL on_confirm callback");
    TAP_NULL(s_confirm_on_cancel,
            "quit_show: passes no on_cancel callback");
}


/* Invoking the captured on_confirm callback requests a graceful stop
 * of the window manager */
static void s_test_confirm_callback_requests_graceful_stop(void)
{
    s_reset();
    dialog_quit_show(s_fake_connection, &s_fake_surface, &s_fake_config);

    TAP_NOT_NULL(s_confirm_on_confirm,
            "quit_show: on_confirm callback was captured before"
            " invoking it");
    s_confirm_on_confirm(s_fake_connection);

    TAP_EQ_INT(s_call_graceful_stop, 1,
            "quit confirm callback: requests exactly one graceful stop");
}


int main(void)
{
    TAP_PLAN(13);

    s_test_show_delegates_to_confirm();
    s_test_show_builds_prompt_with_wm_name();
    s_test_show_passes_labels_and_no_timeout();
    s_test_confirm_callback_requests_graceful_stop();

    return TAP_DONE();
}
