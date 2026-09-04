/**
 * @file tests/cctl/test_launch.c
 *
 * @brief Test battery for program-launch dispatch (cctl/launch.c)
 *
 * cctl/launch.c holds exactly one function, cctl_launch_dispatch, no
 * file-local static helpers at all.  Every one of its own branches
 * (the null/empty surface or prog guard, the null-desktop guard once
 * lookup_current_desktop is consulted, the class_name-present versus
 * class_name-absent dispatch choice, and the "command not found"
 * dialog shown only when every one of its own three conditions,
 * result == -2, a live xcb_connection_get, and a non-null
 * surface->config, all hold at once) is pure dispatch logic over
 * arguments and small stand-in return values, none of it needing a
 * live X server itself, so every branch is covered here directly.
 *
 * cctl/launch.c's own translation unit is linked for real.  Every
 * external symbol it references, lookup_current_desktop (lookup.c),
 * desktop_action_process_launch and
 * desktop_action_process_launch_with_class (desktop.c),
 * xcb_connection_get (utils/xcb/connection.c), and dialog_info_show
 * (menu/dialog/info.c), is a small, controllable, recording stand-in
 * below, letting every scenario assert on cctl_launch_dispatch's own
 * dispatch directly rather than on some other subsystem's real
 * behavior.  gettext itself (via the project's own '_()' macro) is
 * real, unstubbed, standard C library code, and always returns its
 * argument unchanged here since no locale or catalog is ever loaded
 * by this test binary.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>
#include <string.h>
#include <sys/types.h>  /* pid_t */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <desktop.h>
#include <lookup.h>
#include <menu/dialog/info.h>
#include <surface.h>

/* Local includes */
#include <cctl/launch.h>
#include <harness/tap.h>


/* Recording state for the lookup_current_desktop stand-in */
static desktop_td *s_stub_desktop;
static int s_lookup_call_count;

/** Link-only stand-in for lookup_current_desktop (lookup.c) */
desktop_td *lookup_current_desktop(surface_td *surface)
{
    (void) surface;
    s_lookup_call_count++;
    return s_stub_desktop;
}


/* Recording state for the two desktop_action_process_launch* stand-ins */
static int s_plain_call_count;
static desktop_td *s_plain_last_desktop;
static char s_plain_last_prog[128];
static int s_plain_return_value;

static int s_with_class_call_count;
static desktop_td *s_with_class_last_desktop;
static char s_with_class_last_prog[128];
static char s_with_class_last_class[128];
static int s_with_class_return_value;

/** Link-only stand-in for desktop_action_process_launch (desktop.c) */
int desktop_action_process_launch(desktop_td *desktop, const char *prog)
{
    s_plain_call_count++;
    s_plain_last_desktop = desktop;
    (void) strncpy(s_plain_last_prog, prog,
            sizeof(s_plain_last_prog) - 1u);
    s_plain_last_prog[sizeof(s_plain_last_prog) - 1u] = '\0';
    return s_plain_return_value;
}


/** Link-only stand-in for desktop_action_process_launch_with_class
 *  (desktop.c) */
int desktop_action_process_launch_with_class(desktop_td *desktop,
        const char *restrict prog, const char *restrict class_name,
        pid_t *restrict out_pid)
{
    (void) out_pid;
    s_with_class_call_count++;
    s_with_class_last_desktop = desktop;
    (void) strncpy(s_with_class_last_prog, prog,
            sizeof(s_with_class_last_prog) - 1u);
    s_with_class_last_prog[sizeof(s_with_class_last_prog) - 1u] = '\0';
    (void) strncpy(s_with_class_last_class, class_name,
            sizeof(s_with_class_last_class) - 1u);
    s_with_class_last_class[sizeof(s_with_class_last_class) - 1u] = '\0';
    return s_with_class_return_value;
}


/* Recording state for the xcb_connection_get stand-in */
static xcb_connection_t *s_stub_connection;

/** Non-null opaque connection handle, never dereferenced by anything
 *  this file links for real */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;

/** Link-only stand-in for xcb_connection_get (utils/xcb/connection.c) */
xcb_connection_t *xcb_connection_get(void)
{
    return s_stub_connection;
}


/* Recording state for the dialog_info_show stand-in */
static int s_dialog_call_count;
static char s_dialog_last_message[256];
static menu_msg_level_e s_dialog_last_level;

/** Link-only stand-in for dialog_info_show (menu/dialog/info.c) */
void dialog_info_show(xcb_connection_t *connection, surface_td *surface,
        const config_td *config, const char *message,
        menu_msg_level_e level)
{
    (void) connection;
    (void) surface;
    (void) config;
    s_dialog_call_count++;
    (void) strncpy(s_dialog_last_message, message,
            sizeof(s_dialog_last_message) - 1u);
    s_dialog_last_message[sizeof(s_dialog_last_message) - 1u] = '\0';
    s_dialog_last_level = level;
}


/**
 * @brief Reset every recording global back to its starting state
 */
static void s_stub_reset(void)
{
    desktop_td stub_desktop_storage;

    memset(&stub_desktop_storage, 0, sizeof(stub_desktop_storage));
    s_stub_desktop = NULL;
    s_lookup_call_count = 0;
    s_plain_call_count = 0;
    s_plain_last_desktop = NULL;
    s_plain_last_prog[0] = '\0';
    s_plain_return_value = 0;
    s_with_class_call_count = 0;
    s_with_class_last_desktop = NULL;
    s_with_class_last_prog[0] = '\0';
    s_with_class_last_class[0] = '\0';
    s_with_class_return_value = 0;
    s_stub_connection = NULL;
    s_dialog_call_count = 0;
    s_dialog_last_message[0] = '\0';
    s_dialog_last_level = MENU_MSG_LEVEL_NONE;
}


/* A null surface, a null prog, and an empty prog are each refused
 * outright, never reaching lookup_current_desktop at all */
static void s_test_guard_clause_rejects_bad_arguments(void)
{
    surface_td surface;

    memset(&surface, 0, sizeof(surface));
    s_stub_reset();

    cctl_launch_dispatch(NULL, "xterm", NULL);
    TAP_EQ_INT(s_lookup_call_count, 0,
            "a null surface never reaches lookup_current_desktop");

    cctl_launch_dispatch(&surface, NULL, NULL);
    TAP_EQ_INT(s_lookup_call_count, 0,
            "a null prog never reaches lookup_current_desktop");

    cctl_launch_dispatch(&surface, "", NULL);
    TAP_EQ_INT(s_lookup_call_count, 0,
            "an empty prog never reaches lookup_current_desktop");
}


/* A null desktop (lookup_current_desktop found nothing current) stops
 * the dispatch right after the lookup, never reaching either launch
 * function */
static void s_test_null_desktop_stops_dispatch(void)
{
    surface_td surface;

    memset(&surface, 0, sizeof(surface));
    s_stub_reset();
    s_stub_desktop = NULL;

    cctl_launch_dispatch(&surface, "xterm", NULL);

    TAP_EQ_INT(s_lookup_call_count, 1,
            "a non-null surface and prog do reach"
            " lookup_current_desktop");
    TAP_EQ_INT(s_plain_call_count, 0,
            "a null desktop never reaches"
            " desktop_action_process_launch");
    TAP_EQ_INT(s_with_class_call_count, 0,
            "a null desktop never reaches"
            " desktop_action_process_launch_with_class");
}


/* A null or empty class_name dispatches through the plain
 * desktop_action_process_launch, passing the found desktop and prog
 * through unchanged */
static void s_test_dispatch_without_class_name(void)
{
    surface_td surface;
    desktop_td desktop;

    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    s_stub_reset();
    s_stub_desktop = &desktop;
    s_plain_return_value = 0;

    cctl_launch_dispatch(&surface, "xterm -e vim", NULL);

    TAP_EQ_INT(s_plain_call_count, 1,
            "a null class_name dispatches through"
            " desktop_action_process_launch exactly once");
    TAP_OK(s_plain_last_desktop == &desktop,
            "the desktop lookup_current_desktop found is passed"
            " through unchanged");
    TAP_EQ_STR(s_plain_last_prog, "xterm -e vim",
            "the prog string is passed through unchanged");
    TAP_EQ_INT(s_with_class_call_count, 0,
            "a null class_name never reaches"
            " desktop_action_process_launch_with_class");

    s_stub_reset();
    s_stub_desktop = &desktop;
    cctl_launch_dispatch(&surface, "xterm", "");
    TAP_EQ_INT(s_plain_call_count, 1,
            "an empty class_name dispatches through"
            " desktop_action_process_launch, exactly like a null one");
    TAP_EQ_INT(s_with_class_call_count, 0,
            "an empty class_name never reaches"
            " desktop_action_process_launch_with_class");
}


/* A non-empty class_name dispatches through
 * desktop_action_process_launch_with_class instead, passing the
 * desktop, prog, and class_name through unchanged */
static void s_test_dispatch_with_class_name(void)
{
    surface_td surface;
    desktop_td desktop;

    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    s_stub_reset();
    s_stub_desktop = &desktop;

    cctl_launch_dispatch(&surface, "firefox", "Firefox");

    TAP_EQ_INT(s_with_class_call_count, 1,
            "a non-empty class_name dispatches through"
            " desktop_action_process_launch_with_class exactly once");
    TAP_OK(s_with_class_last_desktop == &desktop,
            "the desktop lookup_current_desktop found is passed"
            " through unchanged");
    TAP_EQ_STR(s_with_class_last_prog, "firefox",
            "the prog string is passed through unchanged");
    TAP_EQ_STR(s_with_class_last_class, "Firefox",
            "the class_name string is passed through unchanged");
    TAP_EQ_INT(s_plain_call_count, 0,
            "a non-empty class_name never reaches"
            " desktop_action_process_launch");
}


/* A -2 result with a live connection and a non-null surface->config
 * shows the "command not found" dialog, with the configured prog
 * substituted into the message */
static void s_test_command_not_found_shows_dialog(void)
{
    surface_td surface;
    desktop_td desktop;
    config_td config_storage;

    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&config_storage, 0, sizeof(config_storage));
    s_stub_reset();
    s_stub_desktop = &desktop;
    s_plain_return_value = -2;
    s_stub_connection = s_fake_connection;
    surface.config = &config_storage;

    cctl_launch_dispatch(&surface, "typo-command", NULL);

    TAP_EQ_INT(s_dialog_call_count, 1,
            "a -2 result, a live connection, and a non-null config"
            " together show exactly one dialog");
    TAP_EQ_STR(s_dialog_last_message,
            "Failed to execute 'typo-command': command not found.",
            "the dialog message substitutes the configured prog"
            " string into the not-found format");
    TAP_EQ_INT((int) s_dialog_last_level, (int) MENU_MSG_LEVEL_WARNING,
            "the dialog is shown at warning level");
}


/* Any one of the three conditions failing on its own (a non -2
 * result, no live connection, or a null surface->config) suppresses
 * the dialog entirely */
static void s_test_command_not_found_dialog_suppressed(void)
{
    surface_td surface;
    desktop_td desktop;
    config_td config_storage;

    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&config_storage, 0, sizeof(config_storage));

    /* Success (0), not -2: no dialog regardless of the other two
     * conditions */
    s_stub_reset();
    s_stub_desktop = &desktop;
    s_plain_return_value = 0;
    s_stub_connection = s_fake_connection;
    surface.config = &config_storage;
    cctl_launch_dispatch(&surface, "xterm", NULL);
    TAP_EQ_INT(s_dialog_call_count, 0,
            "a successful (non -2) result never shows the dialog");

    /* -2, but no live connection */
    s_stub_reset();
    s_stub_desktop = &desktop;
    s_plain_return_value = -2;
    s_stub_connection = NULL;
    surface.config = &config_storage;
    cctl_launch_dispatch(&surface, "typo-command", NULL);
    TAP_EQ_INT(s_dialog_call_count, 0,
            "a -2 result with no live xcb_connection_get never shows"
            " the dialog");

    /* -2, live connection, but a null surface->config */
    s_stub_reset();
    s_stub_desktop = &desktop;
    s_plain_return_value = -2;
    s_stub_connection = s_fake_connection;
    surface.config = NULL;
    cctl_launch_dispatch(&surface, "typo-command", NULL);
    TAP_EQ_INT(s_dialog_call_count, 0,
            "a -2 result with a null surface->config never shows the"
            " dialog");
}


int main(void)
{
    TAP_PLAN(23);

    s_test_guard_clause_rejects_bad_arguments();
    s_test_null_desktop_stops_dispatch();
    s_test_dispatch_without_class_name();
    s_test_dispatch_with_class_name();
    s_test_command_not_found_shows_dialog();
    s_test_command_not_found_dialog_suppressed();

    return TAP_DONE();
}
