#ifndef WINDOW_H
#define WINDOW_H

/* System includes */
#include <stdbool.h>    /* bool */

/* External libraries */
#include <X11/Xlib.h>   /* Display, Window */

/* Project includes */
#include <config.h>


/**
 */
enum window_state_e {
    WIN_STATE_IDLE,
    WIN_STATE_ICONIZED,
    WIN_STATE_MAXIMIZED,
    WIN_STATE_FULLSCREEN,
    WIN_STATE_MAX,
};


/**
 */
enum window_flags_e {
    WIN_PROPERTY_VISIBLE   = 1 << 0, /* 0000 0001: window is visible */
    WIN_PROPERTY_FOCUSED   = 1 << 1, /* 0000 0010: window has focus */
    WIN_PROPERTY_STICKY    = 1 << 2, /* 0000 0100: window in all desktops */
    WIN_PROPERTY_DECORATED = 1 << 3, /* 0000 1000: window has decoration */
    WIN_PROPERTY_DISABLED  = 1 << 4, /* 0001 0000: window is disabled */
};


/**
 */
enum window_layer_e {
    WIN_LAYER_TOP,      /**< Always on top */
    WIN_LAYER_NORMAL,   /**< Normal behaviour */
    WIN_LAYER_BOTTOM,   /**< Always behind every window */
    WIN_LAYER_MAX,
};


/**
 */
struct window_properties_s {
    enum window_state_e state;  /**< State (maximized, iconized,...) */
    enum window_layer_e layer;  /**< Layer (top, normal, bottom) */
    enum window_flags_e flags;  /**< Flags (sticky, focused,...) */
};


/**
 */
struct window_geometry_s {
    unsigned int w;     /**< Window width (px) */
    unsigned int h;     /**< Window height (px) */
    int x;              /**< Window X position (px) */
    int y;              /**< Window Y position (px) */
};


/**
 */
typedef struct {
    unsigned long id;   /**< Unique window identifier */
    char *title;        /**< Window title */

    Display *display;   /**< X11 display */
    Window window;      /**< The actual window*/

    struct window_geometry_s geometry;
    struct window_properties_s properties;
    struct config_theme_s *theme;
} window_td;


/**
 */
window_td *window_init(Display *display,
        unsigned int w, unsigned int h, int x, int y,
        struct config_theme_s *config_theme);

/**
 */
void window_destroy(window_td *window);

/**
 */
void window_update(window_td *window);

/**
 */
void window_show(window_td *window);

/**
 */
void window_hide(window_td *window);

/**
 */
void window_iconize(window_td *window);

/**
 */
void window_move(window_td *window, int x, int y);

/**
 */
void window_decorate(window_td *window);

/**
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
 * @param event  Pointer to an @c XEvent structure that contains the
 *               event information to be processed
 */
void window_event_handle(window_td *window, XEvent *event);

/**
 */
void window_focus(window_td *win);

/**
 */
void window_state(window_td *win, enum window_state_e state);

/**
 */
void window_resize(window_td *window,
        unsigned int width, unsigned int height);

/**
 */
#define window_toggle_decoration(w) \
    ((w)->properties.flags ^= WIN_PROPERTY_DECORATED)

/**
 */
#define window_toggle_stikyness(w) \
    ((w)->properties.flags ^= WIN_PROPERTY_STICKY)


#endif  /* ! WINDOW_H */
