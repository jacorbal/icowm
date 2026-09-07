/**
 * @file tests/render/viewport/test_mesh.c
 *
 * @brief Test battery for the viewport mesh (render/viewport/mesh.c)
 *
 * Covers the two pure calculations, deriving the dot color from a
 * desktop background and folding a viewport origin into the tile, plus
 * the visibility decision.  'viewport_mesh_tile_create' and
 * 'viewport_mesh_render' are not exercised: both need a live X server
 * to create a pixmap and paint a root window, and neither holds any
 * logic the two calculations below do not already cover.
 *
 * 'wm_get_surface_by_id' and 'surface_viewport_has_room' are
 * test-controlled, since the visibility decision asks both.  The XCB
 * connection accessor is a link-only stand-in: nothing reached here
 * ever paints.
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

/* Defs includes */
#include <defs/config.h>

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <surface.h>

/* Local includes */
#include <render/viewport/mesh.h>

/* Harness includes */
#include "harness/tap.h"


static config_td s_config;
static desktop_td s_desktop;
static surface_td s_surface;
static xcb_screen_t s_screen;


/** Whether a scenario wants the surface's viewport reported pannable
 * @note Complexity: @e O(1) */
static bool s_viewport_has_room;


/**
 * @brief Test-controlled stand-in for @a surface_viewport_has_room
 *
 * @note Complexity: @e O(1)
 */
bool surface_viewport_has_room(const surface_td *surface)
{
    (void) surface;

    return s_viewport_has_room;
}


/**
 * @brief Test-controlled stand-in for @a wm_get_surface_by_id
 *
 * @note Complexity: @e O(1)
 */
surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;

    return &s_surface;
}


/**
 * @brief Link-only stand-in for @a xcb_connection_get
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/** Ordered log of the X requests the lifecycle scenarios care about,
 *  so that "freed before the root stopped naming it" is a thing a
 *  test can actually see rather than only a thing to read for */
enum s_xreq_e {
    S_XREQ_CREATE_PIXMAP = 0,   /**< A tile was built */
    S_XREQ_SET_BACK_PIXMAP,     /**< The root was pointed at one */
    S_XREQ_FREE_PIXMAP          /**< A tile was released */
};

static enum s_xreq_e s_xlog[32];
static uint32_t s_xlog_pixmap[32];
static uint32_t s_xlog_count;

/** Identifiers 'xcb_generate_id' hands out, one higher each call, so
 *  that each tile in a scenario is told apart from the last */
static uint32_t s_next_id;


/**
 * @brief Record one request, dropping anything past the log's end
 *
 * @param kind   What was asked of the server
 * @param pixmap Pixmap the request names
 *
 * @note Complexity: @e O(1)
 */
static void s_xlog_add(enum s_xreq_e kind, uint32_t pixmap)
{
    if (s_xlog_count >= (uint32_t) (sizeof(s_xlog) /
                sizeof(s_xlog[0]))) {
        return;
    }
    s_xlog[s_xlog_count] = kind;
    s_xlog_pixmap[s_xlog_count] = pixmap;
    s_xlog_count++;
}


/**
 * @brief Whether the log ever frees a pixmap while the root still
 *        names it
 *
 * Walks the log keeping track of which pixmap the root's own
 * @c XCB_CW_BACK_PIXMAP names, and answers @c true the moment a free
 * arrives for that very one.
 *
 * @note Complexity: @e O(n), where @e n is the length of the log
 */
static bool s_xlog_frees_named_pixmap(void)
{
    uint32_t named = 0u;

    for (uint32_t i = 0u; i < s_xlog_count; ++i) {
        if (s_xlog[i] == S_XREQ_SET_BACK_PIXMAP) {
            named = s_xlog_pixmap[i];
        } else if (s_xlog[i] == S_XREQ_FREE_PIXMAP &&
                named != 0u && s_xlog_pixmap[i] == named) {
            return true;
        }
    }

    return false;
}


uint32_t xcb_generate_id(xcb_connection_t *connection)
{
    (void) connection;

    s_next_id++;
    return s_next_id;
}


xcb_void_cookie_t xcb_create_pixmap(xcb_connection_t *connection,
        uint8_t depth, xcb_pixmap_t pid, xcb_drawable_t drawable,
        uint16_t width, uint16_t height)
{
    xcb_void_cookie_t cookie = {0};

    (void) connection;
    (void) depth;
    (void) drawable;
    (void) width;
    (void) height;

    s_xlog_add(S_XREQ_CREATE_PIXMAP, pid);
    return cookie;
}


xcb_void_cookie_t xcb_free_pixmap(xcb_connection_t *connection,
        xcb_pixmap_t pixmap)
{
    xcb_void_cookie_t cookie = {0};

    (void) connection;

    s_xlog_add(S_XREQ_FREE_PIXMAP, pixmap);
    return cookie;
}


xcb_void_cookie_t xcb_change_window_attributes(
        xcb_connection_t *connection, xcb_window_t window,
        uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie = {0};

    (void) connection;
    (void) window;

    if ((value_mask & (uint32_t) XCB_CW_BACK_PIXMAP) != 0u) {
        s_xlog_add(S_XREQ_SET_BACK_PIXMAP,
                ((const uint32_t *) value_list)[0]);
    }
    return cookie;
}


xcb_void_cookie_t xcb_create_gc(xcb_connection_t *connection,
        xcb_gcontext_t cid, xcb_drawable_t drawable,
        uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie = {0};

    (void) connection;
    (void) cid;
    (void) drawable;
    (void) value_mask;
    (void) value_list;

    return cookie;
}


xcb_void_cookie_t xcb_change_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc, uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie = {0};

    (void) connection;
    (void) gc;
    (void) value_mask;
    (void) value_list;

    return cookie;
}


xcb_void_cookie_t xcb_free_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie = {0};

    (void) connection;
    (void) gc;

    return cookie;
}


xcb_void_cookie_t xcb_poly_fill_rectangle(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        uint32_t rectangles_len, const xcb_rectangle_t *rectangles)
{
    xcb_void_cookie_t cookie = {0};

    (void) connection;
    (void) drawable;
    (void) gc;
    (void) rectangles_len;
    (void) rectangles;

    return cookie;
}


xcb_void_cookie_t xcb_clear_area(xcb_connection_t *connection,
        uint8_t exposures, xcb_window_t window, int16_t x, int16_t y,
        uint16_t width, uint16_t height)
{
    xcb_void_cookie_t cookie = {0};

    (void) connection;
    (void) exposures;
    (void) window;
    (void) x;
    (void) y;
    (void) width;
    (void) height;

    return cookie;
}


/**
 * @brief Point a zeroed desktop at a zeroed configuration and screen,
 *        with the mesh enabled and the viewport pannable, so that each
 *        scenario only has to set what it is actually about
 *
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    memset(&s_config, 0, sizeof(s_config));
    memset(&s_desktop, 0, sizeof(s_desktop));
    memset(&s_surface, 0, sizeof(s_surface));
    memset(&s_screen, 0, sizeof(s_screen));

    s_desktop.config = &s_config;
    s_desktop.screen = &s_screen;
    s_desktop.screen_id = 0u;
    s_desktop.background.use_root_pixmap = false;
    s_config.base.viewport.mesh.is_enabled = true;
    s_config.base.viewport.mesh.spacing_horizontal = 64u;
    s_config.base.viewport.mesh.spacing_vertical = 64u;
    s_config.base.viewport.mesh.thickness = 1u;
    s_config.base.viewport.mesh.tone_shift = 20u;
    s_viewport_has_room = true;
    s_xlog_count = 0u;
    s_next_id = 0u;
    viewport_mesh_cache_invalidate();
    viewport_mesh_cache_release_retired((xcb_connection_t *) 1);
    s_xlog_count = 0u;
}


/**
 * @brief A dark background gets a lighter mesh and a light one a
 *        darker mesh, each shifted by the requested percentage
 *
 * @note Complexity: @e O(1)
 */
static void s_test_color_shifts_away_from_background(void)
{
    /* 0x000000 lightened by 20%: 0 + (255 - 0) * 20 / 100 = 51 */
    TAP_EQ_INT((int) viewport_mesh_color_from_background(0x000000u, 20u),
            0x333333,
            "color: pure black lightens to #333333 at 20%");

    /* 0xFFFFFF darkened by 20%: 255 * 80 / 100 = 204 */
    TAP_EQ_INT((int) viewport_mesh_color_from_background(0xFFFFFFu, 20u),
            0xCCCCCC,
            "color: pure white darkens to #cccccc at 20%");

    TAP_EQ_INT((int) viewport_mesh_color_from_background(0x000000u, 100u),
            0xFFFFFF,
            "color: a full shift takes black all the way to white");

    TAP_EQ_INT((int) viewport_mesh_color_from_background(0xFFFFFFu, 100u),
            0x000000,
            "color: a full shift takes white all the way to black");
}


/**
 * @brief The light/dark decision weighs the channels rather than
 *        comparing the packed value, so a bright green is treated as
 *        the light background it is
 *
 * @note Complexity: @e O(1)
 */
static void s_test_color_threshold_uses_luma(void)
{
    uint32_t mesh;

    /* 0x00FF00 is numerically far below 0x808080 but its luma is 149,
     * so it is a light background and its mesh has to darken */
    mesh = viewport_mesh_color_from_background(0x00FF00u, 20u);
    TAP_EQ_INT((int) mesh, 0x00CC00,
            "color: bright green is treated as a light background and"
            " darkens");

    /* 0x0000FF has a luma of 29, genuinely dark, so its mesh
     * lightens */
    mesh = viewport_mesh_color_from_background(0x0000FFu, 20u);
    TAP_EQ_INT((int) mesh, 0x3333FF,
            "color: pure blue is treated as a dark background and"
            " lightens");

    /* A mid grey sits exactly on the midpoint and counts as light */
    mesh = viewport_mesh_color_from_background(0x808080u, 20u);
    TAP_OK(mesh < 0x808080u,
            "color: a background right on the luma midpoint darkens");
}


/**
 * @brief Every viewport origin lands inside the tile, negatives
 *        included
 *
 * @note Complexity: @e O(1)
 */
static void s_test_tile_origin_folds_into_range(void)
{
    TAP_EQ_INT((int) viewport_mesh_tile_origin(0, 64u), 0,
            "origin: a zero origin puts the dot at the tile corner");

    TAP_EQ_INT((int) viewport_mesh_tile_origin(40, 64u), 40,
            "origin: an origin inside one tile is used as it stands");

    TAP_EQ_INT((int) viewport_mesh_tile_origin(64, 64u), 0,
            "origin: an exact multiple of the spacing wraps to zero");

    TAP_EQ_INT((int) viewport_mesh_tile_origin(200, 64u), 8,
            "origin: an origin past several tiles keeps only the"
            " remainder");

    /* -40 % 64 is -40 in C, which would place the dot outside the
     * tile; 24 is the folded equivalent */
    TAP_EQ_INT((int) viewport_mesh_tile_origin(-40, 64u), 24,
            "origin: a negative origin folds forward instead of"
            " staying negative");

    TAP_EQ_INT((int) viewport_mesh_tile_origin(-64, 64u), 0,
            "origin: a negative exact multiple folds to zero");

    TAP_EQ_INT((int) viewport_mesh_tile_origin(40, 0u), 0,
            "origin: a zero spacing reports zero rather than dividing"
            " by it");
}


/**
 * @brief The mesh is drawn only where all three conditions hold, and
 *        each one alone is enough to withhold it
 *
 * @note Complexity: @e O(1)
 */
static void s_test_is_visible_conditions(void)
{
    s_reset();
    TAP_OK(viewport_mesh_is_visible(&s_desktop),
            "visible: enabled, pannable and unowned root draws a"
            " mesh");

    s_reset();
    s_config.base.viewport.mesh.is_enabled = false;
    TAP_OK(!viewport_mesh_is_visible(&s_desktop),
            "visible: a mesh switched off in the configuration is not"
            " drawn");

    s_reset();
    s_viewport_has_room = false;
    TAP_OK(!viewport_mesh_is_visible(&s_desktop),
            "visible: a viewport that cannot pan gets no mesh");

    s_reset();
    s_desktop.background.use_root_pixmap = true;
    TAP_OK(!viewport_mesh_is_visible(&s_desktop),
            "visible: an externally owned root window keeps its"
            " wallpaper");

    s_reset();
    s_desktop.config = NULL;
    TAP_OK(!viewport_mesh_is_visible(&s_desktop),
            "visible: a desktop with no configuration gets no mesh");

    TAP_OK(!viewport_mesh_is_visible(NULL),
            "visible: a null desktop gets no mesh");
}


/* Building a replacement tile never frees the one the root is still
 * naming: the root goes on naming it until 'viewport_mesh_render'
 * installs the new one, so a free before that would leave the
 * attribute pointing at a pixmap the server no longer has */
static void s_test_render_frees_only_after_installing(void)
{
    xcb_connection_t *const connection = (xcb_connection_t *) 1;

    s_reset();
    s_desktop.background.bg.color = 0x202020u;

    TAP_EQ_INT(viewport_mesh_render(connection, &s_desktop), 0,
            "the first render builds and installs a tile");

    /* A pan: same everything but the origin, so the tile is rebuilt */
    s_desktop.viewport_origin.x = 32;
    TAP_EQ_INT(viewport_mesh_render(connection, &s_desktop), 0,
            "and a pan rebuilds it");

    TAP_OK(!s_xlog_frees_named_pixmap(),
            "no tile is ever freed while the root still names it");
    TAP_EQ_INT((int) s_xlog[s_xlog_count - 1u],
            (int) S_XREQ_FREE_PIXMAP,
            "the old tile is released last of all, once the new one"
            " is installed");
}


/* Invalidating the cache alone frees nothing, since the root is still
 * naming whatever was cached; the release that follows a change of
 * attribute is what frees it */
static void s_test_invalidate_defers_the_free(void)
{
    xcb_connection_t *const connection = (xcb_connection_t *) 1;
    uint32_t frees = 0u;

    s_reset();
    s_desktop.background.bg.color = 0x202020u;
    (void) viewport_mesh_render(connection, &s_desktop);

    s_xlog_count = 0u;
    viewport_mesh_cache_invalidate();
    for (uint32_t i = 0u; i < s_xlog_count; ++i) {
        if (s_xlog[i] == S_XREQ_FREE_PIXMAP) {
            frees++;
        }
    }
    TAP_EQ_INT((int) frees, 0,
            "invalidating on its own frees nothing");

    viewport_mesh_cache_release_retired(connection);
    frees = 0u;
    for (uint32_t i = 0u; i < s_xlog_count; ++i) {
        if (s_xlog[i] == S_XREQ_FREE_PIXMAP) {
            frees++;
        }
    }
    TAP_EQ_INT((int) frees, 1,
            "and the release that follows frees the one tile there"
            " was");
}


int main(void)
{
    TAP_PLAN(26);

    s_test_color_shifts_away_from_background();
    s_test_color_threshold_uses_luma();
    s_test_tile_origin_folds_into_range();
    s_test_is_visible_conditions();

    s_test_render_frees_only_after_installing();
    s_test_invalidate_defers_the_free();

    return TAP_DONE();
}
