/**
 * @file tests/test_memguard.c
 *
 * @brief Test battery for the restricted-memory watchdog
 *
 * memguard_init/memguard_max_clients is where essentially all of
 * this module's real, pure logic lives (the client-cap formula), and
 * gets the deepest coverage here.  memguard_tick's own real timing
 * behavior gates on MEMGUARD_CHECK_INTERVAL_SECONDS, a fixed 5-second
 * constant (unlike policy/urgency.c's own caller-configurable
 * interval), so waiting for it to actually elapse is not something
 * a fast unit test should do; only its initial guard clauses (which
 * never reach that wait) are covered here.  menu_message_dialog_is_
 * open/show and sysmem_self_rss_mib are stubbed below purely to
 * satisfy the linker and, for the RSS one, to make the guard clauses
 * that do get exercised deterministic.
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

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <harness/tap.h>
#include <memguard.h>
#include <menu/dialog/message.h>


static int s_dialog_show_calls = 0;

bool menu_message_dialog_is_open(void)
{
    return false;
}

void menu_message_dialog_show(xcb_connection_t *connection,
        stage_td *stage, const config_td *config,
        const char *message, menu_msg_level_e level)
{
    (void) connection;
    (void) stage;
    (void) config;
    (void) message;
    (void) level;
    s_dialog_show_calls++;
}

bool sysmem_self_rss_mib(uint32_t *out_mib)
{
    (void) out_mib;
    return false;
}


/* memguard_init(0) turns the watchdog off entirely: no client cap */
static void s_test_init_zero_disables(void)
{
    memguard_init(0u);
    TAP_EQ_INT((long) memguard_max_clients(), 0,
            "a ceiling of 0 computes a client cap of 0 (disabled)");
}


/* A ceiling above the baseline still computes a real cap:
 * budget = 10 - 8 (baseline) = 2 MiB; 2*1024/256 = 8 clients */
static void s_test_init_min_ceiling(void)
{
    memguard_init(10u);
    TAP_EQ_INT((long) memguard_max_clients(), 8,
            "a 10 MiB ceiling computes a cap of 8 clients");
}


/* A ceiling at or below the baseline itself leaves no budget at
 * all, but the cap still floors at MEMGUARD_MIN_CLIENTS (1), never 0 */
static void s_test_init_ceiling_at_baseline_floors_to_min(void)
{
    memguard_init(6u);
    TAP_EQ_INT((long) memguard_max_clients(), 1,
            "a ceiling equal to the baseline still floors to 1 client");

    memguard_init(3u);
    TAP_EQ_INT((long) memguard_max_clients(), 1,
            "a ceiling below the baseline also floors to 1 client");
}


/* A very large ceiling still clamps the cap at
 * MEMGUARD_ABSOLUTE_MAX_CLIENTS (64), never above it */
static void s_test_init_large_ceiling_caps_at_max(void)
{
    memguard_init(100000u);
    TAP_EQ_INT((long) memguard_max_clients(), 64,
            "an oversized ceiling clamps the cap at 64 clients");
}


/* memguard_tick's own initial guard clauses: a disabled watchdog, or
 * any missing required parameter, returns immediately without ever
 * reaching the dialog */
static void s_test_tick_guards(void)
{
    memguard_init(0u);
    s_dialog_show_calls = 0;
    memguard_tick(NULL, NULL, NULL);
    TAP_EQ_INT(s_dialog_show_calls, 0,
            "a disabled watchdog (ceiling 0) never shows a dialog");

    memguard_init(10u);
    s_dialog_show_calls = 0;
    memguard_tick(NULL, NULL, NULL);
    TAP_EQ_INT(s_dialog_show_calls, 0,
            "a NULL connection is a no-op, no crash");
}


/* memguard_warn_client_cap's own guard clauses behave the same way */
static void s_test_warn_client_cap_guards(void)
{
    s_dialog_show_calls = 0;
    memguard_warn_client_cap(NULL, NULL, NULL);
    TAP_EQ_INT(s_dialog_show_calls, 0,
            "missing required parameters: no dialog, no crash");
}


int main(void)
{
    TAP_PLAN(8);

    s_test_init_zero_disables();
    s_test_init_min_ceiling();
    s_test_init_ceiling_at_baseline_floors_to_min();
    s_test_init_large_ceiling_caps_at_max();
    s_test_tick_guards();
    s_test_warn_client_cap_guards();

    return TAP_DONE();
}
