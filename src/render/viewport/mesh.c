/**
 * @file render/viewport/mesh.c
 *
 * @brief Dot mesh painted on the root window to make viewport panning
 *        visible
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
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/xcb/connection.h>

/* Initial default values */
#include <defs/config.h>

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <stage.h>
#include <stage/viewport.h>
#include <wm.h>

/* Local includes */
#include <render/viewport/mesh.h>


/**
 * @brief One screen's cached mesh tile and the state it was built from
 *
 * Every field but @c tile is what the tile was built against, compared
 * on the next render to decide whether anything has to be rebuilt.
 * Kept per screen rather than per desktop, since all the desktops on
 * one screen share that screen's single root window and would
 * otherwise each repaint over whatever the last one applied without
 * ever noticing.
 *
 * @c is_applied starts false so the very first render always paints,
 * and is cleared again by @a viewport_mesh_cache_invalidate .
 */
struct s_mesh_cache_s {
    xcb_pixmap_t tile;
    xcb_pixmap_t retired;   /**< The tile this one replaced, still
                              *  named by the root's own
                              *  @c XCB_CW_BACK_PIXMAP until the
                              *  caller installs the new one */
    uint32_t background;
    uint32_t dot;
    uint32_t spacing_horizontal;
    uint32_t spacing_vertical;
    uint32_t thickness;
    uint32_t origin_x;
    uint32_t origin_y;
    bool is_applied;
};

static struct s_mesh_cache_s s_cache[CONFIG_MAX_SCREENS];


/**
 * @brief Rec. 601 luma of a packed color
 *
 * @param color Color to weigh, as @c 0x00RRGGBB
 *
 * @return Luma, in @c [0, 255]
 *
 * @note Complexity: @e O(1)
 */
static uint32_t s_luma_rec601(uint32_t color)
{
    const uint32_t red = (color >> 16) & 0xFFu;
    const uint32_t green = (color >> 8) & 0xFFu;
    const uint32_t blue = color & 0xFFu;

    return (RENDER_VIEWPORT_LUMA_WEIGHT_RED * red +
            RENDER_VIEWPORT_LUMA_WEIGHT_GREEN * green +
            RENDER_VIEWPORT_LUMA_WEIGHT_BLUE * blue) /
                RENDER_VIEWPORT_LUMA_DIVISOR;
}


/**
 * @brief Push one channel away from the background, toward black or
 *        toward white
 *
 * @param channel    Channel value to shift, in @c [0, 255]
 * @param tone_shift How far to push it, as a percentage
 * @param is_darker  Whether the mesh is darker than its background
 *
 * @return Shifted channel value, in @c [0, 255]
 *
 * @note Complexity: @e O(1)
 */
static uint32_t s_shift_channel(uint32_t channel, uint32_t tone_shift,
        bool is_darker)
{
    if (is_darker) {
        return channel * (100u - tone_shift) / 100u;
    }

    return channel + (255u - channel) * tone_shift / 100u;
}


/**
 * @brief Look up a screen's cache slot
 *
 * @param screen_id Screen to look the slot up for
 *
 * @return Slot for @p screen_id, or @c NULL where that screen is out of
 *         range
 *
 * @note Complexity: @e O(1)
 */
static struct s_mesh_cache_s *s_cache_slot(uint32_t screen_id)
{
    if (screen_id >= (uint32_t) CONFIG_MAX_SCREENS) {
        return NULL;
    }

    return &s_cache[screen_id];
}


/**
 * @brief Set a screen's cached tile aside, without freeing it
 *
 * The root window's own @c XCB_CW_BACK_PIXMAP may still name that tile,
 * and goes on naming it until something points the attribute elsewhere,
 * so freeing it here would leave the root drawing from a pixmap the
 * server no longer has.  Whoever points the attribute away calls
 * @a viewport_mesh_cache_release_retired afterwards.
 *
 * @param connection Connection the pixmap lives on, or @c NULL when
 *                   there is none left to free anything on
 * @param slot Cache slot to set aside
 *
 * @note An older retired tile, if one is somehow still held, is freed
 *       here: the root cannot be naming two at once
 * @note Complexity: @e O(1)
 */
static void s_cache_retire(xcb_connection_t *connection,
        struct s_mesh_cache_s *slot)
{
    if (connection != NULL && slot->retired != XCB_NONE) {
        xcb_free_pixmap(connection, slot->retired);
    }
    slot->retired = slot->tile;
    slot->tile = XCB_NONE;
    slot->is_applied = false;
}


/* Derive the mesh dot color from the desktop background color */
uint32_t viewport_mesh_color_from_background(uint32_t background,
        uint32_t tone_shift)
{
    const bool is_darker = (s_luma_rec601(background) >=
            RENDER_VIEWPORT_LUMA_MIDPOINT);
    uint32_t red;
    uint32_t green;
    uint32_t blue;

    if (tone_shift > 100u) {
        tone_shift = 100u;
    }

    red = s_shift_channel((background >> 16) & 0xFFu, tone_shift,
            is_darker);
    green = s_shift_channel((background >> 8) & 0xFFu, tone_shift,
            is_darker);
    blue = s_shift_channel(background & 0xFFu, tone_shift, is_darker);

    return (red << 16) | (green << 8) | blue;
}


/* Where one axis' first dot sits inside the mesh tile for a given
 * viewport origin */
uint32_t viewport_mesh_tile_origin(int32_t origin, uint32_t spacing)
{
    int32_t remainder;

    if (spacing == 0u) {
        return 0u;
    }

    /* C's '%' truncates toward zero, so a negative origin yields
     * a negative remainder that would place the dot outside the tile;
     * one spacing added back folds it into '[0, spacing)' */
    remainder = origin % (int32_t) spacing;
    if (remainder < 0) {
        remainder += (int32_t) spacing;
    }

    return (uint32_t) remainder;
}


/* Whether a mesh should be painted for a desktop at all */
bool viewport_mesh_is_visible(const desktop_td *desktop)
{
    if (desktop == NULL || desktop->config == NULL) {
        return false;
    }

    if (!desktop->config->base.viewport.mesh.is_enabled) {
        return false;
    }

    /* An external tool owns the root window's pixels, and the mesh
     * would have to take over the very background pixmap holding them;
     * leave that wallpaper alone instead */
    if (desktop->background.use_root_pixmap) {
        return false;
    }

    return stage_viewport_has_room(
            wm_get_stage_by_id(desktop->screen_id));
}


/* Build the mesh tile pixmap for a desktop's current viewport origin */
xcb_pixmap_t viewport_mesh_tile_create(xcb_connection_t *connection,
        const desktop_td *desktop)
{
    const struct config_viewport_mesh_s *mesh;
    struct s_mesh_cache_s *slot;
    xcb_pixmap_t tile;
    xcb_gcontext_t context;
    xcb_rectangle_t dot;
    uint32_t values[2];
    uint32_t dot_color;

    if (connection == NULL || desktop == NULL ||
            desktop->config == NULL || desktop->screen == NULL) {
        return XCB_NONE;
    }

    slot = s_cache_slot(desktop->screen_id);
    if (slot == NULL) {
        return XCB_NONE;
    }
    /* Retired rather than freed.  The root's own 'XCB_CW_BACK_PIXMAP'
     * still names it, and goes on naming it until
     * 'viewport_mesh_render' installs the tile built here, so freeing
     * it anywhere before that would leave the attribute pointing at
     * a pixmap the server no longer has: any repaint of the root in
     * between, ours or another client's, would draw from it (see
     * 'render_desktop_background_render''s own note on exactly this
     * hazard, in render/desktop/background.c).  Whoever installs the
     * new tile frees it, and 's_cache_release' covers the case where
     * nobody ever does. */
    s_cache_retire(connection, slot);

    mesh = &desktop->config->base.viewport.mesh;
    dot_color = viewport_mesh_color_from_background(
            desktop->background.bg.color, mesh->tone_shift);

    tile = xcb_generate_id(connection);
    xcb_create_pixmap(connection, desktop->screen->root_depth, tile,
            desktop->screen->root,
            (uint16_t) mesh->spacing_horizontal,
            (uint16_t) mesh->spacing_vertical);

    /* Background first, then the single dot on top: the server
     * repeats this one tile across the whole root window itself, so
     * one dot here becomes the entire mesh on screen */
    context = xcb_generate_id(connection);
    values[0] = desktop->background.bg.color;
    values[1] = desktop->background.bg.color;
    xcb_create_gc(connection, context, tile,
            XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, values);

    dot.x = 0;
    dot.y = 0;
    dot.width = (uint16_t) mesh->spacing_horizontal;
    dot.height = (uint16_t) mesh->spacing_vertical;
    xcb_poly_fill_rectangle(connection, tile, context, 1u, &dot);

    values[0] = dot_color;
    xcb_change_gc(connection, context, XCB_GC_FOREGROUND, values);

    /* Negated: the canvas scrolls the opposite way to the origin.
     * Panning east raises the origin and carries every client west, so
     * a mesh whose dots followed the origin would travel against
     * everything else on screen.  'viewport_mesh_tile_origin' folds the
     * negative back into the tile. */
    dot.x = (int16_t) viewport_mesh_tile_origin(
            -desktop->viewport_origin.x, mesh->spacing_horizontal);
    dot.y = (int16_t) viewport_mesh_tile_origin(
            -desktop->viewport_origin.y, mesh->spacing_vertical);
    dot.width = (uint16_t) mesh->thickness;
    dot.height = (uint16_t) mesh->thickness;
    xcb_poly_fill_rectangle(connection, tile, context, 1u, &dot);

    xcb_free_gc(connection, context);

    slot->tile = tile;
    slot->background = desktop->background.bg.color;
    slot->dot = dot_color;
    slot->spacing_horizontal = mesh->spacing_horizontal;
    slot->spacing_vertical = mesh->spacing_vertical;
    slot->thickness = mesh->thickness;
    slot->origin_x = (uint32_t) dot.x;
    slot->origin_y = (uint32_t) dot.y;

    return tile;
}


/* Paint a desktop's mesh onto its root window */
int viewport_mesh_render(xcb_connection_t *connection,
        const desktop_td *desktop)
{
    const struct config_viewport_mesh_s *mesh;
    struct s_mesh_cache_s *slot;
    uint32_t origin_x;
    uint32_t origin_y;
    uint32_t dot_color;
    uint32_t values[1];

    if (connection == NULL || desktop == NULL ||
            desktop->config == NULL || desktop->screen == NULL) {
        return 1;
    }

    slot = s_cache_slot(desktop->screen_id);
    if (slot == NULL) {
        return 1;
    }

    mesh = &desktop->config->base.viewport.mesh;
    origin_x = viewport_mesh_tile_origin(-desktop->viewport_origin.x,
            mesh->spacing_horizontal);
    origin_y = viewport_mesh_tile_origin(-desktop->viewport_origin.y,
            mesh->spacing_vertical);
    dot_color = viewport_mesh_color_from_background(
            desktop->background.bg.color, mesh->tone_shift);

    /* Every input the tile was built from, so that a pan, a background
     * color change and a reloaded mesh setting each repaint while an
     * ordinary full-desktop render does not.  The derived dot color is
     * compared rather than 'tone_shift' itself, since that is what
     * actually reaches the tile */
    if (slot->is_applied && slot->tile != XCB_NONE &&
            slot->background == desktop->background.bg.color &&
            slot->dot == dot_color &&
            slot->spacing_horizontal == mesh->spacing_horizontal &&
            slot->spacing_vertical == mesh->spacing_vertical &&
            slot->thickness == mesh->thickness &&
            slot->origin_x == origin_x &&
            slot->origin_y == origin_y) {
        return 0;
    }

    if (viewport_mesh_tile_create(connection, desktop) == XCB_NONE) {
        LOGGER_ERROR("Could not build the viewport mesh tile for" \
                " screen %u", desktop->screen_id);
        return 1;
    }

    values[0] = slot->tile;
    xcb_change_window_attributes(connection, desktop->screen->root,
            XCB_CW_BACK_PIXMAP, values);
    xcb_clear_area(connection, 0, desktop->screen->root, 0, 0,
            desktop->screen->width_in_pixels,
            desktop->screen->height_in_pixels);
    slot->is_applied = true;

    /* Only now: the attribute above named the old tile right up to this
     * point, so this is the first moment at which nothing can still be
     * drawn from it */
    if (slot->retired != XCB_NONE) {
        xcb_free_pixmap(connection, slot->retired);
        slot->retired = XCB_NONE;
    }

    LOGGER_TRACE("Viewport mesh painted on screen %u at tile" \
            " offset %u,%u", desktop->screen_id, origin_x, origin_y);

    return 0;
}


/* Drop every screen's cached mesh tile and painted state */
void viewport_mesh_cache_invalidate(void)
{
    xcb_connection_t *const connection = xcb_connection_get();

    for (uint32_t i = 0u; i < (uint32_t) CONFIG_MAX_SCREENS; ++i) {
        s_cache_retire(connection, &s_cache[i]);
    }
}


/* Free every tile the root window has already been pointed away from */
void viewport_mesh_cache_release_retired(xcb_connection_t *connection)
{
    if (connection == NULL) {
        return;
    }

    for (uint32_t i = 0u; i < (uint32_t) CONFIG_MAX_SCREENS; ++i) {
        if (s_cache[i].retired != XCB_NONE) {
            xcb_free_pixmap(connection, s_cache[i].retired);
            s_cache[i].retired = XCB_NONE;
        }
    }
}
