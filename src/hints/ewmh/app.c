/**
 * @file hints/ewmh/app.c
 *
 * @brief Implementation of functions for EWMH application window
 */

/* System includes */
#include <stdint.h>     /* int32_t, uint32_t */
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* memcpy */

/* X11 includes */
#include <X11/Xlib.h>   /* Display, Window */
#include <X11/Xatom.h>  /* Atom, XA_* */

/* Project includes */
#include <hints/ewmh/ewmh.h>

/* Local includes */
#include <hints/ewmh/app.h>


                                                         /* Accessors */

/* Get the name of the window manager */
char* ewmh_fetch_net_wm_name(Display *display, Window window)
{
    return _ewmh_fetch_string_property(display, window, "_NET_WM_NAME");
}


/* Get the visible name of the client */
char* ewmh_fetch_net_wm_visible_name(Display *display, Window window)
{
    return _ewmh_fetch_string_property(display, window,
            "_NET_WM_VISIBLE_NAME");
}


/* Get the icon name for the window manager */
char* ewmh_fetch_net_wm_icon_name(Display *display, Window window)
{
    return _ewmh_fetch_string_property(display, window,
            "_NET_WM_ICON_NAME");
}


/* Get the visible icon name for the window manager */
char *ewmh_net_wm_visible_icon(Display *display, Window window)
{
    return _ewmh_fetch_string_property(display, window,
            "_NET_WM_VISIBLE_ICON_NAME");
}


/* Get the desktop index for a window */
uint32_t ewmh_fetch_net_wm_desktop(Display *display, Window window)
{
    return _ewmh_fetch_uint_property(display, window,
            "_NET_WM_DESKTOP");
}


/* Get the type associated with a window */
uint32_t ewmh_fetch_net_wm_window_type(Display *display, Window window,
        char **types_out, int max_types)
{
    Atom property = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_value = NULL;

    if (XGetWindowProperty(display, window,
                property, 0, max_types, False,
                AnyPropertyType, &actual_type, &actual_format,
                &nitems, &bytes_after, &prop_value) != Success) {
        return -1;
    }

    if (nitems > 0 && actual_type == XA_ATOM) {
        Atom *atoms = (Atom *)prop_value;
        for (unsigned long i = 0; i < nitems && i < max_types; ++i) {
            types_out[i] = XGetAtomName(display, atoms[i]);
        }
        XFree(prop_value);
        return nitems;  /* Number of types retrieved */
    }

    XFree(prop_value);
    return 0;   /* No types retrieved */
}


/* Get the state information for a window */
int ewmh_fetch_net_wm_state(Display *display, Window window,
        char **states_out, int max_states)
{
    Atom property = XInternAtom(display, "_NET_WM_STATE", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_value = NULL;

    if (XGetWindowProperty(display, window,
                property, 0, max_states, False,
                AnyPropertyType, &actual_type, &actual_format,
                &nitems, &bytes_after, &prop_value) != Success) {
        return -1;
    }

    if (nitems > 0 && actual_type == XA_ATOM) {
        Atom *atoms = (Atom *)prop_value;
        for (unsigned long i = 0; i < nitems && i < max_states; ++i) {
            states_out[i] = XGetAtomName(display, atoms[i]);
        }
        XFree(prop_value);
        return nitems;  /* Number of states retrieved */
    }

    XFree(prop_value);
    return 0;   /* No states retrieved */
}


/* Get the current allowed operations supported for a window */
int ewmh_fetch_net_wm_allowed_actions(Display *display, Window window,
        char **actions, int max_actions)
{
    Atom property = XInternAtom(display,
            "_NET_WM_ALLOWED_ACTIONS", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_value = NULL;

    if (XGetWindowProperty(display, window, property, 0, max_actions,
                False, AnyPropertyType, &actual_type, &actual_format,
                &nitems, &bytes_after, &prop_value) != Success) {
        return -1;
    }

    if (nitems > 0 && actual_type == XA_ATOM) {
        Atom *atoms = (Atom *)prop_value;
        for (unsigned long i = 0; i < nitems && i < max_actions; ++i) {
            actions[i] = XGetAtomName(display, atoms[i]);
        }
        XFree(prop_value);
        return nitems;  /* Number of actions retrieved */
    }

    XFree(prop_value);
    return 0;   /* No actions retrieved */
}


/* Get the strut property from a window */
void ewmh_fetch_net_wm_strut(Display *display, Window window,
        uint32_t *strut_out)
{
    Atom property = XInternAtom(display, "_NET_WM_STRUT", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_value = NULL;

    if (XGetWindowProperty(display, window,
                property, 0, 4, False,
                AnyPropertyType, &actual_type, &actual_format,
                &nitems, &bytes_after,
                &prop_value) == Success && nitems == 4) {
        memcpy(strut_out, prop_value, sizeof(uint32_t) * 4);
    }
    XFree(prop_value);
}


/* Get the partial strut property from a window */
void ewmh_fetch_net_wm_strut_partial(Display *display, Window window,
        uint32_t *strut_out)
{
    Atom property = XInternAtom(display,
            "_NET_WM_STRUT_PARTIAL", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_value = NULL;

    if (XGetWindowProperty(display, window,
                property, 0, 12, False,
                AnyPropertyType, &actual_type, &actual_format,
                &nitems, &bytes_after,
                &prop_value) == Success && nitems == 12) {
        memcpy(strut_out, prop_value, sizeof(uint32_t) * 12);
    }
    XFree(prop_value);
}


/* Get the icon geometry property of a window */
void ewmh_fetch_net_wm_icon_geometry(Display *display, Window window,
        uint32_t *geometry_out)
{
    Atom property = XInternAtom(display,
            "_NET_WM_ICON_GEOMETRY", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_value = NULL;

    if (XGetWindowProperty(display, window,
                property, 0, 4, False,
                AnyPropertyType, &actual_type, &actual_format,
                &nitems, &bytes_after,
                &prop_value) == Success && nitems == 4) {
        memcpy(geometry_out, prop_value, sizeof(uint32_t) * 4);
    }
    XFree(prop_value);
}


/* Get the array of possible icons for the client */
int ewmh_fetch_net_wm_icon(Display *display, Window window,
        uint32_t **icons_out, int max_icons)
{
    Atom property = XInternAtom(display, "_NET_WM_ICON", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_value = NULL;

    if (XGetWindowProperty(display, window,
                property, 0, max_icons * 2, False,
                AnyPropertyType, &actual_type, &actual_format,
                &nitems, &bytes_after, &prop_value) != Success) {
        return -1;
    }

    if (nitems > 0 && actual_type == XA_CARDINAL) {
        memcpy(*icons_out, prop_value, nitems * sizeof(uint32_t));
        XFree(prop_value);
        return nitems / 2; /* Each icon is defined by two 'uint32_t'
                              (width & height) */
    }

    XFree(prop_value);
    return 0;   /* No icons retrieved */
}


/* Get the PID of the client */
uint32_t ewmh_fetch_net_wm_pid(Display *display, Window window)
{
    return _ewmh_fetch_uint_property(display, window, "_NET_WM_PID");
}


/* Get the handled icons for a (iconified) window */
uint32_t ewmh_fetch_net_wm_handled_icons(Display *display,
        Window window)
{
    return _ewmh_fetch_uint_property(display, window,
            "_NET_WM_HANDLED_ICONS");
}


/* Get the user time for the last activity on a window */
uint32_t ewmh_fetch_net_wm_user_time(Display *display, Window window)
{
    return _ewmh_fetch_uint_property(display, window, "_NET_WM_USER_TIME");
}


/* Get the window XID where the clients set the user time */
Window ewmh_fetch_net_wm_user_time_window(Display *display,
        Window window)
{
    Atom property = XInternAtom(display,
            "_NET_WM_USER_TIME_WINDOW", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_value = NULL;
    Window time_window = 0;

    if (XGetWindowProperty(display, window,
                property, 0, 1, False,
                AnyPropertyType, &actual_type, &actual_format,
                &nitems, &bytes_after,
                &prop_value) == Success && nitems > 0) {
        time_window = *(Window *)prop_value;
    }

    XFree(prop_value);
    return time_window;
}


/* Get the frame extents of a window */
void ewmh_fetch_net_wm_frame_extents(Display *display, Window window,
        int *extents)
{
    Atom property = XInternAtom(display, "_NET_FRAME_EXTENTS", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_value = NULL;

    if (XGetWindowProperty(display, window,
                property, 0, 4, False,
                AnyPropertyType, &actual_type, &actual_format,
                &nitems, &bytes_after,
                &prop_value) == Success && nitems == 4) {
        memcpy(extents, prop_value, sizeof(int) * 4);
    }

    XFree(prop_value);
}


/* Get the preferred bypass compositor value of a window */
uint32_t ewmh_fetch_net_wm_bypass_compositor(Display *display,
        Window window)
{
    return _ewmh_fetch_uint_property(display, window,
            "_NET_WM_BYPASS_COMPOSITOR");
}


/* Get the opacity of a window */
uint32_t ewmh_fetch_net_wm_window_opacity(Display *display,
        Window window)
{
    return _ewmh_fetch_uint_property(display, window,
            "_NET_WM_WINDOW_OPACITY");
}


                                                          /* Mutators */

/* Set the window manager title */
void ewmh_alter_net_wm_name(Display *display, Window window,
        const char *name)
{
    _ewmh_alter_string_property(display, window, "_NET_WM_NAME", name);
}


/* Set the visible name property of a window */
void ewmh_alter_net_wm_visible_name(Display *display, Window window,
        const char *visible_name)
{
    _ewmh_alter_string_property(display, window,
            "_NET_WM_VISIBLE_NAME", visible_name);
}


/* Set the icon name property of a window */
void ewmh_alter_net_wm_icon_name(Display *display, Window window,
        const char *icon_name)
{
    _ewmh_alter_string_property(display, window,
            "_NET_WM_ICON_NAME", icon_name);
}


/* Set the visible icon name property of a window */
void ewmh_alter_net_wm_visible_icon_name(Display *display,
        Window window, const char *visible_icon_name)
{
    _ewmh_alter_string_property(display, window,
            "_NET_WM_VISIBLE_ICON_NAME", visible_icon_name);
}


/* Set the desktop index a window is in */
void ewmh_alter_net_wm_desktop(Display *display, Window window,
        uint32_t desktop)
{
    Atom property = XInternAtom(display, "_NET_WM_DESKTOP", False);
    XChangeProperty(display, window,
            property, XA_CARDINAL,
            32, PropModeReplace,
            (unsigned char *) &desktop, 1);
}


/* Set the window type property for a window */
void ewmh_alter_net_wm_window_type(Display *display, Window window,
        const char **types, int num_types)
{
    Atom property = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom *atoms = malloc(num_types * sizeof(Atom));

    for (int i = 0; i < num_types; ++i) {
        atoms[i] = XInternAtom(display, types[i], False);
    }

    XChangeProperty(display, window,
            property, XA_ATOM,
            32, PropModeReplace,
            (unsigned char *) atoms, num_types);
    free(atoms);
}


/* Set the state of a window */
void ewmh_alter_net_wm_state(Display *display, Window window,
        const char **states, int num_states)
{
    Atom property = XInternAtom(display, "_NET_WM_STATE", False);
    Atom *atoms = malloc(num_states * sizeof(Atom));

    for (int i = 0; i < num_states; ++i) {
        atoms[i] = XInternAtom(display, states[i], False);
    }

    XChangeProperty(display, window, property,
            XA_ATOM,
            32, PropModeReplace,
            (unsigned char *) atoms, num_states);
    free(atoms);
}


/* Set the allowed operations for a window  */
void ewmh_alter_net_wm_allowed_actions(Display *display, Window window,
        const char **actions, int num_actions)
{
    Atom property = XInternAtom(display,
            "_NET_WM_ALLOWED_ACTIONS", False);
    Atom *atoms = malloc(num_actions * sizeof(Atom));

    for (int i = 0; i < num_actions; ++i) {
        atoms[i] = XInternAtom(display, actions[i], False);
    }

    XChangeProperty(display, window, property,
            XA_ATOM,
            32, PropModeReplace,
            (unsigned char *) atoms, num_actions);
    free(atoms);
}


/* Set the strut property for a window */
void ewmh_alter_net_wm_strut(Display *display, Window window,
        uint32_t left, uint32_t right, uint32_t top, uint32_t bottom)
{
    Atom property = XInternAtom(display, "_NET_WM_STRUT", False);
    uint32_t strut[4] = { left, right, top, bottom };

    XChangeProperty(display, window, property,
            XA_CARDINAL,
            32, PropModeReplace,
            (unsigned char *) strut, 4);
}


/* Set the partial strut property for a window */
void ewmh_alter_net_wm_strut_partial(Display *display, Window window,
        uint32_t left, uint32_t right, uint32_t top, uint32_t bottom,
        uint32_t left_start_y, uint32_t left_end_y,
        uint32_t right_start_y, uint32_t right_end_y,
        uint32_t top_start_x, uint32_t top_end_x,
        uint32_t bottom_start_x, uint32_t bottom_end_x)
{
    Atom property = XInternAtom(display,
            "_NET_WM_STRUT_PARTIAL", False);
    uint32_t strut[12] = { left, right, top, bottom,
        left_start_y, left_end_y,
        right_start_y, right_end_y,
        top_start_x, top_end_x,
        bottom_start_x, bottom_end_x };

    XChangeProperty(display, window, property,
            XA_CARDINAL,
            32, PropModeReplace,
            (unsigned char *) strut, 12);
}


/* Set icon geometry in case of collision with an iconfied window */
void ewmh_alter_net_wm_icon_geometry(Display *display, Window window,
        int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    Atom property = XInternAtom(display,
            "_NET_WM_ICON_GEOMETRY", False);
    uint32_t geometry[4] = { x, y, width, height };

    XChangeProperty(display, window, property,
            XA_CARDINAL,
            32, PropModeReplace,
            (unsigned char *) geometry, 4);
}


/* Set the icon property for the client */
void ewmh_alter_net_wm_icon(Display *display, Window window,
        uint32_t **icons, int num_icons)
{
    Atom property = XInternAtom(display, "_NET_WM_ICON", False);

    XChangeProperty(display, window, property,
            XA_CARDINAL,
            32, PropModeReplace,
            (unsigned char *) icons, num_icons * 2);
}


/* Set the PID for the window manager */
void ewmh_alter_net_wm_pid(Display *display, Window window,
        uint32_t pid)
{
    Atom property = XInternAtom(display, "_NET_WM_PID", False);

    XChangeProperty(display, window, property,
            XA_CARDINAL,
            32, PropModeReplace,
            (unsigned char *) &pid, 1);
}


/* Set the handled icons for a (iconified) window */
void ewmh_alter_net_wm_handled_icons(Display *display, Window window)
{
    Atom property = XInternAtom(display,
            "_NET_WM_HANDLED_ICONS", False);
    uint32_t dummy = 1; /* Dummy value for the property */

    XChangeProperty(display, window, property,
            XA_CARDINAL,
            32, PropModeReplace,
            (unsigned char *) &dummy, 1);
}


/* Set the user time for the last activity on a window */
void ewmh_alter_net_wm_user_time(Display *display, Window window,
        uint32_t time)
{
    Atom property = XInternAtom(display, "_NET_WM_USER_TIME", False);

    XChangeProperty(display, window, property,
            XA_CARDINAL,
            32, PropModeReplace,
            (unsigned char *) &time, 1);
}


/* Set the XID of the window where the client sets the user time */
void ewmh_alter_net_wm_user_time_window(Display *display, Window window,
        Window time_window)
{
    Atom property = XInternAtom(display,
            "_NET_WM_USER_TIME_WINDOW", False);

    XChangeProperty(display, window, property,
            XA_WINDOW,
            32, PropModeReplace,
            (unsigned char *) &time_window, 1);
}


/* Set the extents of the window frame */
void ewmh_alter_net_wm_frame_extents(Display *display, Window window,
        int32_t left, int32_t right, int32_t top, int32_t bottom)
{
    Atom property = XInternAtom(display, "_NET_FRAME_EXTENTS", False);
    int extents[4] = {left, right, top, bottom};
    XChangeProperty(display, window,
            property, XA_CARDINAL,
            32, PropModeReplace,
            (unsigned char *) extents, 4);
}


/* Set the '_NET_WM_BYPASS_COMPOSITOR' property of a window */
void ewmh_alter_net_wm_bypass_compositor(Display *display,
        Window window, uint32_t bypass)
{
    Atom property = XInternAtom(display,
            "_NET_WM_BYPASS_COMPOSITOR", False);

    XChangeProperty(display, window, property,
            XA_CARDINAL,
            32, PropModeReplace,
            (unsigned char *) &bypass, 1);
}


/* Set the opacity value of a window */
void ewmh_alter_net_wm_window_opacity(Display *display, Window window,
        uint32_t opacity)
{
    Atom property;

    if (opacity > 0xFFFFFFFF) {
        opacity = 0xFFFFFFFF;   /* Cap it to the maximum */
    }

    property = XInternAtom(display, "_NET_WM_WINDOW_OPACITY", False);
    XChangeProperty(display, window, property,
            XA_CARDINAL,
            32, PropModeReplace,
            (unsigned char *) &opacity, 1);
}
