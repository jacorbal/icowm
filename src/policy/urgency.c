/**
 * @file policy/urgency.c
 *
 * @brief Urgent-client attention blink implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* CLOCK_MONOTONIC */


/* System includes */
#include <stdbool.h>
#include <time.h>       /* clock_gettime, struct timespec, NULL */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Default initial values */
#include <defs/urgency.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <render/desktop.h>
#include <render/icon.h>
#include <render/surface.h>
#include <surface.h>

/* Local includes */
#include <policy/urgency.h>
#include <utils/xcb/connection.h>


/** Current blink phase: @c true during the "swapped colors" half */
static bool s_blink_on = false;

/** When the phase last changed, @c CLOCK_MONOTONIC */
static struct timespec s_last_toggle;

/**
 * @brief Whether at least one client was found urgent as of the most
 *        recent @c urgency_blink_tick call
 *
 * Cached here rather than recomputed by @a urgency_blink_ms_remaining
 * itself, so the one full scan over every client each iteration needs
 * happens exactly once, in @c urgency_blink_tick, the same way
 * @a systray_clock_tick and @a systray_clock_ms_remaining split the
 * same two responsibilities (@c src/systray/text.c).
 */
static bool s_has_urgent = false;


/**
 * @brief Note whether any client on a desktop is asking for attention
 *
 * @param desktop Desktop reached by the walk
 * @param data    Pointer to the @c bool being set
 *
 * @note The whole walk runs even once one is found, a visitor having no
 *       way to end it; the caller stops on the flag instead
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static void s_urgent_search_visit(desktop_td *desktop, void *data)
{
    bool *const is_any_urgent = data;
    void *elem;

    if (is_any_urgent == NULL || desktop->clients == NULL) {
        return;
    }

    ohtbl_foreach(desktop->clients, elem) {
        const client_td *const client = (const client_td *) elem;

        if (client != NULL && client_is_urgent(client)) {
            *is_any_urgent = true;
        }
    }
}


/**
 * @brief Milliseconds elapsed since @p since, per @c CLOCK_MONOTONIC
 *
 * @param since Earlier timestamp to measure from
 *
 * @return Elapsed milliseconds, or a large value if the clock query
 *         itself fails (so a caller comparing against a threshold
 *         treats that as "due" rather than getting stuck never
 *         triggering)
 *
 * @note Complexity: @e O(1)
 */
static long s_ms_since(const struct timespec *since)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return (long) WM_URGENCY_BLINK_INTERVAL_MS;
    }

    return (long) ((now.tv_sec - since->tv_sec) * 1000L +
            (now.tv_nsec - since->tv_nsec) / 1000000L);
}


/**
 * @brief Whether at least one managed client, anywhere, currently has
 *        its urgency hint set
 *
 * A pure scan with no side effects at all, safe to call on every single
 * @a urgency_blink_tick (i.e., every main-loop iteration, not just at
 * the actual @c WM_URGENCY_BLINK_INTERVAL_MS cadence).
 *
 * The blink phase itself must keep advancing consistently regardless of
 * which desktop the urgent client happens to sit on, or how often this
 * is called, so this deliberately still looks at every desktop of every
 * surface, not only each surface's currently visible one.
 *
 * @param surfaces All managed surfaces
 *
 * @return @c true when at least one urgent client was found
 *
 * @note Complexity: @e O(n), where @e n is the total number of managed
 *       clients
 *
 * @see @a s_repaint_urgent_clients for that narrower scope, which is
 *      where the actual, comparatively expensive repainting happens
 *      instead
 */
static bool s_any_client_urgent(list_td *surfaces)
{
    if (surfaces == NULL) {
        return false;
    }

    for (list_item_td *snode = list_head(surfaces); snode != NULL;
            snode = list_next(snode)) {
        const surface_td *const surface =
            (surface_td *) list_data(snode);
        bool is_any_urgent = false;

        if (surface == NULL) {
            continue;
        }

        surface_desktops_walk(surface, s_urgent_search_visit,
                &is_any_urgent);
        if (is_any_urgent) {
            return true;
        }
    } /* ! for (snode) */

    return false;
}


/**
 * @brief Repaint every currently visible urgent client directly, to
 *        match the blink phase that just took effect
 *
 * Deliberately narrow in both scope and mechanism, unlike an earlier
 * version of this function that instead marked whole surfaces and
 * desktops outdated and let the ordinary full-render path pick that up.
 * That meant every single client on a desktop with an urgent one
 * repainted alongside it, on every call, and since this used to be
 * called on every main-loop iteration rather than only at the real
 * blink cadence, that full-desktop repaint fired far more often than
 * the blink itself ever changed, visibly flickering every window and
 * icon on the desktop, not just the urgent one, and stomping over other
 * clients' independent, transient render state along the way (e.g., an
 * icon's cycle-selection highlight mid-drag).
 *
 * Only ever called from the actual blink-phase-toggle branch of
 * @a urgency_blink_tick now, this instead calls
 * @a desktop_render_one_client / @a ri_render_client_icon directly, one
 * at a time, for only the specific client(s) that are both urgent and
 * on a desktop currently visible on some surface (an urgent client
 * sitting on a desktop nobody is looking at right now has nothing to
 * visibly repaint at all.  Its clients are not even mapped, leaving
 * every other client on that same desktop untouched).
 *
 * @param surfaces All managed surfaces
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       each surface's currently visible desktop
 *
 * @see @a desktop_render_clients's comment
 */
static void s_repaint_urgent_clients(list_td *surfaces)
{
    if (surfaces == NULL) {
        return;
    }

    for (list_item_td *snode = list_head(surfaces); snode != NULL;
            snode = list_next(snode)) {
        surface_td *const surface = (surface_td *) list_data(snode);
        desktop_td *desktop;
        void *elem;
        bool repainted_any = false;

        if (surface == NULL) {
            continue;
        }

        desktop = surface_desktop_get(surface, surface->desktop_cur);
        if (desktop == NULL || desktop->clients == NULL) {
            continue;
        }

        ohtbl_foreach(desktop->clients, elem) {
            client_td *const c = (client_td *) elem;

            if (c == NULL || !client_is_urgent(c)) {
                continue;
            }

            if (c->properties.flags & CLIENT_FLAG_HIDDEN) {
                if (client_is_iconified(c)) {
                    ri_render_client_icon(c, true, false);
                    repainted_any = true;
                }
                continue;
            }

            desktop_render_one_client(desktop, c, true);
            repainted_any = true;
        }

        if (repainted_any) {
            surface_render_flush(surface);
        }
    }
}


/* Query the current blink phase */
bool urgency_blink_is_on(void)
{
    return s_blink_on;
}


/* Advance the blink cycle and repaint whatever it changed */
void urgency_blink_tick(list_td *surfaces, const config_td *config)
{
    bool had_urgent = s_has_urgent;
    uint32_t interval_ms = (config != NULL)
        ? config->a11y.urgency.blink_interval_ms
        : WM_URGENCY_BLINK_INTERVAL_MS;

    s_has_urgent = s_any_client_urgent(surfaces);

    /* Sound the accessibility bell right on the transition into
     * urgency, never again on every later tick while it stays urgent,
     * and never while it clears: an audible cue alongside the visual
     * blink every urgent client already gets regardless of this
     * setting */
    if (!had_urgent && s_has_urgent && config != NULL &&
            config->a11y.urgency.sound_bell &&
            surfaces != NULL && !list_is_empty(surfaces)) {
        xcb_connection_t *const connection = xcb_connection_get();

        if (connection != NULL) {
            xcb_bell(connection, 0);
        }
    }

    if (!s_has_urgent) {
        s_blink_on = false;
        return;
    }

    if (s_last_toggle.tv_sec == 0 && s_last_toggle.tv_nsec == 0) {
        (void) clock_gettime(CLOCK_MONOTONIC, &s_last_toggle);
        return;
    }

    if (s_ms_since(&s_last_toggle) >= (long) interval_ms) {
        s_blink_on = !s_blink_on;
        (void) clock_gettime(CLOCK_MONOTONIC, &s_last_toggle);
        s_repaint_urgent_clients(surfaces);
    }
}


/* How many milliseconds until the blink cycle next needs a tick */
int urgency_blink_ms_remaining(const config_td *config)
{
    long elapsed_ms;
    uint32_t interval_ms = (config != NULL)
        ? config->a11y.urgency.blink_interval_ms
        : WM_URGENCY_BLINK_INTERVAL_MS;

    if (!s_has_urgent) {
        return -1;
    }

    if (s_last_toggle.tv_sec == 0 && s_last_toggle.tv_nsec == 0) {
        return 0;
    }

    elapsed_ms = s_ms_since(&s_last_toggle);
    if (elapsed_ms >= (long) interval_ms) {
        return 0;
    }

    return (int) ((long) interval_ms - elapsed_ms);
}
