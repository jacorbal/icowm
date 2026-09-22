/**
 * @file tests/policy/test_focus.c
 *
 * @brief Test battery for the focus policy predicate
 *
 * focus_apply and focus_adopt, this module's other public functions,
 * are deliberately not covered here: they call ccmd_client_focus,
 * ccmd_client_unfocus and their _publish halves,
 * enact_client_raise/enact_client_hide, and issue real XCB requests
 * through each, none of which can run meaningfully without a live X
 * connection.  focus_is_sloppy is the one piece of real,
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
#include <config.h>
#include <client.h>
#include <desktop.h>
#include <stage.h>
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

void ccmd_client_focus_publish(client_td *client)
{
    (void) client;
}

void ccmd_client_unfocus_publish(client_td *client)
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

/* Link-only stand-in: the focus order walks the desktop's own client
 * table to filter by desktop, which no test here builds */
client_td *desktop_find_client_by_id(const desktop_td *desktop,
        xcb_window_t client_id)
{
    (void) desktop;
    (void) client_id;
    return NULL;
}

void ipc_broadcast_event(uint64_t type, cJSON *fields)
{
    (void) type;

    /* The real one takes ownership of the payload its caller built;
     * a stand-in that merely ignored it would leak on every focus */
    cJSON_Delete(fields);
}

client_td *lookup_find_client(list_td *stages, xcb_window_t window,
        stage_td **out_stage, desktop_td **out_desktop)
{
    (void) stages;
    (void) window;
    (void) out_stage;
    (void) out_desktop;
    return NULL;
}

/** Recording stand-in for @a scmd_stage_viewport_center_on_client
 *  (cmds/stage.c): 'focus_apply' brings the client into view before
 *  handing it the keyboard, and this file has no viewport to pan */
static int s_call_viewport_center;
static client_td *s_last_centered;

void scmd_stage_viewport_center_on_client(stage_td *stage,
        client_td *client)
{
    (void) stage;

    s_call_viewport_center++;
    s_last_centered = client;
}


/** For a client with no transient parent of its own, its own top
 *  parent (the redirect target) is always itself, the same as the
 *  real function would report for it; every client this file builds
 *  is exactly that case. */
client_td *ccmd_client_focus_target(client_td *client)
{
    return client;
}

/** For a client with no transient descendants at all, there is
 *  nothing to bring back onto its own desktop; every client this
 *  file builds is exactly that case too. */
void ccmd_client_bring_family(client_td *client)
{
    (void) client;
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


/* Every "focus this client" path in the manager funnels through
 * focus_apply, so bringing the client into view belongs here rather
 * than in each caller: the window list, the cycle menu, the search
 * box and the '_NET_ACTIVE_WINDOW' handler all get it at once */
static void s_test_apply_brings_the_client_into_view(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;

    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    /* ICCCM input model: focus_apply refuses a client that could
     * never take real keyboard focus, before ever reaching the pan */
    client.hints_icccm.hints.accepts_input = true;

    s_call_viewport_center = 0;
    s_last_centered = NULL;
    focus_apply(NULL, &stage, &desktop, &client, false, NULL);

    TAP_EQ_INT(s_call_viewport_center, 1,
            "focusing a client asks the viewport to bring it into"
            " view exactly once");
    TAP_OK(s_last_centered == &client,
            "and the client panned to is the one being focused");
}


int main(void)
{
    TAP_PLAN(5);

    s_test_null_config_is_not_sloppy();
    s_test_reflects_configured_policy();

    s_test_apply_brings_the_client_into_view();

    return TAP_DONE();
}
