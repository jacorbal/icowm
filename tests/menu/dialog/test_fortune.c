/**
 * @file tests/menu/dialog/test_fortune.c
 *
 * @brief Test battery for the 'fortune' easter egg dialog
 *
 * 'dialog_fortune_show' runs a real shell command through 'popen', so
 * this file exercises that end to end by pointing
 * 'config->base.fortune.command' at ordinary POSIX utilities
 * ('printf', a nonexistent binary, 'true') already on any system this
 * project builds on, rather than stubbing 'popen' itself: the genuine
 * read-trim-fallback behavior is exactly what these scenarios exist to
 * check, and a stand-in would only have to reimplement that behavior
 * to be checked against itself.  'menu_message_dialog_show' is a
 * recording stand-in below, since fortune.c's own responsibility ends
 * at composing the text handed to it; how that text is laid out and
 * drawn is message.c's own, separately tested, concern.
 * 'utils/safe/safestr.c' is linked for real, since fortune.c measures
 * the piped-back buffer through 'safe_strlen'.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* popen, pclose */


/* System includes */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <stage.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/dialog/fortune.h>
#include <menu/dialog/message.h>


/** Fake, non-null XCB connection/stage handles, standing in for live
 *  ones wherever fortune.c merely forwards them onward without ever
 *  dereferencing them itself */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;
static stage_td s_fake_stage;

/** Recording stand-in's own call counter and captured arguments,
 *  reset by s_reset between scenarios */
static int s_call_show;
static char s_show_text[2048];
static menu_msg_level_e s_show_level;


/**
 * @brief Reset every stand-in's recorded call state between scenarios
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    s_call_show = 0;
    s_show_text[0] = '\0';
    s_show_level = MENU_MSG_LEVEL_WARNING;
}


/**
 * @brief Recording stand-in for @a menu_message_dialog_show
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_show(xcb_connection_t *connection,
        stage_td *stage, const config_td *config,
        const char *message, menu_msg_level_e level)
{
    (void) connection;
    (void) stage;
    (void) config;
    s_call_show++;
    (void) strncpy(s_show_text, (message != NULL) ? message : "",
            sizeof(s_show_text) - 1u);
    s_show_text[sizeof(s_show_text) - 1u] = '\0';
    s_show_level = level;
}


/**
 * @brief Build a minimal, real config fixture running @p command
 * @note Complexity: @e O(1)
 */
static void s_make_config(config_td *config, const char *command)
{
    memset(config, 0, sizeof(*config));
    (void) strncpy(config->base.fortune.command, command,
            sizeof(config->base.fortune.command) - 1u);
}


/* dialog_fortune_show is a no-op on every guarded NULL argument */
static void s_test_null_guards(void)
{
    config_td config;

    s_make_config(&config, "printf hello");

    s_reset();
    dialog_fortune_show(NULL, &s_fake_stage, &config);
    TAP_EQ_INT(s_call_show, 0, "fortune_show: NULL connection is a no-op");

    s_reset();
    dialog_fortune_show(s_fake_connection, NULL, &config);
    TAP_EQ_INT(s_call_show, 0, "fortune_show: NULL stage is a no-op");

    s_reset();
    dialog_fortune_show(s_fake_connection, &s_fake_stage, NULL);
    TAP_EQ_INT(s_call_show, 0, "fortune_show: NULL config is a no-op");
}


/* A command that succeeds and prints text shows that text, with its
 * trailing newline trimmed, at MENU_MSG_LEVEL_NONE */
static void s_test_successful_command_shows_trimmed_output(void)
{
    config_td config;

    s_make_config(&config, "printf 'a fortune awaits\\n'");
    s_reset();

    dialog_fortune_show(s_fake_connection, &s_fake_stage, &config);

    TAP_EQ_INT(s_call_show, 1,
            "fortune_show: shows the dialog exactly once on success");
    TAP_EQ_STR(s_show_text, "a fortune awaits",
            "fortune_show: shows the command's output with its trailing"
            " newline trimmed");
    TAP_EQ_INT((int) s_show_level, (int) MENU_MSG_LEVEL_NONE,
            "fortune_show: shows real fortune output at level NONE (no"
            " prefix)");
}


/* Trailing newlines, carriage returns, spaces, and tabs are all
 * trimmed, and only from the end */
static void s_test_trims_trailing_whitespace_variety(void)
{
    config_td config;

    s_make_config(&config,
            "printf 'line one\\nline two \\t\\r\\n\\n'");
    s_reset();

    dialog_fortune_show(s_fake_connection, &s_fake_stage, &config);

    TAP_EQ_STR(s_show_text, "line one\nline two",
            "fortune_show: trims every trailing newline/CR/space/tab,"
            " leaving interior whitespace intact");
}


/* A command producing no output at all (redirected to /dev/null, so
 * even a "command not found" complaint never reaches the buffer) falls
 * back to the fixed fortune-unavailable message */
static void s_test_nonexistent_command_falls_back(void)
{
    config_td config;

    s_make_config(&config, "this_command_does_not_exist_anywhere_12345");
    s_reset();

    dialog_fortune_show(s_fake_connection, &s_fake_stage, &config);

    TAP_EQ_INT(s_call_show, 1,
            "fortune_show: still shows a dialog when the command is"
            " missing");
    TAP_OK(s_show_text[0] != '\0',
            "fortune_show: falls back to a non-empty message rather"
            " than an empty dialog");
    TAP_OK(strstr(s_show_text, "a fortune awaits") == NULL,
            "fortune_show: fallback text is not mistaken for real"
            " fortune output");
}


/* A command that exits successfully but prints only whitespace is
 * treated the same as producing no output at all */
static void s_test_whitespace_only_output_falls_back(void)
{
    config_td config;
    char fallback_text[2048];

    s_make_config(&config, "true");
    s_reset();
    dialog_fortune_show(s_fake_connection, &s_fake_stage, &config);
    TAP_OK(s_show_text[0] != '\0',
            "fortune_show: empty command output falls back to a"
            " non-empty message");
    (void) strncpy(fallback_text, s_show_text, sizeof(fallback_text) - 1u);
    fallback_text[sizeof(fallback_text) - 1u] = '\0';

    s_make_config(&config, "printf '   \\n\\t\\n  '");
    s_reset();
    dialog_fortune_show(s_fake_connection, &s_fake_stage, &config);
    TAP_EQ_STR(s_show_text, fallback_text,
            "fortune_show: whitespace-only output falls back to the"
            " exact same message as no output at all");
}


int main(void)
{
    TAP_PLAN(12);

    s_test_null_guards();
    s_test_successful_command_shows_trimmed_output();
    s_test_trims_trailing_whitespace_variety();
    s_test_nonexistent_command_falls_back();
    s_test_whitespace_only_output_falls_back();

    return TAP_DONE();
}
