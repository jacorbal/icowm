/**
 * @file tests/enact/test_surface.c
 *
 * @brief Test battery for surface-level enact actions
 *        (enact/surface.c)
 *
 * Every 'scmd_surface_desktop_switch*' function (cmds/surface.c,
 * already covered on its own by
 * tests/cmds/test_surface_desktop_switch.c) is a call-counting
 * stand-in here, so this file isolates 'enact/surface.c's own
 * responsibility: dispatching to the right command and broadcasting
 * the right IPC event afterward, never re-verifying the command's own
 * internal desktop-switch logic a second time.  'surface_action_
 * desktop_add', 'surface_action_desktop_remove', and 'surface_action_
 * toggle_strutless_maximize' (surface.c) are test-controlled
 * stand-ins reporting whichever outcome each scenario registers, so
 * both the "action succeeded, broadcast and refresh grabs" and the
 * "action refused, do neither" branches run.  'wm_get_keysyms',
 * 'wm_get_config', 'wm_get_surfaces', and 'keyboard_load' are
 * call-counting stand-ins: the real keysyms table needs a live X
 * connection this file has none of.  'ipc_broadcast_event' is
 * a recording stand-in capturing the event type and the two numeric
 * fields cJSON actually built for it (surface_id, desktop_id), with
 * the real cJSON library linked directly, so the broadcast payload
 * itself is exercised for real, only the socket write it would
 * otherwise reach is stood in for.  Each 'scmd_surface_viewport_pan_
 * north/south/east/west' (cmds/surface.c) is a call-counting
 * stand-in too, letting this file isolate 'enact/surface.c's own
 * responsibility for the four panning wrappers: dispatching to the
 * right command alone, without re-verifying the command's own
 * internal panning logic a second time
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
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <cmds/surface.h>
#include <desktop.h>
#include <enact.h>
#include <enact/surface.h>
#include <input/kbd/bind.h>
#include <ipc.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <harness/tap.h>


/** Call counters for each of the five 'scmd_surface_desktop_switch*'
 *  functions, and the desktop id the plain (non-cyclic) one was last
 *  called with */
static int s_call_switch;
static uint32_t s_last_switch_desktop_id;
static int s_call_switch_north;
static int s_call_switch_south;
static int s_call_switch_east;
static int s_call_switch_west;

/** Call-counting stand-in for @a scmd_surface_desktop_switch (cmds/
 *  surface.c)
 *  @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch(surface_td *surface,
        uint32_t desktop_id)
{
    (void) surface;

    s_call_switch++;
    s_last_switch_desktop_id = desktop_id;
}


/** Call-counting stand-in for @a scmd_surface_desktop_switch_north
 *  (cmds/surface.c)
 *  @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch_north(surface_td *surface)
{
    (void) surface;
    s_call_switch_north++;
}


/** Call-counting stand-in for @a scmd_surface_desktop_switch_south
 *  (cmds/surface.c)
 *  @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch_south(surface_td *surface)
{
    (void) surface;
    s_call_switch_south++;
}


/** Call-counting stand-in for @a scmd_surface_desktop_switch_east
 *  (cmds/surface.c)
 *  @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch_east(surface_td *surface)
{
    (void) surface;
    s_call_switch_east++;
}


/** Call-counting stand-in for @a scmd_surface_desktop_switch_west
 *  (cmds/surface.c)
 *  @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch_west(surface_td *surface)
{
    (void) surface;
    s_call_switch_west++;
}


/** Call counter and last-seen direction for
 *  @a scmd_surface_viewport_pan_step, shared by every one of the four
 *  'enact_surface_viewport_pan_*' wrappers */
static int s_call_pan_step;
static enum compass_direction_e s_call_pan_step_direction;

/** Call-counting stand-in for @a scmd_surface_viewport_pan_step
 *  (cmds/surface.c)
 *  @note Complexity: @e O(1)
 */
void scmd_surface_viewport_pan_step(surface_td *surface,
        enum compass_direction_e direction)
{
    (void) surface;
    s_call_pan_step++;
    s_call_pan_step_direction = direction;
}


/** Call counters for the four whole-page pan commands, one each, so
 *  a scenario can tell which of the 'enact_surface_viewport_switch_*'
 *  wrappers reached which */
static int s_call_pan_north;
static int s_call_pan_south;
static int s_call_pan_east;
static int s_call_pan_west;

/** Call-counting stand-in for @a scmd_surface_viewport_pan_north
 *  (cmds/surface.c)
 *  @note Complexity: @e O(1)
 */
void scmd_surface_viewport_pan_north(surface_td *surface)
{
    (void) surface;
    s_call_pan_north++;
}


/** Call-counting stand-in for @a scmd_surface_viewport_pan_south
 *  (cmds/surface.c)
 *  @note Complexity: @e O(1)
 */
void scmd_surface_viewport_pan_south(surface_td *surface)
{
    (void) surface;
    s_call_pan_south++;
}


/** Call-counting stand-in for @a scmd_surface_viewport_pan_east
 *  (cmds/surface.c)
 *  @note Complexity: @e O(1)
 */
void scmd_surface_viewport_pan_east(surface_td *surface)
{
    (void) surface;
    s_call_pan_east++;
}


/** Call-counting stand-in for @a scmd_surface_viewport_pan_west
 *  (cmds/surface.c)
 *  @note Complexity: @e O(1)
 */
void scmd_surface_viewport_pan_west(surface_td *surface)
{
    (void) surface;
    s_call_pan_west++;
}


/** Call counter and last-seen argument for @a scmd_surface_viewport_
 *  goto */
static int s_call_goto;
static uint32_t s_last_goto_page;

/** Call-recording stand-in for @a scmd_surface_viewport_goto
 *  (cmds/surface.c)
 *  @note Complexity: @e O(1)
 */
void scmd_surface_viewport_goto(surface_td *surface, uint32_t page)
{
    (void) surface;
    s_call_goto++;
    s_last_goto_page = page;
}


/** Outcome each of the three surface_action_* stand-ins below should
 *  report, one per action, set by each scenario before calling the
 *  function under test */
static int s_action_desktop_add_result;
static int s_call_action_desktop_add;
static int s_action_desktop_remove_result;
static int s_call_action_desktop_remove;
static int s_action_toggle_strutless_result;
static int s_call_action_toggle_strutless;

/** Test-controlled stand-in for @a surface_action_desktop_add
 *  (surface.c)
 *  @note Complexity: @e O(1)
 */
int surface_action_desktop_add(surface_td *surface)
{
    (void) surface;
    s_call_action_desktop_add++;
    return s_action_desktop_add_result;
}


/** Test-controlled stand-in for @a surface_action_desktop_remove
 *  (surface.c)
 *  @note Complexity: @e O(1)
 */
int surface_action_desktop_remove(surface_td *surface)
{
    (void) surface;
    s_call_action_desktop_remove++;
    return s_action_desktop_remove_result;
}


/** Test-controlled stand-in for
 *  @a surface_action_maximize_toggle_strutless (surface.c)
 *  @note Complexity: @e O(1)
 */
int surface_action_maximize_toggle_strutless(surface_td *surface)
{
    (void) surface;
    s_call_action_toggle_strutless++;
    return s_action_toggle_strutless_result;
}


/** Whichever keysyms table 'wm_get_keysyms' should currently report,
 *  set by each scenario; a null value means "no keysyms table yet",
 *  the exact condition 's_refresh_keyboard_grabs' guards against */
static xcb_key_symbols_t *s_stub_keysyms;
static int s_call_get_keysyms;
static int s_call_get_config;
static int s_call_get_surfaces;
static int s_call_keyboard_load;

/** Test-controlled stand-in for @a wm_get_keysyms (wm.c)
 *  @note Complexity: @e O(1)
 */
xcb_key_symbols_t *wm_get_keysyms(void)
{
    s_call_get_keysyms++;
    return s_stub_keysyms;
}


/** Link-only stand-in for @a wm_get_config (wm.c)
 *  @note Complexity: @e O(1)
 */
config_td *wm_get_config(void)
{
    s_call_get_config++;
    return NULL;
}


/** Link-only stand-in for @a wm_get_surfaces (wm.c)
 *  @note Complexity: @e O(1)
 */
list_td *wm_get_surfaces(void)
{
    s_call_get_surfaces++;
    return NULL;
}


/** Call-counting stand-in for @a keyboard_load (input/kbd/bind.c):
 *  the real one issues XCB grab requests this file has no live
 *  connection to make
 *  @note Complexity: @e O(1)
 */
void keyboard_load(list_td *surfaces, xcb_key_symbols_t *keysyms,
        const config_td *config)
{
    (void) surfaces;
    (void) keysyms;
    (void) config;
    s_call_keyboard_load++;
}


/** Whether, the event type, and the surface_id/desktop_id fields
 *  'ipc_broadcast_event' was last called with; the fields are read
 *  out of the real cJSON object before it is handed back for
 *  cleanup, since this stand-in owns freeing it */
static int s_call_broadcast;
static uint32_t s_last_broadcast_type;
static double s_last_broadcast_surface_id;
static double s_last_broadcast_desktop_id;
static bool s_last_broadcast_had_fields;

/** Recording stand-in for @a ipc_broadcast_event (ipc.c): the real
 *  one writes to every connected IPC socket, which this file has none
 *  of
 *  @note Complexity: @e O(1)
 */
void ipc_broadcast_event(uint32_t type, cJSON *fields)
{
    const cJSON *surface_id_field;
    const cJSON *desktop_id_field;

    s_call_broadcast++;
    s_last_broadcast_type = type;
    s_last_broadcast_had_fields = (fields != NULL);

    if (fields != NULL) {
        surface_id_field = cJSON_GetObjectItem(fields, "surface_id");
        desktop_id_field = cJSON_GetObjectItem(fields, "desktop_id");
        s_last_broadcast_surface_id = (surface_id_field != NULL)
            ? cJSON_GetNumberValue(surface_id_field) : -1.0;
        s_last_broadcast_desktop_id = (desktop_id_field != NULL)
            ? cJSON_GetNumberValue(desktop_id_field) : -1.0;
        cJSON_Delete(fields);
    }
}


static void s_reset(void)
{
    s_call_switch = 0;
    s_last_switch_desktop_id = 0u;
    s_call_switch_north = 0;
    s_call_switch_south = 0;
    s_call_switch_east = 0;
    s_call_switch_west = 0;
    s_call_pan_step = 0;
    s_call_pan_step_direction = COMPASS_NORTH;
    s_call_pan_north = 0;
    s_call_pan_south = 0;
    s_call_pan_east = 0;
    s_call_pan_west = 0;
    s_call_goto = 0;
    s_last_goto_page = 0u;
    s_action_desktop_add_result = 0;
    s_call_action_desktop_add = 0;
    s_action_desktop_remove_result = 0;
    s_call_action_desktop_remove = 0;
    s_action_toggle_strutless_result = 0;
    s_call_action_toggle_strutless = 0;
    s_stub_keysyms = NULL;
    s_call_get_keysyms = 0;
    s_call_get_config = 0;
    s_call_get_surfaces = 0;
    s_call_keyboard_load = 0;
    s_call_broadcast = 0;
    s_last_broadcast_type = 0u;
    s_last_broadcast_surface_id = -1.0;
    s_last_broadcast_desktop_id = -1.0;
    s_last_broadcast_had_fields = false;
}


static surface_td *s_make_surface(uint32_t id, uint32_t desktop_cur)
{
    surface_td *surface = calloc(1, sizeof(*surface));

    surface->id = id;
    surface->desktop_cur = desktop_cur;
    return surface;
}


/* Switching to a specific desktop dispatches to the plain command and
 * always broadcasts the switch event afterward, unconditionally */
static void s_test_desktop_switch_dispatches_and_broadcasts(void)
{
    surface_td *surface = s_make_surface(3u, 7u);

    s_reset();

    enact_surface_desktop_switch(surface, 9u);
    TAP_EQ_INT(s_call_switch, 1,
            "dispatches to scmd_surface_desktop_switch exactly once");
    TAP_EQ_INT((int) s_last_switch_desktop_id, 9,
            "forwards the requested desktop id unchanged");
    TAP_EQ_INT(s_call_broadcast, 1,
            "broadcasts the desktop-switched event exactly once");
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_DESKTOP_SWITCHED,
            "the event type broadcast is IPC_EVENT_DESKTOP_SWITCHED");
    TAP_OK(s_last_broadcast_had_fields,
            "the broadcast carries a fields object, not a null one");
    TAP_EQ_INT((int) s_last_broadcast_surface_id, 3,
            "the fields object carries the surface's own id");
    TAP_EQ_INT((int) s_last_broadcast_desktop_id, 7,
            "the fields object carries the surface's current desktop"
            " (read after the command ran, not the id requested)");

    free(surface);
}


/* Each of the four cyclic directions dispatches to its own command
 * alone, and always broadcasts afterward too */
static void s_test_cyclic_north_dispatches_and_broadcasts(void)
{
    surface_td *surface = s_make_surface(0u, 0u);

    s_reset();

    enact_surface_desktop_switch_north(surface);
    TAP_EQ_INT(s_call_switch_north, 1,
            "north dispatches to scmd_surface_desktop_switch_north"
            " exactly once");
    TAP_EQ_INT(s_call_switch_south + s_call_switch_east +
            s_call_switch_west + s_call_switch, 0,
            "no other switch command runs for a north dispatch");
    TAP_EQ_INT(s_call_broadcast, 1,
            "a north switch still broadcasts the event afterward");

    free(surface);
}


static void s_test_cyclic_south_dispatches_and_broadcasts(void)
{
    surface_td *surface = s_make_surface(0u, 0u);

    s_reset();

    enact_surface_desktop_switch_south(surface);
    TAP_EQ_INT(s_call_switch_south, 1,
            "south dispatches to scmd_surface_desktop_switch_south"
            " exactly once");
    TAP_EQ_INT(s_call_switch_north + s_call_switch_east +
            s_call_switch_west + s_call_switch, 0,
            "no other switch command runs for a south dispatch");
    TAP_EQ_INT(s_call_broadcast, 1,
            "a south switch still broadcasts the event afterward");

    free(surface);
}


static void s_test_cyclic_east_dispatches_and_broadcasts(void)
{
    surface_td *surface = s_make_surface(0u, 0u);

    s_reset();

    enact_surface_desktop_switch_east(surface);
    TAP_EQ_INT(s_call_switch_east, 1,
            "east dispatches to scmd_surface_desktop_switch_east"
            " exactly once");
    TAP_EQ_INT(s_call_switch_north + s_call_switch_south +
            s_call_switch_west + s_call_switch, 0,
            "no other switch command runs for an east dispatch");
    TAP_EQ_INT(s_call_broadcast, 1,
            "an east switch still broadcasts the event afterward");

    free(surface);
}


static void s_test_cyclic_west_dispatches_and_broadcasts(void)
{
    surface_td *surface = s_make_surface(0u, 0u);

    s_reset();

    enact_surface_desktop_switch_west(surface);
    TAP_EQ_INT(s_call_switch_west, 1,
            "west dispatches to scmd_surface_desktop_switch_west"
            " exactly once");
    TAP_EQ_INT(s_call_switch_north + s_call_switch_south +
            s_call_switch_east + s_call_switch, 0,
            "no other switch command runs for a west dispatch");
    TAP_EQ_INT(s_call_broadcast, 1,
            "a west switch still broadcasts the event afterward");

    free(surface);
}


/* Adding a desktop, on success, refreshes the keyboard grabs (when a
 * keysyms table exists) and broadcasts the event */
static void s_test_desktop_add_success_refreshes_and_broadcasts(void)
{
    surface_td *surface = s_make_surface(1u, 0u);
    int fake_keysyms_storage = 0;

    s_reset();
    s_action_desktop_add_result = 0;
    s_stub_keysyms = (xcb_key_symbols_t *) &fake_keysyms_storage;

    enact_surface_desktop_add(surface);
    TAP_EQ_INT(s_call_action_desktop_add, 1,
            "surface_action_desktop_add is called exactly once");
    TAP_EQ_INT(s_call_get_keysyms, 1,
            "the keysyms table is queried once, to decide whether to"
            " refresh grabs");
    TAP_EQ_INT(s_call_keyboard_load, 1,
            "keyboard_load runs once, since a keysyms table exists");
    TAP_EQ_INT(s_call_broadcast, 1,
            "a successful add broadcasts the desktop-switched event");

    free(surface);
}


/* Adding a desktop, on success, but with no keysyms table yet, skips
 * the keyboard refresh entirely without crashing, still broadcasting */
static void s_test_desktop_add_success_no_keysyms_yet(void)
{
    surface_td *surface = s_make_surface(1u, 0u);

    s_reset();
    s_action_desktop_add_result = 0;
    s_stub_keysyms = NULL;

    enact_surface_desktop_add(surface);
    TAP_EQ_INT(s_call_keyboard_load, 0,
            "keyboard_load never runs when there is no keysyms table"
            " yet");
    TAP_EQ_INT(s_call_broadcast, 1,
            "the event is still broadcast even without a keysyms"
            " table");

    free(surface);
}


/* Adding a desktop that fails does neither: no keyboard refresh, no
 * broadcast */
static void s_test_desktop_add_failure_does_neither(void)
{
    surface_td *surface = s_make_surface(1u, 0u);

    s_reset();
    s_action_desktop_add_result = -1;
    s_stub_keysyms = NULL;

    enact_surface_desktop_add(surface);
    TAP_EQ_INT(s_call_get_keysyms, 0,
            "a failed add never even queries the keysyms table");
    TAP_EQ_INT(s_call_broadcast, 0,
            "a failed add never broadcasts the desktop-switched event");

    free(surface);
}


/* Removing a desktop mirrors the add path exactly: success refreshes
 * and broadcasts, failure does neither */
static void s_test_desktop_remove_success_refreshes_and_broadcasts(void)
{
    surface_td *surface = s_make_surface(2u, 0u);
    int fake_keysyms_storage = 0;

    s_reset();
    s_action_desktop_remove_result = 0;
    s_stub_keysyms = (xcb_key_symbols_t *) &fake_keysyms_storage;

    enact_surface_desktop_remove(surface);
    TAP_EQ_INT(s_call_action_desktop_remove, 1,
            "surface_action_desktop_remove is called exactly once");
    TAP_EQ_INT(s_call_keyboard_load, 1,
            "a successful remove refreshes the keyboard grabs");
    TAP_EQ_INT(s_call_broadcast, 1,
            "a successful remove broadcasts the desktop-switched"
            " event");

    free(surface);
}


static void s_test_desktop_remove_failure_does_neither(void)
{
    surface_td *surface = s_make_surface(2u, 0u);

    s_reset();
    s_action_desktop_remove_result = 1;

    enact_surface_desktop_remove(surface);
    TAP_EQ_INT(s_call_keyboard_load, 0,
            "a failed remove never refreshes the keyboard grabs");
    TAP_EQ_INT(s_call_broadcast, 0,
            "a failed remove never broadcasts the event");

    free(surface);
}


/* Toggling strutless-maximize, on success, broadcasts the event but
 * never touches the keyboard grabs at all, unlike add/remove */
static void s_test_toggle_strutless_success_broadcasts_only(void)
{
    surface_td *surface = s_make_surface(4u, 0u);

    s_reset();
    s_action_toggle_strutless_result = 0;

    enact_surface_toggle_strutless_maximize(surface);
    TAP_EQ_INT(s_call_action_toggle_strutless, 1,
            "surface_action_maximize_toggle_strutless is called"
            " exactly once");
    TAP_EQ_INT(s_call_broadcast, 1,
            "a successful toggle broadcasts the desktop-switched"
            " event");
    TAP_EQ_INT(s_call_get_keysyms, 0,
            "toggling strutless-maximize never touches keyboard grabs"
            " at all");

    free(surface);
}


static void s_test_toggle_strutless_failure_does_not_broadcast(void)
{
    surface_td *surface = s_make_surface(4u, 0u);

    s_reset();
    s_action_toggle_strutless_result = 1;

    enact_surface_toggle_strutless_maximize(surface);
    TAP_EQ_INT(s_call_broadcast, 0,
            "a failed toggle never broadcasts the event");

    free(surface);
}


/* Each of the four whole-page wrappers dispatches to the command that
 * moves a whole page, never to the pixel-sized step its similarly
 * named sibling uses, and never broadcasts an event of its own */
static void s_test_viewport_switch_dispatches(void)
{
    surface_td *surface = s_make_surface(0u, 0u);

    s_reset();

    enact_surface_viewport_switch_north(surface);
    TAP_EQ_INT(s_call_pan_north, 1,
            "switch north dispatches to scmd_surface_viewport_pan_"
            "north exactly once");
    TAP_EQ_INT(s_call_pan_step, 0,
            "and never to the pixel-sized step");

    enact_surface_viewport_switch_south(surface);
    TAP_EQ_INT(s_call_pan_south, 1, "switch south dispatches south");

    enact_surface_viewport_switch_east(surface);
    TAP_EQ_INT(s_call_pan_east, 1, "switch east dispatches east");

    enact_surface_viewport_switch_west(surface);
    TAP_EQ_INT(s_call_pan_west, 1, "switch west dispatches west");

    TAP_EQ_INT(s_call_broadcast, 0,
            "a whole-page move never broadcasts an event of its own");

    free(surface);
}


/* Each of the four viewport-pan wrappers dispatches to its own
 * command alone, and never broadcasts an IPC event of its own */
static void s_test_viewport_pan_north_dispatches(void)
{
    surface_td *surface = s_make_surface(0u, 0u);

    s_reset();

    enact_surface_viewport_pan_north(surface);
    TAP_EQ_INT(s_call_pan_step, 1,
            "north dispatches to scmd_surface_viewport_pan_step"
            " exactly once");
    TAP_EQ_INT((int) s_call_pan_step_direction, (int) COMPASS_NORTH,
            "north dispatches with COMPASS_NORTH");
    TAP_EQ_INT(s_call_broadcast, 0,
            "a viewport pan never broadcasts an event of its own");

    free(surface);
}


static void s_test_viewport_pan_south_dispatches(void)
{
    surface_td *surface = s_make_surface(0u, 0u);

    s_reset();

    enact_surface_viewport_pan_south(surface);
    TAP_EQ_INT(s_call_pan_step, 1,
            "south dispatches to scmd_surface_viewport_pan_step"
            " exactly once");
    TAP_EQ_INT((int) s_call_pan_step_direction, (int) COMPASS_SOUTH,
            "south dispatches with COMPASS_SOUTH");
    TAP_EQ_INT(s_call_broadcast, 0,
            "a viewport pan never broadcasts an event of its own");

    free(surface);
}


static void s_test_viewport_pan_east_dispatches(void)
{
    surface_td *surface = s_make_surface(0u, 0u);

    s_reset();

    enact_surface_viewport_pan_east(surface);
    TAP_EQ_INT(s_call_pan_step, 1,
            "east dispatches to scmd_surface_viewport_pan_step"
            " exactly once");
    TAP_EQ_INT((int) s_call_pan_step_direction, (int) COMPASS_EAST,
            "east dispatches with COMPASS_EAST");
    TAP_EQ_INT(s_call_broadcast, 0,
            "a viewport pan never broadcasts an event of its own");

    free(surface);
}


static void s_test_viewport_pan_west_dispatches(void)
{
    surface_td *surface = s_make_surface(0u, 0u);

    s_reset();

    enact_surface_viewport_pan_west(surface);
    TAP_EQ_INT(s_call_pan_step, 1,
            "west dispatches to scmd_surface_viewport_pan_step"
            " exactly once");
    TAP_EQ_INT((int) s_call_pan_step_direction, (int) COMPASS_WEST,
            "west dispatches with COMPASS_WEST");
    TAP_EQ_INT(s_call_broadcast, 0,
            "a viewport pan never broadcasts an event of its own");

    free(surface);
}


static void s_test_viewport_goto_dispatches(void)
{
    surface_td *surface = s_make_surface(0u, 0u);

    s_reset();

    enact_surface_viewport_goto(surface, 3u);
    TAP_EQ_INT(s_call_goto, 1,
            "a page go-to dispatches to scmd_surface_viewport_goto"
            " exactly once");
    TAP_EQ_INT((int) s_last_goto_page, 3,
            "the requested page index is passed through unchanged");
    TAP_EQ_INT(s_call_pan_step, 0,
            "no panning command runs for a page go-to dispatch");
    TAP_EQ_INT(s_call_broadcast, 0,
            "a viewport page go-to never broadcasts an event of its"
            " own");

    free(surface);
}


int main(void)
{
    TAP_PLAN(58);

    s_test_desktop_switch_dispatches_and_broadcasts();
    s_test_cyclic_north_dispatches_and_broadcasts();
    s_test_cyclic_south_dispatches_and_broadcasts();
    s_test_cyclic_east_dispatches_and_broadcasts();
    s_test_cyclic_west_dispatches_and_broadcasts();
    s_test_desktop_add_success_refreshes_and_broadcasts();
    s_test_desktop_add_success_no_keysyms_yet();
    s_test_desktop_add_failure_does_neither();
    s_test_desktop_remove_success_refreshes_and_broadcasts();
    s_test_desktop_remove_failure_does_neither();
    s_test_toggle_strutless_success_broadcasts_only();
    s_test_toggle_strutless_failure_does_not_broadcast();
    s_test_viewport_switch_dispatches();
    s_test_viewport_pan_north_dispatches();
    s_test_viewport_pan_south_dispatches();
    s_test_viewport_pan_east_dispatches();
    s_test_viewport_pan_west_dispatches();
    s_test_viewport_goto_dispatches();

    return TAP_DONE();
}
