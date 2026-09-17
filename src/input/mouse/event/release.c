/**
 * @file input/mouse/event/release.c
 *
 * @brief Mouse button-release handling
 *
 * One of the files @c input/mouse/event/ is made of;
 * ends whatever drag @c input/mouse/event/press.c's
 * @c mouse_handle_press may have started.  See that file's comment
 * for the reasoning behind the three-way split.
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
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <lookup.h>
#include <stage.h>

/* Local includes */
#include <input/mouse/event.h>
#include <input/mouse/drag.h>
#include <input/mouse/drag/background.h>


/* Handle a button-release event to end a drag */
void mouse_handle_release(xcb_connection_t *connection,
        list_td *stages, const xcb_button_release_event_t *event,
        const config_td *config)
{
    client_td *client;
    stage_td *stage = NULL;
    desktop_td *desktop = NULL;
    struct position_s root_pos = { 0, 0 };

    (void) config;

    if (event != NULL) {
        root_pos.x = event->root_x;
        root_pos.y = event->root_y;
    }

    if (drag_background_is_active()) {
        drag_background_end(connection, stages, root_pos);
        return;
    }

    if (!drag_is_active()) {
        return;
    }

    client = drag_client();
    if (client != NULL && stages != NULL) {
        (void) lookup_find_client(stages, client->id,
                &stage, &desktop);
    }

    drag_end(connection, stage, desktop, root_pos);
}
