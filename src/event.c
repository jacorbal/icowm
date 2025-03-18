/**
 * @file event.c
 *
 * @brief Event handler implementation
 */

/* External libraries */
#include <X11/Xlib.h>   /* XConfigureEvent */

/* Project includes */
#include <window.h>

/* Local includes */
#include <event.h>


/* Handle the 'ConfigureNotify' event */
void event_handle_configure_notify(window_td *window,
        XConfigureEvent *event)
{
    window->geometry.x = event->x;
    window->geometry.y = event->y;
    window->geometry.w = (unsigned int) event->width;
    window->geometry.h = (unsigned int) event->height;
}


/* Handle key press logic */
void event_handle_key_press(window_td *window, XKeyEvent *event)
{
    //printf("Key pressed: %d\n", event->keycode);
}


/* Handle key release logic */
void event_handle_key_release(window_td *window, XKeyEvent *event)
{
    //printf("Key released: %d\n", event->keycode);
}


/* Handle button press logic */
void event_handle_button_press(window_td *window, XButtonEvent *event)
{
    //printf("Button pressed: %d at (%d, %d)\n",
    //        event->button, event->x, event->y);
}


/* Handle button release logic */
void event_handle_button_release(window_td *window, XButtonEvent *event)
{
    //printf("Button released: %d at (%d, %d)\n",
    //        event->button, event->x, event->y);
}


/* Handle 'MotionNotify' events */
void event_handle_motion_notify(window_td *window, XMotionEvent *event)
{
    //printf("Mouse moved in window %p at (%d, %d)\n",
    //         window, event->x, event->y);
}


/* Handle 'Expose' events */
void event_handle_expose(window_td *window, XExposeEvent *event)
{
    /* Redraw the affected area of the window */
    //printf("Exposing area in window %p: (%d, %d) width %d height %d\n",
    //        window, event->x, event->y, event->width, event->height);

    /* Code to redraw would go here, using window's drawing functions */
}


/* Handle 'DestroyNotify' events */
void event_handle_destroy_notify(window_td *window,
        XDestroyWindowEvent *event) {
    //printf("Window %p is being destroyed\n", window);

    /* Cleanup window resources or remove it from the manager */
}


/* Handle 'CreateNotify' events */
void event_handle_create_notify(window_td *window,
        XCreateWindowEvent *event)
{
    //printf("New window created: %p\n", window);

    /* Initialize window properties or resources here */
}


/* Handle 'MapNotify' events */
void event_handle_map_notify(window_td *window, XMapEvent *event)
{
    //printf("Window %p is mapped (shown)\n", window);

    /* Update window status or visibility here */
}


/* Handle 'UnmapNotify' events */
void event_handle_unmap_notify(window_td *window, XUnmapEvent *event)
{
    //printf("Window %p is unmapped (hidden)\n", window);

    /* Update window status or visibility here */
}


/* Handle 'EnterNotify' events */
void event_handle_enter_notify(window_td *window, XCrossingEvent *event)
{
    //printf("Mouse entered window %p\n", window);

    /* Possibly highlight or focus the window */
}


/* Handle 'LeaveNotify' events */
void event_handle_leave_notify(window_td *window, XCrossingEvent *event)
{
    //printf("Mouse left window %p\n", window);

    /* Possibly reset highlight or focus */
}


/* Handle 'PropertyNotify' events */
void event_handle_property_notify(window_td *window,
        XPropertyEvent *event)
{
    //printf("Property changed for window %p\n", window);

    /* Handle updated properties (e.g., title, size) */
}


/* Handle 'ClientMessage' events */
void event_handle_client_message(window_td *window,
        XClientMessageEvent *event)
{
    //printf("Received client message for window %p\n", window);

    /* Handle specific client messages, like closing the window */
}


/* Handle 'ResizeRequest' events */
void event_handle_resize_request(window_td *window,
        XResizeRequestEvent *event)
{
    //printf("Resize requested for window %p: width: %d, height: %d\n",
    //        window, event->width, event->height);

    /* Handle the resize, possibly by changing window dimensions */
}


/* Handle 'ConfigureRequest' events */
void event_handle_configure_request(window_td *window,
        XConfigureRequestEvent *event)
{
    //printf("Configure request for window %p\n", window);

    /* Adjust window configuration based on the request */
}
