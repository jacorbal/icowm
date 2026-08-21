/**
 * @file handler/configure.c
 *
 * @brief X @c CONFIGURE_REQUEST and @c CONFIGURE_NOTIFY event handlers
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* struct timespec */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <time.h>       /* struct timespec, NULL */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Command includes */
#include <cmds/client/layer.h>

/* Render includes */
#include <render/desktop.h>
#include <render/outdate.h>

/* Default initial values */
#include <defs/client.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <systray.h>
#include <lookup.h>

/* Utils includes */
#include <utils/time/clock.h>

/* Local includes */
#include <handler.h>


/**
 * @brief Send a synthetic @c ConfigureNotify event to a client window
 *
 * Emits an ICCCM-compliant synthetic @c ConfigureNotify event for
 * reparented clients so they can track their geometry relative to the
 * root window.
 *
 * @param connection XCB connection handle
 * @param client     Target client containing window and geometry data
 *
 * @note No action is taken if @p connection or @p client is null or if
 *       the client window is invalid
 * @note Geometry accounts for frame extents and enforces a minimum
 *       window size (@c WM_MIN_WINDOW_DIMENSION)
 * @note Complexity: @e O(1)
 */
static void s_handler_send_synthetic_configure_notify(
        xcb_connection_t *connection, const client_td *client)
{
    client_send_synthetic_configure_notify(connection, client);
}


/**
 * @brief Adjust frame position to keep the gravity anchor fixed on
 *        resize
 *
 * Computes the displacement that preserves the anchor point defined by
 * @p gravity after the frame changes from (@p old_w x @p old_h) t
 * (@p new_w x @p new_h) and adds it to @p *out_x and @p *out_y.
 * No-op for @c CLIENT_GRAVITY_NORTH_WEST and @c CLIENT_GRAVITY_STATIC.
 * See ICCCM §§4.1.2.3 and 4.1.5.
 *
 * @param out_x   Frame x to adjust in place
 * @param out_y   Frame y to adjust in place
 * @param old_w   Frame width before resize
 * @param old_h   Frame height before resize
 * @param new_w   Frame width after resize
 * @param new_h   Frame height after resize
 * @param gravity Client @a win_gravity value
 *
 * @note Complexity: @e O(1)
 */
static void s_gravity_adjust_pos(int32_t *restrict out_x,
        int32_t *restrict out_y,
        uint32_t old_w, uint32_t old_h,
        uint32_t new_w, uint32_t new_h,
        uint16_t gravity)
{
    int32_t dw = (int32_t) ((uint32_t) old_w - (uint32_t) new_w);
    int32_t dh = (int32_t) ((uint32_t) old_h - (uint32_t) new_h);

    if (gravity == (uint16_t) CLIENT_GRAVITY_NORTH_EAST ||
            gravity == (uint16_t) CLIENT_GRAVITY_EAST ||
            gravity == (uint16_t) CLIENT_GRAVITY_SOUTH_EAST) {
        *out_x = (int32_t) ((uint32_t) *out_x + (uint32_t) dw);
    } else if (gravity == (uint16_t) CLIENT_GRAVITY_NORTH ||
            gravity == (uint16_t) CLIENT_GRAVITY_CENTER ||
            gravity == (uint16_t) CLIENT_GRAVITY_SOUTH) {
        *out_x = (int32_t) ((uint32_t) *out_x + (uint32_t) (dw / 2));
    }

    if (gravity == (uint16_t) CLIENT_GRAVITY_SOUTH_EAST ||
            gravity == (uint16_t) CLIENT_GRAVITY_SOUTH ||
            gravity == (uint16_t) CLIENT_GRAVITY_SOUTH_WEST) {
        *out_y = (int32_t) ((uint32_t) *out_y + (uint32_t) dh);
    } else if (gravity == (uint16_t) CLIENT_GRAVITY_EAST ||
            gravity == (uint16_t) CLIENT_GRAVITY_CENTER ||
            gravity == (uint16_t) CLIENT_GRAVITY_WEST) {
        *out_y = (int32_t) ((uint32_t) *out_y + (uint32_t) (dh / 2));
    }
}


/**
 * @brief Strip WIDTH and/or HEIGHT from @p mask when the requested
 *        value exactly matches @p cur_dim, regardless of any
 *        transition or cooldown
 *
 * Requesting a dimension the window itself already has right now is
 * a true no-op: applying it changes nothing, so absorbing it carries
 * no risk no matter how much time has passed since anything last
 * happened to this client.  Called once, unconditionally, ahead of
 * either transition-specific cooldown below, since this same
 * reasoning holds regardless of which one (if either) might also
 * apply.
 *
 * @param event         Requested geometry to compare
 * @param mask          Value mask bits still under consideration
 * @param cur_dim       Width/height the window manager currently has
 *                      this client set to
 * @param is_reparented Whether this client has a separate frame
 *                      window of its own
 * @param on_inner      Whether @p event targets the content window
 *                      directly rather than the frame
 * @param extents       This client's own current frame extents
 *
 * @return @p mask, with WIDTH and/or HEIGHT cleared wherever its own
 *         requested value already matches what is currently set
 *
 * @note Complexity: @e O(1)
 */
static uint16_t s_handler_configure_wh_matches_current(
        const xcb_configure_request_event_t *event, uint16_t mask,
        struct dimensions_s cur_dim, bool is_reparented,
        bool on_inner, struct sides_s extents)
{
    if (mask & XCB_CONFIG_WINDOW_WIDTH) {
        uint32_t req_w = (is_reparented && on_inner)
            ? (uint32_t) event->width + (uint16_t) extents.left +
                (uint16_t) extents.right
            : (uint32_t) event->width;

        if (req_w == cur_dim.w) {
            mask = (uint16_t) (mask & ~XCB_CONFIG_WINDOW_WIDTH);
        }
    }

    if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
        uint32_t req_h = (is_reparented && on_inner)
            ? (uint32_t) event->height + (uint16_t) extents.top +
                (uint16_t) extents.bottom
            : (uint32_t) event->height;

        if (req_h == cur_dim.h) {
            mask = (uint16_t) (mask & ~XCB_CONFIG_WINDOW_HEIGHT);
        }
    }

    return mask;
}


/**
 * @brief Strip whatever bits of @p transition_mask overlap @p mask,
 *        when @p transition_time is still within @p cooldown_ms of
 *        now
 *
 * Shared by both of @a handler_configure_request's own post-transition
 * checks (shade/unshade, and entering/leaving fullscreen): a client
 * that reacts to the @c ConfigureNotify sequence a window-manager-
 * forced transition just sent it with a delayed @c ConfigureRequest
 * of its own, once it catches up processing that sequence, is far
 * more likely to be a stale echo of whatever geometry (or border
 * width) it had a moment before than an independent request it
 * genuinely wants honored now.
 *
 * WIDTH and HEIGHT specifically are only ever stripped here when the
 * request's own value, once adjusted the same way
 * @a s_handler_configure_wh_matches_current already is, also matches
 * @p old_dim (the client's own dimensions right before this
 * transition): a request for some other, genuinely different size
 * arriving within the same cooldown window is let through rather
 * than blanket-suppressed, since only a value already known to be
 * suspicious (either the imposed one, checked unconditionally by
 * @a s_handler_configure_wh_matches_current already, or the client's
 * own prior one, checked here) has any real chance of being this
 * exact kind of stale echo to begin with.  Every other bit
 * @p transition_mask carries (X, Y, and, for fullscreen,
 * @c XCB_CONFIG_WINDOW_BORDER_WIDTH) has no comparable "old" value
 * of its own worth checking against, so those stay exactly as
 * content-blind as before: stripped whenever @p mask overlaps them
 * at all, for as long as the cooldown itself is still active.
 *
 * @param event           The 'ConfigureRequest' event itself, for the
 *                        same WIDTH/HEIGHT comparison
 * @param mask            Value mask bits still under consideration
 * @param transition_mask Bits this particular transition's own
 *                        cooldown should strip, if still active
 * @param old_dim         Width/height the client itself had right
 *                        before this transition
 * @param is_reparented   Whether this client has a separate frame
 *                        window of its own
 * @param on_inner        Whether @p event targets the content window
 *                        directly rather than the frame
 * @param extents         This client's own current frame extents
 * @param transition_time Monotonic time the transition itself last
 *                        happened at
 * @param cooldown_ms     How long after @p transition_time a request
 *                        still counts as a stale echo
 * @param window          Client window, for the debug log line alone
 * @param kind            Short, human-readable name of the transition
 *                        ("shade" or "fullscreen"), for the same log
 *                        line
 *
 * @return @p mask, with @p transition_mask's own bits cleared as
 *         described above if the cooldown is still active; @p mask
 *         unchanged otherwise, including when @p mask does not
 *         overlap @p transition_mask to begin with
 *
 * @note Complexity: @e O(1)
 */
static uint16_t s_handler_configure_cooldown_mask(
        const xcb_configure_request_event_t *event, uint16_t mask,
        uint16_t transition_mask, struct dimensions_s old_dim,
        bool is_reparented, bool on_inner, struct sides_s extents,
        struct timespec transition_time, unsigned int cooldown_ms,
        xcb_window_t window, const char *kind)
{
    int64_t elapsed_ms;
    uint16_t strip_mask;

    if (!(mask & transition_mask)) {
        return mask;
    }

    elapsed_ms = (int64_t) clock_ms_since(&transition_time);

    if (elapsed_ms >= (int64_t) cooldown_ms) {
        return mask;
    }

    /* Every bit 'transition_mask' carries other than WIDTH/HEIGHT
     * (X, Y, and, for fullscreen, BORDER_WIDTH) stays exactly as
     * content-blind as before; only WIDTH/HEIGHT additionally
     * require matching 'old_dim' once found within the window
     * at all. */
    strip_mask = (uint16_t) (transition_mask &
            ~(XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT));

    if ((transition_mask & mask & XCB_CONFIG_WINDOW_WIDTH)) {
        uint32_t req_w = (is_reparented && on_inner)
            ? (uint32_t) event->width + (uint16_t) extents.left +
                (uint16_t) extents.right
            : (uint32_t) event->width;

        if (req_w == old_dim.w) {
            strip_mask |= XCB_CONFIG_WINDOW_WIDTH;
        }
    }

    if ((transition_mask & mask & XCB_CONFIG_WINDOW_HEIGHT)) {
        uint32_t req_h = (is_reparented && on_inner)
            ? (uint32_t) event->height + (uint16_t) extents.top +
                (uint16_t) extents.bottom
            : (uint32_t) event->height;

        if (req_h == old_dim.h) {
            strip_mask |= XCB_CONFIG_WINDOW_HEIGHT;
        }
    }

    if (!(mask & strip_mask)) {
        return mask;
    }

    LOGGER_DEBUG("Ignoring 'ConfigureRequest' for window=0x%x: %lld ms" \
            " after a %s transition, within the %u ms cooldown",
            window, (long long) elapsed_ms, kind, cooldown_ms);

    return (uint16_t) (mask & ~strip_mask);
}


/* Handle a 'CONFIGURE_REQUEST' event */
void handler_configure_request(xcb_connection_t *connection,
        list_td *surfaces, xcb_configure_request_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;
    uint16_t mask;
    uint16_t target_mask;
    uint32_t target_values[7];
    bool geom_changed;
    int i;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in configure request" \
                " handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Configure request event (window=0x%x, mask=0x%x)",
            event->window, event->value_mask);

    mask = event->value_mask &
        (XCB_CONFIG_WINDOW_X            |
         XCB_CONFIG_WINDOW_Y            |
         XCB_CONFIG_WINDOW_WIDTH        |
         XCB_CONFIG_WINDOW_HEIGHT       |
         XCB_CONFIG_WINDOW_BORDER_WIDTH |
         XCB_CONFIG_WINDOW_SIBLING      |
         XCB_CONFIG_WINDOW_STACK_MODE);

    client = lookup_find_client(surfaces, event->window,
            &surface, &desktop);

    /* A docked systray icon is not a managed client, so it would
     * otherwise fall through to the generic "forward the request
     * unmodified" path below, undoing the fixed size the tray forces
     * on every icon at dock time; see 'systray_icon_size_enforce'. */
    if (client == NULL && systray_icon_size_enforce(event->window)) {
        return;
    }

    if (client != NULL) {
        LOGGER_DEBUG("'ConfigureRequest' matched client window=0x%x:" \
                " frame=0x%x, decorated=%d, on_inner=%d, mask=0x%x," \
                " requested=%ux%u+%+d%+d, operation=%u",
                client->window, client->frame,
                (int) client_is_decorated(client),
                (int) (event->window == client->window),
                mask, event->width, event->height,
                event->x, event->y,
                (unsigned int) client->properties.operation);
    }

    geom_changed = false;
    target_mask = 0;
    i = 0;
    if (client != NULL) {
        bool wm_owns_geometry;
        bool is_reparented = (client->frame != 0) &&
            client_is_decorated(client);
        bool on_inner = (event->window == client->window);
        bool send_synth = false;
        xcb_window_t target = event->window;
        uint32_t req_w = client->layout.geometry.cur.dim.w;
        uint32_t req_h = client->layout.geometry.cur.dim.h;
        uint32_t old_w = client->layout.geometry.cur.dim.w;
        uint32_t old_h = client->layout.geometry.cur.dim.h;
        uint16_t left = (uint16_t) client->layout.frame_extents.left;
        uint16_t right = (uint16_t) client->layout.frame_extents.right;
        uint16_t top = (uint16_t) client->layout.frame_extents.top;
        uint16_t bottom = (uint16_t) client->layout.frame_extents.bottom;
        uint16_t geom_mask =
            XCB_CONFIG_WINDOW_X     |
            XCB_CONFIG_WINDOW_Y     |
            XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT;

        /* Ignored the same way, and for the same reason, whether the
         * WM itself is actively moving/resizing this client right
         * now, or the client is in fullscreen: either way, ITS OWN
         * request for a different position/size is a stale echo of
         * whatever geometry it would rather have, not something to
         * honor, since the WM (not the client) owns this window's
         * geometry for as long as either holds.  Without this,
         * a client that fixes its own size in WM_NORMAL_HINTS (e.g.,
         * min_width == max_width) and reacts to being forced into
         * fullscreen by re-requesting its own preferred size right
         * back would immediately shrink back down, undoing
         * 'ccmd_client_fullscreen''s own deliberate choice (see its
         * own doc comment in cmds/client/state.c) to bypass every one
         * of the client's size hints while fullscreen. */
        wm_owns_geometry =
            client->properties.operation == CLIENT_OPERATION_MOVING ||
            client->properties.operation == CLIENT_OPERATION_RESIZING ||
            client_is_fullscreen(client);
        if (wm_owns_geometry && (mask & geom_mask)) {
            mask = (uint16_t) (mask & ~geom_mask);
            if (mask == 0) {
                if (connection != NULL && is_reparented) {
                    s_handler_send_synthetic_configure_notify(connection,
                            client);
                    xcb_flush(connection);
                }
                return;
            }
        }

        /* A request for a dimension the client already has right now
         * is a true no-op regardless of any transition or cooldown;
         * see 's_handler_configure_wh_matches_current' itself for
         * why this alone is always safe. */
        mask = s_handler_configure_wh_matches_current(event, mask,
                client->layout.geometry.cur.dim,
                is_reparented, on_inner, client->layout.frame_extents);
        if (mask == 0) {
            if (connection != NULL && is_reparented) {
                s_handler_send_synthetic_configure_notify(connection,
                        client);
                xcb_flush(connection);
            }
            return;
        }

        /* Ignore a geometry request that lands shortly after the
         * window manager itself just shaded or unshaded this client:
         * see 'WM_SHADE_CONFIGURE_COOLDOWN_MS' for why such a request
         * is far more likely to be the client's own delayed, stale
         * reaction to that transition than an independent resize it
         * actually wants. */
        mask = s_handler_configure_cooldown_mask(event, mask, geom_mask,
                client->layout.geometry.old.dim,
                is_reparented, on_inner, client->layout.frame_extents,
                client->shade_transition_time,
                (unsigned int) WM_SHADE_CONFIGURE_COOLDOWN_MS,
                client->window, "shade");
        if (mask == 0) {
            if (connection != NULL && is_reparented) {
                s_handler_send_synthetic_configure_notify(connection,
                        client);
                xcb_flush(connection);
            }
            return;
        }

        /* Same reasoning, for the same underlying mechanism, right
         * after entering or leaving fullscreen instead of a shade or
         * unshade; also covers 'XCB_CONFIG_WINDOW_BORDER_WIDTH', not
         * just 'geom_mask', since 'ccmd_client_unfullscreen' restores
         * the client's own border width as part of the same
         * transition this guards, and a stale echo touching only
         * that field would otherwise slip through 'geom_mask' alone
         * and silently undo it, well after the point in this
         * function that already applies every other bit in 'mask'
         * unconditionally. */
        mask = s_handler_configure_cooldown_mask(event, mask,
                (uint16_t) (geom_mask | XCB_CONFIG_WINDOW_BORDER_WIDTH),
                client->layout.geometry.old.dim,
                is_reparented, on_inner, client->layout.frame_extents,
                client->fullscreen_transition_time,
                (unsigned int) WM_FULLSCREEN_CONFIGURE_COOLDOWN_MS,
                client->window, "fullscreen");
        if (mask == 0) {
            if (connection != NULL && is_reparented) {
                s_handler_send_synthetic_configure_notify(connection,
                        client);
                xcb_flush(connection);
            }
            return;
        }

        if (is_reparented) {
            target = client->frame;
        }

        if (mask & XCB_CONFIG_WINDOW_X) {
            int32_t req_x;

            if (client->has_rule_position_locked) {
                /* Position was fixed by a rule; reject the client's
                 * attempt to move the window and keep the locked X */
                send_synth = is_reparented;
            } else {
                if (is_reparented && on_inner) {
                    req_x = (int32_t) ((uint32_t) (int32_t) event->x -
                            (uint32_t) left);
                } else {
                    req_x = event->x;
                }
                if ((uint32_t) req_x !=
                        (uint32_t) client->layout.geometry.cur.pos.x) {
                    geom_changed = true;
                }

                target_values[i++] = (uint32_t) req_x;
                target_mask |= XCB_CONFIG_WINDOW_X;
                client->layout.geometry.cur.pos.x = req_x;
                send_synth = is_reparented;
            }
        }

        if (mask & XCB_CONFIG_WINDOW_Y) {
            int32_t req_y;

            if (client->has_rule_position_locked) {
                /* Position was fixed by a rule; reject the client's
                 * attempt to move the window and keep the locked Y */
                send_synth = is_reparented;
            } else {
                if (is_reparented && on_inner) {
                    /* Clamp before subtracting to keep req_y >= 0 and
                     * avoid the "X - C < 0 => X < C" strict-overflow
                     * transformation */
                    req_y = ((int32_t) event->y > (int32_t) top)
                        ? (int32_t) ((uint32_t) (int32_t) event->y -
                                (uint32_t) top)
                        : 0;
                } else {
                    req_y = (int32_t) event->y;
                    if (req_y < 0) {
                        req_y = 0;
                    }
                }

                if ((uint32_t) req_y !=
                        (uint32_t) client->layout.geometry.cur.pos.y) {
                    geom_changed = true;
                }

                target_values[i++] = (uint32_t) req_y;
                target_mask |= XCB_CONFIG_WINDOW_Y;
                client->layout.geometry.cur.pos.y = req_y;
                send_synth = is_reparented;
            }
        }

        if (mask & XCB_CONFIG_WINDOW_WIDTH) {
            if (is_reparented && on_inner) {
                req_w = (uint32_t) event->width + left + right;
            } else {
                req_w = (uint32_t) event->width;
            }

            if (req_w != client->layout.geometry.cur.dim.w) {
                geom_changed = true;
            }

            target_values[i++] = req_w;
            target_mask |= XCB_CONFIG_WINDOW_WIDTH;
            client->layout.geometry.cur.dim.w = req_w;
            send_synth = is_reparented;
        }

        if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
            if (is_reparented && on_inner) {
                req_h = (uint32_t) event->height + top + bottom;
            } else {
                req_h = (uint32_t) event->height;
            }

            if (req_h != client->layout.geometry.cur.dim.h) {
                geom_changed = true;
            }

            target_values[i++] = req_h;
            target_mask |= XCB_CONFIG_WINDOW_HEIGHT;
            client->layout.geometry.cur.dim.h = req_h;
            send_synth = is_reparented;
        }

        if (mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
            target_values[i++] = (uint32_t) event->border_width;
            target_mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
        }

        if (mask & XCB_CONFIG_WINDOW_SIBLING) {
            target_values[i++] = event->sibling;
            target_mask |= XCB_CONFIG_WINDOW_SIBLING;
        }

        if (mask & XCB_CONFIG_WINDOW_STACK_MODE) {
            target_values[i++] = (uint32_t) event->stack_mode;
            target_mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        }

        /* Honor win_gravity (ICCCM §§4.1.2.3 and 4.1.5): when only the
         * size changes without an explicit new position, keep the
         * gravity anchor point fixed by adjusting the frame position.
         * X/Y have lower mask bits than W/H, so the values array must
         * be prepended and any higher-bit values shifted up by two. */
        if ((target_mask & (XCB_CONFIG_WINDOW_WIDTH |
                        XCB_CONFIG_WINDOW_HEIGHT)) &&
                !(mask & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y)) &&
                client->layout.gravity != 0u &&
                client->layout.gravity !=
                    (uint16_t) CLIENT_GRAVITY_NORTH_WEST &&
                client->layout.gravity !=
                    (uint16_t) CLIENT_GRAVITY_STATIC) {
            int32_t adj_x = client->layout.geometry.cur.pos.x;
            int32_t adj_y = client->layout.geometry.cur.pos.y;

            s_gravity_adjust_pos(&adj_x, &adj_y, old_w, old_h,
                    req_w, req_h, client->layout.gravity);
            if ((uint32_t) adj_x
                    != (uint32_t) client->layout.geometry.cur.pos.x ||
                    (uint32_t) adj_y
                    != (uint32_t) client->layout.geometry.cur.pos.y) {
                for (int j = i - 1; j >= 0; --j) {
                    target_values[j + 2] = target_values[j];
                }
                target_values[0] = (uint32_t) adj_x;
                target_values[1] = (uint32_t) adj_y;
                target_mask |= XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
                client->layout.geometry.cur.pos.x = adj_x;
                client->layout.geometry.cur.pos.y = adj_y;
                send_synth = is_reparented;
                geom_changed = true;
            }
        }

        if (target_mask != 0 && connection != NULL) {
            xcb_configure_window(connection, target,
                    target_mask, target_values);
            if (is_reparented) {
                client_decoration_layout_sync(client);
                if (send_synth) {
                    s_handler_send_synthetic_configure_notify(connection,
                            client);
                }
            }

            xcb_flush(connection);
        }
    } else {
        if (mask & XCB_CONFIG_WINDOW_X) {
            target_values[i++] = (uint32_t) event->x;
            target_mask |= XCB_CONFIG_WINDOW_X;
        }

        if (mask & XCB_CONFIG_WINDOW_Y) {
            target_values[i++] = (uint32_t) event->y;
            target_mask |= XCB_CONFIG_WINDOW_Y;
        }

        if (mask & XCB_CONFIG_WINDOW_WIDTH) {
            target_values[i++] = (uint32_t) event->width;
            target_mask |= XCB_CONFIG_WINDOW_WIDTH;
        }

        if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
            target_values[i++] = (uint32_t) event->height;
            target_mask |= XCB_CONFIG_WINDOW_HEIGHT;
        }

        if (mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
            target_values[i++] = (uint32_t) event->border_width;
            target_mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
        }

        if (mask & XCB_CONFIG_WINDOW_SIBLING) {
            target_values[i++] = event->sibling;
            target_mask |= XCB_CONFIG_WINDOW_SIBLING;
        }

        if (mask & XCB_CONFIG_WINDOW_STACK_MODE) {
            target_values[i++] = (uint32_t) event->stack_mode;
            target_mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        }

        if (target_mask != 0 && connection != NULL) {
            xcb_configure_window(connection, event->window,
                    target_mask, target_values);
            xcb_flush(connection);
        }
    }

    if (geom_changed || (mask & XCB_CONFIG_WINDOW_STACK_MODE)) {
        if (mask & XCB_CONFIG_WINDOW_STACK_MODE) {
            ccmd_desktop_enforce_layers(desktop);
        }

        if (client != NULL && geom_changed) {
            /* A client-initiated resize already had its frame
             * reconfigured directly above, but the per-client
             * decoration repaint (border, titlebar
             * background/text/buttons) in the next render pass only
             * runs for clients with 'is_outdated' set */
            LOGGER_TRACE("Marking window=0x%x outdated after" \
                    " 'ConfigureRequest'" \
                    " (new frame geometry %ux%u+%+d%+d)",
                    client->window,
                    client->layout.geometry.cur.dim.w,
                    client->layout.geometry.cur.dim.h,
                    client->layout.geometry.cur.pos.x,
                    client->layout.geometry.cur.pos.y);
            wm_outdate_client(client);
        }

        wm_outdate_surface(surface);
        wm_outdate_desktop(desktop);
    }
}


/* Handle a 'CONFIGURE_NOTIFY' event */
void handler_configure_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_configure_notify_event_t *event)
{
    client_td *client;
    surface_td *surface = NULL;
    desktop_td *desktop = NULL;

    (void) connection;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in configure handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Configure notify event (window=0x%x," \
            " geom=%ux%u+%+d%+d)",
            event->window, event->width, event->height,
            event->x, event->y);

    client = lookup_find_client(surfaces, event->window,
            &surface, &desktop);
    if (client != NULL) {
        bool is_frame = (client->frame != 0)
            ? (event->window == client->frame)
            : (event->window == client->window ||
               event->window == client->id);
        bool is_inner = (event->window == client->window);

        if (is_frame) {
            bool geom_changed;
            bool size_changed;
            bool is_focused = (desktop != NULL) &&
                (desktop->client_active_id == client->id);

            /* For undecorated clients the window is its own frame and
             * lives as a direct root child.  The X server delivers
             * 'ConfigureNotify' events via two routes:
             *
             *  - 'StructureNotify' ('event->event == window'):
             *    reliable, reflects the position the window manager
             *    last configured.
             *  - 'SubStructureNotify' on root ('event->event !=
             *    window'): also generated for every 'ConfigureWindow'
             *    the window manager issued on the client, including the
             *    border-width adjustment that happens before placement
             *    in client_init.  That pre-placement event carries
             *    the application's initial position, often (0,0), which
             *    can arrive late (after place_window_apply already
             *    stored the centered coordinates) and corrupt the
             *    stored position.  When the subsequent render uses the
             *    corrupted coordinates the window is moved to the
             *    wrong position, which in turn queues another stale
             *    'SubStructureNotify', creating a render loop that
             *    manifests as continuous flickering until the window is
             *    iconified/restored.
             *
             * Ignore 'SubStructureNotify'-delivered 'ConfigureNotify'
             * events for all managed clients (decorated and undecorated
             * alike): the 'StructureNotify' copy (same data, always
             * correct) handles all legitimate updates.  For decorated
             * clients this also prevents stale 'SubStructureNotify'
             * events from placement (place_window_apply) from
             * overwriting the position set by rules_apply: when the
             * event loop sees the stale placement 'SubStructureNotify'
             * it would update the stored position and trigger
             * a re-render, which then re-configures the frame to the
             * old placement position, overriding the rules-specified
             * position entirely. */
            if (event->event != event->window) {
                return;
            }

            geom_changed =
                client->layout.geometry.cur.pos.x !=
                    (int32_t) event->x ||
                client->layout.geometry.cur.pos.y !=
                    (int32_t) event->y ||
                client->layout.geometry.cur.dim.w !=
                    (uint32_t) event->width ||
                client->layout.geometry.cur.dim.h !=
                    (uint32_t) event->height;
            size_changed =
                client->layout.geometry.cur.dim.w !=
                    (uint32_t) event->width ||
                client->layout.geometry.cur.dim.h !=
                    (uint32_t) event->height;

            client->layout.geometry.cur.pos.x = event->x;
            client->layout.geometry.cur.pos.y = event->y;
            client->layout.geometry.cur.dim.w = event->width;
            client->layout.geometry.cur.dim.h = event->height;

            /* Trigger a re-render only when the frame geometry actually
             * changed.  Guarding with 'geom_changed' prevents the
             * feedback loop where the render itself configures the
             * frame to the same dimensions and the resulting
             * 'ConfigureNotify' would re-mark the desktop as
             * outdated. */
            if (size_changed) {
                /* Only redraw the decoration (border, titlebar
                 * background, title text, buttons) when the frame's
                 * SIZE changed: a pure move (position-only) leaves
                 * every one of those pixels correct as-is, since the
                 * X server already relocates the window's rendered
                 * content for free.  Previously this ran on every
                 * single 'ConfigureNotify' during an interactive drag,
                 * including plain moves, which is why dragging
                 * a decorated window felt noticeably heavier than an
                 * undecorated one: each mouse-motion step was paying
                 * for a full titlebar repaint (clear, background,
                 * font-rendered title text, three button glyphs) that
                 * a move never actually needed. */
                if (!(client_is_fullscreen(client) &&
                            client->was_decorated_fullscreen)) {
                    desktop_repaint_frame_decoration(connection, client,
                            is_focused,
                            (desktop != NULL) ? desktop->config_theme
                                : ((client->config != NULL)
                                        ? &client->config->theme : NULL));
                }
                if (connection != NULL) {
                    xcb_flush(connection);
                }
            }
            if (geom_changed) {
                wm_outdate_client(client);
                wm_outdate_surface(surface);
                wm_outdate_desktop(desktop);
            } /* ! if (geom_changed) */
        } else if (is_inner &&
                client->frame != 0 &&
                client_is_decorated(client) &&
                !client_is_fullscreen(client)) {
            uint16_t left;
            uint16_t top;

            left = (uint16_t) client->layout.frame_extents.left;
            top = (uint16_t) client->layout.frame_extents.top;

            /* The inner window's position within the frame must always
             * be (left, top).  If something moved it (rare), put it
             * back.
             *
             * Size changes are intentionally NOT reacted to here.  The
             * window manager controls the inner window size exclusively
             * through 'client_decoration_layout_sync'; reacting to
             * a stale 'ConfigureNotify' with a different size would:
             *
             *   1. overwrite the stored geometry with the pre-snap
             *      value;
             *   2. call client_decoration_layout_sync again, generating
             *      another 'ConfigureNotify' with the old size;
             *   3. create a feedback loop visible as
             *      size-hint-constrained applications flickering and
             *      collapsing during mouse resize, or losing one
             *      character row on every keyboard resize keypress. */
            if ((int32_t) event->x != (int32_t) left ||
                    (int32_t) event->y != (int32_t) top) {
                client_decoration_layout_sync(client);
            }
        } /* ! if (is_frame) */
    } /* ! if (client) */
}
