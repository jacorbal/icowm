/**
 * @file emwh.c
 *
 * @brief Implementation of properties managment functions per EWMH
 */

#include <stdlib.h>
#include <string.h>

/* X11 includes */
#include <X11/Xlib.h>   /* Display, Window, Atom */
#include <X11/Xatom.h>  /* XA_* */

/* Local includes */
#include <hints/ewmh.h>
#include <hints/property.h>
#include <hints/state.h>


/* Assign '_NET_WM_NAME' */
int ewmh_set_wm_name(Display *display, Window window, const char *name)
{
    return property_set_string_utf8(display, window,
            "_NET_WM_NAME", name);
}


/* Retrieve '_NET_WM_NAME' */
char *ewmh_get_wm_name(Display *display, Window window)
{
    return property_get_string_utf8(display, window, "_NET_WM_NAME");
}


/* Assign '_NET_WM_VISIBLE_NAME' */
int ewmh_set_wm_visible_name(Display *display, Window window,
        const char *visible_name)
{
    return property_set_string_utf8(display, window,
            "_NET_WM_VISIBLE_NAME", visible_name);
}


/* Retrieve '_NET_WM_VISIBLE_NAME' */
char *ewmh_get_wm_visible_name(Display *display, Window window)
{
    return property_get_string_utf8(display, window,
            "_NET_WM_VISIBLE_NAME");
}


/* Assign '_NET_WM_ICON_NAME' */
int ewmh_set_wm_icon_name(Display *display, Window window,
        const char *icon_name)
{
    return property_set_string_utf8(display, window,
            "_NET_WM_ICON_NAME", icon_name);
}


/* Retrieve '_NET_WM_ICON_NAME' */
char *ewmh_get_wm_icon_name(Display *display, Window window)
{
    return property_get_string_utf8(display, window,
            "_NET_WM_ICON_NAME");
}


/* Assign '_NET_WM_VISIBLE_ICON_NAME' */
int ewmh_set_wm_visible_icon_name(Display *display, Window window,
        const char *visible_icon_name)
{
    return property_set_string_utf8(display, window,
            "_NET_WM_VISIBLE_ICON_NAME", visible_icon_name);
}


/* Retrieve '_NET_WM_VISIBLE_ICON_NAME' */
char *ewmh_get_wm_visible_icon_name(Display *display, Window window)
{
    return property_get_string_utf8(display, window,
            "_NET_WM_VISIBLE_ICON_NAME");
}


/* Assign '_NET_WM_PID' */
int ewmh_set_wm_pid(Display *display, Window window, unsigned long pid)
{
    return property_set_cardinal(display, window, "_NET_WM_PID", pid);
}


/* Retrieve '_NET_WM_PID' */
unsigned long ewmh_get_wm_pid(Display *display, Window window)
{
    return property_get_cardinal(display, window, "_NET_WM_PID");
}

/* Assign '_NET_ACTIVE_WINDOW' */
int ewmh_set_active_window(Display *display, Window window, Window active_window)
{
    return property_set_window(display, window,
            "_NET_ACTIVE_WINDOW", active_window);
}


/* Retrieve '_NET_ACTIVE_WINDOW' */
Window ewmh_get_active_window(Display *display, Window window)
{
    return property_get_window(display, window,
            "_NET_ACTIVE_WINDOW");
}


/* Assign '_NET_WM_USER_TIME' */
int ewmh_set_wm_user_time(Display *display, Window window, Time time)
{
    return property_set_time(display, window,
            "_NET_WM_USER_TIME", time);
}


/* Retrieve '_NET_WM_USER_TIME' */
Time ewmh_get_wm_user_time(Display *display, Window window)
{
    return property_get_time(display, window, "_NET_WM_USER_TIME");
}


/* Assign '_NET_WM_USER_TIME_WINDOW' */
int ewmh_set_wm_user_time_window(Display *display, Window window,
        Window time_window)
{
    return property_set_time(display, window,
            "_NET_WM_USER_TIME_WINDOW", time_window);
}


/* Retrieve '_NET_WM_USER_TIME_WINDOW' */
Window ewmh_get_wm_user_time_window(Display *display, Window window)
{
    return property_get_time(display, window,
            "_NET_WM_USER_TIME_WINDOW");
}


/* Assign '_NET_WM_STRUT' */
int ewmh_set_strut(Display *display, Window window, long *strut)
{
    Atom atoms[4] = {
        (Atom) strut[0],
        (Atom) strut[1],
        (Atom) strut[2],
        (Atom) strut[3]
    };

    return property_set_atom_array(display, window,
            "_NET_WM_STRUT", atoms, 4);
}


/* Retrieve '_NET_WM_STRUT' */
int ewmh_get_strut(Display *display, Window window, long **strut_out)
{
    unsigned long nitems = 0;
    Atom *strut = property_get_atom_array(display, window,
            "_NET_WM_STRUT", &nitems);

    if (strut == NULL || nitems != 4) {
        return -1;  /* Error or property not set */
    }

    *strut_out = malloc(4 * sizeof(long));
    memcpy(*strut_out, strut, 4 * sizeof(long));

    XFree(strut);
    return 0;   /* Success */
}


/* Assign '_NET_WM_DESKTOP' */
int ewmh_set_desktop(Display *display, Window window, long desktop)
{
    return property_set_cardinal(display, window,
            "_NET_WM_DESKTOP", (unsigned long)desktop);
}


/* Retrieve '_NET_WM_DESKTOP' */
int ewmh_get_desktop(Display *display, Window window,
        long *desktop_out)
{
    *desktop_out = (long) property_get_cardinal(display, window,
            "_NET_WM_DESKTOP");

    return (*desktop_out == 0) ? -1 : 0;    /* Error if value is 0 */
}


/* Assign '_NET_WM_OPACITY' */
int ewmh_set_wm_opacity(Display *display, Window window,
        unsigned long opacity)
{
    return property_set_cardinal(display, window,
            "_NET_WM_OPACITY", opacity);
}


/* Retrieve the _NET_WM_OPACITY property */
int ewmh_get_wm_opacity(Display *display, Window window,
        unsigned long *opacity_out)
{
    *opacity_out = property_get_cardinal(display, window,
            "_NET_WM_OPACITY");

    return (*opacity_out == 0) ? -1 : 0;    /* Error if value is 0 */
}


/* Assign '_NET_WM_BYPASS_COMPOSITOR' */
int ewmh_set_wm_bypass_compositor(Display *display, Window window,
        int bypass)
{
    return property_set_int(display, window,
            "_NET_WM_BYPASS_COMPOSITOR", bypass);
}


/* Retrieve '_NET_WM_BYPASS_COMPOSITOR' */
int ewmh_get_wm_bypass_compositor(Display *display, Window window,
        int *bypass_out)
{
    *bypass_out = property_get_int(display, window,
            "_NET_WM_BYPASS_COMPOSITOR");

    return (*bypass_out == 0) ? -1 : 0; /* Error if the value is 0 */
}


/* Assign '_NET_WM_FRAME_EXTENTS' */
int ewmh_set_wm_frame_extents(Display *display, Window window,
        long *extents)
{
    return property_set_atom_array(display, window,
            "_NET_WM_FRAME_EXTENTS", (Atom *) extents, 4);
}


/* Retrieve '_NET_WM_FRAME_EXTENTS' */
int ewmh_get_wm_frame_extents(Display *display, Window window,
        long *extents_out)
{
    unsigned long nitems = 0;
    Atom *extents = property_get_atom_array(display, window,
            "_NET_WM_FRAME_EXTENTS", &nitems);

    if (extents == NULL || nitems != 4) {
        return -1;  /* Error or property not set */
    }

    memcpy(extents_out, extents, 4 * sizeof(long));
    XFree(extents);
    return 0;   /* Success */
}


                                           /* Types, States & Actions */

/* Retrieve '_NET_WM_ALLOWED_ACTIONS' */
int ewmh_get_allowed_actions(Display *display, Window window,
        Atom **actions_out)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_return = NULL;

    int status = XGetWindowProperty(display, window,
            XInternAtom(display, "_NET_WM_ALLOWED_ACTIONS", False),
            0, 1024, False, AnyPropertyType,
            &actual_type, &actual_format,
            &nitems, &bytes_after,
            &prop_return);

    if (status != Success || nitems == 0) {
        return 0;
    }

    *actions_out = (Atom *) prop_return;
    return (int) nitems;
}


/* Retrieve '_NET_WM_STATE' */
int ewmh_get_window_state(Display *display, Window window,
        Atom **states_out)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_return = NULL;

    int status = XGetWindowProperty(display, window,
            XInternAtom(display, "_NET_WM_STATE", False),
            0, 1024, False, AnyPropertyType,
            &actual_type, &actual_format,
            &nitems, &bytes_after,
            &prop_return);

    if (status != Success || nitems == 0) {
        return 0;
    }

    *states_out = (Atom *) prop_return;
    return (int) nitems;
}


/* Retrieve '_NET_WM_WINDOW_TYPE' */
int ewmh_get_window_type(Display *display, Window window,
        Atom **types_out)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_return = NULL;

    int status = XGetWindowProperty(display, window,
            XInternAtom(display, "_NET_WM_WINDOW_TYPE", False),
            0, 1024, False, AnyPropertyType,
            &actual_type, &actual_format, &nitems, &bytes_after,
            &prop_return);

    if (status != Success || nitems == 0) {
        return 0;
    }

    *types_out = (Atom *) prop_return;
    return (int) nitems;
}


/* Assign '_NET_WM_STATE' */
int ewmh_set_wm_state(Display *display, Window window,
        Atom *states, unsigned long nstates)
{
    Atom property = XInternAtom(display, "_NET_WM_STATE", False);
    int status;
    
    if (property == None) {
        return -1;
    }

    status = XChangeProperty(display, window, property,
            XA_ATOM, 32, PropModeReplace, 
            (unsigned char *) states, (int) nstates);

    /* Check if changes took place */
    return (status == Success) ? 0 : -1;
}


/* Retrieve '_NET_WM_STATE' */
Atom *ewmh_get_wm_state(Display *display, Window window,
        unsigned long *nitems)
{
    return get_window_state(display, window, nitems);
}


/* Add a state to '_NET_WM_STATE' */
int ewmh_add_wm_state(Display *display, Window window, Atom state)
{
    return add_window_state(display, window, state);
}


/* Remove a state from '_NET_WM_STATE' */
int ewmh_remove_wm_state(Display *display, Window window, Atom state)
{
    return remove_window_state(display, window, state);
}


/* Assign '_NET_WM_ALLOWED_ACTIONS' */
int ewmh_set_wm_allowed_actions(Display *display, Window window,
        Atom *actions, unsigned long nactions)
{
    return property_set_atom_array(display, window,
            "_NET_WM_ALLOWED_ACTIONS", actions, nactions);
}


/* Retrieve '_NET_WM_ALLOWED_ACTIONS' */
Atom *ewmh_get_wm_allowed_actions(Display *display, Window window,
        unsigned long *nactions)
{
    return property_get_atom_array(display, window,
            "_NET_WM_ALLOWED_ACTIONS", nactions);
}


/* Assign '_NET_WM_WINDOW_TYPE' */
int ewmh_set_wm_window_type(Display *display, Window window,
        Atom *types, unsigned long ntypes)
{
    return property_set_atom_array(display, window,
            "_NET_WM_WINDOW_TYPE", types, ntypes);
}


/* Retrieve '_NET_WM_WINDOW_TYPE' */
Atom *ewmh_get_wm_window_type(Display *display, Window window,
        unsigned long *ntypes)
{
    return property_get_atom_array(display, window,
            "_NET_WM_WINDOW_TYPE", ntypes);
}


/* Add an action to '_NET_WM_ALLOWED_ACTIONS' */
int ewmh_add_wm_allowed_action(Display *display, Window window,
        Atom action)
{
    return add_atom_to_property(display, window,
            "_NET_WM_ALLOWED_ACTIONS", action);
}


/* Add a type to '_NET_WM_WINDOW_TYPE' */
int ewmh_add_wm_window_type(Display *display, Window window, Atom type)
{
    return add_atom_to_property(display, window,
            "_NET_WM_WINDOW_TYPE", type);
}


/* Remove an action from '_NET_WM_ALLOWED_ACTIONS' */
int ewmh_remove_wm_allowed_action(Display *display, Window window,
        Atom action)
{
    return remove_atom_from_property(display, window,
            "_NET_WM_ALLOWED_ACTIONS", action);
}


/* Remove a type from '_NET_WM_WINDOW_TYPE' */
int ewmh_remove_wm_window_type(Display *display, Window window,
        Atom type)
{
    return remove_atom_from_property(display, window,
            "_NET_WM_WINDOW_TYPE", type);
}
