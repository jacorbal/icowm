/**
 * @file render/client/titlebar.c
 *
 * @brief Client titlebar rendering: background, text and buttons
 *
 * @note Button colors come from the theme passed to
 *       @c s_desktop_titlebar_buttons_draw
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
#include <xcb/xcb_ewmh.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <render/text.h>
#include <surface.h>
#include <surface/viewport.h>
#include <wm.h>

/* Utils includes */
#include <utils/xcb/connection.h>
#include <utils/xcb/pixmap.h>

/* Local includes */
#include <render/client/titlebar.h>


/**
 * @brief Color a single titlebar button should be drawn in
 *
 * Pin, sticky and layer buttons reflect their state (pinned, sticky or
 * non-normal layer) with the active accent color regardless of window
 * focus; every other button reflects window focus instead, the same way
 * the titlebar text itself does.  Maximize and fullscreen fall back to
 * the background color, which makes them effectively invisible, when
 * the client cannot be resized, instead of drawing a button that would
 * do nothing if clicked.
 *
 * @param button         Which titlebar button is being colored
 * @param is_focused     Whether owning client is currently focused
 * @param is_pinned      Whether owning client has the pin flag set
 * @param is_sticky      Whether owning client has the sticky flag set
 * @param is_layered     Whether the client layer is above or below
 *                       normal
 * @param can_maximize   Whether the maximize button is enabled
 * @param color_active   Color used when a state-reflecting button's
 *                       state is on
 * @param color_inactive Color used otherwise
 * @param bg_fill        Background color used for a disabled
 *                       maximize/fullscreen button
 *
 * @return The color @p button should be filled with
 *
 * @note Complexity: @e O(1)
 */
static uint32_t s_titlebar_button_color(
        enum config_titlebar_button_e button, bool is_focused,
        bool is_pinned, bool is_sticky, bool is_layered,
        bool can_maximize, uint32_t color_active,
        uint32_t color_inactive, uint32_t bg_fill)
{
    if (!can_maximize &&
            (button == CONFIG_TITLEBAR_BUTTON_MAXIMIZE ||
             button == CONFIG_TITLEBAR_BUTTON_FULLSCREEN)) {
        return bg_fill;
    }
    if (button == CONFIG_TITLEBAR_BUTTON_PIN) {
        return (is_pinned) ? color_active : color_inactive;
    }
    if (button == CONFIG_TITLEBAR_BUTTON_STICKY) {
        return (is_sticky) ? color_active : color_inactive;
    }
    if (button == CONFIG_TITLEBAR_BUTTON_LAYER) {
        return (is_layered) ? color_active : color_inactive;
    }

    return (is_focused) ? color_active : color_inactive;
}


/**
 * @brief Geometry every button shape is drawn from
 *
 * Worked out once per button and handed to the shape functions, so
 * none of them repeats the same six subtractions.
 */
struct s_btn_box_s {
    int16_t lo;         /**< Left edge of the drawable area */
    int16_t to;         /**< Top edge of the drawable area */
    int16_t hi;         /**< Right edge of the drawable area */
    int16_t bo;         /**< Bottom edge of the drawable area */
    int16_t in;         /**< Inset, and the stroke width in use */
    uint16_t span;      /**< Width and height of the drawable area */
    int16_t x;          /**< Left edge of the whole box */
    int16_t y;          /**< Top edge of the whole box */
    uint16_t side;      /**< Side of the whole box */
};


/**
 * @brief Draw the plain filled square
 *
 * What every button looked like before each got a shape, what the
 * three state-reporting ones still look like, since their color is
 * already saying whether the state is on, and what every button falls
 * back to when @c window.titlebar.buttons.use-symbols is off.
 *
 * @param connection Active XCB connection
 * @param target     Drawable the button lands on
 * @param gc         Context already set to the button's color
 * @param box        Geometry of the button
 *
 * @note Complexity: @e O(1)
 */
static void s_btn_shape_square(xcb_connection_t *connection,
        xcb_drawable_t target, xcb_gcontext_t gc,
        const struct s_btn_box_s *box)
{
    xcb_rectangle_t rect = { box->x, box->y, box->side, box->side };

    xcb_poly_fill_rectangle(connection, target, gc, 1u, &rect);
}


/**
 * @brief Draw the close button: two crossed diagonals
 *
 * @param connection Active XCB connection
 * @param target     Drawable the button lands on
 * @param gc         Context already set to the button's color
 * @param box        Geometry of the button
 *
 * @note The one shape a user recognizes without being told, which is
 *       why it is the button that survives every narrowing
 * @note Complexity: @e O(1)
 */
static void s_btn_shape_close(xcb_connection_t *connection,
        xcb_drawable_t target, xcb_gcontext_t gc,
        const struct s_btn_box_s *box)
{
    xcb_segment_t seg[2];

    seg[0] = (xcb_segment_t) { box->lo, box->to, box->hi, box->bo };
    seg[1] = (xcb_segment_t) { box->hi, box->to, box->lo, box->bo };
    xcb_poly_segment(connection, target, gc, 2u, seg);
}


/**
 * @brief Draw the maximize button: a hollow square
 *
 * @param connection Active XCB connection
 * @param target     Drawable the button lands on
 * @param gc         Context already set to the button's color
 * @param box        Geometry of the button
 *
 * @note The outline of a window grown to fill its space
 * @note Complexity: @e O(1)
 */
static void s_btn_shape_maximize(xcb_connection_t *connection,
        xcb_drawable_t target, xcb_gcontext_t gc,
        const struct s_btn_box_s *box)
{
    xcb_rectangle_t rect = { box->lo, box->to,
        (uint16_t) (box->span - 1u), (uint16_t) (box->span - 1u) };

    xcb_poly_rectangle(connection, target, gc, 1u, &rect);
}


/**
 * @brief Draw the fullscreen button: four corners, no frame
 *
 * @param connection Active XCB connection
 * @param target     Drawable the button lands on
 * @param gc         Context already set to the button's color
 * @param box        Geometry of the button
 *
 * @note Corners pushing outwards with nothing between them: the
 *       window leaving its own edges behind, and what tells this
 *       apart from the closed outline of maximize
 * @note Complexity: @e O(1)
 */
static void s_btn_shape_fullscreen(xcb_connection_t *connection,
        xcb_drawable_t target, xcb_gcontext_t gc,
        const struct s_btn_box_s *box)
{
    const int16_t arm = box->in;
    xcb_segment_t seg[8];

    seg[0] = (xcb_segment_t) { box->lo, box->to,
        (int16_t) (box->lo + arm), box->to };
    seg[1] = (xcb_segment_t) { box->lo, box->to, box->lo,
        (int16_t) (box->to + arm) };
    seg[2] = (xcb_segment_t) { box->hi, box->to,
        (int16_t) (box->hi - arm), box->to };
    seg[3] = (xcb_segment_t) { box->hi, box->to, box->hi,
        (int16_t) (box->to + arm) };
    seg[4] = (xcb_segment_t) { box->lo, box->bo,
        (int16_t) (box->lo + arm), box->bo };
    seg[5] = (xcb_segment_t) { box->lo, box->bo, box->lo,
        (int16_t) (box->bo - arm) };
    seg[6] = (xcb_segment_t) { box->hi, box->bo,
        (int16_t) (box->hi - arm), box->bo };
    seg[7] = (xcb_segment_t) { box->hi, box->bo, box->hi,
        (int16_t) (box->bo - arm) };
    xcb_poly_segment(connection, target, gc, 8u, seg);
}


/**
 * @brief Draw the iconize button: a bar along the bottom
 *
 * @param connection Active XCB connection
 * @param target     Drawable the button lands on
 * @param gc         Context already set to the button's color
 * @param box        Geometry of the button
 *
 * @note The window coming to rest down there, where its icon goes
 * @note Complexity: @e O(1)
 */
static void s_btn_shape_iconize(xcb_connection_t *connection,
        xcb_drawable_t target, xcb_gcontext_t gc,
        const struct s_btn_box_s *box)
{
    xcb_rectangle_t rect = { box->lo,
        (int16_t) (box->bo - box->in + 1), box->span,
        (uint16_t) box->in };

    xcb_poly_fill_rectangle(connection, target, gc, 1u, &rect);
}


/**
 * @brief Draw the shade button: a bar along the top
 *
 * @param connection Active XCB connection
 * @param target     Drawable the button lands on
 * @param gc         Context already set to the button's color
 * @param box        Geometry of the button
 *
 * @note The mirror of iconize: the window rolling up into its own
 *       titlebar
 * @note Complexity: @e O(1)
 */
static void s_btn_shape_shade(xcb_connection_t *connection,
        xcb_drawable_t target, xcb_gcontext_t gc,
        const struct s_btn_box_s *box)
{
    xcb_rectangle_t rect = { box->lo, box->to, box->span,
        (uint16_t) box->in };

    xcb_poly_fill_rectangle(connection, target, gc, 1u, &rect);
}


/**
 * @brief Draw the hide button: a hollow square struck through
 *
 * @param connection Active XCB connection
 * @param target     Drawable the button lands on
 * @param gc         Context already set to the button's color
 * @param box        Geometry of the button
 *
 * @note A window that is still there and simply not being shown,
 *       which is what the stroke over the outline says
 * @note Complexity: @e O(1)
 */
static void s_btn_shape_hide(xcb_connection_t *connection,
        xcb_drawable_t target, xcb_gcontext_t gc,
        const struct s_btn_box_s *box)
{
    xcb_rectangle_t rect = { box->lo, box->to,
        (uint16_t) (box->span - 1u), (uint16_t) (box->span - 1u) };
    xcb_segment_t seg[1];

    xcb_poly_rectangle(connection, target, gc, 1u, &rect);
    seg[0] = (xcb_segment_t) { box->lo, box->bo, box->hi, box->to };
    xcb_poly_segment(connection, target, gc, 1u, seg);
}


/**
 * @brief Send one button to the function that draws its shape
 *
 * @param connection  Active XCB connection
 * @param target      Drawable the button lands on
 * @param gc          Context already set to the button's color, with
 *                    a line width matching the box's inset
 * @param button      Which button is being drawn
 * @param box         Geometry of the button
 * @param use_symbols Whether @c window.titlebar.buttons.use-symbols
 *                    is on
 *
 * @note The three state-reporting buttons take the plain square
 *       whatever @p use_symbols says: their color is already carrying
 *       the state, and a shape would compete with it
 * @note Complexity: @e O(1)
 */
static void s_desktop_titlebar_button_shape(
        xcb_connection_t *connection, xcb_drawable_t target,
        xcb_gcontext_t gc, enum config_titlebar_button_e button,
        const struct s_btn_box_s *box, bool use_symbols)
{
    if (!use_symbols) {
        s_btn_shape_square(connection, target, gc, box);
        return;
    }

    switch (button) {
    case CONFIG_TITLEBAR_BUTTON_CLOSE:
        s_btn_shape_close(connection, target, gc, box);
        break;
    case CONFIG_TITLEBAR_BUTTON_MAXIMIZE:
        s_btn_shape_maximize(connection, target, gc, box);
        break;
    case CONFIG_TITLEBAR_BUTTON_FULLSCREEN:
        s_btn_shape_fullscreen(connection, target, gc, box);
        break;
    case CONFIG_TITLEBAR_BUTTON_ICONIZE:
        s_btn_shape_iconize(connection, target, gc, box);
        break;
    case CONFIG_TITLEBAR_BUTTON_SHADE:
        s_btn_shape_shade(connection, target, gc, box);
        break;
    case CONFIG_TITLEBAR_BUTTON_HIDE:
        s_btn_shape_hide(connection, target, gc, box);
        break;
    case CONFIG_TITLEBAR_BUTTON_PIN:
    case CONFIG_TITLEBAR_BUTTON_STICKY:
    case CONFIG_TITLEBAR_BUTTON_LAYER:
        s_btn_shape_square(connection, target, gc, box);
        break;
    }
}


/**
 * @brief Work out one button's drawing geometry
 *
 * @param x    Left edge of the button's box
 * @param y    Top edge of the button's box
 * @param side Side of the box
 *
 * @return The geometry every shape function reads
 *
 * @note Complexity: @e O(1)
 */
static struct s_btn_box_s s_desktop_titlebar_button_box(int16_t x,
        int16_t y, uint16_t side)
{
    struct s_btn_box_s box;

    box.x = x;
    box.y = y;
    box.side = side;
    box.in = (int16_t) client_titlebar_button_shape_unit(side);
    box.lo = (int16_t) (x + box.in);
    box.to = (int16_t) (y + box.in);
    box.hi = (int16_t) (x + (int16_t) side - box.in - 1);
    box.bo = (int16_t) (y + (int16_t) side - box.in - 1);
    box.span = (uint16_t) ((int16_t) side - 2 * box.in);

    return box;
}


/**
 * @brief Draw the buttons configured in @c window.titlebar.buttons on
 *        a titlebar window or an off-screen buffer standing in for one
 *
 * Draws exactly the buttons in @p left (before the window title) and
 * @p right (after the window title), at the positions
 * @c client_titlebar_layout already computed for them.
 *
 * The position is never recomputed on by this function on its own, so
 * it can never disagree with the click hit-test, which uses the same
 * computed layout.  The fill color for most buttons is taken from
 * @p theme: @c window.active.color.foreground when @p is_focused is
 * @c true, @c window.inactive.color.foreground otherwise; the pin and
 * layer buttons instead reflect their state (pinned/non-normal layer)
 * regardless of focus; maximize and fullscreen fall back to the
 * background color when @p can_maximize is @c false.
 *
 * @param connection   Active XCB connection
 * @param target       Drawable the buttons land on: the titlebar
 *                     window itself, or an off-screen buffer
 *                     @c render_client_titlebar_repaint_content copies onto
 *                     it in one piece once every button is drawn
 * @param btn_y        Y position every button shares, from
 *                     @c client_titlebar_layout
 * @param title_h      Titlebar height, which the button side is
 *                     derived from
 * @param left         Left-side button layout from
 *                     @c client_titlebar_layout
 * @param left_n       Number of entries in @p left
 * @param right        Right-side button layout from
 *                     @c client_titlebar_layout
 * @param right_n      Number of entries in @p right
 * @param is_focused   Whether the owning client is currently focused
 * @param is_pinned    Whether the owning client has the pin flag set
 * @param is_sticky    Whether the owning client has the sticky flag set
 * @param is_layered   Whether the client layer is above or below normal
 * @param can_maximize Whether the maximize button is enabled
 * @param theme        Pointer to the theme providing button colors
 *
 * @note Complexity: @e O(n), where @e n is @p left_n + @p right_n
 */
static void s_desktop_titlebar_buttons_draw(xcb_connection_t *connection,
        xcb_drawable_t target, int16_t btn_y, uint16_t title_h,
        const struct titlebar_button_layout_s *left, uint8_t left_n,
        const struct titlebar_button_layout_s *right, uint8_t right_n,
        bool is_focused, bool is_pinned, bool is_sticky, bool is_layered,
        bool can_maximize, const struct config_theme_s *theme)
{
    xcb_gcontext_t gc;
    uint32_t color;
    uint32_t gc_values[4];
    uint16_t btn_size;
    bool use_symbols;
    struct s_btn_box_s box;

    /* Button colors have their dedicated theme entry, independent of
     * the titlebar text foreground, so a theme can style one without
     * the other changing to match.
     * Cfr. 'window.titlebar.buttons.color'. */
    uint32_t color_active = (theme != NULL)
        ? theme->window.titlebar.buttons.color.on
        : 0x000000u;
    uint32_t color_inactive = (theme != NULL)
        ? theme->window.titlebar.buttons.color.off
        : 0xFFFFFFu;
    uint32_t bg_fill = (theme != NULL)
        ? ((is_focused)
            ? theme->window.active.color.background
            : theme->window.inactive.color.background)
        : color_active;

    if (connection == NULL || target == XCB_NONE) {
        return;
    }

    /* One context for every button, rather than one created and
     * destroyed per button as before: only the foreground changes
     * between them, and a round trip each way per button is a poor
     * price for that.  The line width and joins are set once here,
     * being the same for every shape. */
    btn_size = client_titlebar_button_size(theme, title_h);
    use_symbols = (theme != NULL)
        ? theme->window.titlebar.buttons.use_symbols : true;
    gc_values[0] = color_active;
    gc_values[1] = (uint32_t) client_titlebar_button_shape_unit(
            btn_size);
    gc_values[2] = XCB_CAP_STYLE_ROUND;
    gc_values[3] = XCB_JOIN_STYLE_ROUND;
    gc = xcb_generate_id(connection);
    xcb_create_gc(connection, gc, target,
            XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH |
            XCB_GC_CAP_STYLE | XCB_GC_JOIN_STYLE, gc_values);

    for (uint8_t i = 0u; i < left_n; ++i) {
        color = s_titlebar_button_color(left[i].button, is_focused,
                is_pinned, is_sticky, is_layered, can_maximize,
                color_active, color_inactive, bg_fill);
        xcb_change_gc(connection, gc, XCB_GC_FOREGROUND, &color);
        box = s_desktop_titlebar_button_box(left[i].x, btn_y,
                btn_size);
        s_desktop_titlebar_button_shape(connection, target, gc,
                left[i].button, &box, use_symbols);
    }

    for (uint8_t i = 0u; i < right_n; ++i) {
        color = s_titlebar_button_color(right[i].button, is_focused,
                is_pinned, is_sticky, is_layered, can_maximize,
                color_active, color_inactive, bg_fill);
        xcb_change_gc(connection, gc, XCB_GC_FOREGROUND, &color);
        box = s_desktop_titlebar_button_box(right[i].x, btn_y,
                btn_size);
        s_desktop_titlebar_button_shape(connection, target, gc,
                right[i].button, &box, use_symbols);
    }

    xcb_free_gc(connection, gc);
}


/**
 * @brief Draw a titlebar's text, clipped to the space the buttons leave
 *        available, honoring the theme's chosen alignment
 *
 * A title too wide for the available space is truncated one character
 * at a time until it fits, rather than letting it draw underneath the
 * right-hand buttons.  Whatever ends up actually drawn is kept in sync
 * with @c _NET_WM_VISIBLE_NAME via @a client_sync_visible_name, so
 * a pager showing the same title has a way to know it no longer matches
 * @c _NET_WM_NAME verbatim.
 *
 * @note Complexity: @e O(n), where @e n is the length of @p text
 */
static void s_titlebar_draw_title(xcb_connection_t *connection,
        client_td *client, xcb_drawable_t target,
        int16_t title_x, uint16_t title_w, int16_t text_y,
        const char *text, enum config_titlebar_alignment_e alignment)
{
    char buf[CONFIG_MAX_LENGTH_NAME];
    uint16_t text_w;
    int16_t draw_x;
    bool can_sync;

    if (connection == NULL || text == NULL || text[0] == '\0' ||
            title_w == 0u) {
        return;
    }

    can_sync = (client != NULL && xcb_ewmh_connection_get() != NULL);

    text_truncate_to_width(buf, sizeof(buf), text, title_w);
    text_w = text_string_measure(buf);

    if (can_sync) {
        client_sync_visible_name(client, client->info.visible_name,
                text, buf, xcb_ewmh_set_wm_visible_name_checked,
                xcb_ewmh_connection_get()->_NET_WM_VISIBLE_NAME);
    }

    if (buf[0] == '\0') {
        return;
    }

    draw_x = title_x;
    if (alignment == CONFIG_TITLEBAR_ALIGN_CENTER && text_w < title_w) {
        draw_x = (int16_t) (title_x + (title_w - text_w) / 2);
    } else if (alignment == CONFIG_TITLEBAR_ALIGN_RIGHT &&
            text_w < title_w) {
        draw_x = (int16_t) (title_x + (title_w - text_w));
    }

    text_draw_string(connection, target, XCB_NONE,
            (struct position_s) { draw_x, text_y }, buf);
}


/* Repaint a titlebar's background, text and buttons */
void render_client_titlebar_repaint_content(xcb_connection_t *connection,
        client_td *client, bool is_focused, uint16_t inner_w,
        uint16_t title_h, const struct config_theme_s *theme)
{
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n;
    uint8_t right_n;
    int16_t title_x;
    uint16_t title_w;
    int16_t btn_y;
    int16_t text_y;
    bool can_maximize;
    bool hide_pin;
    bool hide_sticky;
    const surface_td *surface;
    uint32_t bg_color;
    xcb_pixmap_t buffer;
    xcb_drawable_t target;
    xcb_gcontext_t gc;

    if (connection == NULL || client == NULL || theme == NULL ||
            client->titlebar == 0) {
        return;
    }

    surface = wm_get_surface_by_id(client->screen_id);
    hide_pin = surface != NULL && surface->desktop_count <= 1u;

    /* See the matching comment in 'src/input/mouse/event/titlebar.c'
     * ('s_mouse_hit_titlebar_buttons') for why this checks the pannable
     * viewport size rather than 'desktop_count' */
    hide_sticky = !surface_viewport_has_room(surface);
    bg_color = (is_focused)
        ? theme->window.active.color.background
        : theme->window.inactive.color.background;

    /* Only when the color just chosen is not the one already set.
     * Changing a window's background makes the server discard what is
     * on it, and this repaint runs on every title change: a client that
     * renames itself as the user moves about, which a browser does on
     * each page, would have its titlebar dropped and redrawn each time
     * for a color that never moved. */
    if (!client->layout.has_titlebar_bg ||
            client->layout.titlebar_bg != bg_color) {
        client->layout.titlebar_bg = bg_color;
        client->layout.has_titlebar_bg = true;
        xcb_change_window_attributes(connection,
                client->titlebar, XCB_CW_BACK_PIXEL,
                (const uint32_t[]) { bg_color });
    }

    buffer = (surface != NULL)
        ? xcb_offscreen_buffer_create(connection,
                surface->screen->root_depth, client->titlebar,
                inner_w, title_h)
        : XCB_NONE;
    target = (buffer != XCB_NONE) ? buffer : client->titlebar;

    if (buffer != XCB_NONE) {
        gc = xcb_generate_id(connection);
        xcb_create_gc(connection, gc, buffer, XCB_GC_FOREGROUND,
                &bg_color);
        xcb_poly_fill_rectangle(connection, buffer, gc, 1,
                (const xcb_rectangle_t[]) {
                    { 0, 0, inner_w, title_h }
                });
        xcb_free_gc(connection, gc);
    } else {
        xcb_clear_area(connection, 0, client->titlebar, 0, 0, 0, 0);
    }

    (void) text_renderer_use_font(connection,
            (is_focused)
                ? theme->window.active.font
                : theme->window.inactive.font);
    text_renderer_set_color(
            (is_focused)
                ? theme->window.active.color.foreground
                : theme->window.inactive.color.foreground,
            bg_color);

    client_titlebar_layout(theme, inner_w, title_h, hide_pin,
            hide_sticky, left, &left_n, right, &right_n, &title_x,
            &title_w, &btn_y);

    /* Vertically centered against the titlebar's font ascent and
     * descent, the same way 'client_titlebar_layout' above already
     * centers 'btn_y' against the button size, rather than a fixed
     * pixel offset from the bottom: a fixed offset only happens to look
     * centered for whichever font it was tuned against, and drifts
     * visibly off-center for any other (a restricted-memory session's
     * plain X core font included, since that swap changes the font's
     * ascent/descent without this titlebar's own height changing to
     * match). */
    text_y = (int16_t) (((int16_t) title_h -
                (int16_t) (text_font_ascent() +
                    text_font_descent())) / 2 + text_font_ascent());
    s_titlebar_draw_title(connection, client, target,
            title_x, title_w, text_y, client->info.name,
            theme->window.titlebar.alignment);

    can_maximize = !client_is_fullscreen(client) &&
        (bool) client_is_maximizable(client);
    s_desktop_titlebar_buttons_draw(connection, target,
            btn_y, title_h, left, left_n, right, right_n, is_focused,
            (bool) client_is_pinned(client),
            (bool) client_is_sticky(client),
            (client->properties.layer != CLIENT_LAYER_NORMAL),
            can_maximize, theme);

    if (buffer != XCB_NONE) {
        gc = xcb_generate_id(connection);
        xcb_create_gc(connection, gc, client->titlebar, 0u, NULL);
        xcb_copy_area(connection, buffer, client->titlebar, gc,
                0, 0, 0, 0, inner_w, title_h);
        xcb_free_gc(connection, gc);
        xcb_free_pixmap(connection, buffer);
    }
}
