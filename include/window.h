/**
 * @file window.h
 *
 * @brief Window definition and declaration
 */

#ifndef WINDOW_H
#define WINDOW_H


/* System includes */
#include <stdbool.h>    /* bool */
#include <sys/types.h>  /* pid_t */

/* X11 includes */
#include <X11/Xlib.h>   /* Display, Window */

/* Type includes */
#include <types/pair.h> /* dimensions_s, geometry_s */

/* Project includes */
#include <action.h>
#include <config.h>
#include <event.h>
#include <priority.h>


/**
 * @brief Possible window states a window can be in
 */
enum window_state_e {
    WINDOW_STATE_IDLE,          /**< Regular state */
    WINDOW_STATE_ICONIFIED,     /**< Iconified window */
    WINDOW_STATE_MAXIMIZED,     /**< Maximized */
    WINDOW_STATE_MAXIMIZED_HORZ,/**< Maximized horizontally */
    WINDOW_STATE_MAXIMIZED_VERT,/**< Maximized vertically */
    WINDOW_STATE_FULLSCREEN,    /**< Full screen */
};


/**
 * @brief Window characteristics using flags using bitwise flags
 */
enum window_flags_e {
    WINDOW_FLAG_VISIBLE   = 1 << 0, /* 0000 0001: window is visible */
    WINDOW_FLAG_FOCUSED   = 1 << 1, /* 0000 0010: window has focus */
    WINDOW_FLAG_STICKY    = 1 << 2, /* 0000 0100: window in all desktops */
    WINDOW_FLAG_DECORATED = 1 << 3, /* 0000 1000: window has decoration */
    WINDOW_FLAG_DISABLED  = 1 << 4, /* 0001 0000: window is disabled */
};


/**
 * @brief Window possible layers
 *
 * Identifies the layering options for windows, which affect their
 * visibility order on the screen.
 */
enum window_layer_e {
    WINDOW_LAYER_ON_TOP,       /**< Always on top */
    WINDOW_LAYER_NORMAL,       /**< Normal behavior */
    WINDOW_LAYER_ON_BOTTOM,    /**< Always behind every window */
};


/**
 * @brief Window properties
 *
 * Encapsulates various properties of a window: state, layering
 * behavior, and any applicable flags.
 */
struct window_properties_s {
    enum window_state_e state;      /**< State (maximized, iconified,...) */
    enum window_layer_e layer;      /**< Layer (top, normal, bottom) */
    enum window_flags_e flags;      /**< Flags (sticky, focused,...) */

    struct {
        struct geometry_s pos;      /**< Window position (px) */
        struct dimensions_s dim;    /**< Window dimensions (px) */
    } geometry;
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
    Window window;              /**< The actual X11 window */
    struct window_s *parent;    /**< Pointer to the parent window */

    unsigned int screen_id;     /**< Screen index */
    unsigned int desktop_id;    /**< Desktop index */
    unsigned long int id;       /**< Unique window identifier */

    char *name;                 /**< Window name */
    char *class;                /**< Window class */

    struct window_process_s {
        const char *command;    /**< Command to execute in this window */
        pid_t pid;              /**< PID of the running program */
    } process;                  /**< Information of process in window */

    char *icon_path;            /**< Icon image path */
    struct config_theme_s *theme;

    struct window_properties_s properties;
} window_td;


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
 * @brief Action that initializes an event to rename a specified window
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
int window_action_rename(window_td *window, const char *new_name);

/**
 * @brief Action that initializes an event to change the class of
 *        a specified window
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
int window_action_reclass(window_td *window, const char *new_class);

/**
 * @brief Action that initializes an event to move the specified window
 *        to the given @e (x, y) coordinates
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
int window_action_move(window_td *window, int new_x, int new_y);

/**
 * @brief Action that initializes an event to resize the specified
 *        window to the given width and height
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
int window_action_resize(window_td *window,
        unsigned int new_w, unsigned int new_h);

/**
 * @brief Action that initializes an event to change the icon of
 *        a specified window
 *
 * @param window    Pointer to the window for which the icon is to change
 * @param icon_path The file path of the new icon
 *
 * @return Returns the result of adding the event to the queue @p eventq
 *
 * @note Memory for the action data structure @p data must be freed with
 *       @a action_data_window_destroy once the event has finished
 *       processing this action to avoid memory leaks
 * @note Complexity @e O(1)
 */
int window_action_set_icon(window_td *window, const char *icon_path);

/**
 * @brief Generic action for a window
 *
 * Initializes a generic event for a specified action on a window, in
 * particular, those events related to actions that do not require extra
 * data.
 *
 * @param window        Pointer to the target window
 * @param action_window The action to be performed on the window
 * @param priority      The priority level of the action
 *
 * @return Returns the result of adding the event to the queue
 */
int window_action(window_td *window, enum action_window_e action_window,
        enum priority_e priority);

#define window_action_close(w) \
    window_action(w, ACTION_WINDOW_CLOSE, EVENT_PRIORITY_NORMAL)

#define window_action_restore(w) \
    window_action(w, ACTION_WINDOW_RESTORE, EVENT_PRIORITY_NORMAL)

#define window_action_focus(w) \
    window_action(w, ACTION_WINDOW_FOCUS, EVENT_PRIORITY_NORMAL)

#define window_action_unfocus(w) \
    window_action(w, ACTION_WINDOW_FOCUS, EVENT_PRIORITY_NORMAL)

#define window_action_iconify(w) \
    window_action(w, ACTION_WINDOW_ICONIFY, EVENT_PRIORITY_NORMAL)

#define window_action_iconify(w) \
    window_action(w, ACTION_WINDOW_ICONIFY, EVENT_PRIORITY_NORMAL)

#define window_action_maximize(w) \
    window_action(w, ACTION_WINDOW_MAXIMIZE, EVENT_PRIORITY_NORMAL)

#define window_action_sticky(w) \
    window_action(w, ACTION_WINDOW_STICKY, EVENT_PRIORITY_NORMAL)

#define window_action_unsticky(w) \
    window_action(w, ACTION_WINDOW_UNSTICKY, EVENT_PRIORITY_NORMAL)

#define window_action_sticky_toggle(w) \
    window_action(w, ACTION_WINDOW_STICKY_TOGGLE, EVENT_PRIORITY_NORMAL)

#define window_action_fullscreen(w) \
    window_action(w, ACTION_WINDOW_FULLSCREEN, EVENT_PRIORITY_NORMAL)

#define window_action_fullscreen_toggle(w) \
    window_action(w, ACTION_WINDOW_FULLSCREEN_TOGGLE, \
            EVENT_PRIORITY_NORMAL)

#define window_action_raise(w) \
    window_action(w, ACTION_WINDOW_RAISE, EVENT_PRIORITY_NORMAL)

#define window_action_lower(w) \
    window_action(w, ACTION_WINDOW_LOWER, EVENT_PRIORITY_NORMAL)

#define window_action_layer_on_top(w) \
    window_action(w, ACTION_WINDOW_LAYER_ON_TOP, EVENT_PRIORITY_NORMAL)

#define window_action_layer_on_bottom(w) \
    window_action(w, ACTION_WINDOW_LAYER_ON_BOTTOM, EVENT_PRIORITY_NORMAL)

#define window_action_layer_normal(w) \
    window_action(w, ACTION_WINDOW_LAYER_NORMAL, EVENT_PRIORITY_NORMAL)

#define window_action_maximize_horz(w) \
    window_action(w, ACTION_WINDOW_MAXIMIZE_HORZ, EVENT_PRIORITY_NORMAL)

#define window_action_maximize_vert(w) \
    window_action(w, ACTION_WINDOW_MAXIMIZE_VERT, EVENT_PRIORITY_NORMAL)

#define window_action_set_urgent(w) \
    window_action(w, ACTION_WINDOW_SET_URGENT, EVENT_PRIORITY_HIGH)

/**
 * @brief Macro that evaluates to toggling the visibility flag
 *
 * @note Complexity: @e O(1)
 *
 * @see @c window_flags_e
 */
#define window_toggle_visibility(w) \
    ((w)->properties.flags ^= (enum window_flags_e) WINDOW_FLAG_VISIBLE)

/**
 * @brief Macro that evaluates to toggling the focus flag
 *
 * @note Complexity: @e O(1)
 *
 * @see @c window_flags_e
 */
#define window_toggle_focus(w) \
    ((w)->properties.flags ^= (enum window_flags_e) WINDOW_FLAG_FOCUSED)

/**
 * @brief Macro that evaluates to toggling the stickiness flag
 *
 * @note Complexity: @e O(1)
 *
 * @see @c window_flags_e
 */
#define window_toggle_stickiness(w) \
    ((w)->properties.flags ^= (enum window_flags_e) WINDOW_FLAG_STICKY)

/**
 * @brief Macro that evaluates to toggling the decoration flag
 *
 * @note Complexity: @e O(1)
 *
 * @see @c window_flags_e
 */
#define window_toggle_decoration(w) \
    ((w)->properties.flags ^= (enum window_flags_e) WINDOW_FLAG_DECORATED)

/**
 * @brief Macro that evaluates to toggling the availability flag
 *
 * @note Complexity: @e O(1)
 *
 * @see @c window_flags_e
 */
#define window_toggle_availability(w) \
    ((w)->properties.flags ^= (enum window_flags_e) WINDOW_FLAG_DISABLED)


#endif  /* ! WINDOW_H */
