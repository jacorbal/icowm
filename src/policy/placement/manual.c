/**
 * @file policy/placement/manual.c
 *
 * @brief Manual placement: the position the person picks themselves
 *
 * The whole state of the question this policy asks lives here: which
 * window is being pointed at, which ones are waiting their turn behind
 * it, where the outline currently stands, and when the wait for an
 * answer runs out.  No other module keeps any of it.
 *
 * Nothing in this file ever waits in place.  The pointer and the
 * keyboard are grabbed and the function returns, so the answer arrives
 * as ordinary events through the main loop, exactly as a window drag's
 * does.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* CLOCK_MONOTONIC, clock_gettime */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* free, NULL */
#include <time.h>       /* clock_gettime, struct timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>

/* Default initial values */
#include <defs/input.h>
#include <defs/kbd.h>
#include <defs/placement.h>

/* Render includes */
#include <render/outline.h>

/* Utils includes */
#include <utils/time/clock.h>
#include <utils/xcb/window.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <logger.h>
#include <surface.h>

/* Local includes */
#include <policy/placement/manual.h>
#include <policy/placement/monitor.h>
#include <policy/placement/smart.h>


/**
 * @brief One window being placed by hand, or waiting its turn to be
 *
 * Every field is what the map this window is in the middle of will
 * need once its position is settled, kept here because the answer
 * arrives long after the map request that started it has returned.
 *
 * @note Every pointer comes first and the one 32-bit field last, so
 *       the layout carries no hole between fields
 */
struct s_place_manual_entry_s {
    const wm_td *wm;            /**< Window manager it belongs to */
    surface_td *surface;        /**< Surface it will appear on */
    desktop_td *desktop;        /**< Desktop it belongs to */
    client_td *client;          /**< The window itself */
    place_manual_done_fn done;  /**< What finishes its map */
    xcb_cursor_t cursor;        /**< Pointer shape while asking */
};


/**
 * @brief The window being pointed at, first, and those waiting behind
 *        it
 *
 * Kept in arrival order, the one being asked about always at index 0,
 * so a session opening several windows at once is asked about them in
 * the order they opened rather than an order chosen here.
 */
static struct s_place_manual_entry_s
        s_place_manual_queue[WM_PLACE_MANUAL_QUEUE_MAX];

/** How many entries of @a s_place_manual_queue are in use */
static uint32_t s_place_manual_queued = 0u;

/**
 * @brief Whether the head of @a s_place_manual_queue is currently
 *        being pointed at
 *
 * A window sits in the queue from the moment it is taken, but only
 * becomes the one being asked about (outline drawn, pointer and
 * keyboard held, wait running) when its turn comes.
 */
static bool s_place_manual_active = false;

/** When the wait for an answer about the head window runs out */
static struct timespec s_place_manual_due;

/**
 * @brief Where the pointer was last seen, in root coordinates
 *
 * The raw position, before the workarea clamp @a s_place_manual_geom
 * applies, so a pointer sitting still above the workarea top is
 * recognized as not having moved rather than compared against a
 * clamped value it can never equal.
 */
static struct position_s s_place_manual_pointer = { 0, 0 };

/**
 * @brief Workarea the outline is kept inside, resolved once per window
 *
 * Surface-wide rather than clipped to one monitor: on a surface made
 * of several, the person points at whichever one they mean, and a
 * clamp against the monitor resolved when the question opened would
 * fight them for the whole of it.
 */
static struct geometry_s s_place_manual_wa = { { 0, 0 }, { 0u, 0u } };

/** Root window the outline and both grabs belong to */
static xcb_window_t s_place_manual_root = XCB_WINDOW_NONE;

/** The 4 strip windows making up the outline */
static xcb_window_t s_place_manual_outline[4] = {
    XCB_WINDOW_NONE, XCB_WINDOW_NONE, XCB_WINDOW_NONE, XCB_WINDOW_NONE
};

/**
 * @brief The one window @a place_manual_enqueue may accept next, or
 *        @c NULL
 *
 * Set by @a place_window_manual, which the placement dispatcher
 * reaches only for a window this policy really does apply to, and read
 * once by @a place_manual_enqueue, which the map handler calls right
 * afterwards.  Comparing identity rather than trusting the mark alone
 * is what keeps a rearrange pass, which places clients through the
 * same dispatcher and never enqueues any of them, from leaving a mark
 * behind that the next unrelated window would answer to.
 */
static client_td *s_place_manual_candidate = NULL;


/**
 * @brief Where the outline stands for a pointer position
 *
 * The window's top-left corner follows the pointer itself, which is
 * what TWM and FVWM both do and what the outline makes people expect,
 * clamped so the title bar never ends up above the workarea or left of
 * it (behind a panel, or off the screen entirely).
 *
 * @param client  Client being placed, for its frame size
 * @param pointer Pointer position, in root coordinates
 *
 * @return Rectangle the outline occupies, in root coordinates
 *
 * @note Only the top and left edges are clamped, never the right or
 *       the bottom, so a window can still be put deliberately partway
 *       off those
 * @note Complexity: @e O(1)
 */
static struct geometry_s s_place_manual_geom(const client_td *client,
        struct position_s pointer)
{
    struct geometry_s geom;

    geom.pos = pointer;
    geom.dim = client->layout.geometry.cur.dim;

    if (geom.pos.x < s_place_manual_wa.pos.x) {
        geom.pos.x = s_place_manual_wa.pos.x;
    }
    if (geom.pos.y < s_place_manual_wa.pos.y) {
        geom.pos.y = s_place_manual_wa.pos.y;
    }

    return geom;
}


/**
 * @brief Ask the X server where the pointer currently is
 *
 * @param connection XCB connection
 * @param out        Receives the position, in root coordinates
 *
 * @return @c true when the position was read
 *
 * @note Answers @c false when the pointer is on another screen
 *       entirely, which leaves the caller to start the outline
 *       somewhere it chooses instead
 * @note Complexity: @e O(1)
 */
static bool s_place_manual_pointer_read(xcb_connection_t *connection,
        struct position_s *out)
{
    xcb_query_pointer_reply_t *reply;
    bool is_read = false;

    reply = xcb_query_pointer_reply(connection,
            xcb_query_pointer(connection, s_place_manual_root), NULL);
    if (reply == NULL) {
        return false;
    }

    if (reply->same_screen) {
        out->x = reply->root_x;
        out->y = reply->root_y;
        is_read = true;
    }
    free(reply);

    return is_read;
}


/**
 * @brief Start asking about the window at the head of the queue
 *
 * Draws the outline where the pointer already is, takes the pointer
 * and the keyboard, and starts the wait for this window.
 *
 * @param connection XCB connection
 *
 * @note A no-op when the queue is empty, or when a window is already
 *       being asked about
 * @note Both grabs are asked for without checking their replies, the
 *       same way a drag's pointer grab is: a refused grab leaves the
 *       wait running, which places the window unaided a moment later
 *       rather than leaving it stuck unmapped
 * @note Complexity: @e O(m), where @e m is the number of monitors, from
 *       resolving the workarea
 */
static void s_place_manual_head_start(xcb_connection_t *connection)
{
    struct s_place_manual_entry_s *entry;
    struct geometry_s mon_wa;
    struct dimensions_s mon_sz;
    uint32_t color;

    if (connection == NULL || s_place_manual_active ||
            s_place_manual_queued == 0u) {
        return;
    }

    entry = &s_place_manual_queue[0];
    s_place_manual_root = (entry->surface->screen != NULL)
        ? entry->surface->screen->root
        : XCB_WINDOW_NONE;
    if (s_place_manual_root == XCB_WINDOW_NONE) {
        return;
    }

    placement_workarea(entry->wm, entry->surface, entry->client,
            &s_place_manual_wa, &mon_wa, &mon_sz);

    /* Wherever the smart policy already put it, for a pointer that
     * cannot be read: the outline then starts on the very position
     * giving up would settle on, so the question still opens somewhere
     * meaningful rather than at the screen corner */
    if (!s_place_manual_pointer_read(connection,
                &s_place_manual_pointer)) {
        s_place_manual_pointer = entry->client->layout.geometry.cur.pos;
    }

    color = (entry->client->config != NULL)
        ? entry->client->config->theme.window.active.border.color
        : 0u;
    render_outline_show(connection, s_place_manual_root,
            s_place_manual_geom(entry->client, s_place_manual_pointer),
            (uint32_t) WM_DRAG_OUTLINE_BORDER_WIDTH, color,
            XCB_WINDOW_NONE, s_place_manual_outline);

    xcb_grab_pointer(connection,
            0,
            s_place_manual_root,
            XCB_EVENT_MASK_BUTTON_PRESS |
            XCB_EVENT_MASK_POINTER_MOTION,
            XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC,
            XCB_NONE,
            entry->cursor,
            XCB_CURRENT_TIME);

    /* The keyboard too, unlike a drag, which needs none: the window
     * being placed is not mapped and cannot hold focus, so an 'Escape'
     * meant for this question would otherwise go to whatever client
     * held focus before it and never arrive here at all */
    xcb_grab_keyboard(connection,
            0,
            s_place_manual_root,
            XCB_CURRENT_TIME,
            XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC);

    if (clock_gettime(CLOCK_MONOTONIC, &s_place_manual_due) == 0) {
        clock_add_ms(&s_place_manual_due, WM_PLACE_MANUAL_TIMEOUT_MS);
    }

    s_place_manual_active = true;
    xcb_flush(connection);

    LOGGER_DEBUG("Asking where window %#x ('%s') goes",
            entry->client->window, entry->client->info.name);
}


/**
 * @brief Stop asking about the window at the head of the queue
 *
 * Takes the outline down and gives the pointer and the keyboard back,
 * without deciding anything about where the window ends up.
 *
 * @param connection XCB connection
 *
 * @note A no-op when no window is currently being asked about
 * @note Complexity: @e O(1)
 */
static void s_place_manual_head_stop(xcb_connection_t *connection)
{
    if (!s_place_manual_active) {
        return;
    }

    render_outline_hide(connection, s_place_manual_outline);
    s_place_manual_active = false;
    s_place_manual_root = XCB_WINDOW_NONE;

    if (connection != NULL) {
        xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
        xcb_ungrab_keyboard(connection, XCB_CURRENT_TIME);
        xcb_flush(connection);
    }
}


/**
 * @brief Drop the head of the queue and move the rest up
 *
 * @note A no-op on an empty queue
 * @note Complexity: @e O(q), where @e q is the number of windows
 *       waiting their turn
 */
static void s_place_manual_pop(void)
{
    if (s_place_manual_queued == 0u) {
        return;
    }

    s_place_manual_queued--;
    for (uint32_t i = 0u; i < s_place_manual_queued; ++i) {
        s_place_manual_queue[i] = s_place_manual_queue[i + 1u];
    }
}


/**
 * @brief Settle the window being asked about, and go on to the next
 *
 * @param connection    XCB connection
 * @param is_confirmed  @c true to put the window where the outline
 *                      stands, @c false to leave it where the smart
 *                      policy already had it
 *
 * @note What finishes the map is called after the window has left the
 *       queue, so anything it does in turn sees a queue that no longer
 *       mentions a window already settled
 * @note Complexity: @e O(q + n), where @e q is the number of windows
 *       waiting their turn and @e n the number of clients on the
 *       desktop, from the map this hands on to
 */
static void s_place_manual_settle(xcb_connection_t *connection,
        bool is_confirmed)
{
    struct s_place_manual_entry_s entry;
    struct geometry_s geom;

    if (!s_place_manual_active || s_place_manual_queued == 0u) {
        return;
    }

    entry = s_place_manual_queue[0];
    geom = s_place_manual_geom(entry.client, s_place_manual_pointer);
    s_place_manual_head_stop(connection);
    s_place_manual_pop();

    /* Moved without going through the placement dispatcher's
     * gravity step: that step states a position the client itself
     * asked for in terms of its declared gravity, and this one was
     * pointed at directly, so a window declaring center gravity would
     * have half its width taken off again and land left of where the
     * outline stood.  The same reasoning a splash screen's
     * placement already follows ('place_window_apply', policy/
     * placement/window.c). */
    if (is_confirmed) {
        xcb_window_t target =
            (client_is_decorated(entry.client) &&
                entry.client->frame != 0)
            ? entry.client->frame
            : entry.client->window;

        xcb_window_move(target, geom.pos.x, geom.pos.y);
        entry.client->layout.geometry.cur.pos = geom.pos;
    }

    entry.done(entry.wm, entry.surface, entry.desktop, entry.client);

    s_place_manual_head_start(connection);
}


/* Pick where a window goes, asking the person to point at it */
bool place_window_manual(const wm_td *wm, surface_td *surface,
        client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y)
{
    if (wm == NULL || surface == NULL || client == NULL ||
            out_x == NULL || out_y == NULL) {
        return false;
    }

    /* Marked before the search below, not after, and left marked
     * whatever that search answers: a window the smart scan finds no
     * free spot for falls back to the cascade in the dispatcher, which
     * returns without ever coming back here, and it deserves to be
     * asked about just as much as one that did find a spot. */
    s_place_manual_candidate = client;

    return place_window_smart(wm, surface, client, out_x, out_y);
}


/* Take a window that must be placed by hand, and hold it */
bool place_manual_enqueue(xcb_connection_t *connection, const wm_td *wm,
        surface_td *surface, desktop_td *desktop, client_td *client,
        xcb_cursor_t cursor, place_manual_done_fn done)
{
    struct s_place_manual_entry_s *entry;
    client_td *const candidate = s_place_manual_candidate;

    /* Cleared on every call, accepted or not, so a mark left behind by
     * a placement pass that never enqueues anything (a desktop
     * rearrange, which places every client through the same
     * dispatcher) is never answered to by some later window */
    s_place_manual_candidate = NULL;

    if (connection == NULL || wm == NULL || surface == NULL ||
            desktop == NULL || client == NULL || done == NULL ||
            candidate != client) {
        return false;
    }

    if (s_place_manual_queued >= WM_PLACE_MANUAL_QUEUE_MAX) {
        LOGGER_WARNING("Too many windows waiting to be placed by hand;" \
                " mapping window %#x where it already is",
                client->window);
        return false;
    }

    entry = &s_place_manual_queue[s_place_manual_queued];
    entry->wm = wm;
    entry->surface = surface;
    entry->desktop = desktop;
    entry->client = client;
    entry->done = done;
    entry->cursor = cursor;
    s_place_manual_queued++;

    s_place_manual_head_start(connection);

    return true;
}


/* Whether a window is currently being pointed at */
bool place_manual_is_active(void)
{
    return s_place_manual_active;
}


/* Follow the pointer with the outline of the window being placed */
void place_manual_handle_motion(xcb_connection_t *connection,
        struct position_s root_pos)
{
    if (connection == NULL || !s_place_manual_active ||
            s_place_manual_queued == 0u) {
        return;
    }

    /* The X server can deliver a 'MotionNotify' reporting the position
     * the pointer is already resting at right after a grab starts, the
     * same repeat a drag already skips ('drag_update', input/mouse/
     * drag.c); acting on it would reconfigure 4 windows and flush for
     * a rectangle that has not moved */
    if (root_pos.x == s_place_manual_pointer.x &&
            root_pos.y == s_place_manual_pointer.y) {
        return;
    }
    s_place_manual_pointer = root_pos;

    render_outline_move(connection,
            s_place_manual_geom(s_place_manual_queue[0].client,
                    s_place_manual_pointer),
            (uint32_t) WM_DRAG_OUTLINE_BORDER_WIDTH,
            XCB_WINDOW_NONE, s_place_manual_outline);
    xcb_flush(connection);
}


/* Settle the window being placed where the outline stands */
void place_manual_handle_press(xcb_connection_t *connection)
{
    s_place_manual_settle(connection, true);
}


/* Let the window being placed answer a key press */
void place_manual_handle_keypress(xcb_connection_t *connection,
        xcb_keysym_t keysym)
{
    if (!s_place_manual_active) {
        return;
    }

    if (keysym == KS_ESCAPE) {
        s_place_manual_settle(connection, false);
    }
}


/* Milliseconds left before the window being placed is given up on
 * and placed anyway */
int place_manual_ms_remaining(void)
{
    if (!s_place_manual_active) {
        return -1;
    }

    return (int) clock_ms_until(&s_place_manual_due);
}


/* Give up on the window being placed, if its wait has elapsed */
void place_manual_tick(xcb_connection_t *connection)
{
    if (!s_place_manual_active ||
            place_manual_ms_remaining() > 0) {
        return;
    }

    LOGGER_DEBUG("Nobody said where window %#x goes; placing it",
            s_place_manual_queue[0].client->window);

    s_place_manual_settle(connection, false);
}


/* Drop a window that disappeared before it could be placed */
void place_manual_cancel_client(xcb_connection_t *connection,
        const client_td *client)
{
    uint32_t found = WM_PLACE_MANUAL_QUEUE_MAX;

    if (client == NULL) {
        return;
    }

    if (s_place_manual_candidate == client) {
        s_place_manual_candidate = NULL;
    }

    for (uint32_t i = 0u; i < s_place_manual_queued; ++i) {
        if (s_place_manual_queue[i].client == client) {
            found = i;
            break;
        }
    }

    if (found >= s_place_manual_queued) {
        return;
    }

    /* The one being asked about: everything it holds goes back before
     * it is dropped, and the next window in line takes its turn.  One
     * still waiting is simply forgotten, having taken nothing yet. */
    if (found == 0u && s_place_manual_active) {
        s_place_manual_head_stop(connection);
        s_place_manual_pop();
        s_place_manual_head_start(connection);
        return;
    }

    s_place_manual_queued--;
    for (uint32_t i = found; i < s_place_manual_queued; ++i) {
        s_place_manual_queue[i] = s_place_manual_queue[i + 1u];
    }
}
