/**
 * @file wm/clients.c
 *
 * @brief Private helpers shared across @c wm sub-modules implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdint.h>
#include <stddef.h>     /* NULL */

/* ADT includes */
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>

/* Local includes */
#include <wm/internal.h>


/* Visit every currently managed client across every surface and desktop */
uint32_t wm_for_each_client(wm_td *wm, void (*action)(client_td *client,
            void *userdata), void *userdata)
{
    uint32_t count = 0u;
    list_td *surfaces = wm_surfaces(wm);

    if (surfaces == NULL) {
        return 0u;
    }

    for (list_item_td *snode = list_head(surfaces); snode != NULL;
            snode = list_next(snode)) {
        surface_td *const surface = (surface_td *) list_data(snode);

        if (surface == NULL) {
            continue;
        }

        for (uint32_t did = 0u; did < surface->desktop_count; ++did) {
            desktop_td *const desktop = surface_desktop_get(surface, did);
            void *elem;

            if (desktop == NULL || desktop->clients == NULL) {
                continue;
            }

            ohtbl_foreach(desktop->clients, elem) {
                client_td *const client = (client_td *) elem;

                if (client != NULL) {
                    ++count;
                    if (action != NULL) {
                        action(client, userdata);
                    }
                }
            }
        }
    }

    return count;
}
