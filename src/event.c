/**
 * @file event.c
 *
 * @brief Event structure implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>     /* NULL, free, malloc */

/* Project includes */
#include <actdata.h>
#include <action.h>
#include <logger.h>
#include <priority.h>

/* Local includes */
#include <event.h>


/* Initializes a new event structure */
event_td *event_init(void *object, void *object_data,
        action_td action, enum priority_e priority)
{
    event_td *event;

    LOGGER_TRACE("Initializing event data structure", L_NARG);
    event = malloc(sizeof(event_td));
    if (event == NULL) {
        LOGGER_WARNING("Failed to allocate memory for event data" \
                " structure", L_NARG);
        return NULL;
    }

    event->object = object;     /* Cast later */
    event->data = object_data;  /* Cast later */
    event->action = action;
    event->priority = priority;

    return event;
}


/* Deallocate memory for an event structure */
void event_destroy(event_td *event)
{
    LOGGER_TRACE("Destroying event data structure", L_NARG);
    if (event != NULL) {
        if (event->data != NULL) {
            switch (event->action.type) {
                case ACTION_TYPE_CLIENT:
                    action_data_client_destroy(
                            (action_data_client_td *) event->data);
                    break;
                case ACTION_TYPE_DESKTOP:
                    action_data_desktop_destroy(
                            (action_data_desktop_td *) event->data);
                    break;
                case ACTION_TYPE_SURFACE:
                    action_data_surface_destroy(
                            (action_data_surface_td *) event->data);
                    break;
                case ACTION_TYPE_WM:
                    action_data_wm_destroy(
                            (action_data_wm_td *) event->data);
                    break;
            }
            event->data = NULL;
        }
        free(event);
    }
}
