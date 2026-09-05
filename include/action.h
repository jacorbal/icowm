/**
 * @file action.h
 *
 * @brief Enumeration for all possible actions regarding clients,
 *        desktops, surfaces, and the window manager itself
 *
 * @ingroup enact
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef ACTION_H
#define ACTION_H


/**
 * @brief Window-related actions
 */
enum action_client_e {
    ACTION_CLIENT_MIN,

    ACTION_CLIENT_CLOSE = ACTION_CLIENT_MIN,
    /** Kill forcibly, without asking the client to close first */
    ACTION_CLIENT_KILL,
    ACTION_CLIENT_RESTORE,
    ACTION_CLIENT_FOCUS,
    ACTION_CLIENT_UNFOCUS,
    ACTION_CLIENT_RESIZE,
    ACTION_CLIENT_MOVE,
    ACTION_CLIENT_CENTER,

    /* Move to the monitor in a direction, relative to the
     * current one */
    ACTION_CLIENT_MOVE_MONITOR_NORTH,
    ACTION_CLIENT_MOVE_MONITOR_SOUTH,
    ACTION_CLIENT_MOVE_MONITOR_EAST,
    ACTION_CLIENT_MOVE_MONITOR_WEST,
    /** Move to a monitor named by index */
    ACTION_CLIENT_MOVE_TO_MONITOR,

    /** Change the window class */
    ACTION_CLIENT_RECLASS,
    /** Change the window role */
    ACTION_CLIENT_REROLE,
    ACTION_CLIENT_RENAME,

    ACTION_CLIENT_MAXIMIZE,
    ACTION_CLIENT_MAXIMIZE_HORZ,
    ACTION_CLIENT_MAXIMIZE_VERT,

    ACTION_CLIENT_ICONIFY,
    ACTION_CLIENT_HIDE,
    ACTION_CLIENT_UNHIDE,

    /** Shade, rolling the window up into its titlebar */
    ACTION_CLIENT_SHADE,
    ACTION_CLIENT_UNSHADE,
    ACTION_CLIENT_TOGGLE_SHADE,

    /** Pin, so the window shows on every desktop */
    ACTION_CLIENT_PIN,
    ACTION_CLIENT_UNPIN,
    ACTION_CLIENT_TOGGLE_PIN,

    /** Stick, so the window stays fixed on screen across viewport
     * panning; not to be confused with @c ACTION_CLIENT_TOGGLE_PIN
     * above, an unrelated concept despite the similar name (see
     * @c CLIENT_FLAG_STICKY's comment in @c client/state.h) */
    ACTION_CLIENT_TOGGLE_STICKY,

    ACTION_CLIENT_FULLSCREEN,
    ACTION_CLIENT_UNFULLSCREEN,
    ACTION_CLIENT_TOGGLE_FULLSCREEN,

    ACTION_CLIENT_RAISE,
    ACTION_CLIENT_LOWER,
    /** Keep the window always on top */
    ACTION_CLIENT_LAYER_ABOVE,
    ACTION_CLIENT_LAYER_NORMAL,
    /** Keep the window always at the bottom */
    ACTION_CLIENT_LAYER_BELOW,
    /** Cycle the layer between normal, above and below */
    ACTION_CLIENT_CYCLE_LAYER,

    /** Mark the window as urgent */
    ACTION_CLIENT_SET_URGENT,
    ACTION_CLIENT_CLEAR_URGENT,
    ACTION_CLIENT_SET_ICON,
    ACTION_CLIENT_TOGGLE_DECORATION,

    ACTION_CLIENT_MAX = ACTION_CLIENT_TOGGLE_DECORATION,
};


/**
 * @brief Desktop-related actions
 */
enum action_desktop_e {
    ACTION_DESKTOP_MIN,

    /** Change the desktop background */
    ACTION_DESKTOP_SET_BACKGROUND = ACTION_DESKTOP_MIN,
    /** Hide every client so the bare desktop shows */
    ACTION_DESKTOP_SHOW,

    ACTION_DESKTOP_CLIENT_ADD,
    ACTION_DESKTOP_CLIENT_REMOVE,
    /** Send a window to another desktop */
    ACTION_DESKTOP_CLIENT_SEND,
    /** Send a window to the front of the stack */
    ACTION_DESKTOP_CLIENT_SEND_FRONT,
    /** Send a window to the back of the stack */
    ACTION_DESKTOP_CLIENT_SEND_BACK,
    ACTION_DESKTOP_CLIENTS_REARRANGE,
    ACTION_DESKTOP_CLIENTS_ICONIFY_ALL,
    ACTION_DESKTOP_CLIENTS_DEICONIFY_ALL,

    /** Cycle through the active windows */
    ACTION_DESKTOP_CYCLE_CLIENTS_ACTIVE,
    /** Cycle back through the active windows */
    ACTION_DESKTOP_CYCLE_CLIENTS_PREV,
    /** Cycle to the next iconified window */
    ACTION_DESKTOP_CYCLE_CLIENTS_ICONS_NEXT,
    /** Cycle to the previous iconified window */
    ACTION_DESKTOP_CYCLE_CLIENTS_ICONS_PREV,

    /** Launch a program */
    ACTION_DESKTOP_COMMAND_LAUNCH,

    ACTION_DESKTOP_MAX = ACTION_DESKTOP_COMMAND_LAUNCH,
};


/**
 * @brief Screen-related actions
 */
enum action_surface_e {
    ACTION_SURFACE_MIN,

    /** Switch to a desktop named directly */
    ACTION_SURFACE_DESKTOP_SWITCH = ACTION_SURFACE_MIN,

    /* Switch to the desktop in a direction, relative to the
     * current one */
    ACTION_SURFACE_DESKTOP_SWITCH_NORTH,
    ACTION_SURFACE_DESKTOP_SWITCH_SOUTH,
    ACTION_SURFACE_DESKTOP_SWITCH_EAST,
    ACTION_SURFACE_DESKTOP_SWITCH_WEST,

    ACTION_SURFACE_MAX = ACTION_SURFACE_DESKTOP_SWITCH_WEST,
};


/**
 * @brief Window manager actions
 */
enum action_wm_e {
    ACTION_WM_MIN,

    /** Reload the current configuration */
    ACTION_WM_CONFIGURATION_RELOAD = ACTION_WM_MIN,
    ACTION_WM_EXIT,

    ACTION_WM_MAX = ACTION_WM_EXIT,
};


#endif  /* ! ACTION_H */
