
/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* External libraries */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* Project includes */
#include <config.h>
#include <event.h>

/* Local includes */
#include <window.h>



/**
 */
window_td *window_init(Display *display,
        unsigned int w, unsigned int h, int x, int y,
        struct config_theme_s *config_theme)
{
    window_td *window;

    window = malloc(sizeof(window_td));
    if (window == NULL) {
        fprintf(stderr, "Error al alojar memoria para la ventana\n");
        return NULL;
    }

    window->display = display;
    window->theme = config_theme;
    window->geometry = (struct window_geometry_s) {w, h, x, y};
    window->properties.flags = 0;
    window->properties.state = WIN_STATE_IDLE;

    /* Initialize the window in hidden mode */
    //window->properties.flags |= (enum window_flags_e) WIN_PROPERTY_VISIBLE;

    /* Create the window */
    window->window = XCreateSimpleWindow(display,
            DefaultRootWindow(display),
            x, y, w, h,
            config_theme->window.general.border_width,
            BlackPixel(display, 0), WhitePixel(display, 0));

    if (!window->window) {
        fprintf(stderr, "Error al crear la ventana\n");
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


/* Destroy the window and frees memory */
void window_destroy(window_td *window)
{
    if (window) {
        if (window->window) {
            XDestroyWindow(window->display, window->window);
        }
        free(window);
    }
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
        window->geometry.x = x;
        window->geometry.y = y;
        XFlush(window->display);
    }
}


/* Decorate the window */
void window_decorate(window_td *window)
{
    window->geometry.w += 10;
    window->geometry.h += 20;
    // Cambiar el tamaño de la ventana real si es necesario, o agregar
    // decoraciones aquí
}


/* Undecorate window */
void window_undecorate(window_td *window)
{
    window->geometry.w -= 10;
    window->geometry.h -= 20;
    // Ajusta el tamaño de la ventana real si es necesario
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
        event_handle_configure_notify(window, &event->xconfigure);
    }
}


/*  */
void window_focus(window_td *window)
{
    window->properties.flags |= (enum window_flags_e) WIN_PROPERTY_FOCUSED;
    XRaiseWindow(window->display, window->window);
    XSetInputFocus(window->display, window->window, RevertToPointerRoot, CurrentTime);
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
            // Implementar el cambio a pantalla completa
            break;
        case WIN_STATE_IDLE:
            // Resetear a un estado normal
            break;
        default:
            fprintf(stderr, "Estado de ventana no manejado: %d\n", state);
            break;
    }
}


/*  */
void window_resize(window_td *window, unsigned int w, unsigned int h)
{
    if (window->properties.flags & WIN_PROPERTY_VISIBLE) {
        XResizeWindow(window->display, window->window, w, h);
        window->geometry.w = w;
        window->geometry.h = h;
        XFlush(window->display);
    }
}
