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

/* Utils includes */
#include <utils/time/clock.h>

/* Command includes */
#include <cmds/client/layer.h>

/* Render includes */
#include <render/client/decoration.h>
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

/* Local includes */
#include <handler.h>


/**
 * @brief What building one @c ConfigureRequest reply needs
 *
 * Gathered so the per-field work and the gravity adjustment can each be
 * a function of its own rather than another hundred lines inside
 * @a handler_configure_request.  @p values and @p count are the value
 * list @c xcb_configure_window is handed, filled in ascending mask-bit
 * order as X itself requires.
 */
struct s_configure_ctx_s {
    client_td *client;
    const xcb_configure_request_event_t *event;

    uint32_t values[7];
    uint32_t req_w;
    uint32_t req_h;
    uint32_t old_w;
    uint32_t old_h;

    uint16_t mask;
    uint16_t target_mask;
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;

    unsigned int count;

    bool is_reparented;
    bool on_inner;
    bool send_synth;
    bool geom_changed;
};


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
 * @brief Acknowledge a request this handler decided to ignore
 *
 * ICCCM asks a window manager that does not honor a @c ConfigureRequest
 * to say so, by sending the client a synthetic @c ConfigureNotify
 * carrying the geometry the window actually has.  A client waiting on
 * the reply to a silently dropped request would otherwise wait forever.
 *
 * @param connection    XCB connection; may be @c NULL
 * @param client        Client whose request was ignored
 * @param is_reparented Whether the client sits inside a frame, the only
 *                      case the notify is needed in
 *
 * @note Complexity: @e O(1)
 */
static void s_handler_configure_acknowledge(
        xcb_connection_t *connection, const client_td *client,
        bool is_reparented)
{
    if (connection == NULL || !is_reparented) {
        return;
    }

    s_handler_send_synthetic_configure_notify(connection, client);
}


/**
 * @brief Forward a request for a window this window manager does not
 *        manage
 *
 * Every field the client asked for passes through untouched, since
 * there is no frame, no size hint and no policy of ours to reconcile it
 * against.
 *
 * @param connection XCB connection; may be @c NULL
 * @param event      The request as it arrived
 * @param mask       Fields of @p event to forward
 *
 * @note Complexity: @e O(1)
 */
static void s_handler_configure_forward(xcb_connection_t *connection,
        const xcb_configure_request_event_t *event, uint16_t mask)
{
    uint32_t values[7];
    uint16_t target_mask = 0u;
    unsigned int i = 0u;

    if (mask & XCB_CONFIG_WINDOW_X) {
        values[i++] = (uint32_t) event->x;
        target_mask |= XCB_CONFIG_WINDOW_X;
    }

    if (mask & XCB_CONFIG_WINDOW_Y) {
        values[i++] = (uint32_t) event->y;
        target_mask |= XCB_CONFIG_WINDOW_Y;
    }

    if (mask & XCB_CONFIG_WINDOW_WIDTH) {
        values[i++] = (uint32_t) event->width;
        target_mask |= XCB_CONFIG_WINDOW_WIDTH;
    }

    if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
        values[i++] = (uint32_t) event->height;
        target_mask |= XCB_CONFIG_WINDOW_HEIGHT;
    }

    if (mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
        values[i++] = (uint32_t) event->border_width;
        target_mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
    }

    if (mask & XCB_CONFIG_WINDOW_SIBLING) {
        values[i++] = event->sibling;
        target_mask |= XCB_CONFIG_WINDOW_SIBLING;
    }

    if (mask & XCB_CONFIG_WINDOW_STACK_MODE) {
        values[i++] = (uint32_t) event->stack_mode;
        target_mask |= XCB_CONFIG_WINDOW_STACK_MODE;
    }

    if (target_mask != 0 && connection != NULL) {
        xcb_configure_window(connection, event->window, target_mask,
                values);
    }
}


/**
 * @brief Settle a configure request's X coordinate
 *
 * @param ctx What the request is being built into
 *
 * @note A position fixed by a rule is kept, the request answered with
 *       a synthetic notify rather than obeyed
 * @note Complexity: @e O(1)
 */
static void s_configure_build_x(struct s_configure_ctx_s *ctx)
{
if (ctx->mask & XCB_CONFIG_WINDOW_X) {
    int32_t req_x;

    if (ctx->client->has_rule_position_locked) {
        /* Position was fixed by a rule; reject the client's
         * attempt to move the window and keep the locked X */
        ctx->send_synth = ctx->is_reparented;
    } else {
        if (ctx->is_reparented && ctx->on_inner) {
            req_x = (int32_t) ((uint32_t) (int32_t) ctx->event->x -
                    (uint32_t) ctx->left);
        } else {
            req_x = ctx->event->x;
        }
        if ((uint32_t) req_x !=
                (uint32_t) ctx->client->layout.geometry.cur.pos.x) {
            ctx->geom_changed = true;
        }

        ctx->values[ctx->count++] = (uint32_t) req_x;
        ctx->target_mask |= XCB_CONFIG_WINDOW_X;
        ctx->client->layout.geometry.cur.pos.x = req_x;
        ctx->send_synth = ctx->is_reparented;
    }
}
}


/**
 * @brief Settle a configure request's Y coordinate
 *
 * @param ctx What the request is being built into
 *
 * @note A position fixed by a rule is kept, the request answered with
 *       a synthetic notify rather than obeyed
 * @note Complexity: @e O(1)
 */
static void s_configure_build_y(struct s_configure_ctx_s *ctx)
{
if (ctx->mask & XCB_CONFIG_WINDOW_Y) {
    int32_t req_y;

    if (ctx->client->has_rule_position_locked) {
        /* Position was fixed by a rule; reject the client's
         * attempt to move the window and keep the locked Y */
        ctx->send_synth = ctx->is_reparented;
    } else {
        if (ctx->is_reparented && ctx->on_inner) {
            /* Clamp before subtracting to keep req_y >= 0 and
             * avoid the "X - C < 0 => X < C" strict-overflow
             * transformation */
            req_y = ((int32_t) ctx->event->y > (int32_t) ctx->top)
                ? (int32_t) ((uint32_t) (int32_t) ctx->event->y -
                        (uint32_t) ctx->top)
                : 0;
        } else {
            req_y = (int32_t) ctx->event->y;
            if (req_y < 0) {
                req_y = 0;
            }
        }

        if ((uint32_t) req_y !=
                (uint32_t) ctx->client->layout.geometry.cur.pos.y) {
            ctx->geom_changed = true;
        }

        ctx->values[ctx->count++] = (uint32_t) req_y;
        ctx->target_mask |= XCB_CONFIG_WINDOW_Y;
        ctx->client->layout.geometry.cur.pos.y = req_y;
        ctx->send_synth = ctx->is_reparented;
    }
}
}


/**
 * @brief Turn each requested field into a value for the reply
 *
 * Walks the mask in ascending bit order, which is the order X expects
 * the value list in, translating a request aimed at the content window
 * into frame coordinates where the client is reparented.
 *
 * @param ctx State of the request being built
 *
 * @note Complexity: @e O(1)
 */
static void s_handler_configure_build(struct s_configure_ctx_s *ctx)
{
    s_configure_build_x(ctx);
    s_configure_build_y(ctx);

    if (ctx->mask & XCB_CONFIG_WINDOW_WIDTH) {
        if (ctx->is_reparented && ctx->on_inner) {
            ctx->req_w = (uint32_t) ctx->event->width +
                ctx->left + ctx->right;
        } else {
            ctx->req_w = (uint32_t) ctx->event->width;
        }

        if (ctx->req_w != ctx->client->layout.geometry.cur.dim.w) {
            ctx->geom_changed = true;
        }

        ctx->values[ctx->count++] = ctx->req_w;
        ctx->target_mask |= XCB_CONFIG_WINDOW_WIDTH;
        ctx->client->layout.geometry.cur.dim.w = ctx->req_w;
        ctx->send_synth = ctx->is_reparented;
    }

    if (ctx->mask & XCB_CONFIG_WINDOW_HEIGHT) {
        if (ctx->is_reparented && ctx->on_inner) {
            ctx->req_h = (uint32_t) ctx->event->height +
                ctx->top + ctx->bottom;
        } else {
            ctx->req_h = (uint32_t) ctx->event->height;
        }

        if (ctx->req_h != ctx->client->layout.geometry.cur.dim.h) {
            ctx->geom_changed = true;
        }

        ctx->values[ctx->count++] = ctx->req_h;
        ctx->target_mask |= XCB_CONFIG_WINDOW_HEIGHT;
        ctx->client->layout.geometry.cur.dim.h = ctx->req_h;
        ctx->send_synth = ctx->is_reparented;
    }

    if (ctx->mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
        ctx->values[ctx->count++] = (uint32_t) ctx->event->border_width;
        ctx->target_mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
    }

    if (ctx->mask & XCB_CONFIG_WINDOW_SIBLING) {
        ctx->values[ctx->count++] = ctx->event->sibling;
        ctx->target_mask |= XCB_CONFIG_WINDOW_SIBLING;
    }

    if (ctx->mask & XCB_CONFIG_WINDOW_STACK_MODE) {
        ctx->values[ctx->count++] = (uint32_t) ctx->event->stack_mode;
        ctx->target_mask |= XCB_CONFIG_WINDOW_STACK_MODE;
    }
}


/**
 * @brief Keep the gravity anchor fixed when only the size changed
 *
 * @param ctx State of the request being built
 *
 * @note Complexity: @e O(1)
 */
static void s_handler_configure_gravity(
        struct s_configure_ctx_s *ctx)
{
    /* Honor 'win_gravity' (ICCCM §§4.1.2.3 and 4.1.5).  When only the
     * size changes without an explicit new position, keep the
     * gravity anchor point fixed by adjusting the frame position.
     * X/Y have lower ctx->mask bits than W/H, so the values array must
     * be prepended and any higher-bit values shifted up by two. */
    if ((ctx->target_mask & (XCB_CONFIG_WINDOW_WIDTH |
                    XCB_CONFIG_WINDOW_HEIGHT)) &&
            !(ctx->mask &
                (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y)) &&
            ctx->client->layout.gravity != 0u &&
            ctx->client->layout.gravity !=
                (uint16_t) CLIENT_GRAVITY_NORTH_WEST &&
            ctx->client->layout.gravity !=
                (uint16_t) CLIENT_GRAVITY_STATIC) {
        int32_t adj_x = ctx->client->layout.geometry.cur.pos.x;
        int32_t adj_y = ctx->client->layout.geometry.cur.pos.y;

        client_gravity_adjust_pos(&adj_x, &adj_y,
                ctx->old_w, ctx->old_h,
                ctx->req_w, ctx->req_h, ctx->client->layout.gravity);
        if ((uint32_t) adj_x
                != (uint32_t) ctx->client->layout.geometry.cur.pos.x ||
                (uint32_t) adj_y
                != (uint32_t) ctx->client->layout.geometry.cur.pos.y) {
            /* Counted in an unsigned index on purpose.  A signed
             * one lets the optimizer assume its arithmetic never
             * overflows, which is what '-Wstrict-overflow' reports
             * on from level three up, with no source location of
             * its to point at.
             *
             * The highest index written here is 'count' plus one,
             * and 'count' can be at most 5, since this block only
             * runs when neither X nor Y is in the mask, leaving
             * WIDTH, HEIGHT, BORDER_WIDTH, SIBLING and STACK_MODE
             * as the only bits that could have filled it.  That
             * puts the last write at index 6, the final slot of
             * the array. */
            for (unsigned int j = ctx->count; j > 0u; --j) {
                ctx->values[j + 1u] = ctx->values[j - 1u];
            }
            ctx->values[0] = (uint32_t) adj_x;
            ctx->values[1] = (uint32_t) adj_y;
            ctx->target_mask |=
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
            ctx->client->layout.geometry.cur.pos.x = adj_x;
            ctx->client->layout.geometry.cur.pos.y = adj_y;
            ctx->send_synth = ctx->is_reparented;
            ctx->geom_changed = true;
        }
    }
}


/**
 * @brief Strip WIDTH and/or HEIGHT from @p mask when the requested
 *        value exactly matches @p cur_dim, regardless of any transition
 *        or cooldown
 *
 * Requesting a dimension the window itself already has right now is
 * a true no-op: applying it changes nothing, so absorbing it carries no
 * risk no matter how much time has passed since anything last happened
 * to this client.  Called once, unconditionally, ahead of either
 * transition-specific cooldown below, since this same reasoning holds
 * regardless of which one (if either) might also apply.
 *
 * @param event         Requested geometry to compare
 * @param mask          Value mask bits still under consideration
 * @param cur_dim       Width/height the window manager currently has
 *                      this client set to
 * @param is_reparented Whether this client has a separate frame
 *                      window of its own
 * @param on_inner      Whether @p event targets the content window
 *                      directly rather than the frame
 * @param extents       This client's current frame extents
 *
 * @return @p mask, with WIDTH and/or HEIGHT cleared wherever its
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
 *        when @p transition_time is still within @p cooldown_ms of now
 *
 * Shared by both of @a handler_configure_request's post-transition
 * checks (shade/unshade, and entering/leaving fullscreen).  A client
 * that reacts to the @c ConfigureNotify sequence a window-manager-
 * forced transition just sent it with a delayed @c ConfigureRequest of
 * its own, once it catches up processing that sequence, is far more
 * likely to be a stale echo of whatever geometry (or border width) it
 * had a moment before than an independent request it genuinely wants
 * honored now.
 *
 * WIDTH and HEIGHT specifically are only ever stripped here when the
 * request's value, once adjusted the same way
 * @a s_handler_configure_wh_matches_current already is, also matches
 * @p old_dim (the client's dimensions right before this transition):
 * a request for some other, genuinely different size arriving within
 * the same cooldown window is let through rather than
 * blanket-suppressed, since only a value already known to be suspicious
 * (either the imposed one, checked unconditionally by
 * @a s_handler_configure_wh_matches_current already, or the client's
 * own prior one, checked here) has any real chance of being this exact
 * kind of stale echo to begin with.  Every other bit @p transition_mask
 * carries (X, Y, and, for fullscreen,
 * @c XCB_CONFIG_WINDOW_BORDER_WIDTH) has no comparable "old" value of
 * its own worth checking against, so those stay exactly as
 * content-blind as before: stripped whenever @p mask overlaps them at
 * all, for as long as the cooldown itself is still active.
 *
 * @param event           The @c ConfigureRequest event itself, for the
 *                        same WIDTH/HEIGHT comparison
 * @param mask            Value mask bits still under consideration
 * @param transition_mask Bits this particular transition's
 *                        cooldown should strip, if still active
 * @param old_dim         Width/height the client itself had right
 *                        before this transition
 * @param is_reparented   Whether this client has a separate frame
 *                        window of its own
 * @param on_inner        Whether @p event targets the content window
 *                        directly rather than the frame
 * @param extents         This client's current frame extents
 * @param transition_time Monotonic time the transition itself last
 *                        happened at
 * @param cooldown_ms     How long after @p transition_time a request
 *                        still counts as a stale echo
 * @param window          Client window, for the debug log line alone
 * @param kind            Short, human-readable name of the transition
 *                        ("shade" or "fullscreen"), for the same log
 *                        line
 *
 * @return @p mask, with @p transition_mask's bits cleared as described
 *         above if the cooldown is still active; @p mask unchanged
 *         otherwise, including when @p mask does not overlap
 *         @p transition_mask to begin with
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
     * content-blind as before; only WIDTH/HEIGHT additionally require
     * matching 'old_dim' once found within the window at all. */
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
    struct s_configure_ctx_s ctx = {0};
    bool geom_changed;

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
     * unmodified" path below, undoing the fixed size the tray forces on
     * every icon at dock time; see 'systray_icon_size_enforce'. */
    if (client == NULL && systray_icon_size_enforce(event->window)) {
        return;
    }

    if (client != NULL) {
        LOGGER_DEBUG("'ConfigureRequest' matched client window=0x%x:" \
                " frame=0x%x, decorated=%d, on_inner=%d, mask=0x%x," \
                " requested=%ux%u%+d%+d, operation=%u",
                client->window, client->frame,
                (int) client_is_decorated(client),
                (int) (event->window == client->window),
                mask, event->width, event->height,
                event->x, event->y,
                (unsigned int) client->properties.operation);
    }

    geom_changed = false;
    if (client != NULL) {
        uint16_t owned_mask;
        bool is_reparented = (client->frame != 0) &&
            client_is_decorated(client);
        bool on_inner = (event->window == client->window);
        bool send_synth = false;
        xcb_window_t target = event->window;
        uint16_t target_mask;
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

        /* Ignored the same way, and for the same reason, whether the WM
         * itself is actively moving/resizing this client right now, or
         * the client is in fullscreen.  Either way, ITS OWN request for
         * a different position/size is a stale echo of whatever
         * geometry it would rather have, not something to honor, since
         * the WM (not the client) owns this window's geometry for as
         * long as either holds.  Without this, a client that fixes its
         * size in WM_NORMAL_HINTS (e.g., 'min_width' == 'max_width')
         * and reacts to being forced into fullscreen by re-requesting
         * its preferred size right back would immediately shrink back
         * down, undoing 'ccmd_client_fullscreen''s deliberate choice
         * (see its comment in cmds/client/state.c) to bypass every one
         * of the client's size hints while fullscreen.
         *
         * A maximized axis is owned the same way, but only that axis:
         * unlike a drag in progress or fullscreen, which own the whole
         * window, a client maximized on just one axis still genuinely
         * owns the other, free one, the same distinction
         * 'ccmd_client_move'/'_center' (cmds/client/move.c) already
         * draw.  Without stripping the maximized axis's own X/Y and
         * width/height here, the exact same 'min_width' == 'max_width'
         * client reacting to being forced past its own preferred size
         * would silently shrink a maximized axis right back down too,
         * leaving 'CLIENT_STATE_MAXIMIZED_HORZ'/'_VERT' standing over
         * geometry that no longer actually fills the workarea. */
        owned_mask = 0u;
        if (client->properties.operation == CLIENT_OPERATION_MOVING ||
                client->properties.operation ==
                    CLIENT_OPERATION_RESIZING ||
                client_is_fullscreen(client)) {
            owned_mask |= geom_mask;
        }
        if (client_is_maximized_horz(client)) {
            owned_mask |= (uint16_t) XCB_CONFIG_WINDOW_X |
                (uint16_t) XCB_CONFIG_WINDOW_WIDTH;
        }
        if (client_is_maximized_vert(client)) {
            owned_mask |= (uint16_t) XCB_CONFIG_WINDOW_Y |
                (uint16_t) XCB_CONFIG_WINDOW_HEIGHT;
        }
        if (owned_mask != 0u && (mask & owned_mask)) {
            mask = (uint16_t) (mask & ~owned_mask);
            if (mask == 0) {
                s_handler_configure_acknowledge(connection, client,
                        is_reparented);
                return;
            }
        }

        /* A request for a dimension the client already has right now is
         * a true no-op regardless of any transition or cooldown; see
         * 's_handler_configure_wh_matches_current' itself for why this
         * alone is always safe. */
        mask = s_handler_configure_wh_matches_current(event, mask,
                client->layout.geometry.cur.dim,
                is_reparented, on_inner, client->layout.frame_extents);
        if (mask == 0) {
            s_handler_configure_acknowledge(connection, client,
                    is_reparented);
            return;
        }

        /* Ignore a geometry request that lands shortly after the window
         * manager itself just shaded or unshaded this client: see
         * 'WM_SHADE_CONFIGURE_COOLDOWN_MS' for why such a request is
         * far more likely to be the client's delayed, stale reaction to
         * that transition than an independent resize it actually
         * wants. */
        mask = s_handler_configure_cooldown_mask(event, mask, geom_mask,
                client->layout.geometry.old.dim,
                is_reparented, on_inner, client->layout.frame_extents,
                client->shade_transition_time,
                (unsigned int) WM_SHADE_CONFIGURE_COOLDOWN_MS,
                client->window, "shade");
        if (mask == 0) {
            s_handler_configure_acknowledge(connection, client,
                    is_reparented);
            return;
        }

        /* Same reasoning, for the same underlying mechanism, right
         * after entering or leaving fullscreen instead of a shade or
         * unshade; also covers 'XCB_CONFIG_WINDOW_BORDER_WIDTH', not
         * just 'geom_mask', since 'ccmd_client_unfullscreen' restores
         * the client's border width as part of the same transition this
         * guards, and a stale echo touching only that field would
         * otherwise slip through 'geom_mask' alone and silently undo
         * it, well after the point in this function that already
         * applies every other bit in 'mask' unconditionally. */
        mask = s_handler_configure_cooldown_mask(event, mask,
                (uint16_t) (geom_mask | XCB_CONFIG_WINDOW_BORDER_WIDTH),
                client->layout.geometry.old.dim,
                is_reparented, on_inner, client->layout.frame_extents,
                client->fullscreen_transition_time,
                (unsigned int) WM_FULLSCREEN_CONFIGURE_COOLDOWN_MS,
                client->window, "fullscreen");
        if (mask == 0) {
            s_handler_configure_acknowledge(connection, client,
                    is_reparented);
            return;
        }

        if (is_reparented) {
            target = client->frame;
        }

        ctx.client = client;
        ctx.event = event;
        ctx.mask = mask;
        ctx.left = left;
        ctx.right = right;
        ctx.top = top;
        ctx.bottom = bottom;
        ctx.req_w = req_w;
        ctx.req_h = req_h;
        ctx.old_w = old_w;
        ctx.old_h = old_h;
        ctx.is_reparented = is_reparented;
        ctx.on_inner = on_inner;
        ctx.send_synth = send_synth;
        ctx.geom_changed = geom_changed;

        s_handler_configure_build(&ctx);
        s_handler_configure_gravity(&ctx);

        target_mask = ctx.target_mask;
        send_synth = ctx.send_synth;
        geom_changed = ctx.geom_changed;

        if (target_mask != 0 && connection != NULL) {
            xcb_configure_window(connection, target,
                    target_mask, ctx.values);
            if (is_reparented) {
                client_decoration_layout_sync(client);
                if (send_synth) {
                    s_handler_send_synthetic_configure_notify(connection,
                            client);
                }
            }

        }
    } else {
        s_handler_configure_forward(connection, event, mask);
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
                    " (new frame geometry %ux%u%+d%+d)",
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
            " geom=%ux%u%+d%+d)",
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
            bool in_sync;
            bool is_stale_echo;
            bool is_focused = (desktop != NULL) &&
                (desktop->client_active_id == client->id);

            /* For undecorated clients the window is its frame and lives
             * as a direct root child.  The X server delivers
             * 'ConfigureNotify' events via two routes:
             *
             *  - 'StructureNotify' ('event->event == window'):
             *    reliable, reflects the position the window manager
             *    last configured.
             *  - 'SubStructureNotify' on root
             *    ('event->event != window'): also generated for every
             *    'ConfigureWindow' the window manager issued on the
             *    client, including the border-width adjustment that
             *    happens before placement in 'client_init'.  That
             *    pre-placement event carries the application's initial
             *    position, often (0,0), which can arrive late (after
             *    'place_window_apply' already stored the centered
             *    coordinates) and corrupt the stored position.  When
             *    the subsequent render uses the corrupted coordinates
             *    the window is moved to the wrong position, which in
             *    turn queues another stale 'SubStructureNotify',
             *    creating a render loop that manifests as continuous
             *    flickering until the window is iconified/restored.
             *
             * Ignore 'SubStructureNotify'-delivered 'ConfigureNotify'
             * events for all managed clients (decorated and undecorated
             * alike): the 'StructureNotify' copy (same data, always
             * correct) handles all legitimate updates.  For decorated
             * clients this also prevents stale 'SubStructureNotify'
             * events from placement ('place_window_apply') from
             * overwriting the position set by 'rules_apply'.  When the
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

            /* Every configure the manager issues comes back as an echo,
             * and during a burst, a viewport pan drag above all, the
             * echo of an earlier request routinely lands after a later
             * one was already sent.  Taking that stale position would
             * leave the stored geometry a step behind, and a pan, which
             * adds its delta to whatever is stored, would carry the
             * error forward for good rather than correct it on the next
             * step.
             *
             * So a position that contradicts the last one asked for is
             * refused, but only while the stored geometry still agrees
             * with that request.  That second condition is what makes
             * this safe to leave in place: any path that moves the
             * window by writing 'geometry.cur' and configuring the
             * server itself, without going through
             * 'ccmd_client_apply_geometry' or the render pass (the
             * '_NET_MOVERESIZE_WINDOW' and 'ConfigureRequest' handlers
             * both do), leaves the two disagreeing, and the refusal
             * simply does not engage for it.  A client never placed yet
             * has nothing to compare against either.
             *
             * The size is taken either way: it is the client's own to
             * ask for, and no burst of the manager's own makes it
             * stale. */
            in_sync = client->layout.has_requested_pos &&
                client->layout.geometry.cur.pos.x ==
                    client->layout.requested_pos.x &&
                client->layout.geometry.cur.pos.y ==
                    client->layout.requested_pos.y;
            is_stale_echo = in_sync &&
                (client->layout.requested_pos.x != (int32_t) event->x ||
                 client->layout.requested_pos.y != (int32_t) event->y);

            if (!is_stale_echo) {
                client->layout.geometry.cur.pos.x = event->x;
                client->layout.geometry.cur.pos.y = event->y;
            } else {
                geom_changed = size_changed;
            }
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
                    render_client_decoration_repaint_frame(connection,
                            client, is_focused,
                            (desktop != NULL)
                                ? &desktop->config->theme
                                : ((client->config != NULL)
                                        ? &client->config->theme : NULL));
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
             *   2. call 'client_decoration_layout_sync' again,
             *      generating another 'ConfigureNotify' with the old
             *      size;
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
