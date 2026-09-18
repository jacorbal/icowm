/**
 * @file render/desktop.c
 *
 * @brief Desktop render pass orchestration
 *
 * @note Decoration constants come from @c defs/client.h; the actual
 *       painting of a client's titlebar, frame decoration, and the
 *       desktop background lives in @c render/client/titlebar.c,
 *       @c render/client/decoration.c, and
 *       @c render/desktop/background.c respectively, this file only
 *       decides when each of them runs
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

/* Default initial values */
#include <defs/client.h>

/* Policy includes */
#include <policy/stacking.h>
#include <policy/urgency.h>

/* Utils includes */
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <stage.h>
#include <wm.h>

/* Local includes */
#include <render/client/decoration.h>
#include <render/client/titlebar.h>
#include <render/desktop.h>
#include <render/desktop/background.h>
#include <render/icon.h>
#include <render/outdate.h>


/**
 * @brief What rendering one client needs to know about it
 *
 * Gathered so the two paths below, applying an outdated geometry and
 * refreshing decoration colors alone, can each be a function rather
 * than another hundred lines inside @a desktop_render_one_client.
 * Only what both paths need before they start is carried, for the frame
 * extents and the titlebar height are worked out inside each of them,
 * and the theme is reached through @p desktop.
 */
struct s_render_ctx_s {
    desktop_td *desktop;
    client_td *client;
    xcb_window_t target;
    bool is_focused;
    bool hide_decoration;
    bool titlebar_visible;
};


/**
 * @brief What @a s_desktop_render_client_visit needs beyond the client
 */
struct s_desktop_render_ctx_s {
    desktop_td *desktop;    /**< Desktop being rendered */
    int client_count;       /**< How many have been rendered */
    bool is_current;        /**< Whether it is the visible one */
};


/**
 * @brief Give a client's frame or window a new native border width
 *
 * X places a window by the outer corner of its border, so a border
 * that grows or shrinks on a window placed there moves its content by
 * the difference.  A frame's border is always 0, so for one the width
 * is simply sent; for a window with no frame of ours, the one whose
 * border shows focus, the window moves by that same difference in the
 * same request, and its content stays where it was on screen, the way
 * it does for a framed window, whose border lives inside the frame; its
 * restore geometry moves along with it, through
 * @a client_native_border_rebase.  The server reports that move to the client with a real
 * @c ConfigureNotify, as a direct child of the root.
 *
 * @param client       Client whose border is changing
 * @param target       The client's frame, or its own window when it
 *                     has none
 * @param border_width Width to give @p target
 *
 * @note Complexity: @e O(1)
 */
static void s_render_apply_border(client_td *client, xcb_window_t target,
        uint32_t border_width)
{
    if (target != client->window ||
            client->last_border_width == UINT32_MAX) {
        xcb_window_set_border(target, border_width);
        client->last_border_width = border_width;
        return;
    }

    client_native_border_rebase(client, border_width);

    /* Same bookkeeping 'ccmd_client_apply_geometry' does, since this
     * places the very same window without going through it; see
     * 'requested_pos' in client/layout.h */
    client->layout.requested_pos.x = client->layout.geometry.cur.pos.x;
    client->layout.requested_pos.y = client->layout.geometry.cur.pos.y;
    client->layout.has_requested_pos = true;

    xcb_configure_window(xcb_connection_get(), target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                XCB_CONFIG_WINDOW_BORDER_WIDTH,
            (const uint32_t[]) {
                (uint32_t) client->layout.geometry.cur.pos.x,
                (uint32_t) client->layout.geometry.cur.pos.y,
                border_width });
    client->last_border_width = border_width;
}


/**
 * @brief Apply a client's geometry, and repaint what it moves
 *
 * Taken when the client is marked outdated, so its position and size
 * are sent to the server and every decoration is laid out against the
 * geometry that results.
 *
 * @param ctx State of the client being rendered
 *
 * @note Complexity: @e O(1)
 */
static void s_render_apply_geometry(struct s_render_ctx_s *ctx)
{
    desktop_td *const desktop = ctx->desktop;
    client_td *const client = ctx->client;
    const xcb_window_t target = ctx->target;
    const bool is_focused = ctx->is_focused;
    const bool hide_decoration = ctx->hide_decoration;
    const bool titlebar_visible = ctx->titlebar_visible;

    /* Configure position and size; only when the client's geometry or
     * decoration changed.  Skipping this for up-to-date clients
     * prevents the server from generating spurious 'ConfigureNotify'
     * and 'Expose' events that cause other windows to unnecessarily
     * redraw, which appears as flicker during keyboard resize of an
     * unrelated client. */
    LOGGER_TRACE("Render pass applying outdated geometry for" \
            " window=0x%x: target=0x%x (%s frame), %ux%u%+d%+d",
            client->window, target,
            (target != client->window) ? "has" : "no",
            client->layout.geometry.cur.dim.w,
            client->layout.geometry.cur.dim.h,
            client->layout.geometry.cur.pos.x,
            client->layout.geometry.cur.pos.y);

    /* Same bookkeeping 'ccmd_client_apply_geometry' does, since this
     * places the very same window without going through it; see
     * 'requested_pos' in client/layout.h */
    client->layout.requested_pos.x = client->layout.geometry.cur.pos.x;
    client->layout.requested_pos.y = client->layout.geometry.cur.pos.y;
    client->layout.has_requested_pos = true;

    xcb_window_place(target,
            client->layout.geometry.cur.pos.x,
            client->layout.geometry.cur.pos.y,
            client->layout.geometry.cur.dim.w,
            client->layout.geometry.cur.dim.h);
    if (target != client->window) {
        uint16_t top;
        uint16_t bottom;
        uint16_t left;
        uint16_t right;
        uint16_t inner_h;
        uint16_t inner_w;
        uint16_t title_h;

        /* Forced to zero outright for a fullscreen client, rather than
         * trusting 'frame_extents' to already be zero.  This is the
         * exact geometry a click or a losing-focus repaint would
         * otherwise leave stuck at whatever theme padding
         * 'frame_extents' happened to hold, showing the frame's own
         * background (set to the theme's border color by
         * 'render_client_decoration_repaint_frame') through the gap
         * left along the content window's top and left edges.
         * Visually indistinguishable from a real border (almost),
         * though neither an X11 border nor that repaint function was
         * ever actually involved. */
        if (hide_decoration) {
            left = 0;
            right = 0;
            top = 0;
            bottom = 0;
        } else {
            left = (uint16_t) client->layout.frame_extents.left;
            right = (uint16_t) client->layout.frame_extents.right;
            top = (uint16_t) client->layout.frame_extents.top;
            bottom = (uint16_t) client->layout.frame_extents.bottom;
        }
        title_h = (uint16_t) client->title_height;
        inner_w = (client->layout.geometry.cur.dim.w > left + right)
            ? (uint16_t) (client->layout.geometry.cur.dim.w -
                    left - right)
            : 1;
        inner_h = (client->layout.geometry.cur.dim.h > top + bottom)
            ? (uint16_t) (client->layout.geometry.cur.dim.h -
                    top - bottom)
            : 1;

        /* A shaded client's content window is deliberately left
         * unmapped, at whatever geometry it already had (see
         * 'ccmd_client_shade', 'cmds/client/state.c').  None of the
         * three calls below (repositioning it, telling it about that
         * new position, and prompting it to redraw) are meant for it
         * while shaded, since it is never seen regardless of what
         * geometry it holds.  'left'/'top'/ 'inner_w'/'title_h',
         * computed above either way, are still needed below this whole
         * 'if' to position the titlebar correctly even while shaded. */
        if (!client_is_shaded(client)) {
            bool inner_moved = !client->layout.has_placed_inner ||
                client->layout.placed_inner.pos.x != (int32_t) left ||
                client->layout.placed_inner.pos.y != (int32_t) top ||
                client->layout.placed_inner.dim.w != inner_w ||
                client->layout.placed_inner.dim.h != inner_h;
            client->layout.placed_inner.pos.x = (int32_t) left;
            client->layout.placed_inner.pos.y = (int32_t) top;
            client->layout.placed_inner.dim.w = inner_w;
            client->layout.placed_inner.dim.h = inner_h;
            client->layout.has_placed_inner = true;

            /* Only when the content window's frame-relative position
             * or size actually changed.  This pass runs for any
             * reason at all, a changed title among them, and
             * reconfiguring an unmoved content window still sends it
             * a synthetic 'ConfigureNotify' for nothing: a
             * compositing client (e.g., Chromium, Electron) treats
             * that as a cue to recomposite its own buffer even
             * though nothing moved, seen as a brief flicker of its
             * content on every outdated pass a decorated client goes
             * through for an unrelated reason, such as its own title
             * changing. */
            if (inner_moved) {
                xcb_window_place(client->window, left, top,
                        inner_w, inner_h);

                /* ICCCM §4.2.3: the 'xcb_configure_window' above
                 * positions the inner window relative to the frame
                 * (x=left, y=top), so the X server delivers a
                 * 'ConfigureNotify' to the client with those
                 * frame-relative coordinates.  Override it
                 * immediately with a synthetic 'ConfigureNotify'
                 * carrying the true screen-relative position so the
                 * client's last geometry notification is always
                 * correct.  Without this a decorated client sees a
                 * frame-relative 'ConfigureNotify' as its final event
                 * on every render pass that actually moves it,
                 * causing misaligned popups and a content area that
                 * appears not to fill the frame until the next
                 * user-triggered repaint. */
                client_send_synthetic_configure_notify(
                        xcb_connection_get(), client);

                /* Force a repaint AFTER the synthetic
                 * 'ConfigureNotify' so the client always redraws at
                 * its correct screen-relative geometry.  Some
                 * programs do not redraw on 'ConfigureNotify' alone;
                 * this 'Expose' ensures the drawing happens at the
                 * right size and position after every render pass
                 * that actually moves the content window, including
                 * the initial map and post-resize redraws.  Setting
                 * 'exposures=1' causes the X server to generate an
                 * 'Expose' event, which arrives in the client's queue
                 * after both the 'xcb_configure_window' and the
                 * synthetic 'ConfigureNotify' above.
                 *
                 * Skipped for a client already using
                 * '_NET_WM_SYNC_REQUEST': that protocol already tells
                 * it exactly when to redraw, via the sync request
                 * 'ccmd_client_resize' (cmds/client/resize.c) sends
                 * for every interactive resize step, so forcing
                 * a clear here on top of that, once per step, only
                 * blanks a compositing client's own content out from
                 * under it a moment before it repaints on its own,
                 * seen as a brief flicker on every step rather than
                 * a smooth resize. */
                if (!client->hints_ewmh.sync.is_supported ||
                        !wm_sync_is_available()) {
                    xcb_clear_area(xcb_connection_get(), 1,
                            client->window, 0, 0, 0, 0);
                }
            }
        }
        render_client_decoration_repaint_frame_unless_hidden(
                xcb_connection_get(), client, is_focused,
                hide_decoration, &desktop->config->theme);

        if (titlebar_visible) {
            xcb_window_place(client->titlebar, left,
                    (top > title_h) ? top - title_h : 0,
                    inner_w, title_h);
            render_client_titlebar_repaint_content(
                    xcb_connection_get(), client, is_focused,
                    inner_w, title_h, &desktop->config->theme);
        } else if (client->titlebar != 0) {
            xcb_window_hide(client->titlebar);
        }
    }

    wm_validate_client(client);
}


/**
 * @brief Refresh a client's decoration without touching its geometry
 *
 * Taken when the geometry has not changed but the colors have, after
 * a focus change or an urgency blink, so only what is painted needs
 * redoing.
 *
 * @param ctx State of the client being rendered
 *
 * @note Complexity: @e O(1)
 */
static void s_render_refresh_decoration(struct s_render_ctx_s *ctx)
{
    desktop_td *const desktop = ctx->desktop;
    client_td *const client = ctx->client;
    const bool is_focused = ctx->is_focused;
    const bool hide_decoration = ctx->hide_decoration;
    const bool titlebar_visible = ctx->titlebar_visible;
    uint16_t left;
    uint16_t right;
    uint16_t inner_w;
    uint16_t title_h;

    /* The client geometry has not changed; only refresh the
     * focus-sensitive decoration colors (border and titlebar
     * background/text) when the active client actually changed, or when
     * this specific client is urgent (its attention blink alternates
     * these same colors on every phase change, the same reasoning
     * 'ri_render_client_icon', in 'render/icon.c', already applies via
     * its '!client_is_urgent' skip-check condition, just expressed the
     * other way around here).  Skipping this repaint otherwise avoids
     * spurious 'xcb_clear_area + text-draw' calls on every render pass
     * during resize, which was the source of the desktop-wide
     * flickering visible on all non-resized windows. */
    left = (uint16_t) client->layout.frame_extents.left;
    right = (uint16_t) client->layout.frame_extents.right;
    title_h = (uint16_t) client->title_height;
    inner_w = (client->layout.geometry.cur.dim.w > left + right)
        ? (uint16_t) (client->layout.geometry.cur.dim.w -
                left - right)
        : 1;

    /* Skipped entirely, not just recolored, for the same reason the
     * full-repaint branch above never gives a fullscreen client
     * a border in the first place (see its comment by
     * 'client_is_fullscreen' there): a focus change alone must not
     * paint one back in over fullscreen content just because this
     * lighter branch only meant to refresh existing colors, not decide
     * from scratch whether a border belongs here at all. */
    render_client_decoration_repaint_frame_unless_hidden(xcb_connection_get(),
            client, is_focused, hide_decoration,
            &desktop->config->theme);

    if (titlebar_visible) {
        render_client_titlebar_repaint_content(xcb_connection_get(),
                client, is_focused, inner_w, title_h,
                &desktop->config->theme);
    } else if (client->titlebar != 0) {
        xcb_window_hide(client->titlebar);
    }
}


/**
 * @brief Render one client, or its icon when it is iconified
 *
 * @param client Client reached by the walk
 * @param data   Pointer to this walk's render context
 *
 * @note A plainly hidden client is drawn neither way, being unmapped
 *       and having no icon standing in for it
 * @note Complexity: @e O(1)
 */
static void s_desktop_render_client_visit(client_td *client, void *data)
{
    struct s_desktop_render_ctx_s *const render_ctx = data;

    if (client == NULL || render_ctx == NULL) {
        return;
    }

    render_ctx->client_count++;

    /* Keep icon windows visible only for iconified clients.  Plain
     * hidden windows must stay fully unmapped. */
    if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
        if (client_is_iconified(client)) {
            ri_render_client_icon(client, render_ctx->is_current,
                    false, true);
        }
        return;
    }

    desktop_render_one_client(render_ctx->desktop, client,
            render_ctx->is_current);
}


/**
 * @brief Draw all clients on a desktop
 *
 * Iterates through all clients in the desktop's stacking list and
 * configures their geometry.  Windows are only mapped (made visible)
 * when @p is_current is @c true; for a desktop that is not the one
 * currently displayed on its stage, only geometry/stacking is updated
 * so that a stale full-render pass (triggered by an unrelated
 * @p is_outdated flag, e.g., after moving/resizing a client) cannot
 * undo an explicit @a stage_client_hide_all and make a client reappear
 * on top of the desktop the user actually switched to.
 *
 * @param desktop    Pointer to the desktop to draw
 * @param is_current Whether @p desktop is the stage's currently
 *                   displayed desktop; when @c false, clients are not
 *                   (re-)mapped, only their geometry is updated
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to draw clients
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
static int s_desktop_render_clients(desktop_td *desktop, bool is_current)
{
    struct s_desktop_render_ctx_s render_ctx;
    int client_count = 0;
    size_t stacking_size;

    if (desktop == NULL) {
        LOGGER_ERROR("Received null desktop pointer", L_NARG);
        return 1;
    }

    stacking_size = (size_t) stacking_count(desktop);
    LOGGER_DEBUG("Rendering %zu client(s) from stacking list" \
            " on desktop %u ('%s')",
            stacking_size, desktop->id, desktop->name);

    /* If no clients, return early */
    if (stacking_size == 0) {
        LOGGER_TRACE("No clients to render on desktop %u ('%s')",
                desktop->id, desktop->name);
        return 0;
    }

    render_ctx.desktop = desktop;
    render_ctx.is_current = is_current;
    render_ctx.client_count = 0;
    stacking_walk(desktop, s_desktop_render_client_visit, &render_ctx);
    client_count = render_ctx.client_count;

    LOGGER_DEBUG("Successfully rendered %d clients" \
            " on desktop %u ('%s')",
            client_count, desktop->id, desktop->name);

    return 0;
}


/* Render, position, and decorate a single already-non-hidden client
 * during a stacking-order render pass */
void desktop_render_one_client(desktop_td *desktop,
        client_td *client, bool is_current)
{
    struct s_render_ctx_s ctx = {0};
    xcb_window_t target;
    bool is_focused;
    bool hide_decoration;
    bool titlebar_visible;
    uint32_t border_width;

    is_focused = (desktop->client_active_id == client->id);

    /* Swapped for one half of the blink cycle when this client is
     * urgent (see 'policy/urgency.h'): every later use of 'is_focused'
     * below (border, background, font, and text colors) already follows
     * from this one flip, so the titlebar reads as attention-grabbing
     * without a second, separate code path. */
    if (client_is_urgent(client) && urgency_blink_is_on()) {
        is_focused = !is_focused;
    }

    hide_decoration =
        (client_is_fullscreen(client) &&
         client->was_decorated_fullscreen);

    /* Computed once here rather than re-spelled out as 'client->
     * titlebar != 0 && !hide_decoration' at each of the three spots
     * below (the initial map, the full outdated repaint, and the
     * lighter focus-only repaint) that all need to draw exactly the
     * same distinction between "this client currently has a titlebar to
     * show at all" and "decoration is being suppressed right now"
     * (fullscreen). */
    titlebar_visible = (client->titlebar != 0 && !hide_decoration);

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame
        : client->window;

    if (client->properties.type == (uint16_t) CLIENT_TYPE_DOCK ||
            client->properties.type ==
                (uint16_t) CLIENT_TYPE_NOTIFICATION) {
        /* Dock and notification windows must never have a WM border */
        border_width = 0u;
    } else if (client_is_fullscreen(client)) {
        /* A fullscreen client is undecorated (see 'hide_decoration'
         * above), so it falls to the same 'not decorated' branch
         * a plain undecorated window would, which would otherwise apply
         * the theme's regular window border directly to its own raw
         * window: a border painted over video or other fullscreen
         * content, not just an unwanted frame around an ordinary
         * window. */
        border_width = 0u;
    } else if (client_is_decorated(client) && client->frame != 0) {
        border_width = 0u;
    } else {
        border_width = client_border_width(client, is_focused, false);
    }

    /* Only actually send the request when the value would change: an
     * unconditional 'ConfigureWindow' here on every render pass for
     * every client (needed so an icon-cycle selection border
     * appears/disappears promptly) was extra server round-trip traffic
     * for the common case where nothing about this particular client
     * changed at all (e.g., a keyboard resize of one window previously
     * still re-sent border width for every other window on the desktop
     * each time). */
    if (border_width != client->last_border_width) {
        s_render_apply_border(client, target, border_width);
    }

    /* An undecorated client shows its focus through this border and
     * nothing else, a decorated one through the frame repainted just
     * above, so both now follow from 'is_focused' in the same pass.
     * Leaving the color to whoever changes the focus instead means
     * a path that forgets leaves a window still wearing the active
     * border after another has taken the focus from it, where
     * a decorated window would have corrected itself on the next pass.
     * Skipped when unchanged, as the width is. */
    if (!client_is_decorated(client) || client->frame == 0) {
        client_border_color_apply(client, is_focused);
    }

    /* Map the window to make it visible.  Only do this when 'desktop'
     * is the stage's currently displayed desktop.
     *
     * Its also invoked as part of a general
     * 'stage_render_all_desktops' refresh pass whenever ANY desktop's
     * 'is_outdated' flag is set (e.g., after moving or resizing
     * a client, which marks its desktop outdated).
     *
     * If that pass unconditionally mapped clients on a desktop that is
     * not currently shown, it could race with (and undo) an explicit
     * 'stage_client_hide_all' issued by a desktop switch, making
     * a client reappear on top of the desktop the user just switched
     * to.
     *
     * Visibility of non-current desktops must be governed solely by
     * 'stage_client_hide_all'/'stage_client_show_all' */
    if (is_current) {
        if (client->icon_window != 0 && client->is_icon_mapped) {
            xcb_window_hide(client->icon_window);
            client->is_icon_mapped = false;
        }
        if (titlebar_visible) {
            xcb_window_show(client->titlebar);
        } else if (client->titlebar != 0) {
            xcb_window_hide(client->titlebar);
        }
        xcb_window_show(target);

        /* Do not re-map the content window for shaded clients.  The
         * shade operation explicitly unmaps it, and mapping it here
         * would undo the shade and prevent the titlebar-only view from
         * being painted correctly, especially for inactive windows that
         * receive no 'FocusOut'-triggered repaint */
        if (target != client->window && !client_is_shaded(client)) {
            xcb_window_show(client->window);
        }
    }

    ctx.desktop = desktop;
    ctx.client = client;
    ctx.target = target;
    ctx.is_focused = is_focused;
    ctx.hide_decoration = hide_decoration;
    ctx.titlebar_visible = titlebar_visible;

    if (client->is_outdated) {
        s_render_apply_geometry(&ctx);
    } else if (target != client->window &&
            (desktop->is_focus_dirty || client_is_urgent(client))) {
        s_render_refresh_decoration(&ctx);
    }

    LOGGER_TRACE("Rendered client 0x%08x with" \
                 " geometry (%ux%u%+d%+d)",
            client->id,
            client->layout.geometry.cur.dim.w,
            client->layout.geometry.cur.dim.h,
            client->layout.geometry.cur.pos.x,
            client->layout.geometry.cur.pos.y);
}


/* Full desktop render */
int desktop_render_full(desktop_td *desktop, bool is_current)
{
    if (desktop == NULL) {
        LOGGER_ERROR("Received null desktop pointer", L_NARG);
        return 1;
    }

    LOGGER_TRACE("Fully rendering desktop %u ('%s')",
            desktop->id, desktop->name);

    /* Draw background, but only for the desktop currently shown on
     * screen: a non-current desktop's background is never actually
     * visible (the stage-level repaint that calls this, in
     * 'render/stage.c', re-applies the current desktop's background
     * again right after every desktop in the list has been rendered,
     * specifically because earlier ones painting theirs would otherwise
     * overwrite it on the one shared root window), so painting it here
     * for a desktop nobody can see would just be work immediately
     * thrown away. */
    if (is_current && render_desktop_background_render(desktop) != 0) {
        LOGGER_ERROR("Failed to render background on" \
                 " desktop %u ('%s')", desktop->id, desktop->name);
        return 1;
    }

    /* Draw all clients */
    if (s_desktop_render_clients(desktop, is_current) != 0) {
        LOGGER_ERROR("Failed to render clients", L_NARG);
        return 1;
    }

    /* Mark desktop as up-to-date */
    wm_validate_desktop(desktop);

    /* Every client just had its chance, in the loop above, to compare
     * itself against 'is_focus_dirty' and refresh its decoration colors
     * if the active client changed since the last full render.
     * Clearing it here consumes that signal so the next pass (e.g.,
     * a later resize of one otherwise-unrelated client, with focus
     * unchanged since) does not see it still set and re-trigger the
     * exact spurious 'xcb_clear_area + text-draw' repaint on every
     * other window this flag exists to avoid.  See its comment in
     * 'desktop.h' ("since last render pass") and the 'is_focus_dirty'
     * branch in 'desktop_render_one_client' above. */
    desktop->is_focus_dirty = false;

    /* NOTE: Do NOT flush here!  Let the stage handle the flushing */

    return 0;
}
