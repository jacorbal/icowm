/**
 * @file tests/surface/test_viewport.c
 *
 * @brief Test battery for the configured pannable-viewport size
 *        queries (surface/viewport.c)
 *
 * Both functions under test are pure readers of
 * 'surface->config->base.screens[surface->id].viewport', so every
 * scenario here builds the surface and configuration by hand and
 * calls them directly; nothing in surface/viewport.c reaches XCB, the
 * desktop list, or any other module, so this file needs no stand-ins
 * at all.
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

/* Defs includes */
#include <defs/config.h>

/* Project includes */
#include <config.h>
#include <surface.h>
#include <surface/viewport.h>

/* Harness includes */
#include "harness/tap.h"


static config_td s_config;
static surface_td s_surface;


/**
 * @brief Point a zeroed surface at a zeroed configuration, ahead of
 *        one scenario setting the fields it cares about
 *
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    memset(&s_config, 0, sizeof(s_config));
    memset(&s_surface, 0, sizeof(s_surface));
    s_surface.config = &s_config;
    s_surface.id = 0u;
}


/**
 * @brief A configured size is reported back verbatim on both axes
 *
 * @note Complexity: @e O(1)
 */
static void s_test_dims_reports_configured_size(void)
{
    uint32_t columns;
    uint32_t rows;

    s_reset();
    s_config.base.screens[0].viewport.columns = 3u;
    s_config.base.screens[0].viewport.rows = 2u;

    surface_viewport_dims(&s_surface, &columns, &rows);
    TAP_EQ_INT((int) columns, 3,
            "viewport_dims: reports the configured column count");
    TAP_EQ_INT((int) rows, 2,
            "viewport_dims: reports the configured row count");
}


/**
 * @brief Every unusable surface reports a 1x1 viewport rather than
 *        reading anything: a null surface, one with no configuration,
 *        and one whose id sits past the screen array
 *
 * @note Complexity: @e O(1)
 */
static void s_test_dims_falls_back(void)
{
    uint32_t columns;
    uint32_t rows;

    surface_viewport_dims(NULL, &columns, &rows);
    TAP_OK(columns == 1u && rows == 1u,
            "viewport_dims: a null surface falls back to 1x1");

    s_reset();
    s_config.base.screens[0].viewport.columns = 4u;
    s_config.base.screens[0].viewport.rows = 4u;
    s_surface.config = NULL;
    surface_viewport_dims(&s_surface, &columns, &rows);
    TAP_OK(columns == 1u && rows == 1u,
            "viewport_dims: a surface with no config falls back to"
            " 1x1");

    s_reset();
    s_config.base.screens[0].viewport.columns = 4u;
    s_config.base.screens[0].viewport.rows = 4u;
    s_surface.id = (uint32_t) CONFIG_MAX_SCREENS;
    surface_viewport_dims(&s_surface, &columns, &rows);
    TAP_OK(columns == 1u && rows == 1u,
            "viewport_dims: an id past CONFIG_MAX_SCREENS falls back"
            " to 1x1 rather than reading past the array");
}


/**
 * @brief Room is reported for a viewport wider or taller than one
 *        screen, on either axis alone, and denied for a plain 1x1 one
 *
 * @note Complexity: @e O(1)
 */
static void s_test_has_room(void)
{
    s_reset();
    s_config.base.screens[0].viewport.columns = 1u;
    s_config.base.screens[0].viewport.rows = 1u;
    TAP_OK(!surface_viewport_has_room(&s_surface),
            "has_room: a plain 1x1 viewport has no room to pan");

    s_reset();
    s_config.base.screens[0].viewport.columns = 2u;
    s_config.base.screens[0].viewport.rows = 1u;
    TAP_OK(surface_viewport_has_room(&s_surface),
            "has_room: a viewport wider than one screen has room");

    s_reset();
    s_config.base.screens[0].viewport.columns = 1u;
    s_config.base.screens[0].viewport.rows = 2u;
    TAP_OK(surface_viewport_has_room(&s_surface),
            "has_room: a viewport taller than one screen has room");

    TAP_OK(!surface_viewport_has_room(NULL),
            "has_room: a null surface has no room, matching the 1x1"
            " fallback its dims report");
}


/**
 * @brief An unconfigured screen entry, left zeroed rather than loaded,
 *        reports no room: the loader never writes a 0 there, and a
 *        reader must not treat one as pannable
 *
 * @note Complexity: @e O(1)
 */
static void s_test_has_room_on_zeroed_config(void)
{
    s_reset();
    TAP_OK(!surface_viewport_has_room(&s_surface),
            "has_room: a zeroed viewport entry reports no room");
}


int main(void)
{
    TAP_PLAN(10);

    s_test_dims_reports_configured_size();
    s_test_dims_falls_back();
    s_test_has_room();
    s_test_has_room_on_zeroed_config();

    return TAP_DONE();
}
