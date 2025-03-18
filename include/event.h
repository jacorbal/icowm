/**
 * file event.h
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
 * @brief Initialize the event system to start handling events
 *
 * @note Complexity: @e O(1)
 */
void event_init_system(void);

/**
 * @brief Clean up resources related to the event system
 *
 * @note Complexity: @e O(1)
 */
void event_cleanup_system(void);

/**
 * @brief Handle the @c ConfigureNotify event for the specified window
 *
 * This function updates the geometry properties of the window,
 * including its position (@e x, @e y) and size (@e w, @e h), based on
 * the information received in the @c ConfigureNotify event.
 *
 * @param window Pointer to the window whose geometry is to be updated
 * @param event  Pointer to an @c XConfigureEvent structure containing
 *               the new geometry information for the window
 *
 * @note Complexity: @e O(n) where @e n is the number of child windows
 *       to be updated
 */
void event_handle_configure_notify(window_td *window,
        XConfigureEvent *event);

/**
 * @brief Handle the @c FocusIn event for the specified window
 *
 * This function is called when the window gains focus.
 *
 * @param window Pointer to the window
 * @param event A pointer to the @c XFocusChangeEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_focus_in(window_td *window, XFocusChangeEvent *event);

/**
 * @brief Handle the @c FocusOut event for the specified window
 *
 * This function is called when the window loses focus.
 *
 * @param window Pointer to the window
 * @param event  Pointer to the @c XFocusChangeEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_focus_out(window_td *window, XFocusChangeEvent *event);

/**
 * @brief Handle the @c KeyPress event for the specified window
 *
 * @param window Pointer to the window
 * @param event  Pointer to the @c XKeyEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_key_press(window_td *window, XKeyEvent *event);

/**
 * @brief Handle the @c KeyRelease event for the specified window
 *
 * @param window Pointer to the window
 * @param event  Pointer to the @c XKeyEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_key_release(window_td *window, XKeyEvent *event);

/**
 * @brief Handle the @c ButtonPress event for the specified window
 *
 * @param window Pointer to the window
 * @param event  Pointer to the @c XButtonEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_button_press(window_td *window, XButtonEvent *event);

/**
 * @brief Handle the @c ButtonRelease event for the specified window
 *
 * @param window Pointer to the window
 * @param event  Pointer to the @c XButtonEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_button_release(window_td *window, XButtonEvent *event);

/**
 * @brief Handle @c MotionNotify events for a specified window
 *
 * This function is called when the mouse is moved within the window.
 *
 * @param window A pointer to the window being interacted with
 * @param event A pointer to the XMotionEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_motion_notify(window_td *window, XMotionEvent *event);

/**
 * @brief Handle @c Expose events for a specified window.
 *
 * This function is called when a part of the window needs to be redrawn.
 *
 * @param window Pointer to the window being redrawn
 * @param event  Pointer to the @c XExposeEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_expose(window_td *window, XExposeEvent *event);

/**
 * @brief Handle @c DestroyNotify events for a specified window
 *
 * This function is called when a window is being destroyed.
 *
 * @param window Pointer to the window being destroyed
 * @param event  Pointer to the @c XDestroyWindowEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_destroy_notify(window_td *window,
        XDestroyWindowEvent *event);

/**
 * @brief Handle @c CreateNotify events for a specified window
 *
 * This function is called when a new window is created.
 *
 * @param window Pointer to the new window being created
 * @param event  Pointer to the @c XCreateWindowEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_create_notify(window_td *window,
        XCreateWindowEvent *event);

/**
 * @brief Handle @c MapNotify events for a specified window
 *
 * This function is called when a window is mapped (shown).
 *
 * @param window Pointer to the window being mapped
 * @param event  Pointer to the @c XMapEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_map_notify(window_td *window, XMapEvent *event);

/**
 * @brief Handle @c UnmapNotify events for a specified window
 *
 * This function is called when a window is unmapped (hidden).
 *
 * @param window A pointer to the window being unmapped
 * @param event A pointer to the XUnmapEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_unmap_notify(window_td *window, XUnmapEvent *event);

/**
 * @brief Handle @c EnterNotify events for a specified window
 *
 * This function is called when the mouse cursor enters the window.
 *
 * @param window Pointer to the window being entered
 * @param event  Pointer to the XCrossingEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_enter_notify(window_td *window, XCrossingEvent *event);

/**
 * @brief Handle @c LeaveNotify events for a specified window
 *
 * This function is called when the mouse cursor leaves the window.
 *
 * @param window Pointer to the window being left
 * @param event  Pointer to the XCrossingEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_leave_notify(window_td *window,
        XCrossingEvent *event);

/**
 * @brief Handle @c PropertyNotify events for a specified window
 *
 * This function is called when a property of a window has changed.
 *
 * @param window Pointer to the window whose property changed
 * @param event  Pointer to the XPropertyEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_property_notify(window_td *window,
        XPropertyEvent *event);

/**
 * @brief Handle @c ClientMessage events for a specified window
 *
 * This function is called when a client message is received.
 *
 * @param window Pointer to the window receiving the message
 * @param event  Pointer to the @c XClientMessageEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_client_message(window_td *window,
        XClientMessageEvent *event);

/**
 * @brief Handle @c ResizeRequest events for a specified window
 *
 * This function is called when a window requests to change its size
 *
 * @param window Pointer to the window that requested a resize
 * @param event  Pointer to the @c XResizeRequestEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_resize_request(window_td *window,
        XResizeRequestEvent *event);

/**
 * @brief Handle @c ConfigureRequest events for a specified window
 *
 * This function is called when another client requests to change the
 * configuration of a window.
 *
 * @param window Pointer to the window whose information is being requested
 * @param event  Pointer to the @c XConfigureRequestEvent structure
 *
 * @note Complexity: @e O(1)
 */
void event_handle_configure_request(window_td *window,
        XConfigureRequestEvent *event);

/**
 * @brief Generic event handler that delegates events to appropriate
 *        handlers
 *
 * @param window Pointer to the window
 * @param event  Pointer to an @c XEvent structure containing the data
 *
 * @note Complexity: @e O(n) where @e n is the number of specific event
 *       types handled
 */
void event_generic_handler(window_td *window, XEvent *event);


#endif  /* ! EVENT_H */
