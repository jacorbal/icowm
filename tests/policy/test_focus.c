/**
 * @file tests/policy/test_focus.c
 *
 * @brief Test battery for the focus policy predicate
 *
 * focus_apply, this module's other public function, is deliberately
 * not covered here: it calls ccmd_client_focus/ccmd_client_unfocus,
 * enact_client_raise/enact_client_hide, and issues a real XCB
 * request through each, none of which can run meaningfully without a
 * live X connection.  focus_is_sloppy is the one piece of real,
 * pure logic in this module, and gets its own full coverage.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <string.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <harness/tap.h>
#include <policy/focus.h>


/** Link-only stand-ins: policy/focus.c as a whole references every
 *  one of these (all from focus_apply, which this file deliberately
 *  does not test -- see the note above), so the linker needs a
 *  definition for each somewhere even though nothing here ever
 *  calls any of them. */
void ccmd_client_focus(client_td *client)
{
    (void) client;
}

void ccmd_client_unfocus(client_td *client)
{
    (void) client;
}

void enact_client_hide(client_td *client)
{
    (void) client;
}

void enact_client_raise(client_td *client)
{
    (void) client;
}

void ccmd_desktop_enforce_layers(desktop_td *desktop)
{
    (void) desktop;
}

bool scratchpad_is_client(const client_td *client)
{
    (void) client;
    return false;
}

int desktop_action_client_send_front(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;
    return 0;
}

void ipc_broadcast_event(uint32_t type, cJSON *fields)
{
    (void) type;
    (void) fields;
}

client_td *lookup_find_client(list_td *surfaces, xcb_window_t window,
        surface_td **out_surface, desktop_td **out_desktop)
{
    (void) surfaces;
    (void) window;
    (void) out_surface;
    (void) out_desktop;
    return NULL;
}


/* A NULL configuration is never sloppy: focus_is_sloppy defaults to
 * the safer, more conventional click-to-focus behavior */
static void s_test_null_config_is_not_sloppy(void)
{
    TAP_OK(!focus_is_sloppy(NULL),
            "a NULL configuration is never reported as sloppy");
}


/* focus_is_sloppy reflects exactly the configured focus_policy field:
 * true only under CONFIG_FOCUS_POLICY_SLOPPY, false under
 * CONFIG_FOCUS_POLICY_CLICK */
static void s_test_reflects_configured_policy(void)
{
    config_td config;

    memset(&config, 0, sizeof(config));
    config.base.windows.focus_policy = CONFIG_FOCUS_POLICY_SLOPPY;
    TAP_OK(focus_is_sloppy(&config),
            "CONFIG_FOCUS_POLICY_SLOPPY is reported as sloppy");

    config.base.windows.focus_policy = CONFIG_FOCUS_POLICY_CLICK;
    TAP_OK(!focus_is_sloppy(&config),
            "CONFIG_FOCUS_POLICY_CLICK is not reported as sloppy");
}


int main(void)
{
    TAP_PLAN(3);

    s_test_null_config_is_not_sloppy();
    s_test_reflects_configured_policy();

    return TAP_DONE();
}
