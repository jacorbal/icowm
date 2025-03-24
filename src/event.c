/**
 * @file event.c
 *
 * @brief Event handler implementation
 */

/* System includes */
#include <stdlib.h>     /* NULL, free, malloc */

/* External libraries */
#include <X11/Xlib.h>   /* X*Event */
#include <X11/keysym.h> /* XK_* */

/* Local includes */
#include <event.h>
#include <logger.h>


/* Callback invoked on button press event */
static void _handle_button_press(XButtonEvent *event)
{
}


/* Callback invoked on button release event */
static void _handle_button_release(XButtonEvent *event)
{
}


/* Callback invoked on key press event */
static void _handle_key_press(XKeyEvent *event)
{
    /* Get 'KeySym' for pressed key */
    KeySym keysym = XLookupKeysym(event, 0);

    /* Ctrl+Alt+Shift+Backspace */
    if (keysym == XK_BackSpace && 
            (event->state & ControlMask) &&
            (event->state & Mod1Mask) &&
            (event->state & ShiftMask)) {

        LOGGER_DEBUG("Ctrl+Alt+Shift+Backspace was pressed", L_NARG);
    }
}


/* Callback invoked on key release event */
static void _handle_key_release(XKeyEvent *event)
{
}


/* Callback invoked on expose event */
static void _handle_expose(XExposeEvent *event)
{
}


/* Callback invoked on configure notify event */
static void _handle_configure_notify(XConfigureEvent *event)
{
}


/* Callback invoked on configure request event */
static void _handle_configure_request(XConfigureRequestEvent *event)
{
}


/* Callback invoked on map notify event */
static void _handle_map_notify(XMapEvent *event)
{
}


/* Callback invoked on unmap notify event */
static void _handle_unmap_notify(XUnmapEvent *event)
{
}


/* Callback invoked on destroy notify event */
static void _handle_destroy_notify(XDestroyWindowEvent *event)
{
}


/* Callback invoked on focus in event */
static void _handle_focus_in(XFocusInEvent *event)
{
}


/* Callback invoked on focus out event */
static void _handle_focus_out(XFocusOutEvent *event)
{
}


/* Callback invoked on motion notify event */
static void _handle_motion_notify(XMotionEvent *event)
{
}


/* Callback invoked on client message event */
static void _handle_client_message(XClientMessageEvent *event)
{
}


/* Callback invoked on property notify event */
static void _handle_property_notify(XPropertyEvent *event)
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
        XEvent *event)
{
    if (event->type == ButtonPress) {
        if (event_handler->button_press) {
            event_handler->button_press(
                    (XButtonEvent *) &event);
        }
    } else if (event->type == ButtonRelease) {
        if (event_handler->button_release) {
            event_handler->button_release(
                    (XButtonEvent *) &event);
        }
    } else if (event->type == KeyPress) {
        if (event_handler->key_press) {
            event_handler->key_press(
                    (XKeyEvent *) &event);
        }
    } else if (event->type == KeyRelease) {
        if (event_handler->key_release) {
            event_handler->key_release(
                    (XKeyEvent *) &event);
        }
    } else if (event->type == Expose) {
        if (event_handler->expose) {
            event_handler->expose(
                    (XExposeEvent *) &event);
        }
    } else if (event->type == ConfigureNotify) {
        if (event_handler->configure_notify) {
            event_handler->configure_notify(
                    (XConfigureEvent *) &event);
        }
    } else if (event->type == MapNotify) {
        if (event_handler->map_notify) {
            event_handler->map_notify(
                    (XMapEvent *) &event);
        }
    } else if (event->type == UnmapNotify) {
        if (event_handler->unmap_notify) {
            event_handler->unmap_notify(
                    (XUnmapEvent *) &event);
        }
    } else if (event->type == DestroyNotify) {
        if (event_handler->destroy_notify) {
            event_handler->destroy_notify(
                    (XDestroyWindowEvent *) &event);
        }
    } else if (event->type == FocusIn) {
        if (event_handler->focus_in) {
            event_handler->focus_in(
                    (XFocusInEvent *) &event);
        }
    } else if (event->type == FocusOut) {
        if (event_handler->focus_out) {
            event_handler->focus_out(
                    (XFocusOutEvent *) &event);
        }
    } else if (event->type == MotionNotify) {
        if (event_handler->motion_notify) {
            event_handler->motion_notify(
                    (XMotionEvent *) &event);
        }
    } else if (event->type == ClientMessage) {
        if (event_handler->client_message) {
            event_handler->client_message(
                    (XClientMessageEvent *) &event);
        }
    } else if (event->type == PropertyNotify) {
        if (event_handler->property_notify) {
            event_handler->property_notify(
                    (XPropertyEvent *) &event);
        }
    }
}
