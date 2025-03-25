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
#include <eventq.h>
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
