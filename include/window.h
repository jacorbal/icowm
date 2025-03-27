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

/* External libraries */
#include <X11/Xlib.h>   /* Display, Window, Pixmap */

/* Common type structures */
#include <types/pair.h> /* dimensions_s, geometry_s */

/* Project includes */
#include <config.h>


/**
 * @brief Possible window states a window can be in
 */
enum window_state_e {
    WINDOW_STATE_IDLE,         /**< Regular state */
    WINDOW_STATE_ICONIFIED,    /**< Iconified window */
    WINDOW_STATE_MAXIMIZED,    /**< Maximized */
    WINDOW_STATE_FULLSCREEN,   /**< Full screen */
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
    enum window_state_e state;  /**< State (maximized, iconified,...) */
    enum window_layer_e layer;  /**< Layer (on top, normal, on bottom) */
    enum window_flags_e flags;  /**< Flags (sticky, focused,...) */

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

    Pixmap icon;                /**< Icon image */
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
 * @brief Close the window
 *
 * Closes the specified window, but does not deallocate any used
 * resources.
 *
 * @param window Pointer to the window to be closed
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_close(window_td *window);

/**
 * @brief Change properties of the specified window
 *
 * Updates various properties (state, layer, flags) of the specified
 * window.
 *
 * @param window     Pointer to window whose properties will be changed
 * @param properties New properties for the window
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 *
 * @see @c window_properties_s
 */
int window_action_set_property(window_td *window,
        const struct window_properties_s *properties);

/**
 * @brief Restore the window to its normal state
 *
 * Restores a minimized or maximized window to its normal size.
 *
 * @param window Pointer to the window to be restored
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_restore(window_td *window);

/**
 * @brief Focus on the specified window
 *
 * Brings the specified window into focus, making it the active window.
 *
 * @param window Pointer to the window to be focused
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_focus(window_td *window);

/**
 * @brief Unfocus the specified window
 *
 * @param window Pointer to the window to be unfocused
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_unfocus(window_td *window);

/**
 * @brief Resize the specified window
 *
 * @param window Pointer to the window to be resized
 * @param dim    Dimensions structure with new values
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 *
 * @see @c dimensions_s
 */
int window_action_resize(window_td *window, struct dimensions_s dim);

/**
 * @brief Move the specified window to a new position
 *
 * @param window   Pointer to the window to be moved
 * @param geometry Geometry structure with new values
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 *
 * @see @c geometry_s
 */
int window_action_move(window_td *window, struct geometry_s geometry);

/**
 * @brief Rename the specified window
 *
 * @param window Pointer to the window to be renamed
 * @param new_name New name for the window
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 */
int window_action_rename(window_td *window, const char *new_name);

/**
 * @brief Maximize the window horizontally
 *
 * Maximizes the width of the specified window while keeping its height
 * unchanged.
 *
 * @param window Pointer to the window to be maximized horizontally
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_maximize_horizontally(window_td *window);

/**
 * @brief Maximize the window vertically
 *
 * Maximizes the height of the specified window while keeping its width
 * unchanged.
 *
 * @param window Pointer to the window to be maximized vertically
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_maximize_vertically(window_td *window);

/**
 * @brief Maximize the specified window
 *
 * @param window Pointer to the window to be maximized
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_maximize(window_td *window);

/**
 * @brief Iconify (and also minimize) the specified window
 *
 * Reduces the specified window to an icon on the desktop.  Indeed, the
 * minimization occurs, but it's not seen unless there's a taskbar or
 * dock on the desktop.  The default action is to hide (minimize) the
 * window, and replace it with a pixmap icon that's only visible on the
 * desktop the window was, or the current on if the window was sticky,
 * to be restored when clicking the icon.
 *
 * @param window Pointer to the window to be iconified
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_iconify(window_td *window);

/**
 * @brief Toggle the sticky mode of the specified window
 *
 * @param window Pointer to the window to toggle sticky mode
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_toggle_sticky(window_td *window);

/**
 * @brief Toggle full screen mode for the specified window
 *
 * @param window Pointer to the window to toggle full screen mode
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_toggle_fullscreen(window_td *window);

/**
 * @brief Clone an existing window
 *
 * Creates a new window that is a clone of the specified window with
 * same properties, name, and running application.
 *
 * @param window Pointer to the window to clone
 *
 * @return Pointer to the new cloned window, or @c NULL otherwise
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
window_td* window_action_clone(window_td *window);

/**
 * @brief Set the window to always be on top
 *
 * @param window Pointer to the window to be set on top
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_layer_on_top(window_td *window);

/**
 * @brief Set the window to normal layer
 *
 * @param window Pointer to the window to be set to normal layer
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_layer_normal(window_td *window);

/**
 * @brief Set the window to always be at the bottom
 *
 * @param window Pointer to the window to be set at the bottom
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_layer_on_bottom(window_td *window);

/**
 * @brief Update the class of the specified window
 *
 * @param window Pointer to the window whose class will be changed
 * @param class  New class identifier for the window
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_set_class(window_td *window, const char *class);

/**
 * @brief Mark the specified window as urgent
 *
 * Sets the urgency flag for the specified window, typically to grab
 * attention.
 *
 * @param window Pointer to the window to be marked urgent
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_set_urgent(window_td *window);

/**
 * @brief Set an icon for the specified window
 *
 * Assigns an icon to the specified window, which may be displayed on
 * the desktop, and on taskbar or dock if available.
 *
 * @param window    Pointer to the window for which the icon will be set
 * @param icon_path Path to the icon file to be used
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int window_action_set_icon(window_td *window, const char *icon_path);

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
