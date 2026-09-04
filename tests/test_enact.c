/**
 * @file tests/test_enact.c
 *
 * @brief Test battery for the window-manager-level (as opposed to
 *        client, desktop, or surface) actions in enact.c
 *
 * Exercises 'enact_wm_exit', 'enact_wm_restart', and 'enact_wm_
 * configuration_reload' (enact.c) linked against nothing from the
 * rest of the project at all: every one of the three real functions
 * it calls through, 'wm_request_stop' and 'wm_request_restart'
 * (wm.c), and 'wm_action_config_reload' (wm/actions.c), is itself
 * only a thin forwarder in the file under test, so what actually
 * matters here is that each 'enact_wm_*' entry point calls the right
 * one, with the right argument, and returns (or conditionally acts
 * on) exactly what that call reports back.
 *
 * 'wm_request_stop' and 'wm_request_restart' both live in wm.c,
 * a 1000-line file that pulls in session/render/systray/xsettings/
 * startup subsystems and a live 'wm' singleton only 'wm_start' ever
 * initializes; 'wm_action_config_reload' (wm/actions.c) goes further
 * still, reloading configuration from disk, rebinding keyboard and
 * mouse grabs over a real X connection, and re-running theme sync,
 * the same function already left untested in a previous round for
 * exactly that reason (see tests/REPORT_RANDR_WM_ACTIONS.md).  None
 * of that real behavior is what enact.c itself is responsible for,
 * so all three are controllable, recording stand-ins here instead,
 * each returning whatever this file's own scenario primed it to
 * return.  'ipc_broadcast_event' (ipc.c) is a recording stand-in for
 * the same reason 'tests/rules/test_apply.c' already treats it as
 * one: broadcasting itself is IPC's own concern, already covered
 * there and in tests/ipc/, not enact.c's.
 *
 * 'wm_td' is opaque outside wm.c itself; the dummy, non-null address
 * this file passes as one is never dereferenced by any stand-in
 * below, only compared for identity against what 'enact_wm_
 * configuration_reload' itself forwarded, following the same opaque-
 * handle pattern 'tests/rules/test_apply.c' already uses for its own
 * 's_fake_wm'.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdint.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Local includes */
#include <enact.h>
#include <harness/tap.h>
#include <ipc.h>
#include <wm.h>


/** A non-null opaque handle standing in for a real wm_td, which this
 *  file never actually builds, since the type is opaque outside
 *  wm.c itself; see this file's own top comment for why nothing
 *  below ever dereferences it */
static int s_fake_wm_storage;
static wm_td *const s_fake_wm = (wm_td *) &s_fake_wm_storage;

/** Call counters and test-controlled return values for the three
 *  real functions enact.c forwards to, reset by 's_reset' before
 *  each scenario */
static int s_wm_request_stop_calls;
static int s_wm_request_stop_return;

static int s_wm_request_restart_calls;
static int s_wm_request_restart_return;

static int s_wm_action_config_reload_calls;
static const wm_td *s_wm_action_config_reload_last_wm;
static int s_wm_action_config_reload_return;

static int s_ipc_broadcast_calls;
static uint32_t s_ipc_broadcast_last_type;


/**
 * @brief Recording stand-in for @a wm_request_stop
 *
 * @note Complexity: @e O(1)
 */
int wm_request_stop(void)
{
    s_wm_request_stop_calls++;
    return s_wm_request_stop_return;
}


/**
 * @brief Recording stand-in for @a wm_request_restart
 *
 * @note Complexity: @e O(1)
 */
int wm_request_restart(void)
{
    s_wm_request_restart_calls++;
    return s_wm_request_restart_return;
}


/**
 * @brief Recording stand-in for @a wm_action_config_reload
 *
 * @note Complexity: @e O(1)
 */
int wm_action_config_reload(const wm_td *wm)
{
    s_wm_action_config_reload_calls++;
    s_wm_action_config_reload_last_wm = wm;
    return s_wm_action_config_reload_return;
}


/**
 * @brief Recording stand-in for @a ipc_broadcast_event
 *
 * @note Complexity: @e O(1)
 */
void ipc_broadcast_event(uint32_t type, cJSON *fields)
{
    s_ipc_broadcast_calls++;
    s_ipc_broadcast_last_type = type;
    cJSON_Delete(fields);
}


static void s_reset(void)
{
    s_wm_request_stop_calls = 0;
    s_wm_request_stop_return = 0;

    s_wm_request_restart_calls = 0;
    s_wm_request_restart_return = 0;

    s_wm_action_config_reload_calls = 0;
    s_wm_action_config_reload_last_wm = NULL;
    s_wm_action_config_reload_return = 0;

    s_ipc_broadcast_calls = 0;
    s_ipc_broadcast_last_type = 0u;
}


/* enact_wm_exit: forwards straight to wm_request_stop, returning
 * exactly its own status */
static void s_test_wm_exit_forwards_status(void)
{
    int status;

    s_reset();
    s_wm_request_stop_return = 0;
    status = enact_wm_exit();
    TAP_EQ_INT(s_wm_request_stop_calls, 1,
            "'wm_request_stop' ran exactly once");
    TAP_EQ_INT(status, 0,
            "a success status from 'wm_request_stop' passes straight" \
            " through");

    s_reset();
    s_wm_request_stop_return = 1;
    status = enact_wm_exit();
    TAP_EQ_INT(s_wm_request_stop_calls, 1,
            "'wm_request_stop' ran exactly once on the failure path" \
            " too");
    TAP_EQ_INT(status, 1,
            "a failure status from 'wm_request_stop' passes straight" \
            " through as well");
}


/* enact_wm_restart: forwards straight to wm_request_restart,
 * returning exactly its own status */
static void s_test_wm_restart_forwards_status(void)
{
    int status;

    s_reset();
    s_wm_request_restart_return = 0;
    status = enact_wm_restart();
    TAP_EQ_INT(s_wm_request_restart_calls, 1,
            "'wm_request_restart' ran exactly once");
    TAP_EQ_INT(status, 0,
            "a success status from 'wm_request_restart' passes" \
            " straight through");

    s_reset();
    s_wm_request_restart_return = 1;
    status = enact_wm_restart();
    TAP_EQ_INT(s_wm_request_restart_calls, 1,
            "'wm_request_restart' ran exactly once on the failure" \
            " path too");
    TAP_EQ_INT(status, 1,
            "a failure status from 'wm_request_restart' passes" \
            " straight through as well");
}


/* enact_wm_configuration_reload: on success, forwards the same 'wm'
 * pointer to wm_action_config_reload and also broadcasts
 * IPC_EVENT_CONFIG_RELOADED */
static void s_test_configuration_reload_success_broadcasts(void)
{
    int status;

    s_reset();
    s_wm_action_config_reload_return = 0;
    status = enact_wm_configuration_reload(s_fake_wm);

    TAP_EQ_INT(s_wm_action_config_reload_calls, 1,
            "'wm_action_config_reload' ran exactly once");
    TAP_OK(s_wm_action_config_reload_last_wm == s_fake_wm,
            "the exact same 'wm' pointer this call received was" \
            " forwarded on, unchanged");
    TAP_EQ_INT(status, 0,
            "a success status from 'wm_action_config_reload' passes" \
            " straight through");
    TAP_EQ_INT(s_ipc_broadcast_calls, 1,
            "success triggers exactly one IPC broadcast");
    TAP_EQ_INT((long) s_ipc_broadcast_last_type,
            (long) IPC_EVENT_CONFIG_RELOADED,
            "the broadcast event type is 'IPC_EVENT_CONFIG_RELOADED'");
}


/* enact_wm_configuration_reload: on failure, the status is still
 * forwarded, but no broadcast happens */
static void s_test_configuration_reload_failure_no_broadcast(void)
{
    int status;

    s_reset();
    s_wm_action_config_reload_return = 1;
    status = enact_wm_configuration_reload(s_fake_wm);

    TAP_EQ_INT(s_wm_action_config_reload_calls, 1,
            "'wm_action_config_reload' still ran exactly once");
    TAP_EQ_INT(status, 1,
            "a failure status from 'wm_action_config_reload' passes" \
            " straight through");
    TAP_EQ_INT(s_ipc_broadcast_calls, 0,
            "failure never triggers an IPC broadcast");
}


int main(void)
{
    TAP_PLAN(16);

    s_test_wm_exit_forwards_status();
    s_test_wm_restart_forwards_status();
    s_test_configuration_reload_success_broadcasts();
    s_test_configuration_reload_failure_no_broadcast();

    return TAP_DONE();
}
