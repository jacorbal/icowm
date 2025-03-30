/**
 * @file ewmh.c
 *
 * @brief Implementation of EWMH functions
 */

/* System includes */
#include <stdbool.h>    /* bool, false, true */
#include <stdlib.h>     /* NULL, free, malloc, size_t */
#include <sys/types.h>  /* pid_t */

/* X11 includes */
#include <X11/Xlib.h>   /* Window, Display, Pixmap */
#include <X11/Xatom.h>  /* Atom */

/* Utils includes */
#include <utils/safestr.h>

/* Local includes */
#include <ewmh.h>


/* Ping an EWMH-compliant window to check if it is responding */
bool ewmh_ping_window(Display *display, Window window)
{
    Atom ping_atom = XInternAtom(display, "_NET_PING", False);
    XClientMessageEvent event;

    if (ping_atom == None){
        return false;
    }

    /* Prepare message for the event */
    event.type = ClientMessage;
    event.window = window;
    event.message_type = ping_atom;
    event.format = 32;

    /* Extra information could be sent */
    event.data.l[0] = CurrentTime;
    event.data.l[1] = (long) window;

    /* Return true if the event was accepted for sending */
    return (XSendEvent(display, window, False, NoEventMask,
                (XEvent *) &event) == 1);
}


/* Set the icon for an EWMH-compliant window */
int ewmh_set_window_icon(Display *display, Window window, Pixmap *icon,
        int icon_count)
{
    if (icon == NULL || icon_count <= 0) {
        return -1;
    }

    return XChangeProperty(display, window,
            XInternAtom(display, "_NET_WM_ICON", False),
            XA_CARDINAL, 32,
            PropModeReplace,
            (unsigned char *) icon,
            icon_count);
}


/* Set the process ID for an EWMH-compliant window */
int ewmh_set_window_pid(Display *display, Window window, pid_t pid)
{
    Atom atom = XInternAtom(display, "_NET_WM_PID", False);

    if (atom == None) {
        return -2;
    }

    return XChangeProperty(display, window,
            atom,
            XA_CARDINAL, 32,
            PropModeReplace,
            (unsigned char *) &pid, 1);
}


/* Set the desktop number for an EWMH-compliant window */
int ewmh_set_window_desktop(Display *display, Window window,
        int desktop)
{
    Atom atom = XInternAtom(display, "_NET_WM_DESKTOP", False);

    if (atom == None) {
        return -2;
    }

    return XChangeProperty(display, window,
            atom,
            XA_CARDINAL, 32,
            PropModeReplace,
            (unsigned char *) &desktop, 1);
}


/* Set the user time for an EWMH-compliant window */
int ewmh_set_window_user_time(Display *display, Window window,
        Time time)
{
    Atom atom = XInternAtom(display, "_NET_WM_USER_TIME", False);

    if (atom == None) {
        return -2;
    }

    return XChangeProperty(display, window,
            atom,
            XA_CARDINAL, 32,
            PropModeReplace,
            (unsigned char *) &time, 1);
}


/* Set the user time window for an EWMH-compliant window */
int ewmh_set_window_user_time_window(Display *display, Window window,
        Window user_time_window)
{
    Atom atom = XInternAtom(display, "_NET_WM_USER_TIME_WINDOW", False);

    if (atom == None) {
        return -2;
    }

    return XChangeProperty(display, window,
            atom,
            XA_WINDOW, 32,
            PropModeReplace,
            (unsigned char *) &user_time_window, 1);
}


/* Set the opacity for an EWMH-compliant window */
int ewmh_set_window_opacity(Display *display, Window window,
        unsigned long int opacity)
{
    Atom atom;

    /* Validate opacity */
    if (opacity > 0xffffffff) {
        opacity = 0xffffffff;   /* Cap it at maximum opacity */
    }

    atom = XInternAtom(display, "_NET_WM_WINDOW_OPACITY", False);
    if (atom == None) {
        return -2;
    }

    return XChangeProperty(display, window,
            atom,
            XA_CARDINAL, 32,
            PropModeReplace,
            (unsigned char *) &opacity, 1);
}


/* Set the window actions for an EWMH-compliant window */
int ewmh_set_window_actions(Display *display, Window window,
        const char **actions, int action_count)
{
    Atom *atoms;
    Atom atom_action;
    int result;

    atom_action = XInternAtom(display, "_NET_WM_ACTIONS", False);
    if (atom_action == None) {
        return -2;
    }

    atoms = malloc(sizeof(Atom) * (size_t) action_count);
    if (atoms == NULL) {
        return -1;
    }

    for (int i = 0; i < action_count; ++i) {
        atoms[i] = XInternAtom(display, actions[i], False);
    }

    result = XChangeProperty(display, window,
            atom_action,
            XA_ATOM, 32,
            PropModeReplace,
            (unsigned char *) atoms, action_count);

    free(atoms);
    return result;
}


/* Set the frame extents for an EWMH-compliant window */
int ewmh_set_window_frame_extents(Display *display, Window window,
        int *extents)
{
    Atom atom = XInternAtom(display, "_NET_FRAME_EXTENTS", False);

    if (atom == None) {
        return -2;
    }

    return XChangeProperty(display, window,
            atom,
            XA_CARDINAL, 32,
            PropModeReplace,
            (unsigned char *) extents, 4);
}


/* Indicate that a window is shown */
int ewmh_set_window_shown(Display *display, Window window)
{
    Atom atom = XInternAtom(display, "_NET_WM_SHOWN", False);

    if (atom == None) {
        return -2;
    }

    return XChangeProperty(display, window,
            atom,
            XA_ATOM, 32,
            PropModeReplace,
            (unsigned char *) &atom, 1);
}


/* Indicate that a window is being moved or resized */
int ewmh_set_window_moveresize(Display *display, Window window)
{
    Atom atom = XInternAtom(display, "_NET_WM_MOVERESIZE", False);

    if (atom == None) {
        return -2;
    }

    return XChangeProperty(display, window,
            atom,
            XA_ATOM, 32,
            PropModeReplace,
            (unsigned char *) &atom, 1);
}


/* General function to set the type for an EWMH-compliant window */
int ewmh_set_window_type(Display *display, Window window,
        const char *type)
{
    Atom atom_type;

    if (type == NULL) {
        return -1;
    }

    atom_type = XInternAtom(display, type, False);
    if (atom_type == None) {
        return -2;
    }

    return XChangeProperty(display, window,
            XInternAtom(display, "_NET_WM_WINDOW_TYPE", False),
            XA_ATOM, 32,
            PropModeReplace,
            (unsigned char *) &atom_type, 1);
}


/* General function to set a string property for an EWMH-compliant
 * window */
int ewmh_set_window_state_string(Display *display, Window window,
        const char *state, const char *str)
{
    Atom atom;

    if (str == NULL || str[0] == '\0') {
        return -1;
    }

    atom = XInternAtom(display, state, False);
    if (atom == None) {
        return -2;
    }

    return XChangeProperty(display, window,
            atom,
            XA_STRING, 8,
            PropModeReplace,
            (unsigned char *) str,
            (int) safe_strlen(str));
}


/* Check weather a state is set or not */
bool ewmh_is_state_set(Display *display, Window window,
        const char *state)
{
    Atom atom_state = XInternAtom(display, state, False);
    Atom *current_states = NULL;
    unsigned long n_items;
    int actual_format;
    Atom actual_type;
    int status;

    if (state == NULL) {
        return false;
    }

    status = XGetWindowProperty(display, window,
            XInternAtom(display, "_NET_WM_STATE", False),
            0, 1024, False, AnyPropertyType,
            &actual_type, &actual_format,
            &n_items, NULL,
            (unsigned char **) &current_states);

    if (status == Success && n_items > 0 && current_states != NULL) {
        for (unsigned long i = 0; i < n_items; ++i) {
            if (current_states[i] == atom_state) {
                /* Status was found */
                XFree(current_states);
                return true;
            }
        }
    }

    if (current_states) {
        XFree(current_states);
    }

    /* Status wasn't found */
    return false;
}


/* General function to set multiple states for an EWMH-compliant window */
int ewmh_set_window_state_multiple(Display *display, Window window,
        Atom *states, int state_count)
{
    Atom atom;

    if (display == NULL || window == 0) {
        return -1;
    }

    atom = XInternAtom(display, "_NET_WM_STATE", False);
    if (atom == None) {
        return -2;
    }

    return XChangeProperty(display, window,
            atom,
            XA_ATOM,
            32,
            PropModeReplace,
            (unsigned char *) states,
            state_count);
}


/* General function to set a single state for an EWMH-compliant window */
int ewmh_set_window_state(Display *display, Window window,
        const char *state)
{
    Atom atom_state = XInternAtom(display, state, False);

    if (atom_state == None) {
        return -2;
    }

    return ewmh_set_window_state_multiple(display, window,
            &atom_state, 1);
}


/* General function to unset a single state for an EWMH-compliant window */
int ewmh_unset_window_state(Display *display, Window window,
        const char *state)
{
    Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom atom_null;

    if (wm_state == None) {
        return -2;
    }

    /* Remove the state */
    atom_null = None;
    return XChangeProperty(display, window,
            wm_state,
            XA_ATOM, 32,
            PropModeReplace,
            (unsigned char *) &atom_null, 0);
}


/* General function to unset a single state for an EWMH-compliant window
 * by generating a new list of states excluding the one to unset */
int ewmh_unset_window_state_adjust(Display *display, Window window,
        const char *state)
{
    Atom atom_state;
    Atom *current_states;
    Atom actual_type;
    unsigned long int n_items;
    int actual_format;
    int status;

    if (display == NULL || window == 0) {
        return -1;
    }

    /* If the state is not set, there's nothing to unset */
    if (!ewmh_is_state_set(display, window, state)) {
        return -3;
    }

    /* Get current state */
    atom_state = XInternAtom(display, state, False);
    if (atom_state == None) {
        return -2;
    }

    /* Get current '_NET_WM_STATE' property */
    status = XGetWindowProperty(display, window,
            XInternAtom(display,
                "_NET_WM_STATE", False),
            0, 1024, False, AnyPropertyType,
            &actual_type, &actual_format,
            &n_items, NULL,
            (unsigned char **) &current_states);

    /* Check if there are current states */
    if (status == Success && n_items > 0) {
        unsigned long int new_state_count = 0;
        Atom *new_states;

        new_states = malloc(sizeof(Atom) * n_items);
        if (new_states == NULL) {
            if (current_states) {
                XFree(current_states);
            }
            return 1;
        }

        /* Get the state that we want to remove */
        for (unsigned long i = 0; i < n_items; ++i) {
            if (current_states[i] != atom_state) {
                new_states[new_state_count++] = current_states[i];
            }
        }

        /* Set the new state list, only if there are changes */
        if (new_state_count != n_items) {
            if (new_state_count > 0) {
                XChangeProperty(display, window,
                        XInternAtom(display, "_NET_WM_STATE", False),
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        (unsigned char *) new_states,
                        (int) new_state_count);
            } else {
                XDeleteProperty(display, window, XInternAtom(display,
                            "_NET_WM_STATE", False));
            }
        }
        free(new_states);
    }

    if (current_states) {
        XFree(current_states);
    }

    return 0;
}
