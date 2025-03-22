/**
 * @file event.h
 *
 * @brief Event handler function declaration
 */

#ifndef EVENT_H
#define EVENT_H


/* External libraries */
#include <X11/Xlib.h>   /* X*Event */

/* Project includes */
#include <window.h>


/**
 * @brief Event handler structure
 *
 * X
 */
typedef struct {
    void (*button_press)(window_td *window, XButtonEvent *event);
    void (*button_release)(window_td *window, XButtonEvent *event);
    void (*key_press)(window_td *window, XKeyEvent *event);
    void (*key_release)(window_td *window, XKeyEvent *event);
    void (*expose)(window_td *window, XExposeEvent *event);
    void (*configure_notify)(window_td *window,
            XConfigureEvent *event);
    void (*configure_request)(window_td *window,
            XConfigureRequestEvent *event);
    void (*map_notify)(window_td *window, XMapEvent *event);
    void (*unmap_notify)(window_td *window, XUnmapEvent *event);
    void (*destroy_notify)(window_td *window,
            XDestroyWindowEvent *event);
    void (*focus_in)(window_td *window, XFocusInEvent *event);
    void (*focus_out)(window_td *window, XFocusOutEvent *event);
    void (*motion_notify)(window_td *window, XMotionEvent *event);
    void (*client_message)(window_td *window,
            XClientMessageEvent *event);
    void (*property_notify)(window_td *window,
            XPropertyEvent *event);
} event_handler_td;


/* Public interface */
/**
 * @brief Initialize the event system to start handling events
 *
 * @note Complexity: @e O(1)
 */
event_handler_td *event_handler_init(void);

/**
 * @brief Stop the event system
 *
 * @note Complexity: @e O(1)
 */
void event_handler_destroy(event_handler_td *event_handler);

/**
 * @brief Generic event handler
 *
 * @note Complexity: @e O(1)
 */
void event_handler_generic(event_handler_td *event_handler,
        window_td *window, XEvent *event);


#endif  /* ! EVENT_H */
