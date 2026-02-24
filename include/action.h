/**
 * @file action.h
 *
 * @brief Enumeration for all possible actions regarding clients,
 *        desktops and surfaces, and action structure
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
 * @brief Types of actions for the window manager
 *
 * Each action type corresponds to specific environments where
 * interactions within the windowing system might take place.
 */
enum action_type_e {
    ACTION_TYPE_CLIENT,  /**< Window-related actions */
    ACTION_TYPE_DESKTOP, /**< Desktop-related actions */
    ACTION_TYPE_SURFACE, /**< Screen-related actions */
    ACTION_TYPE_WM,      /**< Miscellaneous actions related exclusively
                              to the window manager */
};


/**
 * @brief Window-related actions
 */
enum action_client_e {
    ACTION_CLIENT_MIN,
    ACTION_CLIENT_CREATE =              /**< Create window */
        ACTION_CLIENT_MIN,
//    ACTION_CLIENT_SET_PROPERTY,         /**< Change window properties */
//    ACTION_CLIENT_DESTROY,              /**< Destroy and free resources */
    ACTION_CLIENT_CLOSE,                /**< Close window */
    ACTION_CLIENT_RESTORE,              /**< Restore window */
    ACTION_CLIENT_FOCUS,                /**< Focus on window */
    ACTION_CLIENT_UNFOCUS,              /**< Defocus window */
    ACTION_CLIENT_RESIZE,               /**< Resize window */
    ACTION_CLIENT_MOVE,                 /**< Move window */
    ACTION_CLIENT_RECLASS,              /**< Change the window class */
    ACTION_CLIENT_REROLE,               /**< Change the window role */
    ACTION_CLIENT_RENAME,               /**< Rename window */
    ACTION_CLIENT_MAXIMIZE,             /**< Maximize window */
    ACTION_CLIENT_MAXIMIZE_HORZ,        /**< Maximize window horizontally */
    ACTION_CLIENT_MAXIMIZE_VERT,        /**< Maximize window vertically */
    ACTION_CLIENT_ICONIFY,              /**< Iconify (& minimize) */
    ACTION_CLIENT_HIDE,                 /**< Hide the window */
    ACTION_CLIENT_UNHIDE,               /**< Show the window */
    ACTION_CLIENT_SHADE,                /**< Shade (roll-up) the window */
    ACTION_CLIENT_UNSHADE,              /**< Shade (roll-up) the window */
    ACTION_CLIENT_TOGGLE_SHADE,         /**< Shade (roll-up) the window */
    ACTION_CLIENT_STICKY,               /**< Set window sticky mode */
    ACTION_CLIENT_UNSTICKY,             /**< Remove window sticky mode */
    ACTION_CLIENT_TOGGLE_STICKY,        /**< Toggle window sticky mode */
    ACTION_CLIENT_FULLSCREEN,           /**< Set full surface mode */
    ACTION_CLIENT_UNFULLSCREEN,         /**< Remove full surface mode */
    ACTION_CLIENT_TOGGLE_FULLSCREEN,    /**< Toggle full surface mode */
    ACTION_CLIENT_RAISE,                /**< Raise the window */
    ACTION_CLIENT_LOWER,                /**< Lower the window */
    ACTION_CLIENT_LAYER_ABOVE,          /**< Window always on top */
    ACTION_CLIENT_LAYER_NORMAL,         /**< Window normal layer */
    ACTION_CLIENT_LAYER_BELOW,          /**< Window always on bottom */
    ACTION_CLIENT_SET_URGENT,           /**< Mark window as urgent */
    ACTION_CLIENT_CLEAR_URGENT,         /**< Clear urgency level */
    ACTION_CLIENT_SET_ICON,             /**< Set icon for the window */

    /* Add more as needed */
//    ACTION_CLIENT_SET_OPACITY,          /**< Change window opacity */
//    ACTION_CLIENT_ATTACH,               /**< Attach window to other window */
//    ACTION_CLIENT_TILE,                 /**< Set tiling mode */
//    ACTION_CLIENT_FLOAT,                /**< Set floating mode */
//    ACTION_CLIENT_STACK,                /**< Rearrange Z windows order */

    ACTION_CLIENT_MAX =
        ACTION_CLIENT_SET_ICON,
};

/**
 * @brief Desktop-related actions
 */
enum action_desktop_e {
    ACTION_DESKTOP_MIN,
    ACTION_DESKTOP_RENAME =             /**< Rename desktop */
        ACTION_DESKTOP_MIN,
    ACTION_DESKTOP_SET_BACKGROUND,      /**< Change background */
    ACTION_DESKTOP_CLEAR,               /**< Close and remove all w. */
    ACTION_DESKTOP_CLIENT_ADD,          /**< Add a new window */
    ACTION_DESKTOP_CLIENT_REMOVE,       /**< Remove a window */
    ACTION_DESKTOP_CLIENT_SEND,         /**< Send w. to another desktop */
    ACTION_DESKTOP_CLIENT_CLONE,        /**< Clone an existing window */
    ACTION_DESKTOP_CLIENT_SEND_FRONT,   /**< Set w. to front of stack */
    ACTION_DESKTOP_CLIENT_SEND_BACK,    /**< Set w. to back of stack */
    ACTION_DESKTOP_CLIENTS_REARRANGE,   /**< Rearrange windows */
    ACTION_DESKTOP_CLIENTS_ICONIFY_ALL, /**< Iconify (& minim.) all w. */
    ACTION_DESKTOP_CYCLE_CLIENTS_ACTIVE,/**< Cycle through active w. */
    ACTION_DESKTOP_CYCLE_CLIENTS_ICONS, /**< Cycle through iconified w. */
    ACTION_DESKTOP_LOCK,                /**< Lock desktop session */
    ACTION_DESKTOP_UNLOCK,              /**< Unlock desktop session */
    ACTION_DESKTOP_SET_LAYOUT,          /**< Change desktop layout */
    ACTION_DESKTOP_COMMAND_LAUNCH,      /**< Launch program */
    ACTION_DESKTOP_PROCESS_KILL,        /**< Terminate process */

    /* Add more as needed */
//    ACTION_DESKTOP_SWITCH,              /**< Switch desktop */
//    ACTION_DESKTOP_SWITCH_NEXT,         /**< Switch to next */
//    ACTION_DESKTOP_SWITCH_PREV,         /**< Switch to previous */

    ACTION_DESKTOP_MAX =
        ACTION_DESKTOP_PROCESS_KILL,
};


/**
 * @brief Screen-related actions
 */
enum action_surface_e {
    ACTION_SURFACE_MIN,
    ACTION_SURFACE_DESKTOP_ADD =         /**< Add a new desktop */
        ACTION_SURFACE_MIN,
    ACTION_SURFACE_DESKTOP_REMOVE,       /**< Remove a desktop */
    ACTION_SURFACE_DESKTOP_SWITCH,       /**< Switch another desktop */
    ACTION_SURFACE_DESKTOP_SWITCH_NEXT,  /**< Switch to next desktop */
    ACTION_SURFACE_DESKTOP_SWITCH_PREV,  /**< Switch to previous desktop */
    ACTION_SURFACE_TOGGLE_FULLSCREEN,    /**< Toggle full surface mode */
    ACTION_SURFACE_SET_RESOLUTION,       /**< Change resolution */
    ACTION_SURFACE_SET_ORIENTATION,      /**< Change orientation */
    ACTION_SURFACE_SET_BRIGHTNESS,       /**< Change surface brightness */
    ACTION_SURFACE_SET_CONTRAST,         /**< Change surface contrast */
    ACTION_SURFACE_CONFIGURE_SETTINGS,   /**< Screen configuration */

    /* Add more as needed */
//    ACTION_SURFACE_SWITCH,               /**< Switch active surface */

    ACTION_SURFACE_MAX =
        ACTION_SURFACE_CONFIGURE_SETTINGS,
};


/**
 * @brief Window manager actions
 */
enum action_wm_e {
    ACTION_WM_MIN,
    ACTION_WM_CONFIGURATION_RELOAD =    /**< Reload current configuration */
        ACTION_WM_MIN,
    ACTION_WM_CONFIGURATION_SAVE,       /**< Save current configuration */
    ACTION_SURFACE_ADD,                  /**< Add new surface */
    ACTION_SURFACE_REMOVE,               /**< Remove surface */
//    ACTION_WM_SURFACE_SWITCH,            /**< Switch active surface */
    ACTION_WM_EXIT,                     /**< Exit the window manager */

    /* Add more as needed */

    ACTION_WM_MAX = ACTION_WM_EXIT,
};


/**
 * @brief Possible actions based on type
 *
 * @see action_type_e,
 *      action_client_e, action_desktop_e, action_surface_e, action_wm_e
 */
typedef struct {
    enum action_type_e type;                    /**< Type of the action */
    union {
        enum action_client_e client;            /**< Client actions */
        enum action_desktop_e desktop;          /**< Desktop actions */
        enum action_surface_e surface;          /**< Surface actions */
        enum action_wm_e wm;                    /**< WM actions */
    } object;
} action_td;


#endif  /* ! ACTION_H */
