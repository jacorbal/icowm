/**
 * @file tests/test_scratchpad.c
 *
 * @brief Test battery for the scratchpad
 *
 * scratchpad_position holds essentially all the real, pure logic in
 * this module (area resolution with/without margins, size resolution
 * fixed-vs-"max", clamping, and the per-edge placement switch already
 * covered on its own during the earlier clang-tidy work), so it gets
 * the deepest coverage here.  scratchpad_toggle's own real behavior
 * (launching a process, sending real X focus/hide/unhide requests)
 * cannot run meaningfully without a live connection; it is only ever
 * invoked here, once, as a way to legitimately set the module's own
 * internal "awaiting a launch" flag so scratchpad_notice_client_
 * created has something real to claim afterward, not to test
 * scratchpad_toggle's own behavior.  Every non-macro function
 * scratchpad.c calls elsewhere is stubbed below purely to satisfy the
 * linker (this file links the whole of scratchpad.c as one
 * translation unit); none of those stubs simulate real behavior.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Local includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <harness/tap.h>
#include <scratchpad.h>
#include <surface.h>
#include <wm/internal.h>


/* Captures the arguments of the one real call scratchpad_position
 * itself makes, so tests can assert on them */
static bool s_resize_called;
static int32_t s_resize_x;
static int32_t s_resize_y;
static uint32_t s_resize_w;
static uint32_t s_resize_h;

void enact_client_resize(client_td *client, struct geometry_s geom)
{
    (void) client;
    s_resize_called = true;
    s_resize_x = geom.pos.x;
    s_resize_y = geom.pos.y;
    s_resize_w = geom.dim.w;
    s_resize_h = geom.dim.h;
}


void enact_client_resize_force(client_td *client, struct geometry_s geom)
{
    (void) client;
    s_resize_called = true;
    s_resize_x = geom.pos.x;
    s_resize_y = geom.pos.y;
    s_resize_w = geom.dim.w;
    s_resize_h = geom.dim.h;
}


/** Link-only stand-ins: nothing here simulates real behavior, except
 *  desktop_action_process_launch_with_class, whose success (return
 *  0) is what legitimately arms scratchpad_toggle's own "awaiting a
 *  launch" flag for the one-time setup below. */
void ccmd_client_pin(client_td *client) { (void) client; }

void ccmd_client_reclass(client_td *client,
        const char *restrict class_name,
        const char *restrict instance_name)
{
    (void) client;
    (void) class_name;
    (void) instance_name;
}

void ccmd_client_toggle_decorate(client_td *client) { (void) client; }

void ccmd_client_set_border_override(client_td *client, uint32_t color,
        uint32_t width)
{
    (void) client;
    (void) color;
    (void) width;
}

void client_border_apply(client_td *client, bool use_active_style)
{
    (void) client;
    (void) use_active_style;
}

int desktop_action_process_launch_with_class(desktop_td *desktop,
        const char *restrict executable_path,
        const char *restrict class_name,
        pid_t *restrict out_pid)
{
    (void) desktop;
    (void) executable_path;
    (void) class_name;
    /* Matches the memset-zeroed 'client_td' instances every test in
     * this file claims through 's_claim_as_scratchpad': their own
     * 'process.pid' is 0, not client_init's real '-1' unset default,
     * so the simulated launch here has to report 0 back for the real
     * PID check in 'scratchpad_notice_client_created' to still let
     * this file's own claim path through. */
    if (out_pid != NULL) {
        *out_pid = 0;
    }
    return 0;
}

void enact_client_hide(client_td *client) { (void) client; }
void enact_client_unhide(client_td *client) { (void) client; }

void enact_desktop_client_send(desktop_td *desktop, client_td *client,
        desktop_td *target)
{
    (void) desktop;
    (void) client;
    (void) target;
}

void focus_apply(list_td *surfaces, surface_td *surface,
        desktop_td *desktop, client_td *client, bool raise,
        const config_td *cfg)
{
    (void) surfaces;
    (void) surface;
    (void) desktop;
    (void) client;
    (void) raise;
    (void) cfg;
}

desktop_td *wm_get_client_desktop(const client_td *client)
{
    (void) client;
    return NULL;
}

surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return NULL;
}

/* wm_config and wm_surfaces need no stand-in of their own here:
 * both are already real, genuine functions in wm/instance.c, which
 * this recipe already links (for wm_config specifically, this
 * matters: every test in this file sets 'wm.config' directly on a
 * stack-allocated wm_td before calling scratchpad_toggle, expecting
 * that exact value read back, exactly what the real accessor does). */

/* A small, realistic elapsed time, matching what the real function
 * would actually report for two calls this close together in a
 * test's own fast execution, rather than assuming a large one:
 * scratchpad_toggle's own awaiting-launch timeout check
 * (WM_SCRATCHPAD_AWAIT_TIMEOUT_SECONDS) is meant to catch a launch
 * that genuinely never happened, not the sub-millisecond gap between
 * one test call and the next. */
long clock_ms_since(const struct timespec *start)
{
    (void) start;
    return 0;
}


/**
 * @brief Legitimately arm the scratchpad and claim 'client' as it,
 *        through the real public entry points, so scratchpad_is_
 *        client(client) is true afterward
 *
 * Releases whatever client a previous call to this same helper left
 * claimed first: scratchpad_toggle only takes the "launch a new one"
 * path when its own internal s_scratchpad_client is still NULL, and
 * without this, a second call in a later test would instead walk the
 * "already alive" branch against a now out-of-scope local from a
 * prior test's own stack frame.
 */
static client_td *s_currently_claimed = NULL;

static void s_claim_as_scratchpad(client_td *client)
{
    wm_td wm;
    config_td config;
    desktop_td desktop;

    if (s_currently_claimed != NULL) {
        scratchpad_notice_client_destroyed(s_currently_claimed);
    }

    memset(&wm, 0, sizeof(wm));
    memset(&config, 0, sizeof(config));
    memset(&desktop, 0, sizeof(desktop));
    config.base.scratchpad.is_enabled = true;
    wm.config = &config;

    scratchpad_toggle(&wm, &desktop);
    scratchpad_notice_client_created(client);
    s_currently_claimed = client;
}


/* scratchpad_is_client and scratchpad_notice_client_destroyed:
 * basic lifecycle, driven through the real public claim path above */
static void s_test_is_client_lifecycle(void)
{
    client_td client;
    client_td other;

    memset(&client, 0, sizeof(client));
    memset(&other, 0, sizeof(other));

    TAP_OK(!scratchpad_is_client(&client),
            "not yet claimed: not reported as the scratchpad");
    TAP_OK(!scratchpad_is_client(NULL),
            "NULL is never the scratchpad");

    s_claim_as_scratchpad(&client);

    TAP_OK(scratchpad_is_client(&client),
            "after being claimed, it is reported as the scratchpad");
    TAP_OK(!scratchpad_is_client(&other),
            "an unrelated client is never the scratchpad");

    scratchpad_notice_client_destroyed(&other);
    TAP_OK(scratchpad_is_client(&client),
            "destroying an unrelated client leaves the real one" \
            " untouched");

    scratchpad_notice_client_destroyed(&client);
    TAP_OK(!scratchpad_is_client(&client),
            "destroying the real scratchpad client clears the" \
            " reference");
    s_currently_claimed = NULL;
}


/* scratchpad_notice_client_created's own two guard clauses: neither
 * a NULL client nor an unarmed flag ever claims anything */
static void s_test_notice_created_guards(void)
{
    /* Not currently awaiting a launch (no scratchpad_toggle armed it
     * for this specific call): must be a no-op */
    client_td unclaimed;

    memset(&unclaimed, 0, sizeof(unclaimed));
    scratchpad_notice_client_created(&unclaimed);
    TAP_OK(!scratchpad_is_client(&unclaimed),
            "not currently awaiting a launch: nothing is claimed");

    scratchpad_notice_client_created(NULL);
    TAP_OK(true, "a NULL client is safely ignored, no crash");
}


/* A client reporting a definite, different PID from the one actually
 * launched (the stub above always reports 0) must never be claimed;
 * this is the race scratchpad_notice_client_created's own PID check
 * exists to close */
static void s_test_notice_created_pid_mismatch(void)
{
    wm_td wm;
    config_td config;
    desktop_td desktop;
    client_td wrong_client;
    client_td matching_client;

    memset(&wm, 0, sizeof(wm));
    memset(&config, 0, sizeof(config));
    memset(&desktop, 0, sizeof(desktop));
    memset(&wrong_client, 0, sizeof(wrong_client));
    memset(&matching_client, 0, sizeof(matching_client));
    config.base.scratchpad.is_enabled = true;
    wm.config = &config;

    wrong_client.process.pid = 99999;

    scratchpad_toggle(&wm, &desktop);
    scratchpad_notice_client_created(&wrong_client);
    TAP_OK(!scratchpad_is_client(&wrong_client),
            "a definite PID mismatch is never claimed");

    /* Still awaiting the real launch (PID 0, per the stub above):
     * claim it now with a matching client so the module's own static
     * state is clean before the next test runs */
    matching_client.process.pid = 0;
    scratchpad_notice_client_created(&matching_client);
    scratchpad_notice_client_destroyed(&matching_client);
}


/* A client whose own PID is unknown (client_init's real -1 default,
 * for an application that never sets '_NET_WM_PID') cannot be
 * disproven by the PID check, so it must still be claimable: an
 * application that never reports its own PID needs to keep working,
 * not lose scratchpad support outright */
static void s_test_notice_created_pid_unknown_fallback(void)
{
    wm_td wm;
    config_td config;
    desktop_td desktop;
    client_td client;

    memset(&wm, 0, sizeof(wm));
    memset(&config, 0, sizeof(config));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    config.base.scratchpad.is_enabled = true;
    wm.config = &config;

    client.process.pid = (pid_t) -1;

    scratchpad_toggle(&wm, &desktop);
    scratchpad_notice_client_created(&client);
    TAP_OK(scratchpad_is_client(&client),
            "an application that never reports its own PID is" \
            " still claimed");

    scratchpad_notice_client_destroyed(&client);
}


/* scratchpad_position is a no-op (no resize issued) for anything
 * that is not the current scratchpad client, or with a NULL desktop
 * or surface */
static void s_test_position_guards(void)
{
    client_td not_scratchpad;
    client_td client;
    desktop_td desktop;
    surface_td surface;
    config_td config;

    memset(&not_scratchpad, 0, sizeof(not_scratchpad));
    memset(&client, 0, sizeof(client));
    memset(&desktop, 0, sizeof(desktop));
    memset(&surface, 0, sizeof(surface));
    memset(&config, 0, sizeof(config));
    surface.config = &config;

    s_claim_as_scratchpad(&client);

    s_resize_called = false;
    scratchpad_position(&not_scratchpad, &desktop, &surface);
    TAP_OK(!s_resize_called, "not the scratchpad client: no resize" \
            " issued");

    s_resize_called = false;
    scratchpad_position(&client, NULL, &surface);
    TAP_OK(!s_resize_called, "a NULL desktop: no resize issued");

    s_resize_called = false;
    scratchpad_position(&client, &desktop, NULL);
    TAP_OK(!s_resize_called, "a NULL surface: no resize issued");

    s_resize_called = false;
    surface.config = NULL;
    scratchpad_position(&client, &desktop, &surface);
    TAP_OK(!s_resize_called, "a surface with no config: no resize" \
            " issued");
}


/* scratchpad_position, with ignore_margins set, sizes and positions
 * against the full surface area rather than desktop->workarea; hand-
 * computed for CONFIG_SCRATCHPAD_EDGE_BOTTOM with a fixed size */
static void s_test_position_ignore_margins_bottom_fixed(void)
{
    client_td client;
    desktop_td desktop;
    surface_td surface;
    config_td config;

    memset(&client, 0, sizeof(client));
    memset(&desktop, 0, sizeof(desktop));
    memset(&surface, 0, sizeof(surface));
    memset(&config, 0, sizeof(config));

    s_claim_as_scratchpad(&client);

    surface.config = &config;
    surface.properties.dim.w = 1000u;
    surface.properties.dim.h = 800u;
    /* A nonzero workarea that must be deliberately ignored */
    desktop.workarea.pos.x = 50;
    desktop.workarea.pos.y = 50;
    desktop.workarea.dim.w = 500u;
    desktop.workarea.dim.h = 400u;

    config.base.scratchpad.ignore_margins = true;
    config.base.scratchpad.edge = CONFIG_SCRATCHPAD_EDGE_BOTTOM;
    config.base.scratchpad.width.mode = CONFIG_SCRATCHPAD_SIZE_FIXED;
    config.base.scratchpad.width.pixels = 300u;
    config.base.scratchpad.height.mode = CONFIG_SCRATCHPAD_SIZE_FIXED;
    config.base.scratchpad.height.pixels = 200u;

    s_resize_called = false;
    scratchpad_position(&client, &desktop, &surface);

    /* No border (client->frame == 0 but client->theme == NULL means
     * client_border_width returns 0), area is the full 1000x800
     * surface, width/height fixed at 300x200.  BOTTOM: x centered,
     * y flush with the bottom: x = (1000-300)/2 = 350,
     * y = 800-200 = 600 */
    TAP_OK(s_resize_called, "a resize was issued");
    TAP_EQ_INT(s_resize_x, 350, "BOTTOM edge: x is horizontally centered");
    TAP_EQ_INT(s_resize_y, 600, "BOTTOM edge: y is flush with the bottom");
    TAP_EQ_INT((long) s_resize_w, 300, "fixed width is used as-is");
    TAP_EQ_INT((long) s_resize_h, 200, "fixed height is used as-is");
}


/* scratchpad_position without ignore_margins uses desktop->workarea
 * instead of the full surface; hand-computed for
 * CONFIG_SCRATCHPAD_EDGE_LEFT with a "max" width */
static void s_test_position_workarea_left_max_width(void)
{
    client_td client;
    desktop_td desktop;
    surface_td surface;
    config_td config;

    memset(&client, 0, sizeof(client));
    memset(&desktop, 0, sizeof(desktop));
    memset(&surface, 0, sizeof(surface));
    memset(&config, 0, sizeof(config));

    s_claim_as_scratchpad(&client);

    surface.config = &config;
    surface.properties.dim.w = 1000u;
    surface.properties.dim.h = 800u;
    desktop.workarea.pos.x = 50;
    desktop.workarea.pos.y = 20;
    desktop.workarea.dim.w = 500u;
    desktop.workarea.dim.h = 400u;

    config.base.scratchpad.ignore_margins = false;
    config.base.scratchpad.edge = CONFIG_SCRATCHPAD_EDGE_LEFT;
    config.base.scratchpad.width.mode = CONFIG_SCRATCHPAD_SIZE_MAX;
    config.base.scratchpad.height.mode = CONFIG_SCRATCHPAD_SIZE_FIXED;
    config.base.scratchpad.height.pixels = 100u;

    s_resize_called = false;
    scratchpad_position(&client, &desktop, &surface);

    /* area = workarea (50,20,500,400), no border.  width = "max" =
     * avail_w = 500.  LEFT: x = area_x = 50,
     * y = area_y + (avail_h - height) / 2 = 20 + (400-100)/2 = 20+150
     * = 170 */
    TAP_EQ_INT(s_resize_x, 50, "LEFT edge: x is flush with the" \
            " workarea's own left");
    TAP_EQ_INT(s_resize_y, 170, "LEFT edge: y is vertically centered" \
            " within the workarea");
    TAP_EQ_INT((long) s_resize_w, 500,
            "a \"max\" width resolves to the full available width");
    TAP_EQ_INT((long) s_resize_h, 100, "fixed height is used as-is");
}


/* scratchpad_position clamps an oversized fixed width/height down to
 * the available area, rather than letting the client overflow it */
static void s_test_position_clamps_oversized_fixed_size(void)
{
    client_td client;
    desktop_td desktop;
    surface_td surface;
    config_td config;

    memset(&client, 0, sizeof(client));
    memset(&desktop, 0, sizeof(desktop));
    memset(&surface, 0, sizeof(surface));
    memset(&config, 0, sizeof(config));

    s_claim_as_scratchpad(&client);

    surface.config = &config;
    surface.properties.dim.w = 200u;
    surface.properties.dim.h = 150u;

    config.base.scratchpad.ignore_margins = true;
    config.base.scratchpad.edge = CONFIG_SCRATCHPAD_EDGE_TOP;
    config.base.scratchpad.width.mode = CONFIG_SCRATCHPAD_SIZE_FIXED;
    config.base.scratchpad.width.pixels = 9999u;
    config.base.scratchpad.height.mode = CONFIG_SCRATCHPAD_SIZE_FIXED;
    config.base.scratchpad.height.pixels = 9999u;

    scratchpad_position(&client, &desktop, &surface);

    TAP_EQ_INT((long) s_resize_w, 200,
            "an oversized fixed width clamps down to the available area");
    TAP_EQ_INT((long) s_resize_h, 150,
            "an oversized fixed height clamps down to the surface" \
            " height");
}


int main(void)
{
    TAP_PLAN(25);

    s_test_is_client_lifecycle();
    s_test_notice_created_guards();
    s_test_notice_created_pid_mismatch();
    s_test_notice_created_pid_unknown_fallback();
    s_test_position_guards();
    s_test_position_ignore_margins_bottom_fixed();
    s_test_position_workarea_left_max_width();
    s_test_position_clamps_oversized_fixed_size();

    return TAP_DONE();
}
