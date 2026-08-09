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

/* Util includes */
#include <utils/safe/safestr.h>

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


/* Deep-copy an event structure */
event_td *event_clone(const event_td *src)
{
    event_td *clone;
    action_data_client_td *cdata;
    const action_data_client_td *orig_cdata;
    action_data_desktop_td *ddata;
    action_data_surface_td *sdata;
    action_data_wm_td *wdata;

    if (src == NULL) {
        return NULL;
    }

    clone = malloc(sizeof(event_td));
    if (clone == NULL) {
        LOGGER_WARNING("Failed to allocate memory for event clone",
                L_NARG);
        return NULL;
    }

    clone->object = src->object;    /* Shared reference */
    clone->action = src->action;    /* Value copy (no pointers) */
    clone->priority = src->priority;
    clone->data = NULL;
    if (src->data == NULL) {
        return clone;
    }

    switch (src->action.type) {
        case ACTION_TYPE_CLIENT:
            orig_cdata = (const action_data_client_td *) src->data;
            cdata = malloc(sizeof(action_data_client_td));
            if (cdata == NULL) {
                free(clone);
                return NULL;
            }

            *cdata = *orig_cdata;   /* Shallow copy: client ptr + action */
            switch (orig_cdata->action_client) {
                case ACTION_CLIENT_RENAME:
                case ACTION_CLIENT_RECLASS:
                case ACTION_CLIENT_REROLE:
                case ACTION_CLIENT_SET_ICON:
                    cdata->new_data.str.str0 =
                        safe_strdup(orig_cdata->new_data.str.str0);
                    cdata->new_data.str.str1 =
                        safe_strdup(orig_cdata->new_data.str.str1);
                    if ((orig_cdata->new_data.str.str0 != NULL &&
                                cdata->new_data.str.str0 == NULL) ||
                            (orig_cdata->new_data.str.str1 != NULL &&
                                cdata->new_data.str.str1 == NULL)) {
                        action_data_client_destroy(cdata);
                        free(clone);
                        return NULL;
                    }
                    break;

                case ACTION_CLIENT_CREATE:
                case ACTION_CLIENT_CLOSE:
                case ACTION_CLIENT_KILL:
                case ACTION_CLIENT_RESTORE:
                case ACTION_CLIENT_FOCUS:
                case ACTION_CLIENT_UNFOCUS:
                case ACTION_CLIENT_RESIZE:
                case ACTION_CLIENT_MOVE:
                case ACTION_CLIENT_CENTER:
                case ACTION_CLIENT_MOVE_NEXT_MONITOR:
                case ACTION_CLIENT_MOVE_TO_MONITOR:
                case ACTION_CLIENT_MAXIMIZE:
                case ACTION_CLIENT_MAXIMIZE_HORZ:
                case ACTION_CLIENT_MAXIMIZE_VERT:
                case ACTION_CLIENT_ICONIFY:
                case ACTION_CLIENT_HIDE:
                case ACTION_CLIENT_UNHIDE:
                case ACTION_CLIENT_SHADE:
                case ACTION_CLIENT_UNSHADE:
                case ACTION_CLIENT_TOGGLE_SHADE:
                case ACTION_CLIENT_STICKY:
                case ACTION_CLIENT_UNSTICKY:
                case ACTION_CLIENT_TOGGLE_STICKY:
                case ACTION_CLIENT_FULLSCREEN:
                case ACTION_CLIENT_UNFULLSCREEN:
                case ACTION_CLIENT_TOGGLE_FULLSCREEN:
                case ACTION_CLIENT_RAISE:
                case ACTION_CLIENT_LOWER:
                case ACTION_CLIENT_LAYER_ABOVE:
                case ACTION_CLIENT_LAYER_NORMAL:
                case ACTION_CLIENT_LAYER_BELOW:
                case ACTION_CLIENT_SET_URGENT:
                case ACTION_CLIENT_CYCLE_LAYER:
                case ACTION_CLIENT_CLEAR_URGENT:
                case ACTION_CLIENT_TOGGLE_DECORATION:
                case ACTION_CLIENT_CYCLE_NEXT:
                case ACTION_CLIENT_CYCLE_PREV:
                    /* Geometry / no extra heap data: shallow copy is
                     * correct, already done by struct assignment above */
                    break;
            }
            clone->data = cdata;
            break;

        case ACTION_TYPE_DESKTOP:
            ddata = malloc(sizeof(action_data_desktop_td));
            if (ddata == NULL) {
                free(clone);
                return NULL;
            }
            *ddata = *(const action_data_desktop_td *) src->data;
            clone->data = ddata;
            break;

        case ACTION_TYPE_SURFACE:
            sdata = malloc(sizeof(action_data_surface_td));
            if (sdata == NULL) {
                free(clone);
                return NULL;
            }
            *sdata = *(const action_data_surface_td *) src->data;
            clone->data = sdata;
            break;

        case ACTION_TYPE_WM:
            wdata = malloc(sizeof(action_data_wm_td));
            if (wdata == NULL) {
                free(clone);
                return NULL;
            }
            *wdata = *(const action_data_wm_td *) src->data;
            clone->data = wdata;
            break;
    }

    return clone;
}
