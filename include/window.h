/**
 * @file window.h
 *
 * @brief Window definition and declaration

 * Defines the data structure that encapsulates all relevant information
 * about a specific window and its state within the windowing system
 * environment.  In this regards, provides a framework for managing the
 * visual and operational properties of the window, including its
 * identifier, name, state, graphical attributes, geometry, and behavior
 * preferences.
 */

#ifndef WINDOW_H
#define WINDOW_H


/* System includes */
#include <stdbool.h>
#include <sys/types.h>  /* pid_t */

/* X11 includes */
#include <X11/Xlib.h>   /* Display, Window */

/* Type includes */
#include <types/pair.h> /* geometry_s */

/* Util includes */
#include <utils/safeflg.h>

/* Project includes */
#include <action.h>
#include <config.h>
#include <event.h>
#include <priority.h>


#define WINDOW_PRIORITY_DEFAULT (PRIORITY_NORMAL)


/**
 * @brief Possible window states a window can be in
 */
enum window_state_e {
    WINDOW_STATE_NORMAL,            /**< Regular state */
    WINDOW_STATE_ICONIFIED,         /**< Iconified */
//    WINDOW_STATE_SHADED,            /**< Shaded (rolled-up), if decorated */
    WINDOW_STATE_MAXIMIZED,         /**< Maximized */
    WINDOW_STATE_MAXIMIZED_HORZ,    /**< Maximized horiz. */
    WINDOW_STATE_MAXIMIZED_VERT,    /**< Maximized vert. */
    WINDOW_STATE_FULLSCREEN,        /**< Full screen */
};


/**
 * @brief Possible window types that can be
 */
enum window_type_e {
    WINDOW_TYPE_NORMAL,         /* Normal window */
    WINDOW_TYPE_DIALOG,         /* Dialogues or interaction needed */
    WINDOW_TYPE_TOOLBAR,        /* Quick actions or tools */
    WINDOW_TYPE_NOTIFICATION,   /* Temporal messages */
    WINDOW_TYPE_MENU,           /* Menu options */
    WINDOW_TYPE_DESKTOP,        /* The desktop "window" */
    WINDOW_TYPE_SPLASH,         /* The window is a loading message */
    WINDOW_TYPE_UTILITY,        /* Additional functions: control panels... */
    WINDOW_TYPE_DROPDOWN_MENU,  /* Drop-down menu */
    WINDOW_TYPE_POPUP_MENU,     /* Contextual menu */
    WINDOW_TYPE_COMBO,          /* Part of a combined frame */
    WINDOW_TYPE_TOOLTIP,        /* The window is a tooltip */
    WINDOW_TYPE_DOCK,           /* Dock or panel feature */
    WINDOW_TYPE_DND,            /* The window is being dragged */
};


/**
 * @brief Operations on a window
 */
enum window_operation_e {
    WINDOW_OPERATION_IDLE,      /* No operation ongoing */
    WINDOW_OPERATION_MOVING,    /* Window is being moved */
    WINDOW_OPERATION_RESIZING,  /* Window is being resized */
};


/**
 * @brief Window characteristics using flags using bitwise flags
 */
enum window_flags_e {
    WINDOW_FLAG_HIDDEN       = 1 << 0,
    WINDOW_FLAG_FOCUSABLE    = 1 << 1,
    WINDOW_FLAG_STICKY       = 1 << 2,
    WINDOW_FLAG_SHADED       = 1 << 3,
    WINDOW_FLAG_DECORATED    = 1 << 4,
    WINDOW_FLAG_URGENT       = 1 << 5,
    WINDOW_FLAG_RESIZABLE    = 1 << 6,
    WINDOW_FLAG_DISABLED     = 1 << 7,
    WINDOW_FLAG_SKIP_TASKBAR = 1 << 8,
    WINDOW_FLAG_SKIP_PAGER   = 1 << 9,
    WINDOW_FLAG_MAX = 10,
};


/**
 * @brief Mutual exclusive status about the window focus
 */
enum window_focusing_e {
    WINDOW_FOCUSING_UNFOCUSED,  /* No focus state */
    WINDOW_FOCUSING_FOCUSED,    /* Window has focus */
};


/**
 * @brief Window mutual exclusive possible layers
 *
 * Identifies the layering options for windows, which affect their
 * visibility order on the screen.
 */
enum window_layer_e {
    WINDOW_LAYER_ABOVE,     /**< Always on top, in front */
    WINDOW_LAYER_NORMAL,    /**< Normal behavior */
    WINDOW_LAYER_BELOW,     /**< Always behind every window */
};


/**
 * @brief Window properties
 *
 * Encapsulates various properties of a window: state, layering
 * behavior, and any applicable flags.
 */
struct window_properties_s {
    unsigned int state;     /**< State (maximized, iconified,...) */
    unsigned int layer;     /**< Layer (above, normal, below) */
    unsigned int flags;     /**< Flags (hidden, sticky, focusable,...) */
    unsigned int type;      /**< Type (normal, notification...) */
    unsigned int operation; /**< Operation (moving, resizing...) */
    unsigned int focusing;  /**< Focusing (focused, unfocused) */

    /* This are the position and dimensions of the window.  The "old"
     * one is to save the position when the "cur" one is needed to be
     * recovered later; as in saving the current geometry before
     * maximizing, and restoring it with the "old" position and
     * dimensions. */
    struct geometry_s geometry_cur;
    struct geometry_s geometry_old;
};


/**
 * @brief Structure for a window in an X11 environment
 *
 * The @p screen_id and @p desktop_id fields link the window to its
 * respective screen and desktop, and @p parent is a pointer to its
 * parent window if needed.
 *
 * Additionally, the @p properties field contains various settings that
 * define the behavior and appearance of the window, while the @p theme
 * pointer allows for dynamic theming, enabling customization of the
 * window's visual aspects based on user preferences or system
 * themes.
 */
typedef struct window_s {
    Display *display;           /**< X11 display */
    Window xwindow;             /**< The actual X11 window */
    struct window_s *parent;    /**< Pointer to the parent window */

    XID screen_id;     /**< Screen index */
    XID desktop_id;    /**< Desktop index */
    XID id;           /**< Unique window identifier */

    char *name;                 /**< Window name */
    char *class_name;           /**< Window class */

    struct {
        const char *command;    /**< Command to execute in this window */
        pid_t pid;              /**< PID of the running program */
    } process;                  /**< Information of process in window */

    char *icon_name;            /**< Icon image path */
    struct config_theme_s *theme;

    struct window_properties_s properties;
} window_td;


/* Inline functions */
/* Saves the current geometry of the window to the original geometry */
static inline void window_geometry_save(window_td *window)
{
    window->properties.geometry_old = window->properties.geometry_cur;
}


/* Restores the window's geometry from the saved original geometry */
static inline void window_geometry_restore(window_td *window)
{
    window->properties.geometry_cur = window->properties.geometry_old;
}

/* Set the focus on window, if focusable, but take no action */
static inline void window_focus(window_td *window)
{
    if (window->properties.flags & WINDOW_FLAG_FOCUSABLE) {
        window->properties.focusing = WINDOW_FOCUSING_FOCUSED;
    }
}

/* Remove the focus from the window, but take no action */
static inline void window_unfocus(window_td *window)
{
    window->properties.focusing = WINDOW_FOCUSING_UNFOCUSED;
}


/* Public interface */
/**
 * @brief Initialize a new window with the specified parameters
 *
 * @param display Pointer to the X11 display
 * @param parent  Pointer to the parent window index
 * @param w       Width of the window in pixels
 * @param h       Height of the window in pixels
 * @param x       X-coordinate of the window position
 * @param y       Y-coordinate of the window position
 * @param theme   Pointer to the theme configuration
 *
 * @return A pointer to the newly created window structure
 *
 * @note Complexity: @e O(1) for creating a window structure
 */
window_td *window_init(Display *display, window_td *parent,
        unsigned int w, unsigned int h, int x, int y,
        struct config_theme_s *theme);

/**
 * @brief Destroy the specified window and free associated resources
 *
 * @param window Pointer to the window structure to be destroyed
 *
 * @note Complexity: @e O(1)
 */
void window_destroy(window_td *window);

/**
 * @brief Update the content of the specified window
 *
 * @param window Pointer to the window to be updated
 *
 * @note Complexity: @e O(1)
 */
void window_update(window_td *window);

/**
 * @brief Generic event sender for a window
 *
 * Sends event for a specified action on a window, in particular, those
 * events related to actions that do not require extra data.
 *
 * @param window        Pointer to the target window
 * @param action_window The action to be performed on the window
 * @param priority      The priority level of the action
 *
 * @return Returns the result of adding the event to the queue
 */
int window_send_event(window_td *window, enum action_window_e action_window,
        enum priority_e priority);

/**
 * @brief Send an event to rename a specified window
 *
 * @param window   Pointer to the window to be renamed
 * @param new_name The new name for the window
 *
 * @return Returns the result of adding the event to the queue @p eventq
 *
 * @note Memory for the action data structure @p data must be freed with
 *       @a action_data_window_destroy once the event has finished
 *       processing this action to avoid memory leaks
 * @note Complexity: @e O(1)
 */
int window_send_event_rename(window_td *window, const char *new_name);

/**
 * @brief Send an event to change the class of a specified window
 *
 * @param window    Pointer to the window to be reclassified
 * @param new_class The new class for the window
 *
 * @return Returns the result of adding the event to the queue @p eventq
 *
 * @note Memory for the action data structure @p data must be freed with
 *       @a action_data_window_destroy once the event has finished
 *       processing this action to avoid memory leaks
 * @note Complexity: @e O(1)
 */
int window_send_event_reclass(window_td *window, const char *new_class);

/**
 * @brief Send event to move the specified window to the given  @e (x, y)
 *        coordinates
 *
 * @param window Pointer to the window to be reclassified
 * @param new_x  The new @e x coordinate for the window
 * @param new_y  The new @e y coordinate for the window
 *
 * @return Returns the result of adding the event to the queue @p eventq
 *
 * @note Memory for the action data structure @p data must be freed with
 *       @a action_data_window_destroy once the event has finished
 *       processing this action to avoid memory leaks
 * @note Complexity: @e O(1)
 */
int window_send_event_move(window_td *window, int new_x, int new_y);

/**
 * @brief Send an event to resize the specified window to the given
 *        width and height
 *
 * @param window Pointer to the window to be reclassified
 * @param new_w  The new width for the window
 * @param new_h  The new height for the window
 *
 * @return Returns the result of adding the event to the queue @p eventq
 *
 * @note Memory for the action data structure @p data must be freed with
 *       @a action_data_window_destroy once the event has finished
 *       processing this action to avoid memory leaks
 * @note Complexity: @e O(1)
 */
int window_send_event_resize(window_td *window,
        unsigned int new_w, unsigned int new_h);

/**
 * @brief Send an event to change the icon of a specified window
 *
 * @param window    Pointer to the window for which the icon is to change
 * @param icon_name The file path of the new icon
 *
 * @return Returns the result of adding the event to the queue @p eventq
 *
 * @note Memory for the action data structure @p data must be freed with
 *       @a action_data_window_destroy once the event has finished
 *       processing this action to avoid memory leaks
 * @note Complexity @e O(1)
 */
int window_send_event_set_icon(window_td *window, const char *icon_name);

/**
 * @brief Macro that sends an event to close the specified window
 *
 * @see @a window_send_event
 */
#define window_send_event_close(w) \
    window_send_event(w, ACTION_WINDOW_CLOSE, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that sends an event to restore the specified window
 *
 * @see @a window_send_event
 */
#define window_send_event_restore(w) \
    window_send_event(w, ACTION_WINDOW_RESTORE, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that sends an event to give focus to the specified
 *        window
 *
 * @see @a window_send_event
 */
#define window_send_event_focus(w) \
    window_send_event(w, ACTION_WINDOW_FOCUS, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that sends an event to set unfocused the specified
 *        window
 *
 * @see @a window_send_event
 */
#define window_send_event_unfocus(w) \
    window_send_event(w, ACTION_WINDOW_FOCUS, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        iconify (and minimize) the specified window
 *
 * Iconifying will hide the window, but in reality it's minimized, plus
 * an icon representing the window is placed on the desktop.  If there's
 * interaction with the icon, the window will be restored and the icon
 * vanished.  When using a generic taskbar, the iconified process should
 * appear as minimized, and when restored, the icon should be gone as
 * well.
 *
 * @see @a window_send_event
 */
#define window_send_event_iconify(w) \
    window_send_event(w, ACTION_WINDOW_ICONIFY, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        maximize the specified window
 *
 * @see @a window_send_event
 */
#define window_send_event_maximize(w) \
    window_send_event(w, ACTION_WINDOW_MAXIMIZE, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        horizontally maximize the specified window
 *
 * @see @a window_send_event
 */
#define window_send_event_maximize_horz(w) \
    window_send_event(w, ACTION_WINDOW_MAXIMIZE_HORZ, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        vertically maximize the specified window
 *
 * @see @a window_send_event
 */
#define window_send_event_maximize_vert(w) \
    window_send_event(w, ACTION_WINDOW_MAXIMIZE_VERT, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        make sticky the specified window
 *
 * @see @a window_send_event
 */
#define window_send_event_sticky(w) \
    window_send_event(w, ACTION_WINDOW_STICKY, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        make not sticky the specified window
 *
 * @see @a window_send_event
 */
#define window_send_event_unsticky(w) \
    window_send_event(w, ACTION_WINDOW_UNSTICKY, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        toggle the sticky state the specified window
 *
 * @see @a window_send_event
 */
#define window_send_event_sticky_toggle(w) \
    window_send_event(w, ACTION_WINDOW_STICKY_TOGGLE, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        make full screen the specified window
 *
 * @see @a window_send_event
 */
#define window_send_event_fullscreen(w) \
    window_send_event(w, ACTION_WINDOW_FULLSCREEN, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        remove the full screen state from the specified window
 *
 * @see @a window_send_event
 */
#define window_send_event_fullscreen_toggle(w) \
    window_send_event(w, ACTION_WINDOW_FULLSCREEN_TOGGLE, \
            WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        raise the specified window
 *
 * @see @a window_send_event
 */
#define window_send_event_raise(w) \
    window_send_event(w, ACTION_WINDOW_RAISE, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        lower the specified window
 *
 * @see @a window_send_event
 */
#define window_send_event_lower(w) \
    window_send_event(w, ACTION_WINDOW_LOWER, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        set specified window on the top desktop layer
 *
 * @see @a window_send_event
 */
#define window_send_event_layer_on_top(w) \
    window_send_event(w, ACTION_WINDOW_LAYER_ABOVE, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        set specified window on the normal desktop layer
 *
 * @see @a window_send_event
 */
#define window_send_event_layer_on_bottom(w) \
    window_send_event(w, ACTION_WINDOW_LAYER_BELOW, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        set specified window on the bottom desktop layer
 *
 * @see @a window_send_event
 */
#define window_send_event_layer_normal(w) \
    window_send_event(w, ACTION_WINDOW_LAYER_NORMAL, WINDOW_PRIORITY_DEFAULT)

/**
 * @brief Macro that performs the action that initializes an event to
 *        set specified window on urgency level
 *
 * @see @a window_send_event
 */
#define window_send_event_set_urgent(w) \
    window_send_event(w, ACTION_WINDOW_SET_URGENT, PRIORITY_HIGHER)

/**
 * @brief Macro that evaluates to the window iconify state
 *
 * @note Complexity: @e O(1)
 */
#define window_is_iconified(w) \
    ((w)->properties.state & WINDOW_STATE_ICONIFIED)

/**
 * @brief Macro that evaluates to the window maximization state
 *
 * @note Complexity: @e O(1)
 */
#define window_is_maximized(w) \
    ((w)->properties.state & WINDOW_STATE_MAXIMIZED)

/**
 * @brief Macro that evaluates to the window horizontal maximization
 *        state
 *
 * @note Complexity: @e O(1)
 */
#define window_is_maximized_horz(w) \
    ((w)->properties.state & WINDOW_STATE_MAXIMIZED_HORZ)

/**
 * @brief Macro that evaluates to the window vertical maximization
 *        state
 *
 * @note Complexity: @e O(1)
 */
#define window_is_maximized_vert(w) \
    ((w)->properties.state & WINDOW_STATE_MAXIMIZED_VERT)

/**
 * @brief Macro that evaluates to the window full screen state
 *
 * @note Complexity: @e O(1)
 */
#define window_is_fullscreen(w) \
    ((w)->properties.state & WINDOW_STATE_FULLSCREEN)

/**
 * @brief Macro that evaluates to the window visibility flag
 *
 * @note Complexity: @e O(1)
 */
#define window_is_visible(w) \
    ((w)->properties.flags & WINDOW_FLAG_HIDDEN)

/**
 * @brief Macro that evaluates to the window focused flag
 *
 * @note Complexity: @e O(1)
 */
#define window_is_focusable(w) \
    ((w)->properties.flags & WINDOW_FLAG_FOCUSABLE)


/**
 * @brief Macro that evaluates to the window stickiness flag
 *
 * @note Complexity: @e O(1)
 */
#define window_is_sticky(w) \
    ((w)->properties.flags & WINDOW_FLAG_STICKY)

/**
 * @brief Macro that evaluates to the window decoration flag
 *
 * @note Complexity: @e O(1)
 */
#define window_is_decorated(w) \
    ((w)->properties.flags & WINDOW_FLAG_DECORATED)

/**
 * @brief Macro that evaluates to the window urgency flag
 *
 * @note Complexity: @e O(1)
 */
#define window_is_urgent(w) \
    ((w)->properties.flags & WINDOW_FLAG_URGENT)

/**
 * @brief Macro that evaluates to the window disabled flag
 *
 * @note Complexity: @e O(1)
 */
#define window_is_disabled(w) \
    ((w)->properties.flags & WINDOW_FLAG_DISABLED)

/**
 * @brief Macro that sets the hidden flag of a window
 *
 * @param w Pointer to the window structure whose visibility is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define window_set_hidden(w) \
    safeflg_set(&((w)->properties.flags), \
            WINDOW_FLAG_HIDDEN, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that clears the hidden flag of a window
 *
 * @param w Pointer to the window structure whose visibility is to be
 *          set
 *
 * @note Complexity: @e O(1)
 */
#define window_unset_hidden(w) \
    safeflg_unset(&((w)->properties.flags), \
            WINDOW_FLAG_HIDDEN, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that toggles the hidden flag of a window
 *
 * @param w Pointer to the window structure whose visibility is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define window_toggle_hidden(w) \
    safeflg_toggle(&((w)->properties.flags), \
            WINDOW_FLAG_HIDDEN, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that sets the focus flag of a window
 *
 * @param w Pointer to the window structure whose focus is to be set
 *
 * @note Complexity: @e O(1)
 */
#define window_set_focusable(w) \
    safeflg_set(&((w)->properties.flags), \
            WINDOW_FLAG_FOCUSABLE, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that clears the focus flag of a window
 *
 * @param w Pointer to the window structure whose focus is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define window_unset_focusable(w) \
    safeflg_unset(&((w)->properties.flags), \
            WINDOW_FLAG_FOCUSABLE, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that toggles the focus flag of a window
 *
 * @param w Pointer to the window structure whose focus is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define window_toggle_focusable(w) \
    safeflg_toggle(&((w)->properties.flags), \
            WINDOW_FLAG_FOCUSABLE, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that sets the sticky flag of a window
 *
 * @param w Pointer to the window structure whose sticky is to be set
 *
 * @note Complexity: @e O(1)
 */
#define window_set_sticky(w) \
    safeflg_set(&((w)->properties.flags), \
            WINDOW_FLAG_STICKY, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that clears the sticky flag of a window
 *
 * @param w Pointer to the window structure whose sticky is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define window_unset_sticky(w) \
    safeflg_unset(&((w)->properties.flags), \
            WINDOW_FLAG_STICKY, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that toggles the sticky flag of a window
 *
 * @param w Pointer to the window structure whose sticky is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define window_toggle_sticky(w) \
    safeflg_toggle(&((w)->properties.flags), \
            WINDOW_FLAG_STICKY, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that sets the decoration flag of a window
 *
 * @param w Pointer to the window structure whose decoration is to be set
 *
 * @note Complexity: @e O(1)
 */
#define window_set_decoration(w) \
    safeflg_set(&((w)->properties.flags), \
            WINDOW_FLAG_DECORATED, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that clears the decoration flag of a window
 *
 * @param w Pointer to the window structure whose decoration is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define window_unset_decoration(w) \
    safeflg_unset(&((w)->properties.flags), \
            WINDOW_FLAG_DECORATED, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that toggles the decoration flag of a window
 *
 * @param w Pointer to the window structure whose decoration is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define window_toggle_decoration(w) \
    safeflg_toggle(&((w)->properties.flags), \
            WINDOW_FLAG_DECORATED, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that sets the urgent flag of a window
 *
 * @param w Pointer to the window structure whose urgent is to be set
 *
 * @note Complexity: @e O(1)
 */
#define window_set_urgent(w) \
    safeflg_set(&((w)->properties.flags), \
            WINDOW_FLAG_URGENT, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that clears the urgent flag of a window
 *
 * @param w Pointer to the window structure whose urgent is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define window_unset_urgent(w) \
    safeflg_unset(&((w)->properties.flags), \
            WINDOW_FLAG_URGENT, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that toggles the urgent flag of a window
 *
 * @param w Pointer to the window structure whose urgent is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define window_toggle_urgent(w) \
    safeflg_toggle(&((w)->properties.flags), \
            WINDOW_FLAG_URGENT, (1 << WINDOW_FLAG_MAX))


/**
 * @brief Macro that sets the unfocusable flag of a window
 *
 * @param w Pointer to the window structure whose disable is to be set
 *
 * @note Complexity: @e O(1)
 */
#define window_set_unfocusable(w) \
    safeflg_set(&((w)->properties.flags), \
            WINDOW_FLAG_UNFOCUSABLE, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that clears the unfocusable flag of a window
 *
 * @param w Pointer to the window structure whose disable is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define window_unset_unfocusable(w) \
    safeflg_unset(&((w)->properties.flags), \
            WINDOW_FLAG_UNFOCUSABLE, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that toggles the unfocusable flag of a window
 *
 * @param w Pointer to the window structure whose disable is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define window_toggle_unfocusable(w) \
    safeflg_toggle(&((w)->properties.flags), \
            WINDOW_FLAG_UNFOCUSABLE, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that sets the resizable flag of a window
 *
 * @param w Pointer to the window structure whose resizable is to be set
 *
 * @note Complexity: @e O(1)
 */
#define window_set_resizable(w) \
    safeflg_set(&((w)->properties.flags), \
            WINDOW_FLAG_RESIZABLE, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that clears the resizable flag of a window
 *
 * @param w Pointer to the window structure whose resizable is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define window_unset_resizable(w) \
    safeflg_unset(&((w)->properties.flags), \
            WINDOW_FLAG_RESIZABLE, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that toggles the resizable flag of a window
 *
 * @param w Pointer to the window structure whose resizable is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define window_toggle_resizable(w) \
    safeflg_toggle(&((w)->properties.flags), \
            WINDOW_FLAG_RESIZABLE, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that sets the disable flag of a window
 *
 * @param w Pointer to the window structure whose disable is to be set
 *
 * @note Complexity: @e O(1)
 */
#define window_set_disable(w) \
    safeflg_set(&((w)->properties.flags), \
            WINDOW_FLAG_DISABLED, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that clears the disable flag of a window
 *
 * @param w Pointer to the window structure whose disable is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define window_unset_disable(w) \
    safeflg_unset(&((w)->properties.flags), \
            WINDOW_FLAG_DISABLED, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that toggles the disable flag of a window
 *
 * @param w Pointer to the window structure whose disable is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define window_toggle_disable(w) \
    safeflg_toggle(&((w)->properties.flags), \
            WINDOW_FLAG_DISABLED, (1 << WINDOW_FLAG_MAX))

/**
 * @brief Macro that evaluates to the window 'x' position
 *
 * @note Complexity: @e O(1)
 */
#define window_x(w) ((w)->properties.geometry.pos.x)

/**
 * @brief Macro that evaluates to the window 'y' position
 *
 * @note Complexity: @e O(1)
 */
#define window_y(w) ((w)->properties.geometry.pos.y)

/**
 * @brief Macro that evaluates to the window width
 *
 * @note Complexity: @e O(1)
 */
#define window_width(w) ((w)->properties.geometry.dim.width)

/**
 * @brief Macro that evaluates to the window height
 *
 * @note Complexity: @e O(1)
 */
#define window_height(w) ((w)->properties.geometry.dim.height)


#endif  /* ! WINDOW_H */
