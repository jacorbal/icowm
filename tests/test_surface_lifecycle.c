/**
 * @file tests/test_surface_lifecycle.c
 *
 * @brief Test battery for surface lifecycle: init, resize, destroy
 *        (surface.c)
 *
 * 'surface_init' is exercised against a small, test-controlled list
 * of fake 'xcb_screen_t' entries this file fills directly:
 * 'xcb_get_setup', 'xcb_setup_roots_iterator', and 'xcb_screen_next'
 * are test-controlled stand-ins walking that list, so the real
 * screen-lookup loop (its early exit once the iterator runs out, in
 * particular) runs exactly as it would against a live X server,
 * without needing one.  'desktop_init' and 'surface_desktop_add' are
 * test-controlled stand-ins too, each answering success or failure
 * however a given test asks, so every one of 'surface_init''s own
 * cleanup paths (a failed desktop, a failed insert, both needing to
 * unwind everything allocated before them) is reachable directly.
 * 'surface_monitor_refresh_all', 'xcb_connection_get', 'xcb_screen_
 * allowed_depths_iterator', 'xcb_depth_visuals_iterator', and
 * 'logger_msg' are link-only stand-ins: none of their own branches
 * are what this file means to exercise, 'surface_init''s own control
 * flow already being fully observable through 'surface_td' itself.
 * The real 'cdlist' ('adt/cdlist.c') backs 'surface->desktops',
 * so 'surface_destroy''s teardown runs through the exact same real
 * list every other caller does.
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
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Local includes */
#include <config.h>
#include <desktop.h>
#include <harness/tap.h>
#include <logger.h>
#include <surface.h>


/** Fake screen list @a xcb_setup_roots_iterator walks, filled by
 *  @a s_set_screen_count */
#define MAX_TEST_SCREENS (4)
static xcb_screen_t s_screens[MAX_TEST_SCREENS];
static int s_screen_count;


static void s_set_screen_count(int count)
{
    int i;

    s_screen_count = count;
    for (i = 0; i < count; i++) {
        memset(&s_screens[i], 0, sizeof(s_screens[i]));
    }
}


/**
 * @brief Link-only stand-in for @a xcb_connection_get
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/**
 * @brief Test-controlled stand-in for @a xcb_get_setup
 *
 * Its return value is only ever handed straight to
 * @a xcb_setup_roots_iterator below, itself test-controlled, so what
 * this answers with is never actually dereferenced
 *
 * @note Complexity: @e O(1)
 */
const xcb_setup_t *xcb_get_setup(xcb_connection_t *connection)
{
    (void) connection;
    return (const xcb_setup_t *) 1;
}


/**
 * @brief Test-controlled stand-in for @a xcb_setup_roots_iterator
 *
 * Answers an iterator over @a s_screens, however many
 * @a s_set_screen_count last registered
 *
 * @note Complexity: @e O(1)
 */
xcb_screen_iterator_t xcb_setup_roots_iterator(const xcb_setup_t *setup)
{
    xcb_screen_iterator_t iter;

    (void) setup;
    memset(&iter, 0, sizeof(iter));
    if (s_screen_count > 0) {
        iter.data = &s_screens[0];
        iter.rem = s_screen_count;
        iter.index = 0;
    }

    return iter;
}


/**
 * @brief Test-controlled stand-in for @a xcb_screen_next
 *
 * Advances @p iter one step through @a s_screens, mirroring the
 * real function's own contract of decrementing @c rem and never
 * stepping past the end of what @c rem still promises
 *
 * @note Complexity: @e O(1)
 */
void xcb_screen_next(xcb_screen_iterator_t *iter)
{
    if (iter->rem <= 0) {
        return;
    }
    iter->index++;
    iter->rem--;
    iter->data = (iter->rem > 0) ? &s_screens[iter->index] : NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_screen_allowed_depths_iterator
 *
 * Reached only by 's_properties_update', a static helper whose own
 * DPI/visual branches are not what this file means to exercise
 * directly; an always-empty iterator keeps that helper's visual
 * lookup branch simply skipped
 *
 * @note Complexity: @e O(1)
 */
xcb_depth_iterator_t xcb_screen_allowed_depths_iterator(
        const xcb_screen_t *screen)
{
    xcb_depth_iterator_t iter;

    (void) screen;
    memset(&iter, 0, sizeof(iter));

    return iter;
}


/**
 * @brief Link-only stand-in for @a xcb_depth_visuals_iterator
 *
 * Reached only once 'xcb_screen_allowed_depths_iterator' above
 * already answers a non-empty iterator, which it never does here
 *
 * @note Complexity: @e O(1)
 */
xcb_visualtype_iterator_t xcb_depth_visuals_iterator(
        const xcb_depth_t *depth)
{
    xcb_visualtype_iterator_t iter;

    (void) depth;
    memset(&iter, 0, sizeof(iter));

    return iter;
}


/**
 * @brief Link-only stand-in for @a logger_msg
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;

    return 0;
}


/**
 * @brief Link-only stand-in for @a surface_monitor_refresh_all
 * @note Complexity: @e O(1)
 */
void surface_monitor_refresh_all(surface_td *surface)
{
    (void) surface;
}


/** What @a desktop_init answers with next, and how many times it was
 *  actually called, set up by @a s_set_desktop_init_results */
#define MAX_DESKTOP_RESULTS (8)
static desktop_td *s_desktop_init_results[MAX_DESKTOP_RESULTS];
static int s_desktop_init_results_used;
static int s_desktop_init_calls;

/** Every desktop this file calloc's, freed in one place by
 *  @a s_teardown */
static desktop_td *s_owned_desktops[MAX_DESKTOP_RESULTS];
static int s_owned_desktops_used;


static desktop_td *s_make_desktop(uint32_t id)
{
    desktop_td *desktop = calloc(1, sizeof(*desktop));

    desktop->id = id;
    s_owned_desktops[s_owned_desktops_used] = desktop;
    s_owned_desktops_used++;

    return desktop;
}


/**
 * @brief Test-controlled stand-in for @a desktop_init
 *
 * Answers, in order, whatever @a s_desktop_init_results was filled
 * with, one entry consumed per call; answers @c NULL once that
 * table is exhausted
 *
 * @note Complexity: @e O(1)
 */
desktop_td *desktop_init(xcb_connection_t *connection, uint32_t screen_id,
        uint32_t desktop_id, config_td *config)
{
    desktop_td *result;

    (void) connection;
    (void) screen_id;
    (void) desktop_id;
    (void) config;

    if (s_desktop_init_calls >= s_desktop_init_results_used) {
        s_desktop_init_calls++;
        return NULL;
    }

    result = s_desktop_init_results[s_desktop_init_calls];
    s_desktop_init_calls++;

    return result;
}


/**
 * @brief Link-only stand-in for @a desktop_destroy
 *
 * 'surface_init''s own cleanup calls this directly on a desktop it
 * just got back from 'desktop_init' once 'surface_desktop_add' below
 * then refuses it; the fixture itself is freed once, later, by
 * @a s_teardown, so this must not free it a second time
 *
 * @note Complexity: @e O(1)
 */
void desktop_destroy(desktop_td *desktop)
{
    (void) desktop;
}


/** Whether @a surface_desktop_add should accept or refuse, and after
 *  how many prior successful calls, set by @a s_set_add_fails_after */
static int s_add_fails_after = 1000;
static int s_add_calls;


/**
 * @brief Test-controlled stand-in for @a surface_desktop_add
 *
 * Accepts every call until @a s_add_calls reaches
 * @a s_add_fails_after, then refuses every one after that
 *
 * @note Complexity: @e O(1)
 */
int surface_desktop_add(surface_td *surface, desktop_td *desktop)
{
    (void) surface;
    (void) desktop;

    if (s_add_calls >= s_add_fails_after) {
        s_add_calls++;
        return -1;
    }
    s_add_calls++;

    return 0;
}


static void s_reset(void)
{
    s_screen_count = 0;
    s_desktop_init_results_used = 0;
    s_desktop_init_calls = 0;
    s_add_fails_after = 1000;
    s_add_calls = 0;
}


static void s_teardown(void)
{
    int i;

    for (i = 0; i < s_owned_desktops_used; i++) {
        free(s_owned_desktops[i]);
    }
    s_owned_desktops_used = 0;
}


/* A surface_id past every registered screen fails to resolve at all,
 * the loop's iterator running out before reaching it */
static void s_test_init_unknown_screen_fails(void)
{
    surface_td *surface;

    s_reset();
    s_set_screen_count(1);

    surface = surface_init(NULL, 5u, 0u, (config_td *) 1);

    TAP_OK(surface == NULL,
            "a surface_id past every registered screen fails");
}


/* A screen that exists but whose first desktop_init call fails
 * unwinds cleanly and returns null */
static void s_test_init_first_desktop_fails(void)
{
    surface_td *surface;

    s_reset();
    s_set_screen_count(1);

    surface = surface_init(NULL, 0u, 3u, (config_td *) 1);

    TAP_OK(surface == NULL,
            "a first desktop_init failure unwinds and returns null");
}


/* A screen whose second desktop's surface_desktop_add call fails
 * still unwinds cleanly, cleaning up both the desktop just built and
 * everything allocated before it */
static void s_test_init_desktop_add_fails(void)
{
    surface_td *surface;

    s_reset();
    s_set_screen_count(1);
    s_desktop_init_results[0] = s_make_desktop(0u);
    s_desktop_init_results[1] = s_make_desktop(1u);
    s_desktop_init_results_used = 2;
    s_add_fails_after = 1;

    surface = surface_init(NULL, 0u, 2u, (config_td *) 1);

    TAP_OK(surface == NULL,
            "a surface_desktop_add failure on the second desktop"
            " unwinds and returns null");
}


/* A fully successful init returns a well-formed surface with every
 * fresh-surface field at its documented default */
static void s_test_init_success_fields(void)
{
    surface_td *surface;
    config_td *const config = (config_td *) 0x1234;

    s_reset();
    s_set_screen_count(2);
    s_screens[1].width_in_pixels = 1920u;
    s_screens[1].height_in_pixels = 1080u;
    s_desktop_init_results[0] = s_make_desktop(0u);
    s_desktop_init_results[1] = s_make_desktop(1u);
    s_desktop_init_results_used = 2;

    surface = surface_init(NULL, 1u, 2u, config);

    TAP_OK(surface != NULL, "a fully successful init returns non-null");
    TAP_EQ_INT((int) surface->id, 1, "the surface's own id is set");
    TAP_OK(surface->config == config, "the config pointer is stored"
            " as given");
    TAP_OK(!surface->is_showing_desktop,
            "is_showing_desktop starts false");
    TAP_OK(!surface->strutless_maximize,
            "strutless_maximize starts false");
    TAP_OK(!surface->randr.is_known, "randr.is_known starts false");
    TAP_OK(surface->is_outdated, "is_outdated starts true");
    TAP_EQ_INT((int) surface->properties.dim.w, 1920,
            "surface width comes from the matched fake screen");
    TAP_EQ_INT((int) surface->properties.dim.h, 1080,
            "surface height comes from the matched fake screen");

    surface_destroy(surface);
}


/* A screen with zero physical size (both mm dimensions zero) leaves
 * DPI at zero on both axes instead of dividing by zero */
static void s_test_init_zero_size_screen_dpi_zero(void)
{
    surface_td *surface;

    s_reset();
    s_set_screen_count(1);
    s_screens[0].width_in_pixels = 800u;
    s_screens[0].height_in_pixels = 600u;
    s_screens[0].width_in_millimeters = 0u;
    s_screens[0].height_in_millimeters = 0u;
    s_desktop_init_results[0] = s_make_desktop(0u);
    s_desktop_init_results_used = 1;

    surface = surface_init(NULL, 0u, 1u, (config_td *) 1);

    TAP_OK(surface != NULL, "init still succeeds with a zero-size"
            " physical screen");
    TAP_EQ_INT((int) surface->properties.dpi.x, 0,
            "x DPI is zero rather than a division by zero");
    TAP_EQ_INT((int) surface->properties.dpi.y, 0,
            "y DPI is zero rather than a division by zero");

    surface_destroy(surface);
}


/* Zero desktops is a degenerate but valid case: the loop simply
 * never runs, and init still succeeds */
static void s_test_init_zero_desktops_succeeds(void)
{
    surface_td *surface;

    s_reset();
    s_set_screen_count(1);

    surface = surface_init(NULL, 0u, 0u, (config_td *) 1);

    TAP_OK(surface != NULL, "zero desktops still succeeds");
    TAP_EQ_INT((int) surface->desktop_count, 0,
            "desktop_count stays zero");

    surface_destroy(surface);
}


/* Destroying a null surface is a silent no-op */
static void s_test_destroy_null_is_noop(void)
{
    surface_destroy(NULL);

    TAP_OK(true, "destroying a null surface never crashes");
}


/* Resizing a surface updates its own dimensions directly, nothing
 * else */
static void s_test_resize_updates_dimensions(void)
{
    surface_td surface;

    memset(&surface, 0, sizeof(surface));
    surface.properties.dim.w = 640u;
    surface.properties.dim.h = 480u;

    surface_resize(&surface, 1024u, 768u);

    TAP_EQ_INT((int) surface.properties.dim.w, 1024,
            "width is updated to the new value");
    TAP_EQ_INT((int) surface.properties.dim.h, 768,
            "height is updated to the new value");
}


int main(void)
{
    TAP_PLAN(20);

    s_test_init_unknown_screen_fails();
    s_test_init_first_desktop_fails();
    s_test_init_desktop_add_fails();
    s_test_init_success_fields();
    s_test_init_zero_size_screen_dpi_zero();
    s_test_init_zero_desktops_succeeds();
    s_test_destroy_null_is_noop();
    s_test_resize_updates_dimensions();

    s_teardown();

    return TAP_DONE();
}
