/**
 * @file client.h
 *
 * @brief Window definition and declaration
 *
 * Defines the data structure that encapsulates all relevant information
 * about a specific client and its state within the windowing system
 * environment.  In this regards, provides a framework for managing the
 * visual and operational properties of the window, including its
 * identifier, name, state, graphical attributes, geometry, and behavior
 * preferences.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CLIENT_H
#define CLIENT_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>  /* pid_t */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Type includes */
#include <types/pair.h> /* geometry_s, sides_s */

/* Util includes */
#include <utils/safeflg.h>

/* Project includes */
#include <action.h>
#include <config.h>
#include <event.h>
#include <priority.h>


/* Default priority on client creation */
#define CLIENT_PRIORITY_DEFAULT (PRIORITY_NORMAL)


/**
 * @brief Possible client states a client can be in
 */
enum client_state_e {
    CLIENT_STATE_NORMAL,            /**< Regular state */
    CLIENT_STATE_ICONIFIED,         /**< Iconified */
//    CLIENT_STATE_SHADED,            /**< Shaded (rolled-up), if decorated */
    CLIENT_STATE_MAXIMIZED,         /**< Maximized */
    CLIENT_STATE_MAXIMIZED_HORZ,    /**< Maximized horiz. */
    CLIENT_STATE_MAXIMIZED_VERT,    /**< Maximized vert. */
    CLIENT_STATE_FULLSCREEN,        /**< Full screen */
};


/**
 * @brief Possible client types that can be
 */
enum client_type_e {
    CLIENT_TYPE_NORMAL,         /* Normal client */
    CLIENT_TYPE_DIALOG,         /* Dialogues or interaction needed */
    CLIENT_TYPE_TOOLBAR,        /* Quick actions or tools */
    CLIENT_TYPE_NOTIFICATION,   /* Temporal messages */
    CLIENT_TYPE_MENU,           /* Menu options */
    CLIENT_TYPE_DESKTOP,        /* The desktop "client" */
    CLIENT_TYPE_SPLASH,         /* The client is a loading message */
    CLIENT_TYPE_UTILITY,        /* Additional functions: control panels... */
    CLIENT_TYPE_DROPDOWN_MENU,  /* Drop-down menu */
    CLIENT_TYPE_POPUP_MENU,     /* Contextual menu */
    CLIENT_TYPE_COMBO,          /* Part of a combined frame */
    CLIENT_TYPE_TOOLTIP,        /* The client is a tooltip */
    CLIENT_TYPE_DOCK,           /* Dock or panel feature */
    CLIENT_TYPE_DND,            /* The client is being dragged */
};


/**
 * @brief Operations on a client
 */
enum window_operation_e {
    CLIENT_OPERATION_IDLE,      /* No operation ongoing */
    CLIENT_OPERATION_MOVING,    /* Window is being moved */
    CLIENT_OPERATION_RESIZING,  /* Window is being resized */
};


/**
 * @brief Window characteristics using flags using bitwise flags
 */
enum window_flags_e {
    CLIENT_FLAG_HIDDEN       = 1 << 0,
    CLIENT_FLAG_FOCUSABLE    = 1 << 1,
    CLIENT_FLAG_STICKY       = 1 << 2,
    CLIENT_FLAG_SHADED       = 1 << 3,
    CLIENT_FLAG_DECORATED    = 1 << 4,
    CLIENT_FLAG_URGENT       = 1 << 5,
    CLIENT_FLAG_RESIZABLE    = 1 << 6,
    CLIENT_FLAG_DISABLED     = 1 << 7,
    CLIENT_FLAG_SKIP_TASKBAR = 1 << 8,
    CLIENT_FLAG_SKIP_PAGER   = 1 << 9,
    CLIENT_FLAG_MAX = 10,
};


/**
 * @brief Mutual exclusive status about the client focus
 */
enum client_focusing_e {
    CLIENT_FOCUSING_UNFOCUSED,  /**< No focus state */
    CLIENT_FOCUSING_FOCUSED,    /**< Window has focus */
};


/**
 * @brief Window mutual exclusive possible layers
 *
 * Identifies the layering options for clients, which affect their
 * visibility order on the screen.
 */
enum client_layer_e {
    CLIENT_LAYER_ABOVE,     /**< Always on top, in front */
    CLIENT_LAYER_NORMAL,    /**< Normal behavior */
    CLIENT_LAYER_BELOW,     /**< Always behind every client */
};


/**
 * @brief Anchor point used to determine how a client's position is
 *        adjusted relative to its size when resized
 *
 * @note EWMH: "Window Managers MUST honor the @c win_gravity field of
 *             @c WM_NORMAL_HINTS for both @c MapRequest @e and
 *             @c ConfigureRequest events (ICCCM Version 2.0,
 *                §4.1.2.3 and §4.1.5)"
 */
enum client_gravity_e {     /* Placed at the reference point: */
    CLIENT_GRAVITY_STATIC,      /* 0: left top corner of client client */
    CLIENT_GRAVITY_NORTH_WEST,  /* 1: left top corner of client */
    CLIENT_GRAVITY_NORTH,       /* 2: center of the client's top side */
    CLIENT_GRAVITY_NORTH_EAST,  /* 3: right top corner of client */
    CLIENT_GRAVITY_EAST,        /* 4: center of the client's right side */
    CLIENT_GRAVITY_SOUTH_EAST,  /* 5: right bottom corner of client */
    CLIENT_GRAVITY_SOUTH,       /* 6: center of the client's bottom side */
    CLIENT_GRAVITY_SOUTH_WEST,  /* 7: left bottom corner of client */
    CLIENT_GRAVITY_WEST,        /* 8: center of the client's left side */
    CLIENT_GRAVITY_CENTER,      /* 9: center of the client */
};


/**
 * @brief Window properties
 *
 * Encapsulates various properties of a client: state, layering
 * behavior, and any applicable flags.
 */
struct client_properties_s {
    uint16_t state;      /**< State (maximized, iconified,...) */
    uint16_t layer;      /**< Layer (above, normal, below) */
    uint16_t flags;      /**< Flags (hidden, sticky, focusable,...) */
    uint16_t type;       /**< Type (normal, notification...) */
    uint16_t operation;  /**< Operation (moving, resizing...) */
    uint16_t focusing;   /**< Focusing (focused, unfocused) */

    // TODO: Does this go on the client, or on the WM itself?
    uint16_t gravity;    /**< Window gravity */
};


/**
 * @brief Window layout, position, dimensions and strut
 */
struct client_layout_s {
    /* @biref This are the position and dimensions of the client
     *
     * @note The "old" one is to save the position when the "cur" one is
     *       needed to be recovered later; as in saving the current
     *       geometry before maximizing, and restoring it with the "old"
     *       position and dimensions.
     */
    struct {
        struct geometry_s cur;
        struct geometry_s old;
    } geometry;

    /**
     * @brief Area where the client exist on the screen, plus the area
     *        are marked off-bounds for client placement
     *
     * @note Traditional strut property will be @p strut_partial.sides
     *       when @p .start and @p .end are zero
     */
    struct {
        struct sides_s sides;      /* [left, right, top, bottom] */
        struct sides_s start;      /* [left_start_y, right_start_y,
                                       top_start_x, bottom_start_x] */
        struct sides_s end;        /* [left_end_y, right_end_y,
                                       top_end_x, bottom_end_x] */
    } strut_partial;

    struct sides_s frame_extents;  /* [left, right, top, bottom] */
};


/**
 * @brief Structure for a client in an XCB environment
 *
 * The @p screen_id and @p desktop_id fields link the client to its
 * respective screen and desktop, and @p parent is a pointer to its
 * parent client if needed.
 *
 * Additionally, the @p properties field contains various settings that
 * define the behavior and appearance of the client, while the @p theme
 * pointer allows for dynamic theming, enabling customization of the
 * client's visual aspects based on user preferences or system
 * themes.
 */
typedef struct client_s {
    xcb_connection_t *connection;   /**< XCB display / connection */
    xcb_ewmh_connection_t *ewmh;    /**< Pointer to EWMH connection */
    xcb_window_t window;            /**< The actual XCB client */
    xcb_window_t parent_id;         /**< Pointer to the parent client */
    xcb_window_t id;                /**< Unique client identifier */

    xcb_window_t frame;             /**< Optional decoration frame */
    xcb_window_t titlebar;          /**< Optional titlebar window */
    xcb_window_t icon_window;       /**< Optional iconified placeholder */
    bool is_icon_mapped;            /**< Whether icon window is mapped */
    uint8_t ignore_unmap;           /**< WM-initiated unmaps to suppress */
    int16_t icon_x;                 /**< Saved icon X (−1 = unset) */
    int16_t icon_y;                 /**< Saved icon Y (−1 = unset) */
    uint16_t title_height;          /**< Cached titlebar height */

    uint32_t desktop_id;            /**< Desktop index (0xFFFFFFFF for all) */
    uint32_t screen_id;             /**< Screen index */

    struct {
        char *name;                 /**< Window name */
        char *visible_name;         /**< Visible name on taskbar */
        char *role_name;            /**< Role (for compatibility) */
        char *class_name[2];        /**< Window class (for grouping) */
    } info;

    struct {
        char *icon_name;            /**< Icon image path */
        char *visible_icon_name;    /**< Icon on task bar */
        char **icons;
    } icon_info;

    struct config_theme_s *theme;               /**< User defined theme */
    const struct config_base_s *config_base;    /**< Base configuration */

    uint32_t user_time;             /**< Time since last used */

    struct {
        pid_t pid;                  /**< PID being executed */
        char *command;              /**< Command being executed */
    } process;                      /**< Process information */

    struct client_layout_s layout;
    struct client_properties_s properties;

    bool has_wm_delete_window;      /**< Supports WM_DELETE_WINDOW */
    xcb_window_t transient_for;     /**< Parent window for dialogs
                                         (0 or XCB_WINDOW_NONE if none) */
    /**
     * @brief ICCCM WM_NORMAL_HINTS size constraints
     */
    struct {
        bool valid;         /**< True when hints were read from server */
        int32_t min_w;      /**< Minimum width  (0 = unset) */
        int32_t min_h;      /**< Minimum height (0 = unset) */
        int32_t max_w;      /**< Maximum width  (0 = unset) */
        int32_t max_h;      /**< Maximum height (0 = unset) */
        int32_t base_w;     /**< Base width for increment arithmetic */
        int32_t base_h;     /**< Base height for increment arithmetic */
        int32_t inc_w;      /**< Width increment  (0 or 1 = no grid) */
        int32_t inc_h;      /**< Height increment (0 or 1 = no grid) */
    } size_hints;

} client_td;


/* Inline functions */
/* Save the current geometry of the client to the original geometry */
static inline void client_geometry_save(client_td *client)
{
    client->layout.geometry.old = client->layout.geometry.cur;
}


/* Restore the client's geometry from the saved original geometry */
static inline void client_geometry_restore(client_td *client)
{
    client->layout.geometry.cur = client->layout.geometry.old;
}


/* Set the focus on client, if focusable, but take no action */
static inline void client_focus(client_td *client)
{
    if (client->properties.flags & CLIENT_FLAG_FOCUSABLE) {
        client->properties.focusing = CLIENT_FOCUSING_FOCUSED;
    }
}


/* Remove the focus from the client, but take no action */
static inline void client_unfocus(client_td *client)
{
    client->properties.focusing = CLIENT_FOCUSING_UNFOCUSED;
}


/* Public interface */
/**
 * @brief Initialize a new client with the specified parameters
 *
 * Allocates and initializes a new client structure, attempting to
 * retrieve its properties from the X server (@c WM_NAME, @c WM_CLASS)
 * and creating an XCB window with the given dimensions and position.
 * The client is initialized in a hidden state.
 *
 * @param connection Pointer to the XCB connection
 * @param parent_id  Pointer to the parent client index
 * @param w          Width of the client in pixels
 * @param h          Height of the client in pixels
 * @param x          X-coordinate of the client position
 * @param y          Y-coordinate of the client position
 * @param theme      Pointer to the theme configuration
 * @param config_base Pointer to the base configuration
 *
 * @return A pointer to the newly created client structure, or @c NULL
 *         on failure
 *
 * @note Complexity: @e O(1) for creating a client structure, excluding
 *       X server interactions and property retrieval costs
 */
client_td *client_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        xcb_window_t parent_id,
        uint32_t w, uint32_t h, int32_t x, int32_t y,
        struct config_theme_s *theme,
        const struct config_base_s *config_base);

/**
 * @brief Destroy the specified client and free associated resources
 *
 * Deallocates all memory associated with the client, including the
 * XCB window, all string buffers, and the client structure itself.
 *
 * @param client Pointer to the client structure to be destroyed
 *
 * @note Complexity: @e O(1)
 */
void client_destroy(client_td *client);

/**
 * @brief Adopt an existing X window under window manager control
 *
 * Wraps an existing X window in a client structure without creating a
 * new window.  Reads the @c WM_NAME and @c WM_CLASS hints, queries the
 * current window geometry, and subscribes to property and structure
 * events on the window.
 *
 * @param connection  Pointer to the XCB connection
 * @param ewmh        Pointer to EWMH connection
 * @param window      ID of the existing X window to adopt
 * @param theme       Pointer to the theme configuration
 * @param config_base Pointer to the base configuration
 *
 * @return A pointer to the client structure wrapping the window, or
 *         @c NULL if the window should not be managed (e.g.,
 *         override-redirect) or on allocation failure
 *
 * @note The returned client is @e not mapped by this function; the
 *       caller is responsible for calling @c xcb_map_window when ready
 * @note Complexity: @e O(1)
 */
client_td *client_manage(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        struct config_theme_s *theme,
        const struct config_base_s *config_base);

/**
 * @brief Update the content of the specified client
 *
 * Performs a soft update on the client by refreshing its internal state
 * as needed.  This may include checking for property changes and
 * synchronizing the visual state with the internal representation.
 *
 * @param client Pointer to the client to be updated
 *
 * @note Complexity: @e O(1)
 */
void client_update(client_td *client);

/**
 * @brief Generic event sender for a client
 *
 * Creates and sends an event for a specified action on a client.  The
 * event is inserted into the event priority queue for processing by the
 * main event handler.
 *
 * @param client        Pointer to the target client
 * @param action_client Action to be performed on the client
 * @param priority      Priority level of the action
 *
 * @return 0 on success, -1 otherwise
 *
 * @note Complexity: @e O(log n), where @e n is the number of events
 *       in the priority queue
 */
int client_send_event(client_td *client,
        enum action_client_e action_client, enum priority_e priority);

/**
 * @brief Send an event to rename a specified client
 *
 * Creates an event to update the name of the client.  The new name is
 * passed as part of the event data for processing by the handler.
 *
 * @param client   Pointer to the client to be renamed
 * @param new_name New name for the client
 *
 * @return 0 on success, -1 otherwise
 *
 * @note Complexity: @e O(log n), where @e n is the number of events in
 *       the priority queue
 */
int client_send_event_rename(client_td *client, const char *new_name);

/**
 * @brief Send an event to change the class of a specified client
 *
 * Creates an event to update the class of the client.  The new class is
 * passed as part of the event data for processing by the handler.
 *
 * @param client    Pointer to the client to be reclassified
 * @param new_class New class for the client
 *
 * @return 0 on success, -1 otherwise
 *
 * @note Complexity: @e O(log n), where @e n is the number of events in
 *       the priority queue
 */
int client_send_event_reclass(client_td *client, const char *new_class);

/**
 * @brief Send event to move the specified client to given coordinates
 *
 * Creates an event to move the client to the specified @e (x, y)
 * position.  The new coordinates are passed as part of the event data.
 *
 * @param client Pointer to the client to be moved
 * @param new_x  New @e x coordinate for the client
 * @param new_y  New @e y coordinate for the client
 *
 * @return 0 on success, -1 otherwise
 *
 * @note Complexity: @e O(log n), where @e n is the number of events in
 *       the priority queue
 */
int client_send_event_move(client_td *client,
        int32_t new_x, int32_t new_y);

/**
 * @brief Send an event to resize the specified client
 *
 * Creates an event to resize the client to the specified dimensions.
 * The new width and height are passed as part of the event data.
 *
 * @param client Pointer to the client to be resized
 * @param new_w  New width for the client
 * @param new_h  New height for the client
 *
 * @return 0 on success, -1 otherwise
 *
 * @note Complexity: @e O(log n), where @e n is the number of events in
 *       the priority queue
 */
int client_send_event_resize(client_td *client,
        uint32_t new_w, uint32_t new_h);

/**
 * @brief Send an event to change the icon of a specified client
 *
 * Creates an event to update the icon of the client.  The icon name is
 * passed as part of the event data for processing by the handler.
 *
 * @param client    Pointer to the client for which the icon is to change
 * @param icon_name File path of the new icon
 *
 * @return 0 on success, -1 otherwise
 *
 * @note Complexity: @e O(log n), where @e n is the number of events in
 *       the priority queue
 */
int client_send_event_set_icon(client_td *client, const char *icon_name);

/**
 * @brief Macro that sends an event to close the specified client
 *
 * @see @a client_send_event
 */
#define client_send_event_close(w) \
    client_send_event(w, ACTION_CLIENT_CLOSE, CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that sends an event to restore the specified client
 *
 * @see @a client_send_event
 */
#define client_send_event_restore(w) \
    client_send_event(w, ACTION_CLIENT_RESTORE, CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that sends an event to give focus to the specified
 *        client
 *
 * @see @a client_send_event
 */
#define client_send_event_focus(w) \
    client_send_event(w, ACTION_CLIENT_FOCUS, CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that sends an event to set unfocused the specified
 *        client
 *
 * @see @a client_send_event
 */
#define client_send_event_unfocus(w) \
    client_send_event(w, ACTION_CLIENT_UNFOCUS, CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        iconify (and minimize) the specified client
 *
 * Iconifying will hide the client, but in reality it's minimized, plus
 * an icon representing the client is placed on the desktop.  If there's
 * interaction with the icon, the client will be restored and the icon
 * vanished.  When using a generic taskbar, the iconified process should
 * appear as minimized, and when restored, the icon should be gone as
 * well.
 *
 * @see @a client_send_event
 */
#define client_send_event_iconify(w) \
    client_send_event(w, ACTION_CLIENT_ICONIFY, CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        maximize the specified client
 *
 * @see @a client_send_event
 */
#define client_send_event_maximize(w) \
    client_send_event(w, ACTION_CLIENT_MAXIMIZE, \
            CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        horizontally maximize the specified client
 *
 * @see @a client_send_event
 */
#define client_send_event_maximize_horz(w) \
    client_send_event(w, ACTION_CLIENT_MAXIMIZE_HORZ, \
            CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        vertically maximize the specified client
 *
 * @see @a client_send_event
 */
#define client_send_event_maximize_vert(w) \
    client_send_event(w, ACTION_CLIENT_MAXIMIZE_VERT, \
            CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        make sticky the specified client
 *
 * @see @a client_send_event
 */
#define client_send_event_sticky(w) \
    client_send_event(w, ACTION_CLIENT_STICKY, CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        make not sticky the specified client
 *
 * @see @a client_send_event
 */
#define client_send_event_unsticky(w) \
    client_send_event(w, ACTION_CLIENT_UNSTICKY, \
            CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        toggle the sticky state the specified client
 *
 * @see @a client_send_event
 */
#define client_send_event_sticky_toggle(w) \
    client_send_event(w, ACTION_CLIENT_TOGGLE_STICKY, \
            CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        make full screen the specified client
 *
 * @see @a client_send_event
 */
#define client_send_event_fullscreen(w) \
    client_send_event(w, ACTION_CLIENT_FULLSCREEN, \
            CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        remove the full screen state from the specified client
 *
 * @see @a client_send_event
 */
#define client_send_event_fullscreen_toggle(w) \
    client_send_event(w, ACTION_CLIENT_TOGGLE_FULLSCREEN, \
            CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        center the specified client
 *
 * @see @a client_send_event
 */
#define client_send_event_center(w) \
    client_send_event(w, ACTION_CLIENT_CENTER, \
            CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        raise the specified client
 *
 * @see @a client_send_event
 */
#define client_send_event_raise(w) \
    client_send_event(w, ACTION_CLIENT_RAISE, CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        lower the specified client
 *
 * @see @a client_send_event
 */
#define client_send_event_lower(w) \
    client_send_event(w, ACTION_CLIENT_LOWER, CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        set specified client on the top desktop layer
 *
 * @see @a client_send_event
 */
#define client_send_event_layer_on_top(w) \
    client_send_event(w, ACTION_CLIENT_LAYER_ABOVE, \
            CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        set specified client on the normal desktop layer
 *
 * @see @a client_send_event
 */
#define client_send_event_layer_on_bottom(w) \
    client_send_event(w, ACTION_CLIENT_LAYER_BELOW, \
            CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        set specified client on the bottom desktop layer
 *
 * @see @a client_send_event
 */
#define client_send_event_layer_normal(w) \
    client_send_event(w, ACTION_CLIENT_LAYER_NORMAL, \
            CLIENT_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        set specified client on urgency level
 *
 * @see @a client_send_event
 */
#define client_send_event_set_urgent(w) \
    client_send_event(w, ACTION_CLIENT_SET_URGENT, PRIORITY_HIGHER)

/**
 * @brief Macro that evaluates to the client iconify state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_iconified(w) \
    ((w)->properties.state & CLIENT_STATE_ICONIFIED)

/**
 * @brief Macro that evaluates to the client maximization state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_maximized(w) \
    ((w)->properties.state & CLIENT_STATE_MAXIMIZED)

/**
 * @brief Macro that evaluates to the client horizontal maximization
 *        state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_maximized_horz(w) \
    ((w)->properties.state & CLIENT_STATE_MAXIMIZED_HORZ)

/**
 * @brief Macro that evaluates to the client vertical maximization
 *        state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_maximized_vert(w) \
    ((w)->properties.state & CLIENT_STATE_MAXIMIZED_VERT)

/**
 * @brief Macro that evaluates to the client full screen state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_fullscreen(w) \
    ((w)->properties.state & CLIENT_STATE_FULLSCREEN)

/**
 * @brief Macro that evaluates to the client visibility flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_visible(w) \
    ((w)->properties.flags & CLIENT_FLAG_HIDDEN)

/**
 * @brief Macro that evaluates to the client focused flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_focusable(w) \
    ((w)->properties.flags & CLIENT_FLAG_FOCUSABLE)

/**
 * @brief Macro that evaluates to the client shade flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_shaded(w) \
    ((w)->properties.flags & CLIENT_FLAG_SHADED)

/**
 * @brief Macro that evaluates to the client stickiness flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_sticky(w) \
    ((w)->properties.flags & CLIENT_FLAG_STICKY)

/**
 * @brief Macro that evaluates to the client decoration flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_decorated(w) \
    ((w)->properties.flags & CLIENT_FLAG_DECORATED)

/**
 * @brief Macro that evaluates to the client urgency flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_urgent(w) \
    ((w)->properties.flags & CLIENT_FLAG_URGENT)

/**
 * @brief Macro that evaluates to the client disabled flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_disabled(w) \
    ((w)->properties.flags & CLIENT_FLAG_DISABLED)

/**
 * @brief Macro that evaluates to the client resizable flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_resizable(w) \
    ((w)->properties.flags & CLIENT_FLAG_RESIZABLE)

/**
 * @brief Macro that sets the hidden flag of a client
 *
 * @param w Pointer to the client structure whose visibility is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_set_hidden(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_HIDDEN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the hidden flag of a client
 *
 * @param w Pointer to the client structure whose visibility is to be
 *          set
 *
 * @note Complexity: @e O(1)
 */
#define client_unset_hidden(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_HIDDEN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the hidden flag of a client
 *
 * @param w Pointer to the client structure whose visibility is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_hidden(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_HIDDEN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the focus flag of a client
 *
 * @param w Pointer to the client structure whose focus is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_set_focusable(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_FOCUSABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the focus flag of a client
 *
 * @param w Pointer to the client structure whose focus is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_unset_focusable(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_FOCUSABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the focus flag of a client
 *
 * @param w Pointer to the client structure whose focus is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_focusable(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_FOCUSABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the shade flag of a client
 *
 * @param w Pointer to the client structure whose shade is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_set_shade(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_SHADED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the shade flag of a client
 *
 * @param w Pointer to the client structure whose shade is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_unset_shade(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_SHADED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the shade flag of a client
 *
 * @param w Pointer to the client structure whose shade is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_shade(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_SHADED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the sticky flag of a client
 *
 * @param w Pointer to the client structure whose sticky is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_set_sticky(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_STICKY, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the sticky flag of a client
 *
 * @param w Pointer to the client structure whose sticky is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_unset_sticky(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_STICKY, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the sticky flag of a client
 *
 * @param w Pointer to the client structure whose sticky is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_sticky(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_STICKY, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the decoration flag of a client
 *
 * @param w Pointer to the client structure whose decoration is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_set_decoration(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_DECORATED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the decoration flag of a client
 *
 * @param w Pointer to the client structure whose decoration is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_unset_decoration(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_DECORATED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the decoration flag of a client
 *
 * @param w Pointer to the client structure whose decoration is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_decoration(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_DECORATED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the urgent flag of a client
 *
 * @param w Pointer to the client structure whose urgent is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_set_urgent(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_URGENT, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the urgent flag of a client
 *
 * @param w Pointer to the client structure whose urgent is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_unset_urgent(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_URGENT, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the urgent flag of a client
 *
 * @param w Pointer to the client structure whose urgent is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_urgent(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_URGENT, (1 << CLIENT_FLAG_MAX))


/**
 * @brief Macro that sets the unfocusable flag of a client
 *
 * @param w Pointer to the client structure whose disable is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_set_unfocusable(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_UNFOCUSABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the unfocusable flag of a client
 *
 * @param w Pointer to the client structure whose disable is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_unset_unfocusable(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_UNFOCUSABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the unfocusable flag of a client
 *
 * @param w Pointer to the client structure whose disable is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_unfocusable(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_UNFOCUSABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the resizable flag of a client
 *
 * @param w Pointer to the client structure whose resizable is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_set_resizable(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_RESIZABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the resizable flag of a client
 *
 * @param w Pointer to the client structure whose resizable is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_unset_resizable(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_RESIZABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the resizable flag of a client
 *
 * @param w Pointer to the client structure whose resizable is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_resizable(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_RESIZABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the disable flag of a client
 *
 * @param w Pointer to the client structure whose disable is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_set_disable(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_DISABLED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the disable flag of a client
 *
 * @param w Pointer to the client structure whose disable is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_unset_disable(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_DISABLED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the disable flag of a client
 *
 * @param w Pointer to the client structure whose disable is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_disable(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_DISABLED, (1 << CLIENT_FLAG_MAX))


#endif  /* ! CLIENT_H */
