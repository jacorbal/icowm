/**
 * @file hints/icccm.c
 *
 * @brief Implementation of properties managment functions per ICCCM
 */

/* System includes */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* X11 includes */
#include <X11/Xlib.h>   /* Display, Window, Atom */
#include <X11/Xutil.h>  /* X*Hints */

/* Local includes */
#include <hints/icccm.h>
#include <hints/property.h>


/* Set 'WM_NAME' */
int icccm_set_wm_name(Display *display, Window window,
        const char *name)
{
    return property_set_string(display, window, "WM_NAME", name);
}


/* Get 'WM_NAME' */
char *icccm_get_wm_name(Display *display, Window window)
{
    return property_get_string(display, window, "WM_NAME");
}


/* Set 'WM_ICON_NAME' */
int icccm_set_wm_icon_name(Display *display, Window window,
        const char *icon_name)
{
    return property_set_string(display, window,
            "WM_ICON_NAME", icon_name);
}


/* Get 'WM_ICON_NAME' */
char *icccm_get_wm_icon_name(Display *display, Window window)
{
    return property_get_string(display, window, "WM_ICON_NAME");
}


/* Set 'WM_CLASS' */
int icccm_set_wm_class(Display *display, Window window,
        const char *class_name, const char *class_instance)
{
    const char *class[] = { class_instance, class_name };   /* Instance
                                                               goes
                                                               first */
    return property_set_string_array(display, window,
            "WM_CLASS", class, 2);
}


/* Get 'WM_CLASS' */
char **icccm_get_wm_class(Display *display, Window window,
        unsigned long *nitems)
{
    char **class_list = NULL;

    if (property_get_string_array(display, window, "WM_CLASS",
                &class_list, nitems) == 0) {
        return class_list;  /* Retrieved class list */
    }

    return NULL;
}


/* Set 'WM_PROTOCOLS' */
int icccm_set_wm_protocols(Display *display, Window window,
        Atom *protocols, unsigned long nitems)
{
    return property_set_atom_array(display, window,
            "WM_PROTOCOLS", protocols, nitems);
}


/* Get 'WM_PROTOCOLS' */
Atom *icccm_get_wm_protocols(Display *display, Window window,
        unsigned long *nitems)
{
    return property_get_atom_array(display, window,
            "WM_PROTOCOLS", nitems);
}


/* Set 'WM_HINTS' */
int icccm_set_wm_hints(Display *display, Window window, XWMHints *hints)
{
    return XSetWMHints(display, window, hints);
}


/* Get 'WM_HINTS' */
XWMHints *icccm_get_wm_hints(Display *display, Window window)
{
    return XGetWMHints(display, window);    /* Memory MUST be
                                               deallocated later
                                               via 'XFree'*/
}


/* Set 'WM_SIZE_HINTS' */
void icccm_set_wm_size_hints(Display *display, Window window,
        XSizeHints *hints)
{
    XSizeHints sizeHints;

    /* Only copy size hints */
    sizeHints.flags = PSize | PMinSize | PMaxSize;
    sizeHints.min_width = hints->min_width;
    sizeHints.min_height = hints->min_height;
    sizeHints.max_width = hints->max_width;
    sizeHints.max_height = hints->max_height;

    XSetWMNormalHints(display, window, &sizeHints);
}


/* Get 'WM_SIZE_HINTS' */
XSizeHints *icccm_get_wm_size_hints(Display *display, Window window)
{
    XSizeHints *hints = XAllocSizeHints();
    XSizeHints hints_all;
    long n;

    if (!hints) {
        return NULL;
    }

    XGetWMNormalHints(display, window, &hints_all, &n);

    hints->flags = 0;
    if (hints_all.flags & PMinSize) {
        hints->min_width = hints_all.min_width;
        hints->min_height = hints_all.min_height;
        hints->flags |= PMinSize;
    }
    if (hints_all.flags & PMaxSize) {
        hints->max_width = hints_all.max_width;
        hints->max_height = hints_all.max_height;
        hints->flags |= PMaxSize;
    }

    return hints;   /* Memory MUST be deallocated later with 'XFree' */
}


/* Set 'WM_NORMAL_HINTS' */
void icccm_set_wm_normal_hints(Display *display, Window window,
        XSizeHints *hints)
{
    XSetWMNormalHints(display, window, hints);
}


/* Get 'WM_NORMAL_HINTS' */
XSizeHints *icccm_get_wm_normal_hints(Display *display, Window window)
{
    XSizeHints *hints = XAllocSizeHints();
    long n;

    if (!hints) {
        return NULL;
    }

    XGetWMNormalHints(display, window, hints, &n);
    return hints;
}


/* Set 'WM_STATE' */
int icccm_set_wm_state(Display *display, Window window, long state)
{
    Atom wm_state_atom = XInternAtom(display, "WM_STATE", False);
    Atom wm_state_value;
    long state_values[2];

    /* Determine steate based on input */
    if (state == 0) {           /* 0: Normal */
        wm_state_value = XInternAtom(display, "NormalState", False);
        state_values[0] = (long) wm_state_value;
        state_values[1] = None;
    } else if (state == 1) {    /* 1: Iconified */
        wm_state_value = XInternAtom(display, "IconicState", False);
        state_values[0] = (long) wm_state_value;
        state_values[1] = None;
    } else {                    /* Invalid state */
        return -1;
    }

    /* Establecer la propiedad WM_STATE' */
    XChangeProperty(display, window, wm_state_atom, wm_state_atom, 32,
            PropModeReplace, (unsigned char *) state_values, 2);

    return 0;
}


/* Get 'WM_STATE' */
long icccm_get_wm_state(Display *display, Window window)
{
    unsigned long nitems;
    Atom *state_atoms;
    long state;

    state_atoms = property_get_atom_array(display, window,
            "WM_STATE", &nitems);

    /* Validate if there are elements */
    if (state_atoms == NULL || nitems < 1) {
        return -1;  /* No available state */
    }

    /* First element is current state (second is previous state) */
    state = (long) state_atoms[0];  /* 0: Normal;
                                       1: Iconified */

    XFree(state_atoms);
    return state;
}
