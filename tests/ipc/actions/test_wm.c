/**
 * @file tests/ipc/actions/test_wm.c
 *
 * @brief Test battery for the IPC commands mirroring enact.h's
 *        whole-window-manager actions
 *
 * ipc/actions/wm.c is a thin translation layer over three
 * enact_wm_* calls (enact_wm_exit, enact_wm_restart,
 * enact_wm_configuration_reload), each stubbed below as a
 * controllable stand-in so every test can force both the success and
 * the failure path of each handler without a real shutdown, restart,
 * or configuration reload ever taking place.  ipc_response_ok and
 * ipc_response_error (ipc/response.c) are linked for real, exactly
 * as tests/ipc/test_response.c already exercises them on their own,
 * since the handlers here only ever forward to them, never build a
 * response by hand.
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

/* JSON includes */
#include <cjson/cJSON.h>

/* Local includes */
#include <harness/tap.h>
#include <ipc/actions/wm.h>


/** Controllable return value for the enact_wm_exit stand-in below */
static int s_exit_return;

/** Controllable return value for the enact_wm_restart stand-in below */
static int s_restart_return;

/** Controllable return value for the enact_wm_configuration_reload
 *  stand-in below */
static int s_reload_return;

/** Call counters, reset by s_reset before each scenario */
static int s_call_exit;
static int s_call_restart;
static int s_call_reload;

/** Last wm pointer enact_wm_configuration_reload was called with,
 *  confirming the handler forwards its own wm argument through
 *  unchanged */
static const wm_td *s_last_reload_wm;


/**
 * @brief Controllable stand-in for enact_wm_exit
 *
 * @note Complexity: @e O(1)
 */
int enact_wm_exit(void)
{
    s_call_exit++;
    return s_exit_return;
}


/**
 * @brief Controllable stand-in for enact_wm_restart
 *
 * @note Complexity: @e O(1)
 */
int enact_wm_restart(void)
{
    s_call_restart++;
    return s_restart_return;
}


/**
 * @brief Controllable stand-in for enact_wm_configuration_reload
 *
 * @note Complexity: @e O(1)
 */
int enact_wm_configuration_reload(const wm_td *wm)
{
    s_call_reload++;
    s_last_reload_wm = wm;
    return s_reload_return;
}


/** Reset every stub's controllable state and call counters */
static void s_reset(void)
{
    s_exit_return = 0;
    s_restart_return = 0;
    s_reload_return = 0;
    s_call_exit = 0;
    s_call_restart = 0;
    s_call_reload = 0;
    s_last_reload_wm = NULL;
}


/* ipc_action_exit_wm: a successful shutdown request returns a bare
 * "ok" response */
static void s_test_exit_wm_success(void)
{
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_exit_return = 0;

    resp = ipc_action_exit_wm(NULL, NULL);

    TAP_NOT_NULL(resp, "a successful exit request returns a response");
    TAP_EQ_INT(s_call_exit, 1, "enact_wm_exit is called exactly once");
    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "the response reports ok: true");
    cJSON_Delete(resp);
}


/* ipc_action_exit_wm: a nonzero enact_wm_exit return builds an error
 * response instead */
static void s_test_exit_wm_failure(void)
{
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_exit_return = -1;

    resp = ipc_action_exit_wm(NULL, NULL);

    TAP_NOT_NULL(resp, "a failed exit request still returns a response");
    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "the response reports ok: false on failure");
    cJSON_Delete(resp);
}


/* ipc_action_restart_wm: a successful restart request returns a bare
 * "ok" response */
static void s_test_restart_wm_success(void)
{
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_restart_return = 0;

    resp = ipc_action_restart_wm(NULL, NULL);

    TAP_EQ_INT(s_call_restart, 1,
            "enact_wm_restart is called exactly once");
    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "a successful restart reports ok: true");
    cJSON_Delete(resp);
}


/* ipc_action_restart_wm: a nonzero enact_wm_restart return builds an
 * error response instead */
static void s_test_restart_wm_failure(void)
{
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_restart_return = 1;

    resp = ipc_action_restart_wm(NULL, NULL);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "a failed restart reports ok: false");
    cJSON_Delete(resp);
}


/* ipc_action_reload_config: a successful reload forwards the exact
 * wm pointer it was given and returns a bare "ok" response */
static void s_test_reload_config_success(void)
{
    int fake_wm_storage;
    wm_td *const fake_wm = (wm_td *) &fake_wm_storage;
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_reload_return = 0;

    resp = ipc_action_reload_config(fake_wm, NULL);

    TAP_EQ_INT(s_call_reload, 1,
            "enact_wm_configuration_reload is called exactly once");
    TAP_OK(s_last_reload_wm == fake_wm,
            "the handler's own wm argument is forwarded unchanged");
    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "a successful reload reports ok: true");
    cJSON_Delete(resp);
}


/* ipc_action_reload_config: a nonzero enact_wm_configuration_reload
 * return builds an error response instead */
static void s_test_reload_config_failure(void)
{
    cJSON *resp;
    cJSON *ok_field;
    cJSON *error_field;

    s_reset();
    s_reload_return = 2;

    resp = ipc_action_reload_config(NULL, NULL);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "a failed reload reports ok: false");
    error_field = cJSON_GetObjectItem(resp, "error");
    TAP_OK(error_field != NULL && cJSON_IsString(error_field),
            "a failed reload carries a string error field");
    cJSON_Delete(resp);
}


int main(void)
{
    TAP_PLAN(13);

    s_test_exit_wm_success();
    s_test_exit_wm_failure();
    s_test_restart_wm_success();
    s_test_restart_wm_failure();
    s_test_reload_config_success();
    s_test_reload_config_failure();

    return TAP_DONE();
}
