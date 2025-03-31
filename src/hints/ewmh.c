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
#include <X11/Xlib.h>   /* Window, Pixmap */
#include <X11/Xatom.h>  /* Atom */

/* Utils includes */
#include <utils/safestr.h>

/* Project includes */
#include <window.h>

/* Local includes */
#include <hints/ewmh.h>


/* Send a 'ClientMessage' event to a specified window with a variable
 * number of long integer data items */
int ewmh_send_client_messages(window_td *window,
        const char* msg_type, long int* data, unsigned int data_len)
{
    unsigned int events_to_send;
    int sent = 0;
    Atom atom;

    if (data_len == 0) {
        return -1;
    }

    atom = XInternAtom(window->display, msg_type, False);
    if (atom == None) {
        return -2;
    }

    /* Number of events we need: 5 elements for event (34 bits) */
    events_to_send = (data_len + 4) / 5;

    for (unsigned int i = 0; i < events_to_send; ++i) {
        XEvent event;
        event.type = ClientMessage;
        event.xclient.window = window->xwindow;
        event.xclient.message_type = atom;
        event.xclient.format = 32;  /* long int: 32 bits */

        for (unsigned int j = 0; j < 5; ++j) {
            if (i * 5 + j < data_len) {
                event.xclient.data.l[j] = data[i * 5 + j];
            } else {
                /* Fill with zeroes if there's no more data */
                event.xclient.data.l[j] = 0;
            }
        }

        /* Send the event to the window manager */
        sent += XSendEvent(window->display, window->xwindow,
                False, NoEventMask, &event);
    }

    return sent;
}


/* Send a 'ClientMessage' event to the specified window */
bool ewmh_send_client_message(window_td *window, const char *state)
{
    Atom close_atom = XInternAtom(window->display, state, False);
    long int data[2] = { CurrentTime, (long int) window->xwindow };

    if (close_atom == None) {
        return false;
    }

    return (ewmh_send_client_messages(window, state, data, 2) > 0);
}


/* Update a property on the specified window */
int ewmh_update_window_state(window_td *window, const char* state_type,
        long int *data, unsigned int data_len)
{
    Atom state_atom = XInternAtom(window->display, state_type, False);
    if (state_atom == None) {
        return -2;
    }

    /* Send message to window manager */
    if (ewmh_send_client_messages(window, state_type,
                data, data_len) <= 0) {
        return -1;
    }

    return ewmh_update_window_property(window, "_NET_WM_STATE",
            data, XA_ATOM, data_len);
}


/* Update a property on the specified window */
int ewmh_update_window_property(window_td *window,
        const char* property_name, void* value, int value_type,
        unsigned int data_len)
{
    Atom property_atom;
    int status;

    /* Check the property name is not null and not empty */
    if (property_name == NULL) {
        return -1;
    }

    property_atom = XInternAtom(window->display,
            property_name, False);
    if (property_atom == None) {
        return -2;
    }

    /* Change the property */
    status = XChangeProperty(window->display, window->xwindow,
            property_atom,
            (unsigned long int) value_type, 32,
            PropModeReplace,
            (unsigned char *) value, (int) data_len);

    /* Send the client message */
    if (status == 0) {
        long int *data = NULL;
        
        if (value_type == XA_ATOM) {
            data = (long int *) value;
        }

        ewmh_send_client_messages(window, property_name,
                data, data_len);
    }

    return status;
}


/* General function to set string property for an EWMH-compliant window */
int ewmh_set_window_state_string(window_td *window, const char *state,
        const char *str)
{
    return ewmh_update_window_property(window, state,
            (void*) str, XA_STRING, (unsigned int) safe_strlen(str));
}



/* General function to set the type for an EWMH-compliant window */
int ewmh_set_window_type(window_td *window, const char *type)
{
    Atom atom_type;

    if (type == NULL) {
        return -1;
    }

    atom_type = XInternAtom(window->display, type, False);
    if (atom_type == None) {
        return -2;
    }

    return ewmh_update_window_property(window, "_NET_WM_WINDOW_TYPE",
            &atom_type, XA_ATOM, 1);
}


/* Check weather a state is set or not */
bool ewmh_is_state_set(window_td *window, const char *state)
{
    Atom atom_state = XInternAtom(window->display, state, False);
    Atom *current_states = NULL;
    unsigned long int n_items;
    int actual_format;
    Atom actual_type;
    int status;

    if (state == NULL) {
        return false;
    }

    status = XGetWindowProperty(window->display, window->xwindow,
            XInternAtom(window->display, "_NET_WM_STATE", False),
            0, 1024, False, AnyPropertyType,
            &actual_type, &actual_format,
            &n_items, NULL,
            (unsigned char **) &current_states);

    if (status == Success && n_items > 0 && current_states != NULL) {
        for (unsigned long int i = 0; i < n_items; ++i) {
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


/* Set multiple window states for an EWMH-compliant window */
int ewmh_set_window_state_multiple(window_td *window, Atom *states,
        unsigned int state_count)
{
    if (window == NULL || window->display == NULL ||
            window->xwindow == 0) {
        return -1;
    }

    return ewmh_update_window_property(window, "_NET_WM_STATE",
            states, XA_ATOM, state_count);
}


/* General function to set a single state for an EWMH-compliant window */
int ewmh_set_window_state(window_td *window, const char *state)
{
    Atom atom_state = XInternAtom(window->display, state, False);

    if (atom_state == None) {
        return -2;
    }

    return ewmh_set_window_state_multiple(window, &atom_state, 1);
}


/* General function to unset a state for an EWMH-compliant window */
int ewmh_unset_window_state(window_td *window, const char *state)
{
    Atom wm_state = XInternAtom(window->display,
            "_NET_WM_STATE", False);
    Atom atom_null;

    if (wm_state == None) {
        return -2;
    }

    /* Remove the state */
    atom_null = None;
    return XChangeProperty(window->display, window->xwindow,
            wm_state,
            XA_ATOM, 32,
            PropModeReplace,
            (unsigned char *) &atom_null, 0);
}


/* General function to unset a single state for an EWMH-compliant window
 * by generating a new list of states excluding the one to unset */
int ewmh_unset_window_state_adjust(window_td *window, const char *state)
{
    Atom atom_state;
    Atom *current_states;
    Atom actual_type;
    unsigned long int n_items;
    int actual_format;
    int status;

    if (window->display == NULL || window->xwindow == 0) {
        return -1;
    }

    /* If the state is not set, there's nothing to unset */
    if (!ewmh_is_state_set(window, state)) {
        return -3;
    }

    /* Get current state */
    atom_state = XInternAtom(window->display, state, False);
    if (atom_state == None) {
        return -2;
    }

    /* Get current '_NET_WM_STATE' property */
    status = XGetWindowProperty(window->display, window->xwindow,
            XInternAtom(window->display,
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
        for (unsigned long int i = 0; i < n_items; ++i) {
            if (current_states[i] != atom_state) {
                new_states[new_state_count++] = current_states[i];
            }
        }

        /* Set the new state list, only if there are changes */
        if (new_state_count != n_items) {
            if (new_state_count > 0) {
                XChangeProperty(window->display, window->xwindow,
                        XInternAtom(window->display,
                            "_NET_WM_STATE", False),
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        (unsigned char *) new_states,
                        (int) new_state_count);
            } else {
                XDeleteProperty(window->display, window->xwindow,
                        XInternAtom(window->display,
                            "_NET_WM_STATE", False));
            }
        }
        free(new_states);
    } /* if (status == Success && ...)*/

    if (current_states) {
        XFree(current_states);
    }

    return 0;
}


/* Set the icon for an EWMH-compliant window */
int ewmh_set_window_icon(window_td *window, Pixmap *icon,
        unsigned int icon_count)
{
    if (icon == NULL || icon_count <= 0) {
        return -1;
    }

    return ewmh_update_window_property(window, "_NET_WM_ICON",
            icon, XA_CARDINAL, icon_count);
}


/* Set the process ID for an EWMH-compliant window */
int ewmh_set_window_pid(window_td *window, pid_t pid)
{
    return ewmh_update_window_property(window, "_NET_WM_PID",
            &pid, XA_CARDINAL, 1);
}


/* Set the desktop number for an EWMH-compliant window */
int ewmh_set_window_desktop(window_td *window, int desktop)
{
    return ewmh_update_window_property(window, "_NET_WM_DESKTOP",
            &desktop, XA_CARDINAL, 1);
}


/* Set the user time for an EWMH-compliant window */
int ewmh_set_window_user_time(window_td *window, Time time)
{
    return ewmh_update_window_property(window, "_NET_WM_USER_TIME",
            &time, XA_CARDINAL, 1);
}


/* Set the user time window for an EWMH-compliant window */
int ewmh_set_window_user_time_window(window_td *window,
        Window user_time_window)
{
    return ewmh_update_window_property(window,
            "_NET_WM_USER_TIME_WINDOW", &user_time_window,
            XA_WINDOW, 1);
}


/* Set the opacity for an EWMH-compliant window */
int ewmh_set_window_opacity(window_td *window,
        unsigned long int opacity)
{
    /* Validate opacity */
    if (opacity > 0xffffffff) {
        opacity = 0xffffffff;   /* Cap it at maximum opacity */
    }
    return ewmh_update_window_property(window,
            "_NET_WM_WINDOW_OPACITY", &opacity, XA_CARDINAL, 1);
}


/* Set the window actions for an EWMH-compliant window */
int ewmh_set_window_actions(window_td *window, const char **actions,
        unsigned int action_count)
{
    Atom *atoms;
    int result;

    atoms = malloc(sizeof(Atom) * (size_t) action_count);
    if (atoms == NULL) {
        return -1;
    }

    /* Convert action names to atoms */
    for (unsigned int i = 0; i < action_count; ++i) {
        atoms[i] = XInternAtom(window->display, actions[i], False);
        if (atoms[i] == None) {
            free(atoms);
            return -2;
        }
    }

    result = ewmh_update_window_property(window,
            "_NET_WM_ACTIONS", atoms, XA_ATOM, action_count);

    free(atoms);
    return result;
}


/* Set the frame extents for an EWMH-compliant window */
int ewmh_set_window_frame_extents(window_td *window, int *extents)
{
    return ewmh_update_window_property(window, "_NET_FRAME_EXTENTS", 
            (long int *) extents, XA_CARDINAL, 4);
}


/* Indicate that a window is shown in an EWMH-compliant window */
int ewmh_set_window_shown(window_td *window)
{
    long int data[1] = { 0 };   // TODO:adjust

    return ewmh_update_window_state(window, "_NET_WM_SHOWN", data, 1);
}


/* Indicate a window is being restored in an EWMH-compliant window */
int ewmh_set_window_restore(window_td *window)
{
    unsigned long int data[3] = { 0, XInternAtom(window->display,
            "_NET_WM_STATE_NORMAL", False), 0 };

    return ewmh_update_window_state(window, "_NET_WM_STATE",
            (long int *) data, 3);
}


/* Indicate a window is being moved or resized in a yadda-yadda-yadda */
int ewmh_set_window_moveresize(window_td *window)
{
    long int data[1] = { 0 };   // TODO: adjust

    return ewmh_update_window_state(window, "_NET_WM_MOVERESIZE",
            data, 1);
}
