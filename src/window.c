/**
 * @file window.c
 *
 * @brief Window structure implementation
 */

/* System includes */
#include <stdlib.h>     /* NULL, free, malloc */

/* External libraries */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* Project includes */
#include <config.h>
#include <event.h>
#include <logger.h>

/* Local includes */
#include <window.h>


/* Initialize a new window */
window_td *window_init(Display *display,
        unsigned int w, unsigned int h, int x, int y,
        struct config_theme_s *theme)
{
    window_td *window;

    window = malloc(sizeof(window_td));
    if (window == NULL) {
        LOGGER_ERROR("Memory allocation failure for window", L_NARG);
        return NULL;
    }

    window->display = display;
    window->theme = theme;
    window->geom.x = x; window->geom.y = y;
    window->dim.w = w; window->dim.h = h;
    window->properties.flags = 0;
    window->properties.state = WIN_STATE_IDLE;
    window->properties.layer = WIN_LAYER_NORMAL;
    window->name = NULL;    // <-- TODO

    /* Initialize the window in hidden mode */
    //window->properties.flags |= (enum window_flags_e) WIN_PROPERTY_VISIBLE;

    /* Create the X window */
    window->window = XCreateSimpleWindow(display,
            DefaultRootWindow(display),
            x, y, w, h,
            theme->window.general.border_width,
            BlackPixel(display, 0), WhitePixel(display, 0));

    if (!window->window) {
        LOGGER_ERROR("Failed to create window", L_NARG);
        free(window);
        return NULL;
    }

    /* Configure window */
    XSetStandardProperties(display, window->window,
            "Ventana", "Titulo", None, NULL, 0, NULL);

    XClassHint *class_hint = XAllocClassHint();
    class_hint->res_name = (char *) "my_window";
    class_hint->res_class = (char *) "MyAppClass";
    XSetClassHint(display, window->window, class_hint);
    XFree(class_hint);

    return window;
}


/* Destroy the window and free used memory */
void window_destroy(window_td *window)
{
    LOGGER_TRACE("Deallocating structure for window %#lx ('%s')",
            window->id, window->name);
    if (window) {
        if (window->window) {
            XDestroyWindow(window->display, window->window);
        }
        free(window);
    }
}


/* Update the content of the window */
void window_update(window_td *window)
{
    if (window == NULL) {
        return;
    }

    LOGGER_TRACE("Updating window %#lx ('%s')",
            window->id, window->name);

    /* Clear the window */
    XClearWindow(window->display, window->window);

    /* Draw or update the content over the window */
    //  e.g.: draw_content(window);

    /* Optionally, you might want to flush the output buffer */
    XFlush(window->display);
}


/* Show the window if it was hidden */
void window_show(window_td *window)
{
    if (!(window->properties.flags & WIN_PROPERTY_VISIBLE)) {
        XMapWindow(window->display, window->window);

        /* Update visibility */
        window->properties.flags |=
            (enum window_flags_e) WIN_PROPERTY_VISIBLE;
        XFlush(window->display);
    }
}


/* Hide the window if it was visible */
void window_hide(window_td *window)
{
    if (window->properties.flags & WIN_PROPERTY_VISIBLE) {
        XUnmapWindow(window->display, window->window);

        /* Update visibility */
        window->properties.flags &=
            (enum window_flags_e) ~WIN_PROPERTY_VISIBLE;
        XFlush(window->display);
    }
}


/* Iconize the window */
void window_iconize(window_td *window)
{
    XIconifyWindow(window->display, window->window,
            DefaultScreen(window->display));
    window->properties.state = WIN_STATE_ICONIZED;
    window_hide(window);
}


/* Move the window */
void window_move(window_td *window, int x, int y)
{
    if (window->properties.flags & WIN_PROPERTY_VISIBLE) {
        XMoveWindow(window->display, window->window, x, y);
        window->geom.x = x;
        window->geom.y = y;
        XFlush(window->display);
    }
}


/* Decorate the window */
void window_decorate(window_td *window)
{
    window->dim.w += 10;
    window->dim.h += 20;
    /* TODO: Adjust real window size if necessary, and add decorations */
}


/* Remove decorations from window */
void window_undecorate(window_td *window)
{
    window->dim.w -= 10;
    window->dim.h -= 20;
    /* TODO: Adjust real window size if needed, and remove decorations */
}


/* Handle window-related events */
void window_event_handle(window_td *window, XEvent *event)
{
    if (event->type == FocusIn) {
        window_focus(window);
    } else if (event->type == FocusOut) {
        window->properties.flags &=
            (enum window_flags_e) ~WIN_PROPERTY_FOCUSED;
    } else if (event->type == ConfigureNotify) {
        //event_handle_configure(window, &event->xconfigure);
    }
}


/*  */
void window_focus(window_td *window)
{
    window->properties.flags |= (enum window_flags_e) WIN_PROPERTY_FOCUSED;
    XRaiseWindow(window->display, window->window);
    XSetInputFocus(window->display, window->window,
            RevertToPointerRoot, CurrentTime);
}


/*  */
void window_state(window_td *window, enum window_state_e state)
{
    window->properties.state = state;

    switch (state) {
        case WIN_STATE_MAXIMIZED:
            XMoveResizeWindow(window->display, window->window, 0, 0,
                    (unsigned int) DisplayWidth(window->display,
                        DefaultScreen(window->display)),
                    (unsigned int) DisplayHeight(window->display,
                        DefaultScreen(window->display)));
            break;
        case WIN_STATE_ICONIZED:
            window_iconize(window);
            break;
        case WIN_STATE_FULLSCREEN:
            /* Start full screen (should be a full screen toggle?) */
            break;
        case WIN_STATE_IDLE:
            /* Reset to a normal state */
            break;
        default:
            LOGGER_WARNING("Unrecognized window state: %d", state);
            break;
    }
}


/*  */
void window_resize(window_td *window,
        unsigned int width, unsigned int height)
{
    if (window->properties.flags & WIN_PROPERTY_VISIBLE) {
        XResizeWindow(window->display, window->window, width, height);
        window->dim.w = width;
        window->dim.h = height;
        XFlush(window->display);
    }
}
