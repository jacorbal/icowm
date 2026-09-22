/**
 * @file tests/client/test_geom.c
 *
 * @brief Test battery for client geometry helpers with real ICCCM
 *        size-hint arithmetic
 *
 * Exercises 'client_titlebar_layout', 'client_size_constrain' and
 * 'client_aspect_ratio_clamp' (client/geom.c) linked against the
 * real source file, so the size-hint arithmetic under test runs
 * exactly as it does in the real window manager, along with the
 * geometry 'client_send_synthetic_configure_notify' reports.  Nothing
 * else in that file is reached by any test here (no decoration is
 * created, no window is themed live), so every other external symbol
 * 'client/geom.c' calls is a link-only stand-in below, never actually
 * invoked.
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
#include <xcb/xcb_ewmh.h>

/* Local includes */
#include <client.h>
#include <config.h>
#include <harness/tap.h>
#include <wm.h>


/**
 * @brief Link-only stand-in for @a ccmd_client_apply_geometry
 *
 * Reached only by 'client_decoration_layout_sync' and
 * 'ci_create_decorations', neither of which any test here calls.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_apply_geometry(client_td *client,
        xcb_window_t target, uint16_t mask,
        int32_t x, int32_t y, uint32_t w, uint32_t h,
        uint32_t border_width)
{
    (void) client;
    (void) target;
    (void) mask;
    (void) x;
    (void) y;
    (void) w;
    (void) h;
    (void) border_width;
}


/**
 * @brief Link-only stand-in for @a ccmd_publish_frame_extents
 *
 * Reached only by 'ci_create_decorations', which no test here calls.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_publish_frame_extents(client_td *client,
        uint32_t left, uint32_t right, uint32_t top, uint32_t bottom)
{
    (void) client;
    (void) left;
    (void) right;
    (void) top;
    (void) bottom;
}


/**
 * @brief Link-only stand-in for @a wm_request_client_redraw
 *
 * Reached only by 'client_theme_layout_resync', which nothing here
 * calls.
 *
 * @note Complexity: @e O(1)
 */
void wm_request_client_redraw(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a xcb_connection_get
 *
 * Reached only by 'client_decoration_layout_sync' and
 * 'ci_create_decorations', neither of which any test here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_connection_get
 *
 * Reached only by 'ci_create_decorations', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_window_reparent
 *
 * Reached only by 'ci_create_decorations', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void xcb_window_reparent(xcb_window_t window, xcb_window_t parent,
        int16_t x, int16_t y)
{
    (void) window;
    (void) parent;
    (void) x;
    (void) y;
}


/**
 * @brief Link-only stand-in for @a xcb_window_save_set
 *
 * Reached only by 'ci_create_decorations', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void xcb_window_save_set(xcb_window_t window, bool add)
{
    (void) window;
    (void) add;
}


/**
 * @brief Link-only stand-in for @a wm_sync_is_available
 *
 * Reached only by 'client_decoration_layout_sync', which nothing here
 * calls.
 *
 * @note Complexity: @e O(1)
 */
bool wm_sync_is_available(void)
{
    return false;
}


/**
 * @brief Link-only stand-in for @a xcb_clear_area
 *
 * Reached only by 'client_decoration_layout_sync', which nothing here
 * calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_clear_area(xcb_connection_t *connection,
        uint8_t exposures, xcb_window_t window, int16_t x, int16_t y,
        uint16_t width, uint16_t height)
{
    xcb_void_cookie_t cookie;

    (void) connection;
    (void) exposures;
    (void) window;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_generate_id
 *
 * Reached only by 'ci_create_decorations', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
uint32_t xcb_generate_id(xcb_connection_t *connection)
{
    (void) connection;
    return 0u;
}


/**
 * @brief Link-only stand-in for @a xcb_create_window
 *
 * Reached only by 'ci_create_decorations', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_create_window(xcb_connection_t *connection,
        uint8_t depth, xcb_window_t wid, xcb_window_t parent,
        int16_t x, int16_t y, uint16_t width, uint16_t height,
        uint16_t border_width, uint16_t class, xcb_visualid_t visual,
        uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie;

    (void) connection;
    (void) depth;
    (void) wid;
    (void) parent;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    (void) border_width;
    (void) class;
    (void) visual;
    (void) value_mask;
    (void) value_list;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_grab_button
 *
 * Reached only by 'ci_create_decorations', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_grab_button(xcb_connection_t *connection,
        uint8_t owner_events, xcb_window_t grab_window,
        uint16_t event_mask, uint8_t pointer_mode, uint8_t keyboard_mode,
        xcb_window_t confine_to, xcb_cursor_t cursor, uint8_t button,
        uint16_t modifiers)
{
    xcb_void_cookie_t cookie;

    (void) connection;
    (void) owner_events;
    (void) grab_window;
    (void) event_mask;
    (void) pointer_mode;
    (void) keyboard_mode;
    (void) confine_to;
    (void) cursor;
    (void) button;
    (void) modifiers;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_change_property
 *
 * Reached only by 'ci_create_decorations', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_change_property(xcb_connection_t *connection,
        uint8_t mode, xcb_window_t window, xcb_atom_t property,
        xcb_atom_t type, uint8_t format, uint32_t data_len,
        const void *data)
{
    xcb_void_cookie_t cookie;

    (void) connection;
    (void) mode;
    (void) window;
    (void) property;
    (void) type;
    (void) format;
    (void) data_len;
    (void) data;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/** Last event handed to 'xcb_send_event', for the assertions */
static xcb_configure_notify_event_t s_sent_notify;

/**
 * @brief Recording stand-in for @a xcb_send_event
 *
 * Reached only by 'client_send_synthetic_configure_notify'; keeps
 * a copy of the event it was asked to send.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_send_event(xcb_connection_t *connection,
        uint8_t propagate, xcb_window_t destination, uint32_t event_mask,
        const char *event)
{
    xcb_void_cookie_t cookie;

    (void) connection;
    (void) propagate;
    (void) destination;
    (void) event_mask;
    memcpy(&s_sent_notify, event, sizeof(s_sent_notify));
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/* A null theme produces an empty layout rather than dereferencing
 * anything, with every output parameter left at its zeroed default */
static void s_test_titlebar_layout_null_theme_is_empty(void)
{
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n = 99u;
    uint8_t right_n = 99u;
    int16_t title_x = -1;
    uint16_t title_w = 999u;
    int16_t btn_y = -1;

    client_titlebar_layout(NULL, 300u, 22u, false, false, left,
            &left_n, right, &right_n, &title_x, &title_w, &btn_y);

    TAP_EQ_INT(left_n, 0, "a null theme reports zero left buttons");
    TAP_EQ_INT(right_n, 0, "a null theme reports zero right buttons");
    TAP_EQ_INT(title_x, 0, "a null theme reports title_x reset to zero");
    TAP_EQ_INT(title_w, 0, "a null theme reports title_w reset to zero");
    TAP_EQ_INT(btn_y, 0, "a null theme reports btn_y reset to zero");
}


/* A theme with no buttons on either side leaves the whole frame width
 * (minus the plain edge padding) to the title */
static void s_test_titlebar_layout_no_buttons_full_title_width(void)
{
    struct config_theme_s theme;
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n;
    uint8_t right_n;
    int16_t title_x;
    uint16_t title_w;
    int16_t btn_y;

    memset(&theme, 0, sizeof(theme));
    theme.window.titlebar.buttons.left_count = 0u;
    theme.window.titlebar.buttons.right_count = 0u;
    theme.window.titlebar.padding.horizontal = 4u;
    theme.window.titlebar.padding.vertical = 4u;

    client_titlebar_layout(&theme, 200u, 22u, false, false, left,
            &left_n, right, &right_n, &title_x, &title_w, &btn_y);

    TAP_EQ_INT(left_n, 0, "no configured buttons: zero on the left");
    TAP_EQ_INT(right_n, 0, "no configured buttons: zero on the right");
    TAP_EQ_INT(title_x, 4, "title starts right after the left padding");
    TAP_EQ_INT(title_w, 192,
            "title spans the frame minus both edge paddings");
}


/* One left and one right button each reserve their own slot, and the
 * title span shrinks to fit between them plus the extra button gap */
static void s_test_titlebar_layout_one_button_each_side(void)
{
    struct config_theme_s theme;
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n;
    uint8_t right_n;
    int16_t title_x;
    uint16_t title_w;
    int16_t btn_y;

    memset(&theme, 0, sizeof(theme));
    theme.window.titlebar.buttons.left_count = 1u;
    theme.window.titlebar.buttons.left[0] = CONFIG_TITLEBAR_BUTTON_PIN;
    theme.window.titlebar.buttons.right_count = 1u;
    theme.window.titlebar.buttons.right[0] = CONFIG_TITLEBAR_BUTTON_CLOSE;
    theme.window.titlebar.padding.horizontal = 4u;
    theme.window.titlebar.padding.vertical = 4u;

    client_titlebar_layout(&theme, 200u, 22u, false, false, left,
            &left_n, right, &right_n, &title_x, &title_w, &btn_y);

    TAP_EQ_INT(left_n, 1, "one configured left button is placed");
    TAP_EQ_INT(left[0].button, CONFIG_TITLEBAR_BUTTON_PIN,
            "the placed left button is the one the theme names");
    TAP_EQ_INT(left[0].x, 4,
            "the left button sits right at the horizontal padding");
    TAP_EQ_INT(right_n, 1, "one configured right button is placed");
    TAP_EQ_INT(right[0].button, CONFIG_TITLEBAR_BUTTON_CLOSE,
            "the placed right button is the one the theme names");
    TAP_OK(title_x > 4,
            "the title's left edge is pushed right of the left button");
    TAP_OK(title_w < 192,
            "the title span shrinks to make room for both buttons");
}


/**
 * @brief Lay out a six-button titlebar of the given width
 *
 * The same theme every narrowing scenario below uses: the three
 * state buttons on the left, the three that act on the window on the
 * right, which is how a titlebar is usually arranged.
 *
 * @param titlebar_w Width to lay the row out in
 * @param left       Receives the left row
 * @param left_n     Receives its length
 * @param right      Receives the right row
 * @param right_n    Receives its length
 *
 * @note Complexity: @e O(1)
 */
static void s_layout_six(uint16_t titlebar_w,
        struct titlebar_button_layout_s *left, uint8_t *left_n,
        struct titlebar_button_layout_s *right, uint8_t *right_n)
{
    struct config_theme_s theme;
    int16_t title_x;
    uint16_t title_w;
    int16_t btn_y;

    memset(&theme, 0, sizeof(theme));
    theme.window.titlebar.padding.horizontal = 4u;
    theme.window.titlebar.padding.vertical = 4u;
    theme.window.titlebar.buttons.size =
        (uint16_t) WM_DECOR_BTN_SIZE_DEFAULT;
    theme.window.titlebar.buttons.left_count = 3u;
    theme.window.titlebar.buttons.left[0] = CONFIG_TITLEBAR_BUTTON_LAYER;
    theme.window.titlebar.buttons.left[1] = CONFIG_TITLEBAR_BUTTON_PIN;
    theme.window.titlebar.buttons.left[2] = CONFIG_TITLEBAR_BUTTON_SHADE;
    theme.window.titlebar.buttons.right_count = 3u;
    theme.window.titlebar.buttons.right[0] =
        CONFIG_TITLEBAR_BUTTON_ICONIZE;
    theme.window.titlebar.buttons.right[1] =
        CONFIG_TITLEBAR_BUTTON_MAXIMIZE;
    theme.window.titlebar.buttons.right[2] = CONFIG_TITLEBAR_BUTTON_CLOSE;

    client_titlebar_layout(&theme, titlebar_w, 22u, false, false,
            left, left_n, right, right_n, &title_x, &title_w, &btn_y);
}


/* Whatever the width, the two button groups never reach into each
 * other: before this they simply overlapped, the right group walking
 * back past the left one and, on a narrow enough titlebar, past its
 * left edge entirely */
static void s_test_titlebar_layout_groups_never_overlap(void)
{
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n;
    uint8_t right_n;
    bool all_clear = true;

    for (uint16_t w = 1u; w <= 300u; ++w) {
        s_layout_six(w, left, &left_n, right, &right_n);

        for (uint8_t i = 0u; i < left_n; ++i) {
            if (left[i].x < 0) {
                all_clear = false;
            }
        }
        for (uint8_t i = 0u; i < right_n; ++i) {
            if (right[i].x < 0) {
                all_clear = false;
            }
        }
        /* Not merely non-overlapping: two groups meeting at the
         * same pixel read as one row, and any separation short of
         * the gap kept between adjacent buttons reads as a mistake */
        if (left_n > 0u && right_n > 0u) {
            int32_t left_end = left[left_n - 1u].x +
                (int32_t) WM_DECOR_BTN_SIZE_DEFAULT;
            int32_t right_start = right[right_n - 1u].x;

            if (right_start - left_end < (int32_t) WM_DECOR_BTN_GAP) {
                all_clear = false;
            }
        }
    }

    TAP_OK(all_clear,
            "at no width from 1 to 300 does a button sit off the left"
            " edge, reach into the other group, or come closer to it"
            " than the gap between adjacent buttons");
}


/* The size comes from the theme, not from the titlebar height:
 * making the bar taller is a decision about the bar, and a theme that
 * wanted larger buttons with it would have had them appear without
 * asking for them */
static void s_test_titlebar_button_size_comes_from_theme(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.window.titlebar.buttons.size =
        (uint16_t) WM_DECOR_BTN_SIZE_DEFAULT;

    TAP_EQ_INT((int) client_titlebar_button_size(&theme, 22u), 12,
            "a theme asking for twelve gets twelve");
    TAP_EQ_INT((int) client_titlebar_button_size(&theme, 60u), 12,
            "and still twelve on a titlebar nearly three times as"
            " tall, the height having no say in it");

    theme.window.titlebar.buttons.size = 20u;
    TAP_EQ_INT((int) client_titlebar_button_size(&theme, 40u), 20,
            "a theme asking for twenty gets twenty");
    TAP_EQ_INT((int) client_titlebar_button_size(&theme, 16u), 14,
            "held to two less than the titlebar, so a button always"
            " leaves a pixel of bar above and below it");

    theme.window.titlebar.buttons.size = 1u;
    TAP_EQ_INT((int) client_titlebar_button_size(&theme, 40u), 6,
            "and never below six, under which the inset and the"
            " stroke leave nothing between them");

    theme.window.titlebar.buttons.size = 21u;
    TAP_EQ_INT((int) client_titlebar_button_size(&theme, 40u) % 2, 0,
            "the side is always even, so an even stroke lands"
            " centered rather than on half pixels");

    TAP_EQ_INT((int) client_titlebar_button_size(NULL, 40u), 12,
            "with no theme at all the default is what comes back");
}


/* Inset and stroke keep their proportion to the button as it grows:
 * a two pixel stroke on a twenty-four pixel button would look thin
 * where it looks right on a twelve pixel one */
static void s_test_titlebar_button_shape_unit_scales(void)
{
    TAP_EQ_INT((int) client_titlebar_button_shape_unit(12u), 2,
            "the default button keeps the two pixels the shapes were"
            " drawn for");
    TAP_EQ_INT((int) client_titlebar_button_shape_unit(24u), 4,
            "twice the button gives twice the stroke");
    TAP_EQ_INT((int) client_titlebar_button_shape_unit(6u), 2,
            "and nothing ever falls to a single pixel, which all but"
            " disappears against a patterned titlebar");
}


/* An uneven split is where the two groups came closest: with one
 * button on the left and four on the right they met at the very same
 * pixel, the fit test counting them as fitting the moment they
 * touched */
static void s_test_titlebar_layout_uneven_split_stays_apart(void)
{
    struct config_theme_s theme;
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n;
    uint8_t right_n;
    int16_t title_x;
    uint16_t title_w;
    int16_t btn_y;
    bool all_clear = true;

    memset(&theme, 0, sizeof(theme));
    theme.window.titlebar.padding.horizontal = 4u;
    theme.window.titlebar.padding.vertical = 4u;
    theme.window.titlebar.buttons.size =
        (uint16_t) WM_DECOR_BTN_SIZE_DEFAULT;
    theme.window.titlebar.buttons.left_count = 1u;
    theme.window.titlebar.buttons.left[0] = CONFIG_TITLEBAR_BUTTON_LAYER;
    theme.window.titlebar.buttons.right_count = 4u;
    theme.window.titlebar.buttons.right[0] =
        CONFIG_TITLEBAR_BUTTON_ICONIZE;
    theme.window.titlebar.buttons.right[1] = CONFIG_TITLEBAR_BUTTON_SHADE;
    theme.window.titlebar.buttons.right[2] =
        CONFIG_TITLEBAR_BUTTON_MAXIMIZE;
    theme.window.titlebar.buttons.right[3] = CONFIG_TITLEBAR_BUTTON_CLOSE;

    for (uint16_t w = 1u; w <= 300u; ++w) {
        client_titlebar_layout(&theme, w, 22u, false, false, left,
                &left_n, right, &right_n, &title_x, &title_w, &btn_y);

        if (left_n > 0u && right_n > 0u) {
            int32_t left_end = left[left_n - 1u].x +
                (int32_t) WM_DECOR_BTN_SIZE_DEFAULT;

            if (right[right_n - 1u].x - left_end <
                    (int32_t) WM_DECOR_BTN_GAP) {
                all_clear = false;
            }
        }
    }

    TAP_OK(all_clear,
            "one button against four keeps the two groups apart at"
            " every width");
}


/* Buttons are given up least valuable first, and close outlives every
 * other one however narrow the titlebar gets */
static void s_test_titlebar_layout_gives_up_least_valuable_first(void)
{
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n;
    uint8_t right_n;

    s_layout_six(200u, left, &left_n, right, &right_n);
    TAP_EQ_INT(left_n + right_n, 6,
            "a wide titlebar keeps every configured button");

    /* Room for five: 'layer' is the first to go, and it is on the
     * left, so the left row is the one that shortens */
    s_layout_six(92u, left, &left_n, right, &right_n);
    TAP_EQ_INT(left_n, 2, "the first button given up comes off the left");
    TAP_EQ_INT(left[0].button, CONFIG_TITLEBAR_BUTTON_PIN,
            "and it is 'layer', the least valuable of the six");

    /* Room for one: only 'close' is left, on the right, even though
     * the left row had buttons of its own */
    s_layout_six(24u, left, &left_n, right, &right_n);
    TAP_EQ_INT(left_n, 0, "a titlebar with room for one keeps no left"
            " button");
    TAP_EQ_INT(right_n, 1, "and exactly one on the right");
    TAP_EQ_INT(right[0].button, CONFIG_TITLEBAR_BUTTON_CLOSE,
            "which is 'close', outliving every button on either side");
}


/* Widening brings them back, in the reverse order they went: the
 * layout is a function of the width alone, with nothing remembered */
static void s_test_titlebar_layout_restores_on_widening(void)
{
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n;
    uint8_t right_n;
    uint8_t narrow_total;
    uint8_t wide_total;

    s_layout_six(40u, left, &left_n, right, &right_n);
    narrow_total = (uint8_t) (left_n + right_n);

    s_layout_six(200u, left, &left_n, right, &right_n);
    wide_total = (uint8_t) (left_n + right_n);

    TAP_OK(narrow_total < wide_total,
            "a narrow titlebar holds fewer buttons than a wide one");

    s_layout_six(40u, left, &left_n, right, &right_n);
    TAP_EQ_INT(left_n + right_n, (int) narrow_total,
            "and narrowing again gives exactly the same row back,"
            " nothing having been remembered in between");
}


/* A theme claiming more buttons than 'CONFIG_MAX_TITLEBAR_BUTTONS'
 * on one side is clamped rather than reading past its own fixed
 * array */
static void s_test_titlebar_layout_clamps_button_count(void)
{
    struct config_theme_s theme;
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n;
    uint8_t right_n;
    int16_t title_x;
    uint16_t title_w;
    int16_t btn_y;

    memset(&theme, 0, sizeof(theme));
    /* A value larger than the array the theme itself declares can
     * hold; the function must never trust it blindly */
    theme.window.titlebar.buttons.left_count =
        (uint8_t) (CONFIG_MAX_TITLEBAR_BUTTONS + 5u);
    for (unsigned int i = 0u; i < CONFIG_MAX_TITLEBAR_BUTTONS; ++i) {
        theme.window.titlebar.buttons.left[i] =
            CONFIG_TITLEBAR_BUTTON_ICONIZE;
    }

    client_titlebar_layout(&theme, 400u, 22u, false, false, left,
            &left_n, right, &right_n, &title_x, &title_w, &btn_y);

    TAP_EQ_INT(left_n, CONFIG_MAX_TITLEBAR_BUTTONS,
            "the left button count is clamped to the fixed array size,"
            " never reading past it");
}


/* A pin button is skipped entirely when 'hide_pin' is set, closing
 * the gap for whatever follows it rather than leaving a blank slot */
static void s_test_titlebar_layout_hide_pin_skips_it(void)
{
    struct config_theme_s theme;
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n;
    uint8_t right_n;
    int16_t title_x;
    uint16_t title_w;
    int16_t btn_y;

    memset(&theme, 0, sizeof(theme));
    theme.window.titlebar.buttons.left_count = 2u;
    theme.window.titlebar.buttons.left[0] = CONFIG_TITLEBAR_BUTTON_PIN;
    theme.window.titlebar.buttons.left[1] = CONFIG_TITLEBAR_BUTTON_LAYER;

    client_titlebar_layout(&theme, 300u, 22u, true, false, left,
            &left_n, right, &right_n, &title_x, &title_w, &btn_y);

    TAP_EQ_INT(left_n, 1,
            "hide_pin drops the pin button, leaving only one placed");
    TAP_EQ_INT(left[0].button, CONFIG_TITLEBAR_BUTTON_LAYER,
            "the surviving button is the one after pin, shifted into"
            " pin's own slot");
}


/* A sticky button is skipped entirely when 'hide_sticky' is set,
 * closing the gap for whatever follows it rather than leaving a blank
 * slot, the same way 'hide_pin' does for the pin button */
static void s_test_titlebar_layout_hide_sticky_skips_it(void)
{
    struct config_theme_s theme;
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n;
    uint8_t right_n;
    int16_t title_x;
    uint16_t title_w;
    int16_t btn_y;

    memset(&theme, 0, sizeof(theme));
    theme.window.titlebar.buttons.left_count = 2u;
    theme.window.titlebar.buttons.left[0] = CONFIG_TITLEBAR_BUTTON_STICKY;
    theme.window.titlebar.buttons.left[1] = CONFIG_TITLEBAR_BUTTON_LAYER;

    client_titlebar_layout(&theme, 300u, 22u, false, true, left,
            &left_n, right, &right_n, &title_x, &title_w, &btn_y);

    TAP_EQ_INT(left_n, 1,
            "hide_sticky drops the sticky button, leaving only one"
            " placed");
    TAP_EQ_INT(left[0].button, CONFIG_TITLEBAR_BUTTON_LAYER,
            "the surviving button is the one after sticky, shifted"
            " into sticky's own slot");
}


/* Vertical centering falls back to plain, uninset centering when the
 * configured vertical padding alone would not leave room for a full
 * button, rather than producing a negative button Y */
static void s_test_titlebar_layout_falls_back_when_padding_too_tall(void)
{
    struct config_theme_s theme;
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n;
    uint8_t right_n;
    int16_t title_x;
    uint16_t title_w;
    int16_t btn_y;

    memset(&theme, 0, sizeof(theme));
    /* 'title_h' of 10 leaves only 10 - 2*8 = -6 once inset by the
     * configured vertical padding, less than 'WM_DECOR_BTN_SIZE_DEFAULT' */
    theme.window.titlebar.padding.vertical = 8u;

    client_titlebar_layout(&theme, 300u, 10u, false, false, left,
            &left_n, right, &right_n, &title_x, &title_w, &btn_y);

    TAP_OK(btn_y >= 0,
            "an oversized vertical padding never produces a negative"
            " button Y");
}


/* A width and height inside every ICCCM bound (no min, max or
 * increment binds) pass through completely untouched */
static void s_test_size_constrain_no_hints_passes_through(void)
{
    client_td *client = calloc(1, sizeof(*client));
    uint32_t width = 640u;
    uint32_t height = 480u;

    client->hints_icccm.size.is_valid = false;

    client_size_constrain(client, &width, &height);

    TAP_EQ_INT((long) width, 640,
            "with no valid hints, width passes through unchanged");
    TAP_EQ_INT((long) height, 480,
            "with no valid hints, height passes through unchanged");

    free(client);
}


/* With no valid hints at all, a request smaller than the absolute
 * floor is still raised to 'WM_MIN_WINDOW_DIMENSION' */
static void s_test_size_constrain_no_hints_floors_to_minimum(void)
{
    client_td *client = calloc(1, sizeof(*client));
    uint32_t width = 1u;
    uint32_t height = 1u;

    client->hints_icccm.size.is_valid = false;

    client_size_constrain(client, &width, &height);

    TAP_EQ_INT((long) width, (long) WM_MIN_WINDOW_DIMENSION,
            "width below the absolute floor is raised to it");
    TAP_EQ_INT((long) height, (long) WM_MIN_WINDOW_DIMENSION,
            "height below the absolute floor is raised to it");

    free(client);
}


/* A client's declared minimum size wins over a smaller request */
static void s_test_size_constrain_clamps_to_min(void)
{
    client_td *client = calloc(1, sizeof(*client));
    uint32_t width = 50u;
    uint32_t height = 50u;

    client->hints_icccm.size.is_valid = true;
    client->hints_icccm.size.min.w = 200u;
    client->hints_icccm.size.min.h = 150u;

    client_size_constrain(client, &width, &height);

    TAP_EQ_INT((long) width, 200,
            "a request below the declared minimum is raised to it");
    TAP_EQ_INT((long) height, 150,
            "a request below the declared minimum is raised to it");

    free(client);
}


/* A client's declared maximum size wins over a larger request */
static void s_test_size_constrain_clamps_to_max(void)
{
    client_td *client = calloc(1, sizeof(*client));
    uint32_t width = 5000u;
    uint32_t height = 5000u;

    client->hints_icccm.size.is_valid = true;
    client->hints_icccm.size.max.w = 1024u;
    client->hints_icccm.size.max.h = 768u;

    client_size_constrain(client, &width, &height);

    TAP_EQ_INT((long) width, 1024,
            "a request above the declared maximum is lowered to it");
    TAP_EQ_INT((long) height, 768,
            "a request above the declared maximum is lowered to it");

    free(client);
}


/* A client declaring a resize-increment grid snaps a request down to
 * the nearest whole increment above its base size */
static void s_test_size_constrain_snaps_to_increment_grid(void)
{
    client_td *client = calloc(1, sizeof(*client));
    uint32_t width = 95u;
    uint32_t height = 45u;

    client->hints_icccm.size.is_valid = true;
    client->hints_icccm.size.base.w = 10u;
    client->hints_icccm.size.base.h = 5u;
    client->hints_icccm.size.inc.w = 10u;
    client->hints_icccm.size.inc.h = 10u;

    client_size_constrain(client, &width, &height);

    /* 95: over base (10) is 85, floored to the nearest multiple of 10
     * is 80, so the snapped width is 10 + 80 = 90 */
    TAP_EQ_INT((long) width, 90,
            "width snaps down to the nearest whole increment above"
            " base");
    /* 45: over base (5) is 40, already an exact multiple of 10, so
     * the snapped height is 5 + 40 = 45 (unchanged) */
    TAP_EQ_INT((long) height, 45,
            "height already on the grid is left as it is");

    free(client);
}


/* Passing a null client, width or height pointer is a no-op, never a
 * crash */
static void s_test_size_constrain_null_args_are_a_no_op(void)
{
    client_td *client = calloc(1, sizeof(*client));
    uint32_t width = 42u;
    uint32_t height = 42u;

    client_size_constrain(NULL, &width, &height);
    client_size_constrain(client, NULL, &height);
    client_size_constrain(client, &width, NULL);

    TAP_EQ_INT((long) width, 42,
            "a null client or dimension pointer leaves width untouched");
    TAP_EQ_INT((long) height, 42,
            "a null client or dimension pointer leaves height"
            " untouched");

    free(client);
}


/* With no aspect-ratio bound declared, height is left exactly as
 * given */
static void s_test_aspect_ratio_clamp_no_bound_is_a_no_op(void)
{
    client_td *client = calloc(1, sizeof(*client));
    uint32_t height = 300u;

    client->hints_icccm.size.is_valid = true;

    client_aspect_ratio_clamp(client, 400u, &height);

    TAP_EQ_INT((long) height, 300,
            "with no aspect bound declared, height is untouched");

    free(client);
}


/* A width/height pair narrower than the minimum aspect ratio is
 * widened by growing height down to the ratio's own floor */
static void s_test_aspect_ratio_clamp_enforces_minimum(void)
{
    client_td *client = calloc(1, sizeof(*client));
    uint32_t height = 400u;

    client->hints_icccm.size.is_valid = true;
    /* Minimum ratio 1:1 (never taller than it is wide) */
    client->hints_icccm.size.aspect.min.num = 1;
    client->hints_icccm.size.aspect.min.den = 1;

    client_aspect_ratio_clamp(client, 200u, &height);

    TAP_EQ_INT((long) height, 200,
            "height above the 1:1 minimum ratio is lowered to match"
            " the width");

    free(client);
}


/* A width/height pair beyond the maximum aspect ratio is corrected by
 * shrinking height up to the ratio's own ceiling */
static void s_test_aspect_ratio_clamp_enforces_maximum(void)
{
    client_td *client = calloc(1, sizeof(*client));
    uint32_t height = 50u;

    client->hints_icccm.size.is_valid = true;
    /* Maximum ratio 2:1 (never more than twice as wide as tall) */
    client->hints_icccm.size.aspect.max.num = 2;
    client->hints_icccm.size.aspect.max.den = 1;

    client_aspect_ratio_clamp(client, 400u, &height);

    TAP_EQ_INT((long) height, 200,
            "height below the 2:1 maximum ratio is raised to match"
            " the width");

    free(client);
}


/* Passing a null client or height pointer is a no-op, never a crash */
static void s_test_aspect_ratio_clamp_null_args_are_a_no_op(void)
{
    client_td *client = calloc(1, sizeof(*client));
    uint32_t height = 77u;

    client->hints_icccm.size.is_valid = true;
    client->hints_icccm.size.aspect.min.num = 1;
    client->hints_icccm.size.aspect.min.den = 1;

    client_aspect_ratio_clamp(NULL, 400u, &height);
    client_aspect_ratio_clamp(client, 400u, NULL);

    TAP_EQ_INT((long) height, 77,
            "a null client or height pointer leaves height untouched");

    free(client);
}


/* A frameless window reports the outer corner of its native border
 * together with that border's width, so the client adds it back and
 * finds its content where it really is; a framed one has none */
static void s_test_synthetic_configure_notify_border(void)
{
    client_td *client = calloc(1, sizeof(*client));

    client->window = 0x500u;
    client->layout.geometry.cur.pos.x = 300;
    client->layout.geometry.cur.pos.y = 250;
    client->layout.geometry.cur.dim.w = 400u;
    client->layout.geometry.cur.dim.h = 300u;
    client->last_border_width = 6u;
    memset(&s_sent_notify, 0, sizeof(s_sent_notify));

    client_send_synthetic_configure_notify((xcb_connection_t *) 1, client);

    TAP_OK(s_sent_notify.x == 300 && s_sent_notify.y == 250 &&
            s_sent_notify.border_width == 6u,
            "a frameless window reports its border's outer corner and"
            " the 6 pixel border it has");

    client->frame = 0x501u;
    client->properties.flags = (uint32_t) CLIENT_FLAG_DECORATED;
    client->layout.frame_extents.left = 6;
    client->layout.frame_extents.right = 6;
    client->layout.frame_extents.top = 28;
    client->layout.frame_extents.bottom = 6;
    memset(&s_sent_notify, 0, sizeof(s_sent_notify));

    client_send_synthetic_configure_notify((xcb_connection_t *) 1, client);

    TAP_OK(s_sent_notify.x == 306 && s_sent_notify.y == 278 &&
            s_sent_notify.border_width == 0u,
            "a framed window reports its content's corner and no"
            " border, whatever width it had before being framed");

    free(client);
}


/* Rebasing moves both stored positions by what the border loses or
 * gains, and leaves a framed client or an unmeasured one alone */
static void s_test_native_border_rebase(void)
{
    client_td *client = calloc(1, sizeof(*client));

    client->window = 0x600u;
    client->layout.geometry.cur.pos.x = 300;
    client->layout.geometry.cur.pos.y = 250;
    client->layout.geometry.old.pos.x = 100;
    client->layout.geometry.old.pos.y = 80;
    client->last_border_width = 6u;

    client_native_border_rebase(client, 2u);

    TAP_OK(client->layout.geometry.cur.pos.x == 304 &&
            client->layout.geometry.cur.pos.y == 254 &&
            client->layout.geometry.old.pos.x == 104 &&
            client->layout.geometry.old.pos.y == 84,
            "a border going from 6 to 2 moves both stored corners 4"
            " pixels in, leaving the content where it was");

    client->last_border_width = UINT32_MAX;
    client_native_border_rebase(client, 6u);

    TAP_OK(client->layout.geometry.cur.pos.x == 304 &&
            client->layout.geometry.old.pos.x == 104,
            "with no border ever sent, nothing is moved");

    client->last_border_width = 2u;
    client->frame = 0x601u;
    client->properties.flags = (uint32_t) CLIENT_FLAG_DECORATED;
    client_native_border_rebase(client, 6u);

    TAP_OK(client->layout.geometry.cur.pos.x == 304 &&
            client->layout.geometry.old.pos.x == 104,
            "a framed client's positions are the frame's, never moved");

    free(client);
}


int main(void)
{
    TAP_PLAN(63);

    s_test_titlebar_layout_null_theme_is_empty();
    s_test_titlebar_layout_no_buttons_full_title_width();
    s_test_titlebar_layout_one_button_each_side();
    s_test_titlebar_button_size_comes_from_theme();
    s_test_titlebar_button_shape_unit_scales();
    s_test_titlebar_layout_groups_never_overlap();
    s_test_titlebar_layout_uneven_split_stays_apart();
    s_test_titlebar_layout_gives_up_least_valuable_first();
    s_test_titlebar_layout_restores_on_widening();
    s_test_titlebar_layout_clamps_button_count();
    s_test_titlebar_layout_hide_pin_skips_it();
    s_test_titlebar_layout_hide_sticky_skips_it();
    s_test_titlebar_layout_falls_back_when_padding_too_tall();
    s_test_size_constrain_no_hints_passes_through();
    s_test_size_constrain_no_hints_floors_to_minimum();
    s_test_size_constrain_clamps_to_min();
    s_test_size_constrain_clamps_to_max();
    s_test_size_constrain_snaps_to_increment_grid();
    s_test_size_constrain_null_args_are_a_no_op();
    s_test_aspect_ratio_clamp_no_bound_is_a_no_op();
    s_test_aspect_ratio_clamp_enforces_minimum();
    s_test_aspect_ratio_clamp_enforces_maximum();
    s_test_aspect_ratio_clamp_null_args_are_a_no_op();
    s_test_synthetic_configure_notify_border();
    s_test_native_border_rebase();

    return TAP_DONE();
}
