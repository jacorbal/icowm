/**
 * @file eventq/dispatch.c
 *
 * @brief Event dispatcher: routes events from the queue to command
 *        handlers by action type
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

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <actdata.h>
#include <action.h>
#include <client.h>
#include <desktop.h>
#include <event.h>
#include <eventq.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Commands includes */
#include <cmds/ccmd.h>
#include <cmds/dcmd.h>
#include <cmds/geom.h>
#include <cmds/layer.h>
#include <cmds/meta.h>
#include <cmds/scmd.h>


/**
 * @brief Handle client events
 *
 * Process client-related events by dispatching them to appropriate
 * command handlers based on the action type.
 *
 * @param event Pointer to the client event to handle
 *
 * @note Complexity: @e O(1) for dispatch
 */
static void s_event_handle_client(event_td *event)
{
    desktop_td *desktop = NULL;
    client_td *client;
    action_data_client_td *client_data;


    if (event == NULL) {
        LOGGER_ERROR("Received null client event to process" \
                " in event queue", L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_CLIENT) {
        event_destroy(event);
        return; /* Invalid type */
    }

    if (event->action.object.client < ACTION_CLIENT_MIN ||
            event->action.object.client > ACTION_CLIENT_MAX) {
        LOGGER_WARNING("Invalid client action type: %d",
                event->action.object.client);
        event_destroy(event);
        return; /* Invalid action */
    }

    /* Point to the actual client and its data if needed */
    client = (client_td *) event->object;
    client_data = (action_data_client_td *) event->data;

    if (client == NULL) {
        LOGGER_ERROR("Received null client object in event", L_NARG);
        event_destroy(event);
        return;
    }

    LOGGER_TRACE("Processing client event: action=%d, client=%p",
            event->action.object.client, (void *) client);

    switch (event->action.object.client) {
        case ACTION_CLIENT_CREATE:
            /* This creates the client in 'client->xclient', but does
             * not allocates the memory of the client object.  This
             * action is intended to be called by the desktop,
             * therefore, it's responsibility of the desktop to execute
             * this action after invoking 'client_create'. */
            break;

        case ACTION_CLIENT_CLOSE:
            wcmd_client_close(client);
            break;

        case ACTION_CLIENT_KILL:
            wcmd_client_kill(client);
            break;

        case ACTION_CLIENT_RESTORE:
            wcmd_client_restore(client);
            break;

        case ACTION_CLIENT_FOCUS:
            wcmd_client_focus(client);
            break;

        case ACTION_CLIENT_UNFOCUS:
            wcmd_client_unfocus(client);
            break;

        case ACTION_CLIENT_MOVE:
            wcmd_client_move(client, client_data);
            break;

        case ACTION_CLIENT_CENTER:
            wcmd_client_center(client);
            break;

        case ACTION_CLIENT_RESIZE:
            wcmd_client_resize(client, client_data);
            break;

        case ACTION_CLIENT_RENAME:
            wcmd_client_rename(client, client_data);
            break;

        case ACTION_CLIENT_RECLASS:
            wcmd_client_reclass(client, client_data);
            break;

        case ACTION_CLIENT_REROLE:
            wcmd_client_rerole(client, client_data);
            break;

        case ACTION_CLIENT_MAXIMIZE:
            wcmd_client_maximize(client);
            break;

        case ACTION_CLIENT_MAXIMIZE_HORZ:
            wcmd_client_maximize_horz(client);
            break;

        case ACTION_CLIENT_MAXIMIZE_VERT:
            wcmd_client_maximize_vert(client);
            break;

        case ACTION_CLIENT_ICONIFY:
            wcmd_client_iconify(client);
            break;

        case ACTION_CLIENT_HIDE:
            wcmd_client_hide(client);
            break;

        case ACTION_CLIENT_UNHIDE:
            wcmd_client_unhide(client);
            break;

        case ACTION_CLIENT_SHADE:
            wcmd_client_shade(client);
            break;

        case ACTION_CLIENT_UNSHADE:
            wcmd_client_unshade(client);
            break;

        case ACTION_CLIENT_TOGGLE_SHADE:
            wcmd_client_toggle_shade(client);
            break;

        case ACTION_CLIENT_STICKY:
            wcmd_client_sticky(client);
            break;

        case ACTION_CLIENT_UNSTICKY:
            wcmd_client_unsticky(client);
            break;

        case ACTION_CLIENT_TOGGLE_STICKY:
            wcmd_client_toggle_sticky(client);
            break;

        case ACTION_CLIENT_FULLSCREEN:
            wcmd_client_fullscreen(client);
            break;

        case ACTION_CLIENT_UNFULLSCREEN:
            wcmd_client_unfullscreen(client);
            break;

        case ACTION_CLIENT_TOGGLE_FULLSCREEN:
            wcmd_client_toggle_fullscreen(client);
            break;

        case ACTION_CLIENT_RAISE:
            wcmd_client_raise(client);
            break;

        case ACTION_CLIENT_LOWER:
            wcmd_client_lower(client);
            break;

        case ACTION_CLIENT_LAYER_ABOVE:
            wcmd_client_layer_above(client);
            break;

        case ACTION_CLIENT_LAYER_NORMAL:
            wcmd_client_layer_normal(client);
            break;

        case ACTION_CLIENT_LAYER_BELOW:
            wcmd_client_layer_below(client);
            break;

        case ACTION_CLIENT_CYCLE_LAYER:
            wcmd_client_cycle_layer(client);
            break;

        case ACTION_CLIENT_SET_URGENT:
            wcmd_client_set_urgent(client);
            break;

        case ACTION_CLIENT_CLEAR_URGENT:
            wcmd_client_clear_urgent(client);
            break;

        case ACTION_CLIENT_SET_ICON:
            wcmd_client_set_icon(client, client_data);
            break;

        case ACTION_CLIENT_CYCLE_NEXT:
            desktop = wm_get_client_desktop(client);
            if (desktop != NULL) {
                dcmd_desktop_clients_cycle_active(desktop);
            }
            break;

        case ACTION_CLIENT_CYCLE_PREV:
            desktop = wm_get_client_desktop(client);
            if (desktop != NULL) {
                dcmd_desktop_clients_cycle_prev(desktop);
            }
            break;

        case ACTION_CLIENT_TOGGLE_DECORATION:
            wcmd_client_toggle_decoration(client);
            break;
    }

    xcb_flush(client->connection);
    event_destroy(event);
}


/**
 * @brief Handle desktop events
 *
 * Process desktop-related events by dispatching them to appropriate
 * command handlers based on the action type.
 *
 * @param event Pointer to the desktop event to handle
 *
 * @note Complexity: @e O(1) for dispatch
 */
static void s_event_handle_desktop(event_td *event)
{
    desktop_td *desktop;
    action_data_desktop_td *desktop_data;

    if (event == NULL) {
        LOGGER_ERROR("Received null desktop event to process" \
                " in event queue", L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_DESKTOP) {
        event_destroy(event);
        return; /* Invalid type */
    }

    if (event->action.object.desktop < ACTION_DESKTOP_MIN ||
        event->action.object.desktop > ACTION_DESKTOP_MAX) {
        LOGGER_WARNING("Invalid desktop action type: %d",
                event->action.object.desktop);
        event_destroy(event);
        return; /* Invalid action */
    }

    /* Point to the actual desktop and its data if needed */
    desktop = (desktop_td *) event->object;
    desktop_data = (action_data_desktop_td *) event->data;

    if (desktop == NULL) {
        LOGGER_ERROR("Received null desktop object in event", L_NARG);
        event_destroy(event);
        return;
    }

    LOGGER_TRACE("Processing desktop event: action=%d, desktop=%p",
            event->action.object.desktop, (void *) desktop);

    switch (event->action.object.desktop) {
        case ACTION_DESKTOP_RENAME:
            dcmd_desktop_rename(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_SET_BACKGROUND:
            dcmd_desktop_bg_color(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_CLEAR:
            dcmd_desktop_clear(desktop);
            break;

        case ACTION_DESKTOP_CLIENT_ADD:
            dcmd_desktop_client_add(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_CLIENT_REMOVE:
            dcmd_desktop_client_rem(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_CLIENT_SEND:
            dcmd_desktop_client_send(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_CLIENT_CLONE:
            dcmd_desktop_client_clone(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_CLIENT_SEND_FRONT:
            dcmd_desktop_client_send_front(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_CLIENT_SEND_BACK:
            dcmd_desktop_client_send_back(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_CLIENTS_REARRANGE:
            dcmd_desktop_clients_rearrange(desktop);
            break;

        case ACTION_DESKTOP_CLIENTS_ICONIFY_ALL:
            dcmd_desktop_clients_iconify_all(desktop);
            break;

        case ACTION_DESKTOP_CYCLE_CLIENTS_PREV:
            dcmd_desktop_clients_cycle_prev(desktop);
            break;

        case ACTION_DESKTOP_CYCLE_CLIENTS_ACTIVE:
            dcmd_desktop_clients_cycle_active(desktop);
            break;

        case ACTION_DESKTOP_CYCLE_CLIENTS_ICONS:
            dcmd_desktop_clients_cycle_icons(desktop);
            break;

        case ACTION_DESKTOP_LOCK:
            dcmd_desktop_lock(desktop);
            break;

        case ACTION_DESKTOP_UNLOCK:
            dcmd_desktop_unlock(desktop);
            break;

        case ACTION_DESKTOP_SET_LAYOUT:
            dcmd_desktop_layout(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_COMMAND_LAUNCH:
            dcmd_desktop_process_launch(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_PROCESS_KILL:
            dcmd_desktop_process_kill(desktop, desktop_data);
            break;
    }

    event_destroy(event);
}


/**
 * @brief Handle surface events
 *
 * Process surface-related events by dispatching them to appropriate
 * command handlers based on the action type.
 *
 * @param event Pointer to the surface event to handle
 *
 * @note Complexity: @e O(1) for dispatch
 */
static void s_event_handle_surface(event_td *event)
{
    surface_td *surface;
    action_data_surface_td *surface_data;

    if (event == NULL) {
        LOGGER_ERROR("Received null screen event to process in" \
                " event queue", L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_SURFACE) {
        event_destroy(event);
        return; /* Invalid type */
    }

    if (event->action.object.surface < ACTION_SURFACE_MIN ||
        event->action.object.surface > ACTION_SURFACE_MAX) {
        LOGGER_WARNING("Invalid surface action type: %d",
                event->action.object.surface);
        event_destroy(event);
        return; /* Invalid action */
    }

    /* Point to the actual surface and its data if needed */
    surface = (surface_td *) event->object;
    surface_data = (action_data_surface_td *) event->data;

    if (surface == NULL) {
        LOGGER_ERROR("Received null surface object in event", L_NARG);
        event_destroy(event);
        return;
    }

    LOGGER_TRACE("Processing surface event: action=%d, surface=%p",
            event->action.object.surface, (void *) surface);

    switch (event->action.object.surface) {
        case ACTION_SURFACE_DESKTOP_ADD:
            scmd_surface_desktop_add(surface, surface_data);
            break;

        case ACTION_SURFACE_DESKTOP_REMOVE:
            scmd_surface_desktop_rem(surface, surface_data);
            break;

        case ACTION_SURFACE_DESKTOP_SWITCH:
            scmd_surface_desktop_switch(surface, surface_data);
            break;

        case ACTION_SURFACE_DESKTOP_SWITCH_NEXT:
            scmd_surface_desktop_switch_next(surface);
            break;

        case ACTION_SURFACE_DESKTOP_SWITCH_PREV:
            scmd_surface_desktop_switch_prev(surface);
            break;

        case ACTION_SURFACE_TOGGLE_FULLSCREEN:
            scmd_surface_toggle_fullscreen(surface);
            break;

        case ACTION_SURFACE_SET_RESOLUTION:
            scmd_surface_set_resolution(surface, surface_data);
            break;

        case ACTION_SURFACE_SET_ORIENTATION:
            scmd_surface_set_orientation(surface, surface_data);
            break;

        case ACTION_SURFACE_SET_BRIGHTNESS:
            scmd_surface_set_brightness(surface, surface_data);
            break;

        case ACTION_SURFACE_SET_CONTRAST:
            scmd_surface_set_contrast(surface, surface_data);
            break;

        case ACTION_SURFACE_CONFIGURE_SETTINGS:
            scmd_surface_configure_settings(surface, surface_data);
            break;
    }

    event_destroy(event);
}


/**
 * @brief Handle window manager events
 *
 * Process window manager-related events by dispatching them to
 * appropriate command handlers based on the action type.
 *
 * @param event Pointer to the window manager event to handle
 *
 * @note Complexity: @e O(1) for dispatch
 */
static void s_event_handle_wm(event_td *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received null window manager event to process" \
                " in event queue", L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_WM) {
        event_destroy(event);
        return; /* Invalid type */
    }

    if (event->action.object.wm < ACTION_WM_MIN ||
            event->action.object.wm > ACTION_WM_MAX) {
        LOGGER_WARNING("Invalid window manager action type: %d",
                event->action.object.wm);
        event_destroy(event);
        return; /* Invalid action */
    }

    LOGGER_TRACE("Processing window manager event: action=%d",
            event->action.object.wm);

    switch (event->action.object.wm) {
        case ACTION_WM_CONFIGURATION_RELOAD:
            LOGGER_DEBUG("Window manager configuration reload requested",
                    L_NARG);
            break;

        case ACTION_WM_CONFIGURATION_SAVE:
            LOGGER_DEBUG("Window manager configuration save requested",
                    L_NARG);
            break;

        case ACTION_WM_SURFACE_ADD:
            LOGGER_DEBUG("Surface addition requested", L_NARG);
            break;

        case ACTION_WM_SURFACE_REMOVE:
            LOGGER_DEBUG("Surface removal requested", L_NARG);
            break;

        case ACTION_WM_EXIT:
            LOGGER_DEBUG("Window manager exit action requested", L_NARG);
            wm_request_stop();
            break;
    }

    event_destroy(event);
}


/* Process events in event queue */
int eventq_process(void)
{
    event_td *processed_event;

    /* Loop until the queue is empty; 'eventq_extract' handles sync. */
    while (true) {
        processed_event = eventq_extract();
        if (processed_event == NULL) {
            break;
        }

        /* Handle each type of event by dispatching to the appropriate
         * handler function */
        switch (processed_event->action.type) {
            case ACTION_TYPE_CLIENT:
                s_event_handle_client(processed_event);
                break;
            case ACTION_TYPE_DESKTOP:
                s_event_handle_desktop(processed_event);
                break;
            case ACTION_TYPE_SURFACE:
                s_event_handle_surface(processed_event);
                break;
            case ACTION_TYPE_WM:
                s_event_handle_wm(processed_event);
                break;
        }
    }

    return 0;
}
