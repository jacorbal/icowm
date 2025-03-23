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
 * Define a set of event handlers for managing events within a window.
 * Each handler is a pointer to a function that takes a pointer to the
 * window and the corresponding event as arguments.
 */
typedef struct {
    /**
     * @brief Callback invoked on button press event in the window
     *
     * @param window Pointer to the affected window structure
     * @param event  Pointer to the button press event
     */
    void (*button_press)(window_td *window, XButtonEvent *event);

    /**
     * @brief Callback invoked on button release event in the window
     *
     * @param window Pointer to the affected window structure
     * @param event  Pointer to the button release event
     */
    void (*button_release)(window_td *window, XButtonEvent *event);

    /**
     * @brief Callback invoked on key press event in the window
     *
     * @param window Pointer to the affected window structure
     * @param event  Pointer to the key press event
     */
    void (*key_press)(window_td *window, XKeyEvent *event);

    /**
     * @brief Callback invoked on key release event in the window
     *
     * @param window Pointer to the affected window structure
     * @param event  Pointer to the key release event
     */
    void (*key_release)(window_td *window, XKeyEvent *event);

    /**
     * @brief Callback invoked on expose event called when the window
     *        needs to be redrawn
     *
     * @param window Pointer to the affected window structure
     * @param event  Pointer to the expose event
     */
    void (*expose)(window_td *window, XExposeEvent *event);

    /**
     * @brief Callback invoked on configure notify event when the window
     *        configuration has changed
     *
     * @param window Pointer to the affected window structure
     * @param event  Pointer to the configure event
     */
    void (*configure_notify)(window_td *window,
            XConfigureEvent *event);

    /**
     * @brief Callback invoked on configure request event called when
     *        a request to change the window's configuration is made
     *
     * @param window Pointer to the affected window structure
     * @param event  Pointer to the configure request event
     */
    void (*configure_request)(window_td *window,
            XConfigureRequestEvent *event);

    /**
     * @brief Callback invoked on map notify event called when the
     *        window is mapped (made visible)
     *
     * @param window Pointer to the affected window structure
     * @param event  Pointer to the map event
     */
    void (*map_notify)(window_td *window, XMapEvent *event);

    /**
     * @brief Callback invoked on unmap notify event called when the
     *        window is unmapped (made invisible)
     *
     * @param window Pointer to the affected window structure
     * @param event  Pointer to the unmap event
     */
    void (*unmap_notify)(window_td *window, XUnmapEvent *event);

    /**
     * @brief Callback invoked on destroy notify event called when the
     *        window is destroyed
     *
     * @param window Pointer to the affected window structure
     * @param event  Pointer to the destroy window event
     */
    void (*destroy_notify)(window_td *window,
            XDestroyWindowEvent *event);

    /**
     * @brief Callback invoked on focus in event called when the window
     *        gains focus
     *
     * @param window Pointer to the affected window structure
     * @param event  Pointer to the focus in event
     */
    void (*focus_in)(window_td *window, XFocusInEvent *event);

    /**
     * @brief Callback invoked on focus out event called when the window
     *        loses focus
     *
     * @param window Pointer to the affected window structure
     * @param event  Pointer to the focus out event
     */
    void (*focus_out)(window_td *window, XFocusOutEvent *event);

    /**
     * @brief Callback invoked on motion notify event called when the
     *        mouse is moved within the window
     *
     * @param window Pointer to the affected window structure
     * @param event  Pointer to the motion event
     */
    void (*motion_notify)(window_td *window, XMotionEvent *event);

    /**
     * @brief Callback invoked on client message event called when
     *        a client-specific message is received
     *
     * @param window Pointer to the affected window structure
     * @param event  Pointer to the client message event
     */
    void (*client_message)(window_td *window,
            XClientMessageEvent *event);

    /**
     * @brief Callback invoked on property notify event called when
     *        there is a change in a property of the window
     *
     * @param window Pointer to the affected window structure.
     * @param event  Pointer to the property event.
     */
    void (*property_notify)(window_td *window,
            XPropertyEvent *event);
}
event_handler_td;


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
 * Usefull when it's needed to use the event handler accessing by
 * reference to its structure
 *
 * @note Complexity: @e O(1)
 */
void event_handler_process(event_handler_td *event_handler,
        window_td *window, XEvent *event);


#endif  /* ! EVENT_H */
