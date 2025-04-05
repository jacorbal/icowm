/**
 * @file hints/state.c
 *
 * @brief Implementation of functions for managing states of X11 windows
 */

/* System includes */
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* memcpy */

/* X11 includes */
#include <X11/Xlib.h>   /* Display, Window, Atom */
#include <X11/Xatom.h>  /* XA_* */

/* Local includes */
#include <hints/state.h>
#include <hints/property.h>


/* Assign '_NET_WM_ALLOWED_ACTIONS' or '_NET_WM_WINDOW_TYPE' */
int set_window_property(Display *display, Window window,
        const char *property_name, Atom *atoms, unsigned long count)
{
    Atom property = XInternAtom(display, property_name, False);
    if (property == None) {
        return 0;   /* Error */
    }
    /* Assign property as atom array */
    return property_set_atom_array(display, window,
            property_name, atoms, count);
}


/* Get the list of states for a given window */
Atom *get_window_state(Display *display, Window window,
        unsigned long *nitems)
{
    Atom prop = XInternAtom(display, "_NET_WM_STATE", False);
    Atom actual_type;
    int actual_format;
    unsigned long bytes_after;
    Atom *data = NULL;

    int status = XGetWindowProperty(display, window, prop, 0, (~0L),
            False, XA_ATOM, &actual_type,
            &actual_format, nitems, &bytes_after,
            (unsigned char **) &data);

    if (status != Success || actual_format != 32 || *nitems == 0 ||
            actual_type != XA_ATOM) {
        if (data) {
            XFree(data);
        }
        *nitems = 0;    /* Indicate zero items on error */
        return NULL;
    }

    return data;    /* Return pointer to the list of states */
}


/* Set state for a window */
void set_window_state(Display *display, Window window,
        Atom *states, unsigned long count, int action)
{
    XEvent e;
    e.xclient.type = ClientMessage;
    e.xclient.window = window;
    e.xclient.message_type = XInternAtom(display,
            "_NET_WM_STATE", False);
    e.xclient.format = 32;
    e.xclient.data.l[0] = action;   /* 1: add; 0: remove */

    for (unsigned long i = 0; i < count && i < 5; ++i) {
        e.xclient.data.l[i + 1] = (long) states[i];
        /* Use only the next four slots */
    }

    XSendEvent(display, DefaultRootWindow(display), False,
            SubstructureRedirectMask | SubstructureNotifyMask, &e);
}


/* Set the states for a given window */
void set_window_states(Display *display, Window window, Atom *states,
        unsigned long nstates)
{
    XChangeProperty(display, window,
            XInternAtom(display, "_NET_WM_STATE", False),
            XA_ATOM, 32, PropModeReplace,
            (unsigned char *) states,
            (int) nstates);
}


/* Add an atom to a specified property of a window */
int add_atom_to_property(Display *display, Window window,
        const char *property_name, Atom atom)
{
    Atom *atoms;
    unsigned long nitems;
    int found = 0;
    int status;

    /* Check if the atom already exists */
    atoms = property_get_atom_array(display, window,
            property_name, &nitems);
    for (unsigned long i = 0; i < nitems; ++i) {
        if (atoms[i] == atom) {
            found = 1;
            break;
        }
    }

    /* If not present, create new array */
    if (!found) {
        Atom *new_atoms = malloc((nitems + 1) * sizeof(Atom));
        if (new_atoms == NULL) {
            return -1;
        }

        /* Copy existent atoms */
        for (unsigned long i = 0; i < nitems; ++i) {
            new_atoms[i] = atoms[i];
        }

        /* Add the new atom */
        new_atoms[nitems] = atom;

        /* Write the new array */
        status = property_set_atom_array(display, window, property_name,
                new_atoms, nitems + 1);

        free(new_atoms);
        return status;
    }

    XFree(atoms);
    return 0;   /* Atom already present */
}


/* Add a new state to the window's state list */
int add_window_state(Display *display, Window window, Atom state)
{
    unsigned long nitems;
    Atom *current_states = get_window_state(display, window, &nitems);
    Atom *new_states;

    /* Check if the state already exists */
    for (unsigned long i = 0; i < nitems; ++i) {
        if (current_states[i] == state) {
            XFree(current_states);
            return 2;   /* State already exists */
        }
    }

    /* Create new array that includes existing states plus the new state */
    new_states = malloc((nitems + 1) * sizeof(Atom));
    if (new_states == NULL) {
        XFree(current_states);
        return 1;
    }

    /* Copy existing states to the new list */
    memcpy(new_states, current_states, nitems * sizeof(Atom));
    new_states[nitems] = state; /* Add new state */

    /* Set the new list of states */
    set_window_states(display, window, new_states, nitems + 1);

    free(new_states);
    XFree(current_states);
    return 0;   /* Successfully added the state */
}


/* Add multiple states */
void add_window_states_multiple(Display *display, Window window,
        Atom *new_states, unsigned long num_new_states)
{
    unsigned long current_nitems;
    Atom *current_states;
    Atom *combined_states;
    unsigned long j;

    current_states = get_window_state(display, window, &current_nitems);

    /* Save space for current states, plus new ones */
    combined_states =
        malloc((current_nitems + num_new_states) * sizeof(Atom));
    if (combined_states == NULL) {
        if (current_states) XFree(current_states);
        return;
    }

    /* Copy current states */
    memcpy(combined_states, current_states,
            current_nitems * sizeof(Atom));

    /* Add new states (without duplicates) */
    j = current_nitems;
    for (unsigned long i = 0; i < num_new_states; ++i) {
        int exists = 0;
        for (unsigned long k = 0; k < current_nitems; ++k) {
            if (new_states[i] == current_states[k]) {
                exists = 1;
                break;  /* Existing state; don't add it */
            }
        }
        if (!exists) {
            combined_states[j++] = new_states[i];   /* Add new state */
        }
    }

    /* Set new list of states */
    set_window_states(display, window, combined_states, j);

    free(combined_states);
    if (current_states) XFree(current_states);
}


/* Remove an atom from a specified property of a window */
int remove_atom_from_property(Display *display, Window window,
        const char *property_name, Atom atom)
{
    Atom *atoms;
    Atom *new_atoms;
    unsigned long nitems;
    unsigned long new_nitems = 0;
    int status;

    atoms = property_get_atom_array(display, window,
            property_name, &nitems);
    new_atoms = malloc(nitems * sizeof(Atom));
    if (new_atoms == NULL) {
        return -1;
    }

    /* Create a new array for the list of states minus the one to be
     * removed */
    for (unsigned long i = 0; i < nitems; ++i) {
        if (atoms[i] != atom) {
            new_atoms[new_nitems++] = atoms[i];
        }
    }

    /* Write new array */
    status = property_set_atom_array(display, window, property_name,
                    new_atoms, new_nitems);
    free(new_atoms);
    XFree(atoms);
    return status;
}


/* Remove a state from the window's state list */
int remove_window_state(Display *display, Window window, Atom state)
{
    unsigned long nitems;
    unsigned long j;
    Atom *new_states;
    Atom *current_states = get_window_state(display, window, &nitems);

    /* If there are no current states, there's nothing to remove */
    if (current_states == NULL || nitems == 0) {
        return 2;
    }

    /* Create a new array for the list of states minus the one to be
     * removed */
    new_states = malloc((nitems - 1) * sizeof(Atom));
    if (new_states == NULL) {
        XFree(current_states);
        return 1;
    }

    j = 0;
    for (unsigned long i = 0; i < nitems; ++i) {
        if (current_states[i] != state) {
            /* Copy only the states not equal to state */
            new_states[j++] = current_states[i];
        }
    }

    /* Set new list of states */
    set_window_states(display, window, new_states, j);  /* Pass the
                                                           remaining
                                                           count */

    free(new_states);
    XFree(current_states);
    return (j < nitems) ? 0 : 1;    /* Return: 0 if state was removed;
                                               1 otherwise */
}


/* Remove multiple states or actions from the window's property list */
void remove_window_states_multiple(Display *display, Window window,
        Atom *values, unsigned long num_values)
{
    unsigned long current_nitems;
    Atom *current_values;
    Atom *new_values;
    unsigned long j;

    current_values = get_window_state(display, window, &current_nitems);

    /* Allocate space for the remaining values after removal */
    new_values = malloc(current_nitems * sizeof(Atom));
    if (new_values == NULL) {
        if (current_values) XFree(current_values);
        return;
    }

    /* Copy current values to new_values, removing specified values */
    j = 0;
    for (unsigned long i = 0; i < current_nitems; ++i) {
        int remove = 0;
        for (unsigned long k = 0; k < num_values; ++k) {
            if (current_values[i] == values[k]) {
                remove = 1; /* Mark value for removal */
                break;
            }
        }
        if (!remove) {
            new_values[j++] = current_values[i];    /* Keep value */
        }
    }

    /* Set the new list of properties for the window */
    set_window_states(display, window, new_values, j);  /* 'j' is the
                                                           count of
                                                           values
                                                           remaining */

    free(new_values);
    if (current_values) {
        XFree(current_values);
    }
}
