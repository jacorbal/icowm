/**
 * @file handler/sync.c
 *
 * @brief XSync extension event handler for @c _NET_WM_SYNC_REQUEST
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/sync.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Command includes */
#include <cmds/client/resize.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <stage.h>
#include <wm.h>

/* Local includes */
#include <handler.h>
#include <handler/sync.h>


/**
 * @brief Find the client that owns the given XSync alarm
 *
 * Scans every stage, desktop, and client looking for a
 * @c hints_ewmh.sync.alarm match.  Alarms are only ever created for
 * clients that advertise @c _NET_WM_SYNC_REQUEST, so most clients are
 * skipped immediately via their zeroed @p hints_ewmh.sync.alarm.
 *
 * @param stages List of managed stages
 * @param alarm  XSync alarm XID from the @c AlarmNotify event
 *
 * @return The owning client, or @c NULL if none matches
 *
 * @note Complexity: @e O(n), where @e n is the total number of managed
 *       clients across all stages and desktops
 *
 * @see @c client_init
 */
static client_td *s_find_client_by_alarm(list_td *stages,
        uint32_t alarm)
{
    if (stages == NULL || alarm == 0u) {
        return NULL;
    }

    for (list_item_td *snode = list_head(stages);
            snode != NULL; snode = list_next(snode)) {
        stage_td *const stage = (stage_td *) list_data(snode);
        cdlist_item_td *dnode;
        const cdlist_item_td *dinitial;

        if (stage == NULL || stage->desktops == NULL ||
                cdlist_size(stage->desktops) == 0) {
            continue;
        }

        dnode = cdlist_head(stage->desktops);
        dinitial = dnode;
        if (dnode == NULL) {
            continue;
        }

        do {
            desktop_td *const desktop =
                (desktop_td *) cdlist_data(dnode);

            if (desktop != NULL && desktop->clients != NULL) {
                void *elem;

                ohtbl_foreach(desktop->clients, elem) {
                    client_td *const client = (client_td *) elem;

                    if (client->hints_ewmh.sync.alarm == alarm) {
                        return client;
                    }
                }
            }
            dnode = cdlist_next(dnode);
        } while (dnode != NULL && dnode != dinitial);
    }

    return NULL;
}


/* Handle XSync extension notifications */
void handler_sync_event(const wm_td *wm, xcb_generic_event_t *event)
{
    uint8_t event_type;
    uint8_t alarm_notify_type;
    xcb_sync_alarm_notify_event_t *alarm_event;
    client_td *client;
    list_td *stages = wm_stages(wm);

    if (wm == NULL || event == NULL || stages == NULL ||
            !wm_sync_available(wm)) {
        return;
    }

    event_type = (uint8_t) (event->response_type & ~0x80u);
    alarm_notify_type =
        (uint8_t) (wm_sync_base_event(wm) + XCB_SYNC_ALARM_NOTIFY);

    if (event_type != alarm_notify_type) {
        return;
    }

    alarm_event = (xcb_sync_alarm_notify_event_t *) event;
    client = s_find_client_by_alarm(stages,
            (uint32_t) alarm_event->alarm);
    if (client == NULL) {
        LOGGER_TRACE("'AlarmNotify' for unknown alarm=0x%x; ignoring",
                (unsigned int) alarm_event->alarm);
        return;
    }

    LOGGER_TRACE("'AlarmNotify' acknowledges sync request for" \
            " window=0x%x (pending=%d)", client->window,
            (int) client->hints_ewmh.sync.has_pending);

    /* The client has caught up to (or past) the last size the window
     * manager sent it; release the wait and, if a newer resize step
     * arrived meanwhile, apply it now and re-arm the wait for the next
     * one, so an ongoing interactive resize keeps throttling
     * correctly */
    ccmd_client_resize_flush_pending(client);
}
