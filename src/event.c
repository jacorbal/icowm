/**
 * @file event.c
 *
 * @brief Event handler implementation
 */

/* System includes */
#include <stdlib.h>     /* NULL, free, malloc */

/* External libraries */
//#include <X11/keysym.h> /* XK_* */
#include <X11/Xlib.h>   /* X*Event */

/* Project includes */
#include <window.h>

/* Local includes */
#include <event.h>
#include <logger.h>


/* Callback invoked on button press event */
static void _handle_button_press(window_td *window, XButtonEvent *event)
{
}


/* Callback invoked on button release event */
static void _handle_button_release(window_td *window, XButtonEvent *event)
{
}


/* Callback invoked on key press event */
static void _handle_key_press(window_td *window, XKeyEvent *event)
{
}


/* Callback invoked on key release event */
static void _handle_key_release(window_td *window, XKeyEvent *event)
{
}


/* Callback invoked on expose event */
static void _handle_expose(window_td *window, XExposeEvent *event)
{
}


/* Callback invoked on configure notify event */
static void _handle_configure_notify(window_td *window,
        XConfigureEvent *event)
{
}


/* Callback invoked on configure request event */
static void _handle_configure_request(window_td *window,
        XConfigureRequestEvent *event)
{
}


/* Callback invoked on map notify event */
static void _handle_map_notify(window_td *window, XMapEvent *event)
{
}


/* Callback invoked on unmap notify event */
static void _handle_unmap_notify(window_td *window, XUnmapEvent *event)
{
}


/* Callback invoked on destroy notify event */
static void _handle_destroy_notify(window_td *window,
        XDestroyWindowEvent *event)
{
}


/* Callback invoked on focus in event */
static void _handle_focus_in(window_td *window, XFocusInEvent *event)
{
}


/* Callback invoked on focus out event */
static void _handle_focus_out(window_td *window, XFocusOutEvent *event)
{
}


/* Callback invoked on motion notify event */
static void _handle_motion_notify(window_td *window, XMotionEvent *event)
{
}


/* Callback invoked on client message event */
static void _handle_client_message(window_td *window,
        XClientMessageEvent *event)
{
}


/* Callback invoked on property notify event */
static void _handle_property_notify(window_td *window,
        XPropertyEvent *event)
{
}


/* Initialize the event system to start handling events */
event_handler_td *event_handler_init(void)
{
    event_handler_td *event_handler;

    LOGGER_INFO("Initializing event system", L_NARG);
    event_handler = malloc(sizeof(event_handler_td));
    if (event_handler == NULL) {
        LOGGER_FATAL("Failed to allocate memory for event system",
                L_NARG);
        return NULL;
    }

    /* Set the pointer to event functions */
    event_handler->button_press = _handle_button_press;
    event_handler->button_release = _handle_button_release;
    event_handler->key_press = _handle_key_press;
    event_handler->key_release = _handle_key_release;
    event_handler->expose = _handle_expose;
    event_handler->configure_notify = _handle_configure_notify;
    event_handler->configure_request = _handle_configure_request;
    event_handler->map_notify = _handle_map_notify;
    event_handler->unmap_notify = _handle_unmap_notify;
    event_handler->destroy_notify = _handle_destroy_notify;
    event_handler->focus_in = _handle_focus_in;
    event_handler->focus_out = _handle_focus_out;
    event_handler->motion_notify = _handle_motion_notify;
    event_handler->client_message = _handle_client_message;
    event_handler->property_notify = _handle_property_notify;

    return event_handler;
}

/* @brief Destroy the event system */
void event_handler_destroy(event_handler_td *event_handler)
{
    LOGGER_DEBUG("Destroying event system", L_NARG);
    free(event_handler);
}


/* Generic event handler */
void event_handler_process(event_handler_td *event_handler,
        window_td *window, XEvent *event)
{
    if (event->type == ButtonPress) {
        if (event_handler->button_press) {
            event_handler->button_press(window,
                    (XButtonEvent *) &event);
        }
    } else if (event->type == ButtonRelease) {
        if (event_handler->button_release) {
            event_handler->button_release(window,
                    (XButtonEvent *) &event);
        }
    } else if (event->type == KeyPress) {
        if (event_handler->key_press) {
            event_handler->key_press(window,
                    (XKeyEvent *) &event);
        }
    } else if (event->type == KeyRelease) {
        if (event_handler->key_release) {
            event_handler->key_release(window,
                    (XKeyEvent *) &event);
        }
    } else if (event->type == Expose) {
        if (event_handler->expose) {
            event_handler->expose(window,
                    (XExposeEvent *) &event);
        }
    } else if (event->type == ConfigureNotify) {
        if (event_handler->configure_notify) {
            event_handler->configure_notify(window,
                    (XConfigureEvent *) &event);
        }
    } else if (event->type == MapNotify) {
        if (event_handler->map_notify) {
            event_handler->map_notify(window,
                    (XMapEvent *) &event);
        }
    } else if (event->type == UnmapNotify) {
        if (event_handler->unmap_notify) {
            event_handler->unmap_notify(window,
                    (XUnmapEvent *) &event);
        }
    } else if (event->type == DestroyNotify) {
        if (event_handler->destroy_notify) {
            event_handler->destroy_notify(window,
                    (XDestroyWindowEvent *) &event);
        }
    } else if (event->type == FocusIn) {
        if (event_handler->focus_in) {
            event_handler->focus_in(window,
                    (XFocusInEvent *) &event);
        }
    } else if (event->type == FocusOut) {
        if (event_handler->focus_out) {
            event_handler->focus_out(window,
                    (XFocusOutEvent *) &event);
        }
    } else if (event->type == MotionNotify) {
        if (event_handler->motion_notify) {
            event_handler->motion_notify(window,
                    (XMotionEvent *) &event);
        }
    } else if (event->type == ClientMessage) {
        if (event_handler->client_message) {
            event_handler->client_message(window,
                    (XClientMessageEvent *) &event);
        }
    } else if (event->type == PropertyNotify) {
        if (event_handler->property_notify) {
            event_handler->property_notify(window,
                    (XPropertyEvent *) &event);
        }
    }
}
