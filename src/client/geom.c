/**
 * @file client/geom.c
 *
 * @brief Client geometry and decoration layout helpers
 *
 * Covers three closely related concerns that all operate on the
 * @c layout sub-struct of a @c client_td:
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>     /* NULL, malloc */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Utils includes */
#include <utils/safe/safemem.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>

/* Default initial values */
#include <defs/config.h>
#include <defs/client.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <wm.h>

/* Command includes */
#include <cmds/client/move.h>

/* Local includes */
#include <client/internal.h>


/* Allocate and zero all heap string buffers for a client */
int ci_alloc_strings(client_td *client)
{
    client->info.name = malloc(CONFIG_MAX_LENGTH_NAME);
    client->info.visible_name = malloc(CONFIG_MAX_LENGTH_NAME);
    client->info.role_name = malloc(CONFIG_MAX_LENGTH_NAME);
    client->info.class_name[0] = malloc(CONFIG_MAX_LENGTH_NAME);
    client->info.class_name[1] = malloc(CONFIG_MAX_LENGTH_NAME);
    client->icon_info.icon_name = malloc(CONFIG_MAX_LENGTH_NAME);
    client->icon_info.visible_icon_name = malloc(CONFIG_MAX_LENGTH_NAME);

    if (client->info.name == NULL ||
            client->info.visible_name == NULL ||
            client->info.role_name == NULL ||
            client->info.class_name[0] == NULL ||
            client->info.class_name[1] == NULL ||
            client->icon_info.icon_name == NULL ||
            client->icon_info.visible_icon_name == NULL) {
        safe_free((void **) &client->info.name);
        safe_free((void **) &client->info.visible_name);
        safe_free((void **) &client->info.role_name);
        safe_free((void **) &client->info.class_name[0]);
        safe_free((void **) &client->info.class_name[1]);
        safe_free((void **) &client->icon_info.icon_name);
        safe_free((void **) &client->icon_info.visible_icon_name);
        return -1;
    }

    client->info.name[0] = '\0';
    client->info.visible_name[0] = '\0';
    client->info.role_name[0] = '\0';
    client->info.class_name[0][0] = '\0';
    client->info.class_name[1][0] = '\0';
    client->icon_info.icon_name[0] = '\0';
    client->icon_info.visible_icon_name[0] = '\0';
    client->icon_info.icons = NULL;

    return 0;
}


/* Apply decoration defaults from the loaded theme */
void ci_set_decoration_defaults(client_td *client)
{
    uint32_t border_width = 0;
    bool is_decorated;
    bool has_config;

    if (client == NULL) {
        return;
    }

    has_config = client->config != NULL;
    client->title_height = has_config
        ? client->config->theme.window.titlebar.height
        : WM_TITLEBAR_DEFAULT_HEIGHT;
    if (has_config) {
        border_width = client->config->theme.window.active.border.width;
    }

    /* Accessibility: never let the focus indicator go thinner than
     * 'a11y.focus-indicator.min-border-width', regardless of
     * what the theme itself specifies */
    if (has_config &&
            border_width < client->config->a11y.focus_indicator
                .min_border_width) {
        border_width =
            client->config->a11y.focus_indicator.min_border_width;
    }

    /* A theme's 'window.titlebar.height' of 0 is equivalent to
     * 'window.is-decorated: false': a titlebar with no height has
     * nothing to draw and nowhere to put its buttons, so there is no
     * point pretending the window is still decorated just because the
     * theme's 'is-decorated' flag itself was left (or set) to true. */
    is_decorated = has_config && client->config->theme.window.is_decorated
        && client->title_height > 0u;

    if (is_decorated) {
        client_decorate(client);
        client->layout.frame_extents.left = (int32_t) border_width;
        client->layout.frame_extents.right = (int32_t) border_width;
        client->layout.frame_extents.top =
            (int32_t) (border_width + client->title_height);
        client->layout.frame_extents.bottom = (int32_t) border_width;
    } else {
        client_undecorate(client);
        client->layout.frame_extents = (struct sides_s) {0, 0, 0, 0};
    }
}


/* Update a decorated client's border width and titlebar height to match
 * the current theme and focus state */
void client_theme_layout_resync(client_td *client, bool is_active)
{
    uint32_t new_border;
    uint32_t new_title_height;
    int32_t inner_x;
    int32_t inner_y;
    int32_t inner_w;
    int32_t inner_h;

    if (client == NULL || client->config == NULL ||
            !client_is_decorated(client) || client->frame == 0 ||
            client_is_fullscreen(client)) {
        return;
    }

    new_border = (is_active)
        ? client->config->theme.window.active.border.width
        : client->config->theme.window.inactive.border.width;
    new_title_height = client->config->theme.window.titlebar.height;

    /* Accessibility: never let the focus indicator go thinner than
     * 'a11y.focus-indicator.min-border-width', regardless of
     * what the theme itself specifies */
    if (new_border < client->config->a11y.focus_indicator
                .min_border_width) {
        new_border = client->config->a11y.focus_indicator.min_border_width;
    }

    /* 'left' alone is enough to detect "border width unchanged":
     * 'left', 'right', and 'bottom' are always set equal to each other
     * by 'ci_set_decoration_defaults' and by this same function */
    if (new_border == (uint32_t) client->layout.frame_extents.left &&
            new_title_height == client->title_height) {
        return;
    }

    /* Recover the content window's true on-screen position and size
     * (the one invariant across any border/titlebar change) from the
     * OLD frame extents still in effect, before either of them is
     * touched below.  Every field this function sets is then rebuilt
     * from this recovered pair alone, never incrementally from the
     * previous frame geometry the way an applied "delta" would (the
     * frame growing or shrinking "by" some difference).  Anchoring each
     * call to the content's fixed truth instead of the prior call's
     * own output leaves no way for any small per-call error to compound
     * across repeated toggles. */
    inner_x = client->layout.geometry.cur.pos.x +
        (int32_t) client->layout.frame_extents.left;
    inner_y = client->layout.geometry.cur.pos.y +
        (int32_t) client->layout.frame_extents.top;
    inner_w = (int32_t) client->layout.geometry.cur.dim.w -
        (int32_t) client->layout.frame_extents.left -
        (int32_t) client->layout.frame_extents.right;
    inner_h = (int32_t) client->layout.geometry.cur.dim.h -
        (int32_t) client->layout.frame_extents.top -
        (int32_t) client->layout.frame_extents.bottom;

    client->title_height = new_title_height;
    client->layout.frame_extents.left = (int32_t) new_border;
    client->layout.frame_extents.right = (int32_t) new_border;
    client->layout.frame_extents.bottom = (int32_t) new_border;
    client->layout.frame_extents.top =
        (int32_t) (new_border + new_title_height);

    client->layout.geometry.cur.pos.x = inner_x - (int32_t) new_border;
    client->layout.geometry.cur.pos.y =
        inner_y - (int32_t) client->layout.frame_extents.top;
    client->layout.geometry.cur.dim.w =
        (uint32_t) (inner_w + 2 * (int32_t) new_border);
    client->layout.geometry.cur.dim.h =
        (uint32_t) (inner_h + (int32_t) client->layout.frame_extents.top +
                (int32_t) new_border);

    /* The render pass picks this client up from here: it applies
     * 'geometry.cur' to the frame via 'xcb_configure_window' and then
     * calls 'client_decoration_layout_sync' (below) to reposition the
     * content window and titlebar to match the new 'frame_extents'; the
     * exact same sequence any other geometry change already goes
     * through, so there is nothing further to duplicate here. */
    wm_request_client_redraw(client);
}


/* Compute where every configured titlebar button goes */
void client_titlebar_layout(const struct config_theme_s *theme,
        uint16_t frame_w, uint16_t title_h, bool hide_pin,
        struct titlebar_button_layout_s *restrict out_left,
        uint8_t *restrict out_left_n,
        struct titlebar_button_layout_s *restrict out_right,
        uint8_t *restrict out_right_n,
        int16_t *restrict out_title_x, uint16_t *restrict out_title_w,
        int16_t *restrict out_btn_y)
{
    uint16_t btn = (uint16_t) WM_DECOR_BTN_SIZE;
    uint16_t gap = (uint16_t) WM_DECOR_BTN_GAP;
    uint16_t pad_h;
    uint16_t pad_v;
    uint8_t configured_left_n;
    uint8_t configured_right_n;
    uint8_t left_n;
    uint8_t right_n;
    enum config_titlebar_button_e btn_kind;
    int32_t x;
    int32_t left_extent;
    int32_t right_extent;
    int32_t title_x;
    int32_t title_right;
    int32_t inset_avail;

    if (out_left_n != NULL) { *out_left_n = 0u; }
    if (out_right_n != NULL) { *out_right_n = 0u; }
    if (out_title_x != NULL) { *out_title_x = 0; }
    if (out_title_w != NULL) { *out_title_w = 0u; }
    if (out_btn_y != NULL) { *out_btn_y = 0; }

    if (theme == NULL || out_left == NULL || out_left_n == NULL ||
            out_right == NULL || out_right_n == NULL ||
            out_title_x == NULL || out_title_w == NULL ||
            out_btn_y == NULL) {
        return;
    }

    pad_h = (uint16_t) theme->window.titlebar.padding.horizontal;
    pad_v = (uint16_t) theme->window.titlebar.padding.vertical;

    /* Vertically center every button as a group: inset top and bottom
     * by 'padding.vertical' first, then center within whatever room
     * that leaves; if the padding alone would already exceed the
     * titlebar height (a theme with a very short titlebar and generous
     * padding), fall back to plain centering with no inset instead of
     * producing a negative position. */
    inset_avail = (int32_t) title_h - 2 * (int32_t) pad_v;
    if (inset_avail >= (int32_t) btn) {
        *out_btn_y = (int16_t) (pad_v + (inset_avail -
                    (int32_t) btn) / 2);
    } else {
        *out_btn_y = (title_h > btn)
            ? (int16_t) ((title_h - btn) / 2u) : 0;
    }

    /* A pin button dropped here (single-desktop surface, see
     * 'hide_pin') is skipped entirely rather than drawn inert: the
     * output index only advances for a button actually placed, so the
     * next configured button slides into its slot and the extent below
     * reflects the real, possibly-shorter row, the same as if the theme
     * itself had never listed pin at all. */
    configured_left_n = theme->window.titlebar.buttons.left_count;
    if (configured_left_n > (uint8_t) CONFIG_MAX_TITLEBAR_BUTTONS) {
        configured_left_n = (uint8_t) CONFIG_MAX_TITLEBAR_BUTTONS;
    }
    x = (int32_t) pad_h;
    left_n = 0u;
    for (uint8_t i = 0u; i < configured_left_n; ++i) {
        btn_kind = theme->window.titlebar.buttons.left[i];
        if (hide_pin && btn_kind == CONFIG_TITLEBAR_BUTTON_PIN) {
            continue;
        }
        out_left[left_n].button = btn_kind;
        out_left[left_n].x = (int16_t) x;
        x += (int32_t) (btn + gap);
        ++left_n;
    }
    *out_left_n = left_n;
    left_extent = (left_n == 0u) ? 0
        : (int32_t) (left_n * btn + (left_n - 1u) * gap);

    configured_right_n = theme->window.titlebar.buttons.right_count;
    if (configured_right_n > (uint8_t) CONFIG_MAX_TITLEBAR_BUTTONS) {
        configured_right_n = (uint8_t) CONFIG_MAX_TITLEBAR_BUTTONS;
    }
    x = (int32_t) frame_w - (int32_t) pad_h - (int32_t) btn;
    right_n = 0u;
    for (uint8_t i = 0u; i < configured_right_n; ++i) {
        btn_kind = theme->window.titlebar.buttons.right[i];
        if (hide_pin && btn_kind == CONFIG_TITLEBAR_BUTTON_PIN) {
            continue;
        }
        out_right[right_n].button = btn_kind;
        out_right[right_n].x = (int16_t) x;
        x -= (int32_t) (btn + gap);
        ++right_n;
    }
    *out_right_n = right_n;
    right_extent = (right_n == 0u) ? 0
        : (int32_t) (right_n * btn + (right_n - 1u) * gap);

    /* An extra 'WM_DECOR_BTN_GAP' beyond the plain edge padding gives
     * the title a bit more breathing room next to whichever button
     * group it is adjacent to, matching how buttons in the same group
     * are spaced from each other. */
    title_x = (int32_t) pad_h + left_extent + ((left_n > 0u)
        ? (int32_t) (pad_h + gap) : 0);
    title_right = (int32_t) frame_w - (int32_t) pad_h - right_extent -
        ((right_n > 0u) ? (int32_t) (pad_h + gap) : 0);

    *out_title_x = (int16_t) title_x;
    *out_title_w = (uint16_t) ((title_right > title_x)
        ? (title_right - title_x) : 0);
}


/* Synchronize the inner and titlebar geometry with the current frame
 * extents */
void client_decoration_layout_sync(client_td *client)
{
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;
    uint16_t title_h;
    uint16_t inner_w;
    uint16_t inner_h;
    uint16_t title_y;
    bool is_shaded_now;

    if (client == NULL || client->frame == 0 ||
            !client_is_decorated(client)) {
        return;
    }

    is_shaded_now = client_is_shaded(client);

    left = (uint16_t) client->layout.frame_extents.left;
    right = (uint16_t) client->layout.frame_extents.right;
    top = (uint16_t) client->layout.frame_extents.top;
    bottom = (uint16_t) client->layout.frame_extents.bottom;
    title_h = (uint16_t) client->title_height;
    title_y = (top > title_h) ? (uint16_t) (top - title_h) : 0u;
    inner_w = (client->layout.geometry.cur.dim.w > left + right)
        ? (uint16_t) (client->layout.geometry.cur.dim.w - left - right)
        : WM_MIN_WINDOW_DIMENSION;
    inner_h = (client->layout.geometry.cur.dim.h > top + bottom)
        ? (uint16_t) (client->layout.geometry.cur.dim.h - top - bottom)
        : WM_MIN_WINDOW_DIMENSION;

    /* While shaded, 'geometry.cur.dim.h' is only the frame's
     * collapsed titlebar height (see 'ccmd_client_shade',
     * cmds/client/state.c), never the client's real content height,
     * so 'inner_h' above is meaningless for it and must never reach
     * the content window itself.  Matches Openbox's
     * 'frame_adjust_area' (frame.c): it repositions the client
     * window (a plain 'XMoveWindow') but never resizes it while
     * shaded, leaving the client's real on-screen geometry
     * untouched the whole time it sits unmapped.  Applying 'inner_h'
     * here instead would send the client a real 'ConfigureNotify'
     * reporting a tiny height; many toolkits (GTK among them) cache
     * that as the window's last known size and persist it on
     * exit, so an application closed while shaded would reopen
     * collapsed to a sliver next time, unable to be worked with
     * until manually resized again. */
    if (!is_shaded_now) {
        ccmd_client_apply_geometry(client, client->window,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                left, top, inner_w, inner_h, 0u);
    }

    if (client->titlebar != 0) {
        ccmd_client_apply_geometry(client, client->titlebar,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                left, title_y, inner_w, title_h, 0u);
    }

    /* Force the reparented client area to repaint immediately after the
     * frame/title layout changes.  Using 'exposures=1' causes the
     * X server to generate an 'Expose' event so applications that do
     * not repaint on 'ConfigureNotify' alone redraw the newly exposed
     * lower area without requiring an additional user-triggered
     * action.  Skipped while shaded: the content window is unmapped
     * then, and X11 never generates 'Expose' for an unmapped window,
     * so this would be a no-op request anyway. */
    if (!is_shaded_now) {
        xcb_clear_area(xcb_connection_get(), 1, client->window, 0, 0, 0, 0);
    }
}


/* Clamp a width/height pair into a client's aspect-ratio bounds */
void client_aspect_ratio_clamp(const client_td *client,
        uint32_t width, uint32_t *height)
{
    if (client == NULL || height == NULL ||
            !client->hints_icccm.size.is_valid) {
        return;
    }

    /* Comparisons cross-multiply instead of dividing so no fractional
     * rounding of the ratio itself ever creeps in. */
    if (client->hints_icccm.size.aspect.min.num > 0 &&
            client->hints_icccm.size.aspect.min.den > 0) {
        uint64_t lhs = (uint64_t) width *
            (uint64_t) client->hints_icccm.size.aspect.min.den;
        uint64_t rhs = (uint64_t) client->hints_icccm.size.aspect.min.num *
            (uint64_t) *height;

        if (lhs < rhs) {
            *height = (uint32_t) (((uint64_t) width *
                        (uint64_t)
                        client->hints_icccm.size.aspect.min.den) /
                    (uint64_t) client->hints_icccm.size.aspect.min.num);
        }
    }

    if (client->hints_icccm.size.aspect.max.num > 0 &&
            client->hints_icccm.size.aspect.max.den > 0) {
        uint64_t lhs = (uint64_t) width *
            (uint64_t) client->hints_icccm.size.aspect.max.den;
        uint64_t rhs = (uint64_t) client->hints_icccm.size.aspect.max.num *
            (uint64_t) *height;

        if (lhs > rhs) {
            *height = (uint32_t) (((uint64_t) width *
                        (uint64_t)
                        client->hints_icccm.size.aspect.max.den) /
                    (uint64_t) client->hints_icccm.size.aspect.max.num);
        }
    }
}


/* Apply ICCCM size-hint constraints to a requested width and height */
void client_size_constrain(const client_td *client,
        uint32_t *restrict width, uint32_t *restrict height)
{
    uint32_t req_w;
    uint32_t req_h;

    if (client == NULL || width == NULL || height == NULL) {
        return;
    }

    req_w = *width;
    req_h = *height;

    /* The absolute floor every resize is guaranteed never to fall
     * below, applied first so the client's explicit 'min_w'/
     * 'min_h' just below (when it specifies one) can still only ever
     * raise this, never lower it: 1 resize-increment unit for a
     * client that measures itself in one (a terminal counting
     * character columns/rows, say, via 'width_inc'/'height_inc'),
     * or 'WM_MIN_WINDOW_DIMENSION' pixels otherwise. */
    if (client->hints_icccm.size.is_valid &&
            client->hints_icccm.size.inc.w > 1) {
        uint32_t base_w = (client->hints_icccm.size.base.w > 0)
            ? client->hints_icccm.size.base.w
            : 0u;

        if (req_w < base_w +
                WM_MIN_WINDOW_DIMENSION_UNITS *
                    client->hints_icccm.size.inc.w) {
            req_w = base_w +
                WM_MIN_WINDOW_DIMENSION_UNITS *
                    client->hints_icccm.size.inc.w;
        }
    } else if (req_w < (uint32_t) WM_MIN_WINDOW_DIMENSION) {
        req_w = (uint32_t) WM_MIN_WINDOW_DIMENSION;
    }

    if (client->hints_icccm.size.is_valid &&
            client->hints_icccm.size.inc.h > 1) {
        uint32_t base_h = (client->hints_icccm.size.base.h > 0)
            ? client->hints_icccm.size.base.h
            : 0u;

        if (req_h < base_h +
                WM_MIN_WINDOW_DIMENSION_UNITS *
                    client->hints_icccm.size.inc.h) {
            req_h = base_h +
                WM_MIN_WINDOW_DIMENSION_UNITS *
                    client->hints_icccm.size.inc.h;
        }
    } else if (req_h < (uint32_t) WM_MIN_WINDOW_DIMENSION) {
        req_h = (uint32_t) WM_MIN_WINDOW_DIMENSION;
    }

    if (client->hints_icccm.size.is_valid) {
        if (client->hints_icccm.size.min.w > 0 &&
                req_w < client->hints_icccm.size.min.w) {
            req_w = client->hints_icccm.size.min.w;
        }

        if (client->hints_icccm.size.max.w > 0 &&
                req_w > client->hints_icccm.size.max.w) {
            req_w = client->hints_icccm.size.max.w;
        }

        if (client->hints_icccm.size.min.h > 0 &&
                req_h < client->hints_icccm.size.min.h) {
            req_h = client->hints_icccm.size.min.h;
        }

        if (client->hints_icccm.size.max.h > 0 &&
                req_h > client->hints_icccm.size.max.h) {
            req_h = client->hints_icccm.size.max.h;
        }

        if (client->hints_icccm.size.inc.w > 1) {
            uint32_t base;
            uint32_t inc;
            uint32_t over;
            /* ICCCM §4.1.2.3: when 'BASE_SIZE' is absent, 'MIN_SIZE'
             * serves as the base for the increment grid */
            base = (client->hints_icccm.size.base.w > 0)
                ? client->hints_icccm.size.base.w
                : ((client->hints_icccm.size.min.w > 0)
                        ? client->hints_icccm.size.min.w
                        : 0u);
            inc = client->hints_icccm.size.inc.w;
            over = (req_w > base) ? (req_w - base) : 0u;
            req_w = base + (over / inc) * inc;
        }

        if (client->hints_icccm.size.inc.h > 1) {
            uint32_t base;
            uint32_t inc;
            uint32_t over;
            /* ICCCM §4.1.2.3: when 'BASE_SIZE' is absent, 'MIN_SIZE'
             * serves as the base for the increment grid */
            base = (client->hints_icccm.size.base.h > 0)
                ? client->hints_icccm.size.base.h
                : ((client->hints_icccm.size.min.h > 0)
                        ? client->hints_icccm.size.min.h
                        : 0u);
            inc = client->hints_icccm.size.inc.h;
            over = (req_h > base) ? (req_h - base) : 0u;
            req_h = base + (over / inc) * inc;
        }

        /* ICCCM §4.1.2.3: clamp the width/height ratio into
         * ['min_aspect', 'max_aspect'], via 'client_aspect_ratio_clamp'
         * (shared with 'ik_handle_resize' in input/kbd/interact.c, for
         * exactly the reasoning its comment gives).  Kept as
         * the very last adjustment in this whole block, after every
         * other constraint above (including the grid and the second
         * 'min_w'/'min_h' floor just below), so nothing that runs
         * afterward can push the ratio back out of range again. */
        if (client->hints_icccm.size.min.w > 0 &&
                req_w < client->hints_icccm.size.min.w) {
            req_w = client->hints_icccm.size.min.w;
        }

        if (client->hints_icccm.size.min.h > 0 &&
                req_h < client->hints_icccm.size.min.h) {
            req_h = client->hints_icccm.size.min.h;
        }

        client_aspect_ratio_clamp(client, req_w, &req_h);
    }

    *width = req_w;
    *height = req_h;
}


/* Create frame and titlebar windows for a decorated client */
int ci_create_decorations(client_td *client)
{
    uint32_t mask;
    uint32_t values[3];
    struct geometry_s frame;
    int32_t frame_x32;
    int32_t frame_y32;
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;
    uint16_t inner_w;
    uint16_t title_h;
    uint16_t title_y;
    static const xcb_button_t s_grab_buttons[] = {
        XCB_BUTTON_INDEX_1,
        XCB_BUTTON_INDEX_2,
        XCB_BUTTON_INDEX_3,
        6,   /* extra side buttons */
        7
    };
    size_t nb = sizeof(s_grab_buttons) / sizeof(s_grab_buttons[0]);

    if (client == NULL || !client_is_decorated(client) ||
            client->config == NULL || client->parent_id == 0) {
        return 0;
    }

    left = (uint16_t) client->layout.frame_extents.left;
    right = (uint16_t) client->layout.frame_extents.right;
    top = (uint16_t) client->layout.frame_extents.top;
    bottom = (uint16_t) client->layout.frame_extents.bottom;
    inner_w = (uint16_t) client->layout.geometry.cur.dim.w;
    title_h = (uint16_t) client->title_height;
    title_y = (top > title_h) ? (uint16_t) (top - title_h) : 0u;
    frame_x32 = client->layout.geometry.cur.pos.x - (int32_t) left;
    frame_y32 = client->layout.geometry.cur.pos.y - (int32_t) top;

    if (frame_x32 < INT16_MIN) {
        frame.pos.x = INT16_MIN;
    } else if (frame_x32 > INT16_MAX) {
        frame.pos.x = INT16_MAX;
    } else {
        frame.pos.x = frame_x32;
    }

    if (frame_y32 < INT16_MIN) {
        frame.pos.y = INT16_MIN;
    } else if (frame_y32 > INT16_MAX) {
        frame.pos.y = INT16_MAX;
    } else {
        frame.pos.y = frame_y32;
    }

    frame.dim.w = (uint16_t)
        (client->layout.geometry.cur.dim.w + left + right);
    frame.dim.h = (uint16_t)
        (client->layout.geometry.cur.dim.h + top + bottom);

    client->frame = xcb_generate_id(xcb_connection_get());
    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = client->config->theme.window.inactive.border.color;
    values[1] = client->config->theme.window.inactive.border.color;
    /* 'XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT' is essential here, not
     * optional: once the client's top-level window is reparented
     * into this frame, its *parent* for X11 purposes becomes the frame
     * instead of the root.
     *
     * A client's attempt to reconfigure itself is delivered as
     * a 'ConfigureRequest' to whichever client selected
     * substructure-redirect on its *parent*; if that is only ever
     * selected on the root window (needed for top-level 'MapRequest's)
     * and not on every frame the window manager itself
     * creates, the server has nothing to redirect a reparented client's
     * own resize to, and simply performs it directly with no
     * 'ConfigureRequest' ever generated at all. */
    values[2] = XCB_EVENT_MASK_EXPOSURE             |
                XCB_EVENT_MASK_BUTTON_PRESS         |
                XCB_EVENT_MASK_ENTER_WINDOW         |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY     |
                XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY  |
                XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                XCB_EVENT_MASK_POINTER_MOTION;
    xcb_create_window(xcb_connection_get(),
            XCB_COPY_FROM_PARENT,
            client->frame,
            client->parent_id,
            (int16_t) frame.pos.x, (int16_t) frame.pos.y,
            (uint16_t) frame.dim.w, (uint16_t) frame.dim.h,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    client->titlebar = xcb_generate_id(xcb_connection_get());
    mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = client->config->theme.window.inactive.color.background;
    values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_ENTER_WINDOW;
    xcb_create_window(xcb_connection_get(),
            XCB_COPY_FROM_PARENT,
            client->titlebar,
            client->frame,
            (int16_t) left, (int16_t) title_y,
            inner_w, title_h,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    xcb_window_reparent(client->window,
            client->frame,
            (int16_t) left, (int16_t) top);

    /* Now a child of 'frame' rather than of root, so from here on
     * this window manager dying takes 'frame' down with it unless
     * the save set has already been told to hand the window back to
     * root instead of letting it go with its frame (Scheifler and
     * Gettys, 1994, "Inter-Client Communication Conventions Manual",
     * v2.0, §4.1.2). */
    xcb_window_save_set(client->window, true);

    ccmd_client_apply_geometry(client, client->window,
            (uint16_t) XCB_CONFIG_WINDOW_BORDER_WIDTH,
            0, 0, 0u, 0u, 0u);

    /* Passive grab: selected button, any modifier, SYNC pointer mode.
     * With 'owner_events=0' button presses on the frame or any of its
     * children (titlebar, client window) are delivered to the window
     * manager through this grab rather than via SelectInput.  SYNC mode
     * freezes pointer events until the window manager calls
     * 'xcb_allow_events', which lets the manager focus the window
     * before deciding whether to replay the click to the application or
     * consume it silently.  'XCB_MOD_MASK_ANY' already covers all
     * lock-modifier combinations, so no lock-modifier loop is required.
     *
     * Scroll-wheel buttons 4 and 5 are intentionally excluded.  A sync
     * passive grab on those buttons intercepts scroll events before the
     * application can receive them; even though 'ReplayPointer' is
     * issued, some applications (e.g., Chromium, pcmanfm) use their
     * grabs internally and the event is not re-delivered correctly.
     * Leaving 4 and 5 ungrabbed here allows the X server to deliver
     * scroll events directly to the focused application window. */
    /* Root-level 'MOD1+button' grabs are more specific (specific
     * modifier beats 'XCB_MOD_MASK_ANY') and therefore still take
     * priority for move/resize interactions. */
    for (size_t bi = 0; bi < nb; ++bi) {
        xcb_grab_button(xcb_connection_get(),
                0,                              /* owner_events */
                client->frame,
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_BUTTON_RELEASE,
                XCB_GRAB_MODE_SYNC,         /* freeze until allowed */
                XCB_GRAB_MODE_ASYNC,
                XCB_NONE,
                XCB_NONE,
                s_grab_buttons[bi],
                XCB_MOD_MASK_ANY);
    }

    client->layout.geometry.cur = frame;
    client->layout.geometry.old = client->layout.geometry.cur;
    client_decoration_layout_sync(client);

    /* Publish '_NET_FRAME_EXTENTS' so clients and taskbars know the
     * size of the WM-added decoration around the content window */
    if (xcb_ewmh_connection_get() != NULL) {
        uint32_t extents[4];
        extents[0] = (uint32_t) client->layout.frame_extents.left;
        extents[1] = (uint32_t) client->layout.frame_extents.right;
        extents[2] = (uint32_t) client->layout.frame_extents.top;
        extents[3] = (uint32_t) client->layout.frame_extents.bottom;
        xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
                client->window, xcb_ewmh_connection_get()->_NET_FRAME_EXTENTS,
                XCB_ATOM_CARDINAL, 32, 4, extents);
    }


    return 0;
}


/* Send a synthetic 'ConfigureNotify' to inform the client of its
 * screen-relative geometry (ICCCM §4.2.3) */
void client_send_synthetic_configure_notify(xcb_connection_t *connection,
        const client_td *client)
{
    xcb_configure_notify_event_t notify;
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;

    if (connection == NULL || client == NULL || client->window == 0) {
        return;
    }

    left = (uint16_t) client->layout.frame_extents.left;
    right = (uint16_t) client->layout.frame_extents.right;
    top = (uint16_t) client->layout.frame_extents.top;
    bottom = (uint16_t) client->layout.frame_extents.bottom;

    memset(&notify, 0, sizeof(notify));
    notify.response_type = XCB_CONFIGURE_NOTIFY;
    notify.event = client->window;
    notify.window = client->window;
    notify.above_sibling = XCB_NONE;
    notify.x = (int16_t) (client->layout.geometry.cur.pos.x +
            (int32_t) left);
    notify.y = (int16_t) (client->layout.geometry.cur.pos.y +
            (int32_t) top);
    notify.width =
        (uint16_t) ((client->layout.geometry.cur.dim.w > left + right)
                ? (client->layout.geometry.cur.dim.w - left - right)
                : WM_MIN_WINDOW_DIMENSION);
    notify.height =
        (uint16_t) ((client->layout.geometry.cur.dim.h > top + bottom)
                ? (client->layout.geometry.cur.dim.h - top - bottom)
                : WM_MIN_WINDOW_DIMENSION);
    notify.border_width = 0;
    notify.override_redirect = 0;

    xcb_send_event(connection, 0, client->window,
            XCB_EVENT_MASK_STRUCTURE_NOTIFY, (const char *) &notify);
}
