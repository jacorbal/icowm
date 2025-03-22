/**
 * @file window.h
 *
 * @brief Window definition and declaration
 */

#ifndef WINDOW_H
#define WINDOW_H


/* External libraries */
#include <X11/Xlib.h>   /* Display, Window */

/* Common type structures */
#include <types/pair.h> /* dimensions_s, geometry_s */

/* Project includes */
#include <config.h>


/**
 * @brief Possible window states
 */
enum window_state_e {
    WIN_STATE_IDLE,         /** Regular state */
    WIN_STATE_ICONIZED,     /**< Iconized window */
    WIN_STATE_MAXIMIZED,    /**< Maximized */
    WIN_STATE_FULLSCREEN,   /**< Full screen */
    WIN_STATE_MAX = WIN_STATE_FULLSCREEN,
};


/**
 * @brief Window characteristics using flags
 */
enum window_flags_e {
    WIN_PROPERTY_VISIBLE   = 1 << 0, /* 0000 0001: window is visible */
    WIN_PROPERTY_FOCUSED   = 1 << 1, /* 0000 0010: window has focus */
    WIN_PROPERTY_STICKY    = 1 << 2, /* 0000 0100: window in all desktops */
    WIN_PROPERTY_DECORATED = 1 << 3, /* 0000 1000: window has decoration */
    WIN_PROPERTY_DISABLED  = 1 << 4, /* 0001 0000: window is disabled */
};


/**
 * @brief Window possible layers
 */
enum window_layer_e {
    WIN_LAYER_ON_TOP,       /**< Always on top */
    WIN_LAYER_NORMAL,       /**< Normal behavior */
    WIN_LAYER_ON_BOTTOM,    /**< Always behind every window */
    WIN_LAYER_MAX = WIN_LAYER_ON_BOTTOM,
};


/**
 * @brief Window properties
 */
struct window_properties_s {
    enum window_state_e state;  /**< State (maximized, iconized,...) */
    enum window_layer_e layer;  /**< Layer (on top, normal, on bottom) */
    enum window_flags_e flags;  /**< Flags (sticky, focused,...) */
};


/**
 * @brief Window structure
 */
typedef struct {
    Display *display;           /**< X11 display */
    Window window;              /**< The actual X11 window */

    unsigned int screen_id;     /**< Screen index */
    unsigned int desktop_id;    /**< Desktop index */
    unsigned long int id;       /**< Unique window identifier */
    char *name;                 /**< Window name */

    struct geometry_s geom;     /**< Window geometry (px) */
    struct dimensions_s dim;    /**< Window dimensions (px) */

    struct window_properties_s properties;
    struct config_theme_s *theme;
} window_td;


/* Public interface */
/**
 * @brief Initialize a new window with the specified parameters
 *
 * @param display Pointer to the X11 display
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
window_td *window_init(Display *display,
        unsigned int w, unsigned int h, int x, int y,
        struct config_theme_s *theme);

/**
 * @brief Destroy the specified window and free associated resources
 *
 * @param window Pointer to the window to be destroyed
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
 * @brief Show the specified window on the screen
 *
 * @param window Pointer to the window to be shown
 *
 * @note Complexity: @e O(1)
 */
void window_show(window_td *window);

/**
 * @brief Hide the specified window from the screen
 *
 * @param window Pointer to the window to be hidden
 *
 * @note Complexity: @e O(1)
 */
void window_hide(window_td *window);

/**
 * @brief Iconize the specified window, minimizing it to an icon
 *
 * @param window Pointer to the window to be iconized
 *
 * @notes Complexity: @e O(1)
 */
void window_iconize(window_td *window);

/**
 * @brief Move the specified window to the given coordinates
 *
 * @param window Pointer to the window to be moved
 * @param x      New X-coordinate for the window
 * @param y      New Y-coordinate for the window
 *
 * @note Complexity: @e O(1)
 */
void window_move(window_td *window, int x, int y);

/**
 * @brief Add decorations to the specified window
 *
 * @param window Pointer to the window to be decorated
 *
 * @note Complexity: @e O(1)
 */
void window_decorate(window_td *window);

/**
 * @brief Remove decorations to the specified window
 *
 * @param window Pointer to the window to be undecorated
 *
 * @note Complexity: @e O(1)
 */
void window_undecorate(window_td *window);

/**
 * @brief Handle various window-related events for the specified window
 *
 * This function processes incoming X events and updates the state of
 * the window based on the event type.  Specifically, it handles focus
 * events and configuration notifications, updating the window's focused
 * state and geometry as needed.
 *
 * @param window Pointer to the window for which events are being handled
 * @param event  Pointer to an @p XEvent structure that contains the
 *               event information to be processed
 */
void window_event_handle(window_td *window, XEvent *event);

/**
 * @brief Set focus to the specified window
 *
 * @param window Pointer to the window to receive focus
 *
 * @note Complexity: @e O(1)
 */
void window_focus(window_td *window);

/**
 * @brief Modify the state of the specified window
 *
 * @param window Pointer to the window to be updated
 * @param state  New state to be set for the window
 *
 * @note Complexity: @e O(1)
 */
void window_state(window_td *window, enum window_state_e state);

/**
 * @brief Resize the specified window to the new dimensions
 *
 * @param window Pointer to the window to be resized
 * @param width  New width for the window in pixels
 * @param height New height for the window in pixels
 *
 * @note Complexity: @e O(1)
 */
void window_resize(window_td *window,
        unsigned int width, unsigned int height);

/**
 * @brief Macro that evaluates to toggling the visibility flag
 *
 * @note Complexity: @e O(1)
 *
 * @see window_flags_e
 */
#define window_toggle_visibility(w) \
    ((w)->properties.flags ^= (enum window_flags_e) WIN_PROPERTY_VISIBLE)

/**
 * @brief Macro that evaluates to toggling the focus flag
 *
 * @note Complexity: @e O(1)
 *
 * @see window_flags_e
 */
#define window_toggle_focus(w) \
    ((w)->properties.flags ^= (enum window_flags_e) WIN_PROPERTY_FOCUSED)

/**
 * @brief Macro that evaluates to toggling the stickiness flag
 *
 * @note Complexity: @e O(1)
 *
 * @see window_flags_e
 */
#define window_toggle_stickiness(w) \
    ((w)->properties.flags ^= (enum window_flags_e) WIN_PROPERTY_STICKY)

/**
 * @brief Macro that evaluates to toggling the decoration flag
 *
 * @note Complexity: @e O(1)
 *
 * @see window_flags_e
 */
#define window_toggle_decoration(w) \
    ((w)->properties.flags ^= (enum window_flags_e) WIN_PROPERTY_DECORATED)

/**
 * @brief Macro that evaluates to toggling the availability flag
 *
 * @note Complexity: @e O(1)
 *
 * @see window_flags_e
 */
#define window_toggle_availability(w) \
    ((w)->properties.flags ^= (enum window_flags_e) WIN_PROPERTY_DISABLED)


#endif  /* ! WINDOW_H */
