/**
 * @file window.c
 *
 * @brief Window structure implementation
 */

/* System includes */
#include <stdarg.h>     /* va_list, va_start, va_end */
#include <stdbool.h>    /* bool, false, true */
#include <stdlib.h>     /* NULL, free, malloc */

/* X11 includes */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>

/* Type includes */
#include <types/pair.h> /* geometry_s */

/* Project includes */
#include <action.h>
#include <actdata.h>
#include <config.h>
#include <event.h>
#include <eventq.h>
#include <logger.h>
#include <priority.h>
#include <window.h>


/* Initialize a new window */
window_td *window_init(Display *display, window_td *parent,
        unsigned int w, unsigned int h, int x, int y,
        struct config_theme_s *theme)
{
    window_td *window;
    XClassHint *class_hint;

    window = malloc(sizeof(window_td));
    if (window == NULL) {
        LOGGER_ERROR("Failed to allocate memory for new window",
                L_NARG);
        return NULL;
    }

    window->display = display;
    window->parent = parent;

    /* Set the current geometry, and the "old" as the current one */
    window->properties.geometry_cur =
        (struct geometry_s) {.pos = {.x = x, .y = y},
                             .dim = {.w = w, .h = h}};
    window->properties.geometry_old = window->properties.geometry_cur;

    // TODO: Test this...
    window->properties.flags =
        WINDOW_FLAG_HIDDEN | WINDOW_FLAG_FOCUSABLE | WINDOW_FLAG_RESIZABLE;
    window->properties.type = WINDOW_TYPE_NORMAL;
    window->properties.state = WINDOW_STATE_NORMAL;
    window->properties.layer = WINDOW_LAYER_NORMAL;
    window->properties.operation = WINDOW_OPERATION_IDLE;
    window->properties.focusing = WINDOW_FOCUSING_FOCUSED;

    window->process.pid = -1;
    window->process.command = NULL;
    window->theme = theme;

    window->name = NULL;        // <-- TODO
    window->class_name = NULL;  // <-- TODO
    window->icon_path = NULL;   // <-- TODO

    /* Create the X window */
    window->xwindow = XCreateSimpleWindow(display,
            DefaultRootWindow(display),
            x, y, w, h,
            theme->window.general.border_width,
            BlackPixel(display, 0), WhitePixel(display, 0));

    if (!window->xwindow) {
        LOGGER_ERROR("Failed to create window", L_NARG);
        free(window);
        return NULL;
    }

    // TODO: This should go in 'window_action_create'
    /* Configure window */
    XSetStandardProperties(display, window->xwindow,
            "Ventana", "Titulo", None, NULL, 0, NULL);

    class_hint = XAllocClassHint();
    class_hint->res_name = (char *) "my_window";
    class_hint->res_class = (char *) "my_class";
    XSetClassHint(display, window->xwindow, class_hint);
    XFree(class_hint);

    return window;
}


/* Destroy the window and free used memory */
void window_destroy(window_td *window)
{
    LOGGER_TRACE("Deallocating structure for window %#lx ('%s')",
            window->id, window->name);
    if (window) {
        if (window->xwindow) {
            XDestroyWindow(window->display, window->xwindow);
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
    XClearWindow(window->display, window->xwindow);

    /* Draw or update the content over the window */
    //  e.g.: draw_content(window);

    /* Optionally, you might want to flush the output buffer */
    //XFlush(window->display);
}


/* Action to rename a window */
int window_action_rename(window_td *window, const char *new_name)
{
    event_td *event;
    action_td action;
    action_data_window_td *data;

    action.type = ACTION_TYPE_WINDOW;
    action.object.window = ACTION_WINDOW_RENAME;

    /* NOTE: Memory for the action data structure 'data' must be freed
     *       with 'action_data_window_destroy' once the event has
     *       finished processing this action to avoid memory leaks */
    data = action_data_window_init(window, action.object.window);
    data->new_data.name = safe_strdup(new_name);

    event = event_init((void *) window, (void *) data,
            action, PRIORITY_NORMAL);

    return eventq_add(event);
}


/* Action to change class of a window */
int window_action_reclass(window_td *window, const char *new_class)
{
    event_td *event;
    action_td action;
    action_data_window_td *data;

    action.type = ACTION_TYPE_WINDOW;
    action.object.window = ACTION_WINDOW_RECLASS;

    /* NOTE: Memory for the action data structure 'data' must be freed
     *       with 'action_data_window_destroy' once the event has
     *       finished processing this action to avoid memory leaks */
    data = action_data_window_init(window, action.object.window);
    data->new_data.class_name = safe_strdup(new_class);

    event = event_init((void *) window, (void *) data,
            action, PRIORITY_NORMAL);

    return eventq_add(event);
}


/* Action to move a window to a new position */
int window_action_move(window_td *window, int new_x, int new_y)
{
    event_td *event;
    action_td action;
    action_data_window_td *data;

    action.type = ACTION_TYPE_WINDOW;
    action.object.window = ACTION_WINDOW_MOVE;

    /* NOTE: Memory for the action data structure 'data' must be freed
     *       with 'action_data_window_destroy' once the event has
     *       finished processing this action to avoid memory leaks */
    data = action_data_window_init(window, action.object.window);

    data->new_data.geometry.pos.x = new_x;
    data->new_data.geometry.pos.y = new_y;

    event = event_init((void *) window, (void *) data,
            action, PRIORITY_NORMAL);

    return eventq_add(event);
}


/* Action to resize a window to a new position */
int window_action_resize(window_td *window,
        unsigned int new_w, unsigned int new_h)
{
    event_td *event;
    action_td action;
    action_data_window_td *data;

    action.type = ACTION_TYPE_WINDOW;
    action.object.window = ACTION_WINDOW_RESIZE;

    /* NOTE: Memory for the action data structure 'data' must be freed
     *       with 'action_data_window_destroy' once the event has
     *       finished processing this action to avoid memory leaks */
    data = action_data_window_init(window, action.object.window);

    data->new_data.geometry.dim.w = new_w;
    data->new_data.geometry.dim.h = new_h;

    event = event_init((void *) window, (void *) data,
            action, PRIORITY_NORMAL);

    return eventq_add(event);
}


/* Action to change window icon */
int window_action_set_icon(window_td *window, const char *icon_path)
{
    event_td *event;
    action_td action;
    action_data_window_td *data;

    action.type = ACTION_TYPE_WINDOW;
    action.object.window = ACTION_WINDOW_SET_ICON;

    /* NOTE: Memory for the action data structure 'data' must be freed
     *       with 'action_data_window_destroy' once the event has
     *       finished processing this action to avoid memory leaks */
    data = action_data_window_init(window, action.object.window);
    data->new_data.icon_path = safe_strdup(icon_path);

    event = event_init((void *) window, (void *) data,
            action, PRIORITY_NORMAL);

    return eventq_add(event);
}


/* Generic action for a window */
int window_action(window_td *window, enum action_window_e action_window,
        enum priority_e priority)
{
    event_td *event;
    action_td action;

    action.type = ACTION_TYPE_WINDOW;
    action.object.window = action_window;

    event = event_init((void *) window, NULL, action, priority);

    return eventq_add(event);
}
