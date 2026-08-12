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
#include <stddef.h>     /* NULL */
#include <time.h>       /* clock_gettime, struct timespec */

/* ADT includes */
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Default initial values */
#include <defs/urgency.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>

/* Local includes */
#include <policy/urgency.h>


/** Current blink phase: @c true during the "swapped colors" half */
static bool s_blink_on = false;

/** When the phase last changed, @c CLOCK_MONOTONIC */
static struct timespec s_last_toggle;

/**
 * @brief Whether at least one client was found urgent as of the most
 *        recent @c urgency_blink_tick call
 *
 * Cached here rather than recomputed by @c urgency_blink_ms_remaining
 * itself, so the one full scan over every client each iteration
 * needs happens exactly once, in @c urgency_blink_tick, the same way
 * @c systray_clock_tick and @c systray_clock_ms_remaining split the
 * same two responsibilities (src/systray/text.c).
 */
static bool s_has_urgent = false;


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
 * @brief Mark every surface and desktop that owns at least one
 *        urgent client as needing a repaint
 *
 * Only sets 'surface_td::is_outdated' (so 'loop_update' picks it up
 * on this same iteration) and 'desktop_td::focus_dirty' (so
 * 's_desktop_render_one_client' in render/desktop.c takes its
 * cheaper "only refresh focus-sensitive colors" branch rather than a
 * full geometry recompute); never touches any per-client flag, since
 * both 's_desktop_render_one_client' and 'ri_render_client_icon'
 * (render/icon.c) already special-case an urgent client on their own
 * to repaint regardless of what changed.
 *
 * @param surfaces All managed surfaces
 *
 * @return @c true when at least one urgent client was found
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       managed clients
 */
static bool s_mark_urgent_outdated(list_td *surfaces)
{
    bool found = false;

    if (surfaces == NULL) {
        return false;
    }

    for (list_item_td *snode = list_head(surfaces); snode != NULL;
            snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);
        bool surface_has_urgent = false;

        if (surface == NULL) {
            continue;
        }

        for (uint32_t di = 0; di < surface->desktop_count; ++di) {
            desktop_td *desktop = surface_desktop_get(surface, di);
            void *elem;
            bool desktop_has_urgent = false;

            if (desktop == NULL || desktop->clients == NULL) {
                continue;
            }

            ohtbl_foreach(desktop->clients, elem) {
                client_td *c = (client_td *) elem;

                if (c != NULL && client_is_urgent(c)) {
                    desktop_has_urgent = true;
                    found = true;
                }
            }

            if (desktop_has_urgent) {
                desktop->focus_dirty = true;
                surface_has_urgent = true;
            }
        }

        if (surface_has_urgent) {
            surface->is_outdated = true;
        }
    }

    return found;
}


/* Query the current blink phase */
bool urgency_blink_is_on(void)
{
    return s_blink_on;
}


/* Advance the blink cycle and repaint whatever it changed */
void urgency_blink_tick(list_td *surfaces)
{
    s_has_urgent = s_mark_urgent_outdated(surfaces);

    if (!s_has_urgent) {
        s_blink_on = false;
        return;
    }

    if (s_last_toggle.tv_sec == 0 && s_last_toggle.tv_nsec == 0) {
        (void) clock_gettime(CLOCK_MONOTONIC, &s_last_toggle);
        return;
    }

    if (s_ms_since(&s_last_toggle) >=
            (long) WM_URGENCY_BLINK_INTERVAL_MS) {
        s_blink_on = !s_blink_on;
        (void) clock_gettime(CLOCK_MONOTONIC, &s_last_toggle);
        /* The outdated/focus_dirty marking above already covers every
         * urgent client found this same tick with the phase about to
         * take effect; nothing further to mark here. */
    }
}


/* How many milliseconds until the blink cycle next needs a tick */
int urgency_blink_ms_remaining(void)
{
    long elapsed_ms;

    if (!s_has_urgent) {
        return -1;
    }

    if (s_last_toggle.tv_sec == 0 && s_last_toggle.tv_nsec == 0) {
        return 0;
    }

    elapsed_ms = s_ms_since(&s_last_toggle);
    if (elapsed_ms >= (long) WM_URGENCY_BLINK_INTERVAL_MS) {
        return 0;
    }

    return (int) ((long) WM_URGENCY_BLINK_INTERVAL_MS - elapsed_ms);
}
