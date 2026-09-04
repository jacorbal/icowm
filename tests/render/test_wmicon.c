/**
 * @file tests/render/test_wmicon.c
 *
 * @brief Test battery for client-supplied icon rendering geometry and
 *        cache logic
 *
 * Every XCB entry point @c render/wmicon.c calls, raw @c xcb_* and
 * @c xcb_render_*, the @c xcb-render-util convenience calls
 * (@c xcb_render_util_query_formats and friends), the @c xcb-icccm
 * @c WM_HINTS getter, and the @c xcb-ewmh @c _NET_WM_ICON getter, are
 * stubbed below as controllable, call-recording stand-ins, matching
 * the pattern in @c tests/systray/test_layout.c: none of libxcb,
 * libxcb-render, libxcb-render-util, libxcb-icccm, or libxcb-ewmh is
 * linked in, since every one of those calls is a genuine X server
 * round trip (or wraps one) this test drives without a real X server
 * or a real window manager around it.
 *
 * This lets every real branch in @c wmicon_draw_at exercise for real:
 * the cache-hit fast paths, the EWMH icon best-fit scoring loop
 * (picking whichever published icon size minimizes @c |dw|+|dh|
 * against the target draw size), the ICCCM @c WM_HINTS fallback (both
 * its depth-1 stencil and full-depth color-image forms), the
 * default-icon fallback when neither property is usable, and the
 * aspect-ratio-preserving scale arithmetic in @c s_icon_scale_apply
 * and the margin arithmetic in @c s_draw_default_icon, both reached
 * only indirectly through the public entry points since both are
 * @c static.
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

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/render.h>
#include <xcb/xcb_renderutil.h>

/* Local includes */
#include <harness/tap.h>
#include <render/wmicon.h>


/* Controllable stand-in state */

static uint32_t s_next_id = 1000u;

static int s_create_pixmap_calls = 0;
static int s_put_image_calls = 0;
static uint32_t s_put_image_width = 0u;
static uint32_t s_put_image_height = 0u;
static int s_free_pixmap_calls = 0;
static int s_create_gc_calls = 0;
static int s_free_gc_calls = 0;
static int s_change_gc_calls = 0;
static uint32_t s_last_change_gc_color = 0u;
static int s_poly_fill_rectangle_calls = 0;
static xcb_rectangle_t s_last_fill_rects[4];
static int s_render_create_picture_calls = 0;
static int s_render_free_picture_calls = 0;
static int s_render_set_transform_calls = 0;
static xcb_render_transform_t s_last_transform;
static int s_render_set_filter_calls = 0;
static int s_render_composite_calls = 0;
static int16_t s_last_composite_dst_x = 0;
static int16_t s_last_composite_dst_y = 0;
static uint16_t s_last_composite_w = 0u;
static uint16_t s_last_composite_h = 0u;
static xcb_render_picture_t s_last_composite_mask = XCB_NONE;
static int s_render_create_solid_fill_calls = 0;
static int s_render_set_clip_calls = 0;

/* xcb-render-util stand-in state */
static xcb_render_query_pict_formats_reply_t s_formats_reply;
static xcb_render_pictforminfo_t s_argb32_info = { 1u, 0u, 32u,
    { 0u, 0u }, { 0, 0, 0, 0, 0, 0, 0, 0 }, 0u };
static xcb_render_pictforminfo_t s_a1_info = { 2u, 0u, 1u,
    { 0u, 0u }, { 0, 0, 0, 0, 0, 0, 0, 0 }, 0u };
static xcb_render_pictvisual_t s_root_visual_info = { 500u, 3u };
static bool s_formats_query_fails = false;
static bool s_find_visual_format_fails = false;

/* xcb-icccm WM_HINTS stand-in state */
static bool s_wm_hints_available = false;
static xcb_icccm_wm_hints_t s_wm_hints_stub;
static xcb_get_geometry_reply_t s_pixmap_geometry_stub;
static bool s_get_geometry_fails = false;

/* xcb-ewmh _NET_WM_ICON stand-in state */
static bool s_ewmh_icon_available = false;
/** Up to 4 fixture icon entries; the stand-in iterator walks these */
static uint32_t s_icon_widths[4];
static uint32_t s_icon_heights[4];
static uint32_t *s_icon_data[4];
static unsigned int s_icon_count = 0u;


/* Raw XCB stand-ins (link-only, matching tests/systray/test_layout.c;
 * libxcb itself is never linked in this test binary) */

uint32_t xcb_generate_id(xcb_connection_t *connection)
{
    (void) connection;
    return s_next_id++;
}

xcb_void_cookie_t xcb_create_pixmap(xcb_connection_t *connection,
        uint8_t depth, xcb_pixmap_t pid, xcb_drawable_t drawable,
        uint16_t width, uint16_t height)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) depth;
    (void) pid;
    (void) drawable;
    (void) width;
    (void) height;
    s_create_pixmap_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_free_pixmap(xcb_connection_t *connection,
        xcb_pixmap_t pixmap)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) pixmap;
    s_free_pixmap_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_create_gc(xcb_connection_t *connection,
        xcb_gcontext_t cid, xcb_drawable_t drawable, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) cid;
    (void) drawable;
    (void) value_mask;
    (void) value_list;
    s_create_gc_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_free_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) gc;
    s_free_gc_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_change_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc, uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) gc;
    (void) value_mask;
    s_change_gc_calls++;
    s_last_change_gc_color = *(const uint32_t *) value_list;
    return cookie;
}

xcb_void_cookie_t xcb_poly_fill_rectangle(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        uint32_t rectangles_len, const xcb_rectangle_t *rectangles)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) drawable;
    (void) gc;
    if (s_poly_fill_rectangle_calls <
            (int) (sizeof(s_last_fill_rects) /
                sizeof(s_last_fill_rects[0]))) {
        s_last_fill_rects[s_poly_fill_rectangle_calls] = rectangles[0];
    }
    s_poly_fill_rectangle_calls++;
    (void) rectangles_len;
    return cookie;
}

xcb_void_cookie_t xcb_put_image(xcb_connection_t *connection,
        uint8_t format, xcb_drawable_t drawable, xcb_gcontext_t gc,
        uint16_t width, uint16_t height, int16_t dst_x, int16_t dst_y,
        uint8_t left_pad, uint8_t depth, uint32_t data_len,
        const uint8_t *data)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) format;
    (void) drawable;
    (void) gc;
    (void) dst_x;
    (void) dst_y;
    (void) left_pad;
    (void) depth;
    (void) data_len;
    (void) data;
    s_put_image_calls++;
    s_put_image_width = width;
    s_put_image_height = height;
    return cookie;
}

const struct xcb_setup_t *xcb_get_setup(xcb_connection_t *connection)
{
    (void) connection;
    return (const struct xcb_setup_t *) 1;
}

/** A single fixture root screen, its own root/root_visual, returned
 *  through every s_get_formats()-adjacent lookup that needs one */
static xcb_screen_t s_fake_screen;

xcb_screen_iterator_t xcb_setup_roots_iterator(const xcb_setup_t *setup)
{
    xcb_screen_iterator_t iter;

    (void) setup;
    memset(&iter, 0, sizeof(iter));
    iter.data = &s_fake_screen;
    iter.rem = 1;
    iter.index = 0;
    return iter;
}

xcb_get_geometry_cookie_t xcb_get_geometry(xcb_connection_t *connection,
        xcb_drawable_t drawable)
{
    xcb_get_geometry_cookie_t cookie = { 0u };

    (void) connection;
    (void) drawable;
    return cookie;
}

xcb_get_geometry_reply_t *xcb_get_geometry_reply(
        xcb_connection_t *connection, xcb_get_geometry_cookie_t cookie,
        xcb_generic_error_t **e)
{
    xcb_get_geometry_reply_t *reply;

    (void) connection;
    (void) cookie;
    if (e != NULL) {
        *e = NULL;
    }
    if (s_get_geometry_fails) {
        return NULL;
    }
    reply = malloc(sizeof(*reply));
    *reply = s_pixmap_geometry_stub;
    return reply;
}


/* 'xcb/render.h' stand-ins */

xcb_void_cookie_t xcb_render_create_picture(xcb_connection_t *connection,
        xcb_render_picture_t pid, xcb_drawable_t drawable,
        xcb_render_pictformat_t format, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) pid;
    (void) drawable;
    (void) format;
    (void) value_mask;
    (void) value_list;
    s_render_create_picture_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_render_free_picture(xcb_connection_t *connection,
        xcb_render_picture_t picture)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) picture;
    s_render_free_picture_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_render_set_picture_transform(
        xcb_connection_t *connection, xcb_render_picture_t picture,
        xcb_render_transform_t transform)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) picture;
    s_render_set_transform_calls++;
    s_last_transform = transform;
    return cookie;
}

xcb_void_cookie_t xcb_render_set_picture_filter(
        xcb_connection_t *connection, xcb_render_picture_t picture,
        uint16_t filter_len, const char *filter, uint32_t values_len,
        const xcb_render_fixed_t *values)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) picture;
    (void) filter_len;
    (void) filter;
    (void) values_len;
    (void) values;
    s_render_set_filter_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_render_set_picture_clip_rectangles(
        xcb_connection_t *connection, xcb_render_picture_t picture,
        int16_t clip_x_origin, int16_t clip_y_origin,
        uint32_t rectangles_len, const xcb_rectangle_t *rectangles)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) picture;
    (void) clip_x_origin;
    (void) clip_y_origin;
    (void) rectangles_len;
    (void) rectangles;
    s_render_set_clip_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_render_composite(xcb_connection_t *connection,
        uint8_t op, xcb_render_picture_t src, xcb_render_picture_t mask,
        xcb_render_picture_t dst, int16_t src_x, int16_t src_y,
        int16_t mask_x, int16_t mask_y, int16_t dst_x, int16_t dst_y,
        uint16_t width, uint16_t height)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) op;
    (void) src;
    (void) dst;
    (void) src_x;
    (void) src_y;
    (void) mask_x;
    (void) mask_y;
    s_render_composite_calls++;
    s_last_composite_mask = mask;
    s_last_composite_dst_x = dst_x;
    s_last_composite_dst_y = dst_y;
    s_last_composite_w = width;
    s_last_composite_h = height;
    return cookie;
}

xcb_void_cookie_t xcb_render_create_solid_fill(xcb_connection_t *connection,
        xcb_render_picture_t picture, xcb_render_color_t color)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) picture;
    (void) color;
    s_render_create_solid_fill_calls++;
    return cookie;
}


/* 'xcb/xcb_renderutil.h' stand-ins */

const xcb_render_query_pict_formats_reply_t *xcb_render_util_query_formats(
        xcb_connection_t *connection)
{
    xcb_render_query_pict_formats_reply_t *heap_copy;

    (void) connection;
    if (s_formats_query_fails) {
        return NULL;
    }
    /* render/wmicon.c's own s_get_formats() caches this pointer and
     * eventually frees it with a plain free(), exactly like the real
     * xcb_render_util_query_formats would return a malloc()-ed reply,
     * so this stand-in must hand back real heap memory too */
    heap_copy = malloc(sizeof(*heap_copy));
    *heap_copy = s_formats_reply;
    return heap_copy;
}

xcb_render_pictforminfo_t *xcb_render_util_find_standard_format(
        const xcb_render_query_pict_formats_reply_t *formats,
        xcb_pict_standard_t format)
{
    (void) formats;
    if (format == XCB_PICT_STANDARD_ARGB_32) {
        return &s_argb32_info;
    }
    if (format == XCB_PICT_STANDARD_A_1) {
        return &s_a1_info;
    }
    return NULL;
}

xcb_render_pictvisual_t *xcb_render_util_find_visual_format(
        const xcb_render_query_pict_formats_reply_t *formats,
        xcb_visualid_t visual)
{
    (void) formats;
    (void) visual;
    return s_find_visual_format_fails ? NULL : &s_root_visual_info;
}


/* 'xcb/xcb_icccm.h' stand-in */

xcb_get_property_cookie_t xcb_icccm_get_wm_hints(xcb_connection_t *connection,
        xcb_window_t window)
{
    xcb_get_property_cookie_t cookie = { 0u };

    (void) connection;
    (void) window;
    return cookie;
}

uint8_t xcb_icccm_get_wm_hints_reply(xcb_connection_t *connection,
        xcb_get_property_cookie_t cookie, xcb_icccm_wm_hints_t *hints,
        xcb_generic_error_t **e)
{
    (void) connection;
    (void) cookie;
    if (e != NULL) {
        *e = NULL;
    }
    if (!s_wm_hints_available) {
        return 0u;
    }
    *hints = s_wm_hints_stub;
    return 1u;
}


/* 'xcb/xcb_ewmh.h' stand-ins */

xcb_get_property_cookie_t xcb_ewmh_get_wm_icon(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window)
{
    xcb_get_property_cookie_t cookie = { 0u };

    (void) ewmh;
    (void) window;
    return cookie;
}

uint8_t xcb_ewmh_get_wm_icon_reply(xcb_ewmh_connection_t *ewmh,
        xcb_get_property_cookie_t cookie,
        xcb_ewmh_get_wm_icon_reply_t *wm_icon, xcb_generic_error_t **e)
{
    (void) ewmh;
    (void) cookie;
    if (e != NULL) {
        *e = NULL;
    }
    if (!s_ewmh_icon_available) {
        return 0u;
    }
    /* '_reply' is never dereferenced by this test's own iterator
     * stand-ins below, only carried between these calls */
    wm_icon->num_icons = s_icon_count;
    wm_icon->_reply = NULL;
    return 1u;
}

xcb_ewmh_wm_icon_iterator_t xcb_ewmh_get_wm_icon_iterator(
        const xcb_ewmh_get_wm_icon_reply_t *wm_icon)
{
    xcb_ewmh_wm_icon_iterator_t iter;

    memset(&iter, 0, sizeof(iter));
    iter.index = 0u;
    iter.rem = wm_icon->num_icons;
    if (iter.rem > 0u) {
        iter.width = s_icon_widths[0];
        iter.height = s_icon_heights[0];
        iter.data = s_icon_data[0];
    }
    return iter;
}

void xcb_ewmh_get_wm_icon_next(xcb_ewmh_wm_icon_iterator_t *iterator)
{
    if (iterator->rem == 0u) {
        return;
    }
    iterator->index++;
    iterator->rem--;
    if (iterator->rem > 0u) {
        iterator->width = s_icon_widths[iterator->index];
        iterator->height = s_icon_heights[iterator->index];
        iterator->data = s_icon_data[iterator->index];
    }
}

void xcb_ewmh_get_wm_icon_reply_wipe(xcb_ewmh_get_wm_icon_reply_t *wm_icon)
{
    (void) wm_icon;
}


/* Fixture helpers */

/** Resets every stub call counter, recorded argument, and controllable
 *  fixture flag/value to its starting state, without touching any
 *  wmicon_cache_td a test built itself */
static void s_reset_stub_state(void)
{
    s_next_id = 1000u;
    s_create_pixmap_calls = 0;
    s_put_image_calls = 0;
    s_put_image_width = 0u;
    s_put_image_height = 0u;
    s_free_pixmap_calls = 0;
    s_create_gc_calls = 0;
    s_free_gc_calls = 0;
    s_change_gc_calls = 0;
    s_last_change_gc_color = 0u;
    s_poly_fill_rectangle_calls = 0;
    memset(s_last_fill_rects, 0, sizeof(s_last_fill_rects));
    s_render_create_picture_calls = 0;
    s_render_free_picture_calls = 0;
    s_render_set_transform_calls = 0;
    memset(&s_last_transform, 0, sizeof(s_last_transform));
    s_render_set_filter_calls = 0;
    s_render_composite_calls = 0;
    s_last_composite_dst_x = 0;
    s_last_composite_dst_y = 0;
    s_last_composite_w = 0u;
    s_last_composite_h = 0u;
    s_last_composite_mask = XCB_NONE;
    s_render_create_solid_fill_calls = 0;
    s_render_set_clip_calls = 0;
    memset(&s_formats_reply, 0, sizeof(s_formats_reply));
    s_formats_query_fails = false;
    s_find_visual_format_fails = false;
    s_wm_hints_available = false;
    memset(&s_wm_hints_stub, 0, sizeof(s_wm_hints_stub));
    memset(&s_pixmap_geometry_stub, 0, sizeof(s_pixmap_geometry_stub));
    s_get_geometry_fails = false;
    s_ewmh_icon_available = false;
    memset(s_icon_widths, 0, sizeof(s_icon_widths));
    memset(s_icon_heights, 0, sizeof(s_icon_heights));
    memset(s_icon_data, 0, sizeof(s_icon_data));
    s_icon_count = 0u;
    memset(&s_fake_screen, 0, sizeof(s_fake_screen));
    s_fake_screen.root = 1u;
    s_fake_screen.root_visual = 500u;
}

/** One EWMH icon fixture entry at the given index, backed by a static
 *  pixel buffer so 's_build_icon_picture''s per-pixel premultiply loop
 *  has real memory to read; sized to cover every width*height this
 *  test battery ever constructs (largest fixture used is 400 x 4) */
static uint32_t s_icon_pixels[4][1600];

static void s_set_ewmh_icon(unsigned int index, uint32_t width,
        uint32_t height)
{
    s_icon_widths[index] = width;
    s_icon_heights[index] = height;
    s_icon_data[index] = &s_icon_pixels[index][0];
}


/* ==================================================================== *
 * wmicon_draw_at: guard clauses                                         *
 * ==================================================================== */

static void s_test_draw_at_null_guards(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 0, 0 };

    memset(&cache, 0, sizeof(cache));
    s_reset_stub_state();

    wmicon_draw_at(NULL, ewmh, 1u, 2u, pos, 16u, 0u, 0u, &cache);
    wmicon_draw_at(connection, NULL, 1u, 2u, pos, 16u, 0u, 0u, &cache);
    wmicon_draw_at(connection, ewmh, XCB_NONE, 2u, pos, 16u, 0u, 0u,
            &cache);
    wmicon_draw_at(connection, ewmh, 1u, XCB_NONE, pos, 16u, 0u, 0u,
            &cache);
    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 0u, 0u, 0u, &cache);
    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 16u, 0u, 0u, NULL);

    TAP_EQ_INT(s_render_composite_calls + s_poly_fill_rectangle_calls, 0,
            "every null/zero guard returns before drawing anything");
}


/* ==================================================================== *
 * wmicon_draw_at: cache hit fast paths                                  *
 * ==================================================================== */

/* A cache already holding a Picture built at the exact same draw_size
 * this call resolves to composites it directly, without touching any
 * property-fetch stand-in at all */
static void s_test_draw_at_cache_hit_picture(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 3, 5 };

    memset(&cache, 0, sizeof(cache));
    /* area_size 20 * WM_ICON_PIXMAP_SCALE_PERCENT(75) / 100 == 15 */
    cache.picture = 42u;
    cache.draw_size = 15u;
    cache.dest_w = 10u;
    cache.dest_h = 12u;

    s_reset_stub_state();
    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 20u, 0u, 0u, &cache);

    TAP_EQ_INT(s_render_composite_calls, 1,
            "a matching cached picture composites directly");
    TAP_EQ_INT(s_ewmh_icon_available, false,
            "...without ever needing a live EWMH property (never"
            " armed by this fixture)");
    TAP_EQ_INT((long) s_last_composite_w, 10,
            "...compositing at the cached dest_w");
    TAP_EQ_INT((long) s_last_composite_h, 12,
            "...and the cached dest_h");
}

/* A cache confirmed to have no icon at all, at a matching draw_size,
 * redraws the default icon directly, skipping both property fetches */
static void s_test_draw_at_cache_hit_no_icon(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 0, 0 };

    memset(&cache, 0, sizeof(cache));
    cache.has_no_icon = true;
    cache.draw_size = 15u; /* 20 * 75 / 100 */

    s_reset_stub_state();
    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 20u, 0x112233u,
            0x445566u, &cache);

    TAP_EQ_INT(s_poly_fill_rectangle_calls > 0, 1,
            "a cached no-icon result redraws the default icon");
    TAP_EQ_INT(s_render_composite_calls, 0,
            "...without ever compositing a real icon picture");
}

/* A cache holding a stale picture at a different draw_size (an
 * 'area_size' change between calls) is not reused: this falls through
 * to a real fetch instead of the fast path */
static void s_test_draw_at_cache_miss_on_size_change(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 0, 0 };

    memset(&cache, 0, sizeof(cache));
    cache.picture = 42u;
    cache.draw_size = 999u; /* deliberately not this call's draw_size */

    s_reset_stub_state();
    s_ewmh_icon_available = false; /* falls all the way to default */
    s_wm_hints_available = false;
    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 20u, 0u, 0u, &cache);

    TAP_EQ_INT(s_render_free_picture_calls, 1,
            "a size mismatch invalidates the stale cached picture");
    TAP_EQ_INT(s_poly_fill_rectangle_calls > 0, 1,
            "...and falls through to the default icon, since neither"
            " property stand-in is armed");
}


/* ==================================================================== *
 * wmicon_draw_at: EWMH best-fit scoring                                 *
 * ==================================================================== */

/* With several candidate sizes published, the one minimizing
 * |dw| + |dh| against the resolved draw_size is chosen, not simply
 * the largest or the first one listed */
static void s_test_draw_at_ewmh_picks_closest_size(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 0, 0 };

    memset(&cache, 0, sizeof(cache));
    s_reset_stub_state();
    s_ewmh_icon_available = true;
    /* draw_size resolves to 16 * 75 / 100 == 12 */
    s_set_ewmh_icon(0u, 48u, 48u);  /* score |48-12|*2 = 72 */
    s_set_ewmh_icon(1u, 16u, 16u);  /* score |16-12|*2 = 8: closest */
    s_set_ewmh_icon(2u, 8u, 8u);    /* score |8-12|*2 = 8: tied */
    s_icon_count = 2u; /* only the first two entries: index 1 wins */

    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 16u, 0u, 0u, &cache);

    TAP_EQ_INT((long) s_put_image_width, 16,
            "the 16x16 candidate (closest to draw_size 12) is the one"
            " actually uploaded, not the larger 48x48 one");
    TAP_EQ_INT(s_render_composite_calls, 1,
            "a usable EWMH icon composites without falling back");
}

/* A tie in score keeps whichever candidate was seen first, since the
 * comparison is a strict '<', not '<=' */
static void s_test_draw_at_ewmh_tie_keeps_first_seen(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 0, 0 };

    memset(&cache, 0, sizeof(cache));
    s_reset_stub_state();
    s_ewmh_icon_available = true;
    /* draw_size resolves to 16 * 75 / 100 == 12; both entries below
     * score identically (|16-12|+|16-12| == |8-12|+|8-12| == 8) */
    s_set_ewmh_icon(0u, 16u, 16u);
    s_set_ewmh_icon(1u, 8u, 8u);
    s_icon_count = 2u;

    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 16u, 0u, 0u, &cache);

    TAP_EQ_INT((long) s_put_image_width, 16,
            "a tied score keeps the first candidate seen (16x16), not"
            " a later one that scores no better");
}

/* A candidate wider than WMICON_MAX_SIDE (512) is rejected from the
 * scoring entirely, even when it would otherwise have won */
static void s_test_draw_at_ewmh_rejects_oversized_candidate(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 0, 0 };

    memset(&cache, 0, sizeof(cache));
    s_reset_stub_state();
    s_ewmh_icon_available = true;
    s_set_ewmh_icon(0u, 600u, 600u); /* over WMICON_MAX_SIDE: rejected */
    s_set_ewmh_icon(1u, 32u, 32u);   /* the only eligible candidate */
    s_icon_count = 2u;

    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 16u, 0u, 0u, &cache);

    TAP_EQ_INT((long) s_put_image_width, 32,
            "an oversized candidate is skipped; the only eligible one"
            " left is what gets uploaded");
}

/* A zero-width or zero-height candidate is likewise never eligible */
static void s_test_draw_at_ewmh_rejects_zero_size_candidate(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 0, 0 };

    memset(&cache, 0, sizeof(cache));
    s_reset_stub_state();
    s_ewmh_icon_available = true;
    s_set_ewmh_icon(0u, 0u, 16u);  /* zero width: rejected */
    s_set_ewmh_icon(1u, 24u, 24u);
    s_icon_count = 2u;

    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 16u, 0u, 0u, &cache);

    TAP_EQ_INT((long) s_put_image_width, 24,
            "a zero-width candidate is skipped");
}

/* An EWMH property present but with zero icons in it (num_icons == 0)
 * falls through to the ICCCM fallback exactly like no property at all */
static void s_test_draw_at_ewmh_empty_falls_back(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 0, 0 };

    memset(&cache, 0, sizeof(cache));
    s_reset_stub_state();
    s_ewmh_icon_available = true;
    s_icon_count = 0u; /* property present, but empty */
    s_wm_hints_available = false; /* and ICCCM has nothing either */

    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 16u, 0x102030u,
            0x405060u, &cache);

    TAP_EQ_INT(s_put_image_calls, 0,
            "no EWMH candidate at all: no picture is ever uploaded");
    TAP_EQ_INT(s_poly_fill_rectangle_calls > 0, 1,
            "...falling all the way through to the default icon");
    TAP_OK(cache.has_no_icon,
            "...and caching that no icon was found for next time");
}


/* ==================================================================== *
 * wmicon_draw_at: ICCCM WM_HINTS fallback                               *
 * ==================================================================== */

/* No EWMH icon, but a depth-1 icon_pixmap set via WM_HINTS: treated as
 * an ICCCM stencil, filled in a solid color through the pixmap used
 * directly as the RENDER mask */
static void s_test_draw_at_icccm_depth1_stencil(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 0, 0 };

    memset(&cache, 0, sizeof(cache));
    s_reset_stub_state();
    s_ewmh_icon_available = false;
    s_wm_hints_available = true;
    s_wm_hints_stub.flags = XCB_ICCCM_WM_HINT_ICON_PIXMAP;
    s_wm_hints_stub.icon_pixmap = 777u;
    s_pixmap_geometry_stub.width = 20u;
    s_pixmap_geometry_stub.height = 20u;
    s_pixmap_geometry_stub.depth = 1u;

    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 16u, 0u, 0u, &cache);

    TAP_EQ_INT(s_render_create_solid_fill_calls, 1,
            "a depth-1 icon_pixmap paints a solid-color fill as its"
            " source");
    TAP_EQ_INT(s_render_composite_calls, 1,
            "...composited through the pixmap itself as the mask");
    TAP_OK(cache.mask_picture != XCB_NONE,
            "...for a depth-1 stencil, the bitmap itself becomes the"
            " RENDER mask (result.mask), while the solid black fill"
            " is the source being masked through it");
}

/* A full-depth icon_pixmap (what xterm actually publishes) is treated
 * as a real color image using the root visual format, and clipped
 * through a separately set icon_mask when present */
static void s_test_draw_at_icccm_full_depth_with_mask(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 0, 0 };

    memset(&cache, 0, sizeof(cache));
    s_reset_stub_state();
    s_ewmh_icon_available = false;
    s_wm_hints_available = true;
    s_wm_hints_stub.flags = XCB_ICCCM_WM_HINT_ICON_PIXMAP |
        XCB_ICCCM_WM_HINT_ICON_MASK;
    s_wm_hints_stub.icon_pixmap = 778u;
    s_wm_hints_stub.icon_mask = 779u;
    s_pixmap_geometry_stub.width = 24u;
    s_pixmap_geometry_stub.height = 24u;
    s_pixmap_geometry_stub.depth = 24u;

    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 16u, 0u, 0u, &cache);

    TAP_EQ_INT(s_render_create_solid_fill_calls, 0,
            "a full-depth icon_pixmap is not painted as a stencil"
            " fill");
    TAP_EQ_INT(s_render_composite_calls, 1,
            "...it composites as a real color image");
    TAP_OK(s_last_composite_mask != XCB_NONE,
            "...composited with a real mask picture built from"
            " icon_mask, not XCB_NONE");
    TAP_OK(cache.mask_picture != XCB_NONE,
            "the built icon_mask ends up in the cache slot's own"
            " mask_picture field too");
}

/* An icon_pixmap missing entirely (WM_HINT_ICON_PIXMAP unset, or set
 * to XCB_NONE) is not usable: falls through to the default icon */
static void s_test_draw_at_icccm_missing_pixmap_falls_back(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 0, 0 };

    memset(&cache, 0, sizeof(cache));
    s_reset_stub_state();
    s_ewmh_icon_available = false;
    s_wm_hints_available = true;
    s_wm_hints_stub.flags = 0; /* ICON_PIXMAP bit not set */

    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 16u, 0u, 0u, &cache);

    TAP_EQ_INT(s_render_create_picture_calls, 0,
            "no icon_pixmap flag set: no Picture is ever built for it");
    TAP_EQ_INT(s_poly_fill_rectangle_calls > 0, 1,
            "...falling through to the default icon instead");
}

/* An oversized icon_pixmap (either axis over WMICON_MAX_SIDE) is
 * rejected the same way an oversized EWMH candidate is */
static void s_test_draw_at_icccm_oversized_pixmap_falls_back(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 0, 0 };

    memset(&cache, 0, sizeof(cache));
    s_reset_stub_state();
    s_ewmh_icon_available = false;
    s_wm_hints_available = true;
    s_wm_hints_stub.flags = XCB_ICCCM_WM_HINT_ICON_PIXMAP;
    s_wm_hints_stub.icon_pixmap = 777u;
    s_pixmap_geometry_stub.width = 600u; /* over WMICON_MAX_SIDE */
    s_pixmap_geometry_stub.height = 600u;
    s_pixmap_geometry_stub.depth = 24u;

    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 16u, 0u, 0u, &cache);

    TAP_EQ_INT(s_render_create_picture_calls, 0,
            "an oversized icon_pixmap's geometry is rejected before"
            " any Picture is built from it");
    TAP_EQ_INT(s_poly_fill_rectangle_calls > 0, 1,
            "...falling through to the default icon");
}


/* ==================================================================== *
 * wmicon_draw_at: aspect-preserving scale arithmetic                    *
 * ==================================================================== */

/* A wider-than-tall source is letterboxed: scaled by its narrower
 * dimension's ratio, so it never overflows draw_size on either axis */
static void s_test_draw_at_scale_letterboxes_wide_source(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 0, 0 };

    memset(&cache, 0, sizeof(cache));
    s_reset_stub_state();
    s_ewmh_icon_available = true;
    s_set_ewmh_icon(0u, 40u, 20u); /* 2:1 aspect ratio */
    s_icon_count = 1u;
    /* draw_size resolves to 20 * 75 / 100 == 15 */

    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 20u, 0u, 0u, &cache);

    TAP_EQ_INT((long) cache.dest_w, 15,
            "the wider axis is scaled to fill the full draw_size");
    TAP_EQ_INT((long) cache.dest_h, 8,
            "...and the narrower axis keeps the source's own 2:1"
            " aspect ratio (15 / 2, rounded to nearest)");
}

/* A source dimension so much larger than the other that the scaled
 * result would round to 0 is clamped up to 1, never left at 0 (which
 * would make an unusable, invisible destination image) */
static void s_test_draw_at_scale_clamps_to_at_least_one_pixel(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;
    struct position_s pos = { 0, 0 };

    memset(&cache, 0, sizeof(cache));
    s_reset_stub_state();
    s_ewmh_icon_available = true;
    s_set_ewmh_icon(0u, 400u, 4u); /* 100:1 aspect ratio */
    s_icon_count = 1u;
    /* draw_size resolves to 20 * 75 / 100 == 15;
     * naive h = 15 * 4 / 400 == 0.15, rounds to 0 without the clamp */

    wmicon_draw_at(connection, ewmh, 1u, 2u, pos, 20u, 0u, 0u, &cache);

    TAP_OK(cache.dest_h >= 1u,
            "the narrower scaled axis is clamped to at least 1 pixel,"
            " never rounded down to 0");
}


/* ==================================================================== *
 * wmicon_invalidate                                                     *
 * ==================================================================== */

static void s_test_invalidate_null_cache_is_noop(void)
{
    s_reset_stub_state();
    wmicon_invalidate((xcb_connection_t *) 1, NULL);

    TAP_EQ_INT(s_render_free_picture_calls, 0,
            "a NULL cache slot is a no-op");
}

/* A cache holding both a picture and a mask_picture frees both, only
 * when a live connection is given */
static void s_test_invalidate_frees_both_pictures(void)
{
    wmicon_cache_td cache;

    memset(&cache, 0, sizeof(cache));
    cache.picture = 10u;
    cache.mask_picture = 20u;
    cache.draw_size = 15u;
    cache.dest_w = 8u;
    cache.dest_h = 8u;
    cache.has_no_icon = true;

    s_reset_stub_state();
    wmicon_invalidate((xcb_connection_t *) 1, &cache);

    TAP_EQ_INT(s_render_free_picture_calls, 2,
            "both a picture and a mask_picture are freed");
    TAP_EQ_INT((long) cache.picture, (long) XCB_NONE,
            "...and the slot's picture is reset to XCB_NONE");
    TAP_EQ_INT((long) cache.mask_picture, (long) XCB_NONE,
            "...its mask_picture too");
    TAP_EQ_INT((long) cache.draw_size, 0,
            "...draw_size is reset to 0");
    TAP_EQ_INT(cache.has_no_icon, false,
            "...and has_no_icon is reset to false");
}

/* A NULL connection resets every field the same way, but never calls
 * through to free anything server-side (there is no live connection
 * to free it on) */
static void s_test_invalidate_null_connection_resets_without_freeing(void)
{
    wmicon_cache_td cache;

    memset(&cache, 0, sizeof(cache));
    cache.picture = 10u;
    cache.mask_picture = 20u;

    s_reset_stub_state();
    wmicon_invalidate(NULL, &cache);

    TAP_EQ_INT(s_render_free_picture_calls, 0,
            "a NULL connection never calls xcb_render_free_picture");
    TAP_EQ_INT((long) cache.picture, (long) XCB_NONE,
            "...but the cache slot's fields are still reset");
}

/* Invalidating an already-empty cache slot touches neither stub */
static void s_test_invalidate_already_empty_is_noop(void)
{
    wmicon_cache_td cache;

    memset(&cache, 0, sizeof(cache));

    s_reset_stub_state();
    wmicon_invalidate((xcb_connection_t *) 1, &cache);

    TAP_EQ_INT(s_render_free_picture_calls, 0,
            "an already-empty cache slot frees nothing");
}


/* ==================================================================== *
 * wmicon_draw: thin (0, 0)-offset wrapper                               *
 * ==================================================================== */

static void s_test_draw_delegates_at_origin(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 1;
    xcb_ewmh_connection_t *ewmh = (xcb_ewmh_connection_t *) 1;
    wmicon_cache_td cache;

    memset(&cache, 0, sizeof(cache));
    cache.picture = 42u;
    cache.draw_size = 15u; /* 20 * 75 / 100 */
    /* dest_w/dest_h deliberately equal area_size, so the destination
     * fills the square exactly and s_composite_cached's own centering
     * math (area_size > dest_dim.w/h) contributes no extra offset of
     * its own; this isolates wmicon_draw's fixed (0, 0) offset_pos
     * from that separate centering behavior, which is covered on its
     * own merits by the letterboxing tests above */
    cache.dest_w = 20u;
    cache.dest_h = 20u;

    s_reset_stub_state();
    wmicon_draw(connection, ewmh, 1u, 2u, 20u, 0u, 0u, &cache);

    TAP_EQ_INT(s_render_composite_calls, 1,
            "wmicon_draw reaches the same composite path as"
            " wmicon_draw_at");
    TAP_EQ_INT((long) s_last_composite_dst_x, 0,
            "...offset at (0, 0) on the x axis, its fixed position");
    TAP_EQ_INT((long) s_last_composite_dst_y, 0,
            "...and on the y axis too");
}


/* ==================================================================== *
 * wmicon_renderer_destroy                                               *
 * ==================================================================== */

/* By this point in the test battery, earlier tests have already
 * driven the default-icon fallback (which lazily creates and caches
 * s_default_icon_gc the first time it runs, held for the life of the
 * process per its own Doxygen comment), so this call is expected to
 * free that one real cached gc; it must not, however, crash or
 * double-free on a second call right after, since s_default_icon_gc
 * is reset to XCB_NONE once freed */
static void s_test_renderer_destroy_frees_cached_gc_once(void)
{
    s_reset_stub_state();
    wmicon_renderer_destroy();

    TAP_EQ_INT(s_free_gc_calls, 1,
            "wmicon_renderer_destroy frees the one default-icon gc"
            " earlier tests already caused to be cached");

    s_reset_stub_state();
    wmicon_renderer_destroy();

    TAP_EQ_INT(s_free_gc_calls, 0,
            "...and calling it again right after is a no-op, since"
            " the cached gc was already reset to XCB_NONE");
}


int main(void)
{
    TAP_PLAN(45);

    s_test_draw_at_null_guards();

    s_test_draw_at_cache_hit_picture();
    s_test_draw_at_cache_hit_no_icon();
    s_test_draw_at_cache_miss_on_size_change();

    s_test_draw_at_ewmh_picks_closest_size();
    s_test_draw_at_ewmh_tie_keeps_first_seen();
    s_test_draw_at_ewmh_rejects_oversized_candidate();
    s_test_draw_at_ewmh_rejects_zero_size_candidate();
    s_test_draw_at_ewmh_empty_falls_back();

    s_test_draw_at_icccm_depth1_stencil();
    s_test_draw_at_icccm_full_depth_with_mask();
    s_test_draw_at_icccm_missing_pixmap_falls_back();
    s_test_draw_at_icccm_oversized_pixmap_falls_back();

    s_test_draw_at_scale_letterboxes_wide_source();
    s_test_draw_at_scale_clamps_to_at_least_one_pixel();

    s_test_invalidate_null_cache_is_noop();
    s_test_invalidate_frees_both_pictures();
    s_test_invalidate_null_connection_resets_without_freeing();
    s_test_invalidate_already_empty_is_noop();

    s_test_draw_delegates_at_origin();

    s_test_renderer_destroy_frees_cached_gc_once();

    return TAP_DONE();
}
