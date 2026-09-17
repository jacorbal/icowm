/**
 * @file tests/menu/dialog/test_rrsafe.c
 *
 * @brief Test battery for the RandR output-profile confirm dialog
 *        thin wrapper
 *
 * 'dialog_rrsafe_show' delegates every layout and interaction concern
 * to the generic confirm dialog (menu/dialog/confirm.h), so
 * 'menu_confirm_dialog_show' is a recording stand-in below rather than
 * the real confirm.c, which has its own test battery
 * (tests/menu/dialog/test_confirm.c) already exercising that machinery
 * directly.  'stage_action_randr_revert_profiles' is likewise a
 * recording stand-in: it lives in stage.c, a whole other module this
 * file has no reason to link just to observe that rrsafe.c's own
 * cancel callback reaches it.
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

/* Default initial values */
#include <defs/dialog.h>

/* Project includes */
#include <config.h>
#include <stage.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/dialog/confirm.h>
#include <menu/dialog/rrsafe.h>


/** Fake, non-null XCB connection/stage/config handles, standing in
 *  for live ones wherever rrsafe.c merely forwards them onward without
 *  ever dereferencing them itself */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;
static stage_td s_fake_stage;
static config_td s_fake_config;

/** Recording stand-ins' own call counters and captured arguments,
 *  reset by s_reset between scenarios */
static int s_call_confirm_show;
static xcb_connection_t *s_confirm_connection;
static stage_td *s_confirm_stage;
static const config_td *s_confirm_config;
static char s_confirm_prompt[256];
static char s_confirm_cancel_label[64];
static char s_confirm_confirm_label[64];
static void (*s_confirm_on_confirm)(xcb_connection_t *);
static void (*s_confirm_on_cancel)(xcb_connection_t *);
static uint32_t s_confirm_timeout_seconds;

static int s_call_revert_randr;


/**
 * @brief Reset every stand-in's recorded call state between scenarios
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    s_call_confirm_show = 0;
    s_confirm_connection = NULL;
    s_confirm_stage = NULL;
    s_confirm_config = NULL;
    s_confirm_prompt[0] = '\0';
    s_confirm_cancel_label[0] = '\0';
    s_confirm_confirm_label[0] = '\0';
    s_confirm_on_confirm = NULL;
    s_confirm_on_cancel = NULL;
    s_confirm_timeout_seconds = 0u;
    s_call_revert_randr = 0;
}


/**
 * @brief Recording stand-in for @a menu_confirm_dialog_show
 * @note Complexity: @e O(1)
 */
void menu_confirm_dialog_show(xcb_connection_t *connection,
        stage_td *stage, const config_td *config,
        const char *prompt, const char *cancel_label,
        const char *confirm_label,
        void (*on_confirm)(xcb_connection_t *),
        void (*on_cancel)(xcb_connection_t *),
        uint32_t timeout_seconds)
{
    s_call_confirm_show++;
    s_confirm_connection = connection;
    s_confirm_stage = stage;
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
 * @brief Link-only stand-in for @a stage_action_randr_revert_profiles
 * @note Complexity: @e O(1)
 */
void stage_action_randr_revert_profiles(void)
{
    s_call_revert_randr++;
}


/* dialog_rrsafe_show delegates to menu_confirm_dialog_show exactly
 * once, forwarding the connection, stage, and config unchanged */
static void s_test_show_delegates_to_confirm(void)
{
    s_reset();
    dialog_rrsafe_show(s_fake_connection, &s_fake_stage, &s_fake_config);

    TAP_EQ_INT(s_call_confirm_show, 1,
            "rrsafe_show: delegates to menu_confirm_dialog_show exactly"
            " once");
    TAP_OK(s_confirm_connection == s_fake_connection,
            "rrsafe_show: forwards the connection unchanged");
    TAP_OK(s_confirm_stage == &s_fake_stage,
            "rrsafe_show: forwards the stage unchanged");
    TAP_OK(s_confirm_config == &s_fake_config,
            "rrsafe_show: forwards the config unchanged");
}


/* dialog_rrsafe_show passes non-empty prompt and button labels, both
 * callbacks, and the fixed RandR confirm countdown */
static void s_test_show_passes_labels_and_timeout(void)
{
    s_reset();
    dialog_rrsafe_show(s_fake_connection, &s_fake_stage, &s_fake_config);

    TAP_OK(s_confirm_prompt[0] != '\0',
            "rrsafe_show: passes a non-empty prompt");
    TAP_OK(s_confirm_cancel_label[0] != '\0',
            "rrsafe_show: passes a non-empty cancel label");
    TAP_OK(s_confirm_confirm_label[0] != '\0',
            "rrsafe_show: passes a non-empty confirm/keep label");
    TAP_EQ_INT((int) s_confirm_timeout_seconds,
            (int) DIALOG_RANDR_CONFIRM_TIMEOUT_SECONDS,
            "rrsafe_show: passes the fixed RandR confirm countdown");
    TAP_NOT_NULL(s_confirm_on_confirm,
            "rrsafe_show: passes a non-NULL on_confirm callback");
    TAP_NOT_NULL(s_confirm_on_cancel,
            "rrsafe_show: passes a non-NULL on_cancel callback");
}


/* Invoking the captured on_cancel callback reverts the just-applied
 * RandR profile */
static void s_test_cancel_callback_reverts_randr(void)
{
    s_reset();
    dialog_rrsafe_show(s_fake_connection, &s_fake_stage, &s_fake_config);

    TAP_NOT_NULL(s_confirm_on_cancel,
            "rrsafe_show: on_cancel callback was captured before"
            " invoking it");
    s_confirm_on_cancel(s_fake_connection);

    TAP_EQ_INT(s_call_revert_randr, 1,
            "rrsafe cancel callback: reverts the RandR profile exactly"
            " once");
}


/* Invoking the captured on_confirm callback does nothing: the profile
 * is already live and confirming it only means not reverting it */
static void s_test_confirm_callback_is_a_pure_noop(void)
{
    s_reset();
    dialog_rrsafe_show(s_fake_connection, &s_fake_stage, &s_fake_config);

    TAP_NOT_NULL(s_confirm_on_confirm,
            "rrsafe_show: on_confirm callback was captured before"
            " invoking it");
    s_confirm_on_confirm(s_fake_connection);

    TAP_EQ_INT(s_call_revert_randr, 0,
            "rrsafe confirm callback: never reverts the RandR profile");
}


int main(void)
{
    TAP_PLAN(14);

    s_test_show_delegates_to_confirm();
    s_test_show_passes_labels_and_timeout();
    s_test_cancel_callback_reverts_randr();
    s_test_confirm_callback_is_a_pure_noop();

    return TAP_DONE();
}
