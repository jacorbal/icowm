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
 * @brief Possible window states a window can be in
 */
enum window_state_e {
    WIN_STATE_IDLE,         /** Regular state */
    WIN_STATE_ICONIZED,     /**< Iconized window */
    WIN_STATE_MAXIMIZED,    /**< Maximized */
    WIN_STATE_FULLSCREEN,   /**< Full screen */
    WIN_STATE_MAX = WIN_STATE_FULLSCREEN,
};


/**
 * @brief Window characteristics using flags using bitwise flags
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
 *
 * Identifies the layering options for windows, which affect their
 * visibility order on the screen.
 */
enum window_layer_e {
    WIN_LAYER_ON_TOP,       /**< Always on top */
    WIN_LAYER_NORMAL,       /**< Normal behavior */
    WIN_LAYER_ON_BOTTOM,    /**< Always behind every window */
    WIN_LAYER_MAX = WIN_LAYER_ON_BOTTOM,
};


/**
 * @brief Window properties
 *
 * Encapsulates various properties of a window: state, layering
 * behavior, and any applicable flags.
 */
struct window_properties_s {
    enum window_state_e state;  /**< State (maximized, iconized,...) */
    enum window_layer_e layer;  /**< Layer (on top, normal, on bottom) */
    enum window_flags_e flags;  /**< Flags (sticky, focused,...) */
};


/**
 * @brief Window structure
 *
 * This structure represents a window in an X11 environment,
 * encapsulating essential attributes for window management, including
 * its unique identifier, dimensions, and graphical properties.
 *
 * The @p screen_id and @p desktop_id fields link the window to its
 * respective screen and desktop, facilitating the organization of
 * windows within the graphical user interface.
 *
 * Additionally, the @p properties field contains various settings that
 * define the behavior and appearance of the window, while the @p theme
 * pointer allows for dynamic theming, enabling customization of the
 * window's visual aspects based on user preferences or system
 * themes.
 */
typedef struct {
    Display *display;           /**< X11 display */
    Window window;              /**< The actual X11 window */

    unsigned int screen_id;     /**< Screen index */
    unsigned int desktop_id;    /**< Desktop index */
    unsigned long int id;       /**< Unique window identifier */
    char *name;                 /**< Window name */

//    Pixmap pixmap;              /**< Icon image */
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
 */


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
