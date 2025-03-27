/**
 * @file actions.h
 *
 * @brief Enumeration for all possible actions regarding windows,
 *        desktops and screens
 */

#ifndef ACTIONS_H
#define ACTIONS_H


/**
 * @brief Types of actions for the window manager
 *
 * Each action type corresponds to specific environments where
 * interactions within the windowing system might take place.
 */
enum action_type_e {
    ACTION_TYPE_WINDOW,  /**< Window-related actions */
    ACTION_TYPE_DESKTOP, /**< Desktop-related actions */
    ACTION_TYPE_SCREEN,  /**< Screen-related actions */
    ACTION_TYPE_WM,      /**< Miscellaneous actions related exclusively
                              to the window manager */
};


/**
 * @brief Window-related actions
 */
enum action_window_e {
    ACTION_WINDOW_CREATE,               /**< Set memory and create window */
//    ACTION_WINDOW_DESTROY,              /**< Destroy and free resources */
    ACTION_WINDOW_CLOSE,                /**< Close window */
    ACTION_WINDOW_SET_PROPERTY,         /**< Change properties */
    ACTION_WINDOW_RESTORE,              /**< Restore window */
    ACTION_WINDOW_FOCUS,                /**< Focus on window */
    ACTION_WINDOW_UNFOCUS,              /**< Defocus window */
    ACTION_WINDOW_RESIZE,               /**< Resize window */
    ACTION_WINDOW_MOVE,                 /**< Move window */
    ACTION_WINDOW_RENAME,               /**< Rename window */
    ACTION_WINDOW_MAXIMIZE_HORIZONTALLY,/**< Maximize window horizontally */
    ACTION_WINDOW_MAXIMIZE_VERTICALLY,  /**< Maximize window vertically */
    ACTION_WINDOW_MAXIMIZE,             /**< Maximize window */
    ACTION_WINDOW_ICONIFY,              /**< Iconify (& minimize) */
    ACTION_WINDOW_TOGGLE_STICKY,        /**< Toggle window sticky mode */
    ACTION_WINDOW_TOGGLE_FULLSCREEN,    /**< Toggle full screen mode */
    ACTION_WINDOW_CLONE,                /**< Clone an existing window */
    ACTION_WINDOW_LAYER_ON_TOP,         /**< Window always on top */
    ACTION_WINDOW_LAYER_NORMAL,         /**< Window normal layer */
    ACTION_WINDOW_LAYER_ON_BOTTOM,      /**< Window always on bottom */
    ACTION_WINDOW_SET_CLASS,            /**< Change the window class */
    ACTION_WINDOW_SET_URGENT,           /**< Mark window as urgent */
    ACTION_WINDOW_SET_ICON,             /**< Set icon for the window */

    /* Add more as needed */
//    ACTION_WINDOW_SET_OPACITY,          /**< Change window opacity */
//    ACTION_WINDOW_ATTACH,               /**< Attach window to other window */
//    ACTION_WINDOW_TILE,                 /**< Set tiling mode */
//    ACTION_WINDOW_FLOAT,                /**< Set floating mode */
//    ACTION_WINDOW_STACK,                /**< Rearrange Z windows order */
};

/**
 * @brief Desktop-related actions
 */
enum action_desktop_e {
    ACTION_DESKTOP_RENAME,              /**< Rename desktop */
    ACTION_DESKTOP_SET_BACKGROUND,      /**< Change background */
    ACTION_DESKTOP_CLEAR,               /**< Close and remove all windows */
    ACTION_DESKTOP_WINDOW_ADD,          /**< Add a new window */
    ACTION_DESKTOP_WINDOW_REMOVE,       /**< Remove a window */
    ACTION_DESKTOP_WINDOW_SEND,         /**< Send window to another desktop */
    ACTION_DESKTOP_WINDOW_SEND_FRONT,   /**< Set window to front */
    ACTION_DESKTOP_WINDOW_SEND_BACK,    /**< Set window to back */
    ACTION_DESKTOP_WINDOWS_REARRANGE,   /**< Rearrange windows */
    ACTION_DESKTOP_WINDOWS_ICONIFY_ALL, /**< Iconify (& minim.) all windows */
    ACTION_DESKTOP_CYCLE_WINDOWS_ACTIVE,/**< Cycle through active windows */
    ACTION_DESKTOP_CYCLE_WINDOWS_ICONS, /**< Cycle through iconified windows */
    ACTION_DESKTOP_LOCK,                /**< Lock desktop session */
    ACTION_DESKTOP_UNLOCK,              /**< Unlock desktop session */
    ACTION_DESKTOP_SET_LAYOUT,          /**< Change desktop layout */
    ACTION_DESKTOP_APPLICATION_LAUNCH,  /**< Launch application */
    ACTION_DESKTOP_APPLICATION_KILL,    /**< Terminate application */

    /* Add more as needed */
//    ACTION_DESKTOP_SWITCH,              /**< Switch desktop */
//    ACTION_DESKTOP_SWITCH_NEXT,         /**< Switch to next */
//    ACTION_DESKTOP_SWITCH_PREV,         /**< Switch to previous */
};


/**
 * @brief Screen-related actions
 */
enum action_screen_e {
    ACTION_SCREEN_DESKTOP_ADD,          /**< Add a new desktop */
    ACTION_SCREEN_DESKTOP_REMOVE,       /**< Remove a desktop */
    ACTION_SCREEN_DESKTOP_SWITCH,       /**< Switch another desktop */
    ACTION_SCREEN_DESKTOP_SWITCH_NEXT,  /**< Switch to next desktop */
    ACTION_SCREEN_DESKTOP_SWITCH_PREV,  /**< Switch to previous desktop */
    ACTION_SCREEN_TOGGLE_FULLSCREEN,    /**< Toggle full screen mode */
    ACTION_SCREEN_SET_RESOLUTION,       /**< Change resolution */
    ACTION_SCREEN_SET_ORIENTATION,      /**< Change orientation */
    ACTION_SCREEN_SET_BRIGHTNESS,       /**< Change screen brightness */
    ACTION_SCREEN_SET_CONTRAST,         /**< Change screen contrast */
    ACTION_SCREEN_CONFIGURE_SETTINGS,   /**< Screen configuration */

    /* Add more as needed */
//    ACTION_SCREEN_SWITCH,               /**< Switch active screen */
};


/**
 * @brief Window manager actions
 */
enum action_wm_e {
    ACTION_WM_CONFIGURATION_RELOAD,     /**< Reload current configuration */
    ACTION_WM_CONFIGURATION_SAVE,       /**< Save current configuration */
    ACTION_SCREEN_ADD,                  /**< Add new screen */
    ACTION_SCREEN_REMOVE,               /**< Remove screen */
//    ACTION_WM_SCREEN_SWITCH,            /**< Switch active screen */
    ACTION_WM_EXIT,                     /**< Exit the window manager */

    /* Add more as needed */
};


/**
 * @brief Possible actions based on type
 *
 * @see action_type_e,
 *      action_window_e, action_desktop_e, action_screen_e, action_wm_e
 */
typedef struct {
    enum action_type_e type;                    /**< Type of the action */
    union {
        enum action_window_e action_window;     /**< Window actions */
        enum action_desktop_e action_desktop;   /**< Desktop actions */
        enum action_screen_e action_screen;     /**< Screen actions */
        enum action_wm_e action_wm;             /**< WM actions */
    } action;
} action_td;


#endif  /* ! ACTIONS_H */
