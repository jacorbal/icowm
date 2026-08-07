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

/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Command includes */
#include <cmds/layer.h>

/* Render includes */
#include <render/desktop.h>

/* Default initial values */
#include <defs/wm.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <invalidate.h>
#include <logger.h>
#include <surface.h>
#include <systray.h>
#include <lookup.h>

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
        xcb_connection_t *connection, client_td *client)
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
 * See ICCCM §4.1.2.3 and §4.1.5.
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
static void s_gravity_adjust_pos(int32_t *out_x, int32_t *out_y,
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
    bool interactive_geom;
    int i;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in configure request" \
                " handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Configure request event: window=0x%x, mask=0x%x",
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
     * on every icon at dock time; see 'systray_enforce_icon_size'. */
    if (client == NULL && systray_enforce_icon_size(event->window)) {
        return;
    }

    if (client != NULL) {
        LOGGER_TRACE("'ConfigureRequest' matched client window=0x%x:" \
                " frame=0x%x, decorated=%d, on_inner=%d, mask=0x%x," \
                " requested=%ux%u+%d+%d, operation=%u",
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
        bool is_reparented = (client->frame != 0) &&
            client_is_decorated(client);
        bool on_inner = (event->window == client->window);
        bool send_synth = false;
        xcb_window_t target = event->window;
        int32_t req_x = client->layout.geometry.cur.pos.x;
        int32_t req_y = client->layout.geometry.cur.pos.y;
        uint32_t req_w = client->layout.geometry.cur.dim.w;
        uint32_t req_h = client->layout.geometry.cur.dim.h;
        uint32_t old_w = client->layout.geometry.cur.dim.w;
        uint32_t old_h = client->layout.geometry.cur.dim.h;
        int32_t adj_x = 0;
        int32_t adj_y = 0;
        uint16_t left = (uint16_t) client->layout.frame_extents.left;
        uint16_t right = (uint16_t) client->layout.frame_extents.right;
        uint16_t top = (uint16_t) client->layout.frame_extents.top;
        uint16_t bottom = (uint16_t) client->layout.frame_extents.bottom;
        uint16_t geom_mask =
            XCB_CONFIG_WINDOW_X     |
            XCB_CONFIG_WINDOW_Y     |
            XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT;

        interactive_geom =
            client->properties.operation == CLIENT_OPERATION_MOVING ||
            client->properties.operation == CLIENT_OPERATION_RESIZING;
        if (interactive_geom && (mask & geom_mask)) {
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

        if (is_reparented) {
            target = client->frame;
        }

        if (mask & XCB_CONFIG_WINDOW_X) {
            if (client->rule_position_locked) {
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
            if (is_reparented && on_inner) {
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

        /* Honor win_gravity (ICCCM §4.1.2.3 and §4.1.5): when only the
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
            adj_x = client->layout.geometry.cur.pos.x;
            adj_y = client->layout.geometry.cur.pos.y;
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
                i += 2;
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
                client_sync_decoration_layout(client);
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
            wcmd_desktop_enforce_layers(desktop);
        }

        if (client != NULL && geom_changed) {
            /* A client-initiated resize already had its frame
             * reconfigured directly above, but the per-client
             * decoration repaint (border, titlebar
             * background/text/buttons) in the next render pass only
             * runs for clients with 'is_outdated' set */
            LOGGER_TRACE("Marking window=0x%x outdated after" \
                    " 'ConfigureRequest' (new frame geometry %ux%u+%d+%d)",
                    client->window,
                    client->layout.geometry.cur.dim.w,
                    client->layout.geometry.cur.dim.h,
                    client->layout.geometry.cur.pos.x,
                    client->layout.geometry.cur.pos.y);
            wm_invalidate_client(client);
        }

        wm_invalidate_surface(surface);
        wm_invalidate_desktop(desktop);
    }
}


/* Handle a 'CONFIGURE_NOTIFY' event */
void handler_configure_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_configure_notify_event_t *event)
{
    client_td *client;
    surface_td *surface = NULL;
    desktop_td *desktop = NULL;
    bool geom_changed;
    bool size_changed;

    (void) connection;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in configure handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Configure notify event: window=0x%x, geom=%ux%u+%d+%d",
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
             *    in client_manage.  That pre-placement event carries
             *    the application's initial position, often (0,0), which
             *    can arrive late (after place_apply already stored the
             *    centred coordinates) and corrupt the stored position.
             *    When the subsequent render uses the corrupted
             *    coordinates the window is moved to the wrong position,
             *    which in turn queues another stale
             *    'SubStructureNotify', creating a render loop that
             *    manifests as continuous flickering until the window is
             *    iconified/restored.
             *
             * Ignore 'SubStructureNotify'-delivered 'ConfigureNotify'
             * events for all managed clients (decorated and undecorated
             * alike): the 'StructureNotify' copy (same data, always
             * correct) handles all legitimate updates.  For decorated
             * clients this also prevents stale 'SubStructureNotify'
             * events from placement (place_apply) from overwriting the
             * position set by rules_apply: when the event loop sees the
             * stale placement 'SubStructureNotify' it would update the
             * stored position and trigger a re-render, which then
             * re-configures the frame to the old placement position,
             * overriding the rules-specified position entirely. */
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
                desktop_repaint_frame_decoration(connection, client,
                        is_focused,
                        (desktop != NULL) ? desktop->config_theme
                                          : client->theme);
                if (connection != NULL) {
                    xcb_flush(connection);
                }
            }
            if (geom_changed) {
                wm_invalidate_surface(surface);
                wm_invalidate_desktop(desktop);
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
             * through 'client_sync_decoration_layout'; reacting to
             * a stale 'ConfigureNotify' with a different size would:
             *
             *   1. overwrite the stored geometry with the pre-snap
             *      value;
             *   2. call client_sync_decoration_layout again, generating
             *      another 'ConfigureNotify' with the old size;
             *   3. create a feedback loop visible as
             *      size-hint-constrained applications flickering and
             *      collapsing during mouse resize, or losing one
             *      character row on every keyboard resize keypress. */
            if ((int32_t) event->x != (int32_t) left ||
                    (int32_t) event->y != (int32_t) top) {
                client_sync_decoration_layout(client);
            }
        } /* ! if (is_frame) */
    } /* ! if (client) */
}
