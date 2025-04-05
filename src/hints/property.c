/**
 * @file hints/property.c
 *
 * @brief Implementation of functions for managing window properties
 */

/* System includes */
#include <stdlib.h>     /* NULL, free, malloc */

/* X11 includes */
#include <X11/Xlib.h>   /* Display, Window, Atom, Time */
#include <X11/Xatom.h>  /* XA_* */

/* Utils includes */
#include <utils/safestr.h>

/* Local includes */
#include <hints/property.h>

                                                          /* Mutators */

/* Write an array of atoms to a specified property of a window */
int property_set_atom_array(Display *display, Window window,
        const char *property_name, Atom *atoms, unsigned long nitems)
{
    int status;

    status = XChangeProperty(display, window,
            XInternAtom(display, property_name, False),
            XA_ATOM,
            32, PropModeReplace,   /* Replace property */
            (unsigned char *) atoms,
            (int) nitems);

    return (status == Success) ? 0 : -1;    /* Return: 0 for success;
                                                      -1 for error */
}

/* Write an atom property to a window */
int property_set_atom(Display *display, Window window,
        const char *property_name, Atom atom)
{
    Atom prop = XInternAtom(display, property_name, False);
    return XChangeProperty(display, window, prop,
            XA_ATOM,
            32, PropModeReplace,
            (unsigned char *) &atom, 1);
}


/* Write a string array property to a window */
int property_set_string_array(Display *display, Window window,
        const char *property_name, const char **values, int num_values)
{
    Atom prop = XInternAtom(display, property_name, False);
    return XChangeProperty(display, window, prop,
            XA_STRING,
            8, PropModeReplace,
            (unsigned char *) values, num_values);
}


/* Write a string property to a window */
int property_set_string(Display *display, Window window,
        const char *property_name, const char *value)
{
    Atom prop = XInternAtom(display, property_name, False);
    return XChangeProperty(display, window, prop,
            XA_STRING,
            8, PropModeReplace,
            (unsigned char *) value,
            (int) safe_strlen(value) + 1);
}


/* Write a string property (UTF-8) to a window */
int property_set_string_utf8(Display *display, Window window,
        const char *property_name, const char *value)
{
    Atom prop = XInternAtom(display, property_name, False);
    return XChangeProperty(display, window, prop,
            XInternAtom(display, "UTF8_STRING", False),
            8, PropModeReplace,
            (unsigned char *) value,
            (int) safe_strlen(value) + 1);
}


/* Write an integer property to a window */
int property_set_int(Display *display, Window window,
        const char *property_name, int value)
{
    Atom prop = XInternAtom(display, property_name, False);
    return XChangeProperty(display, window, prop,
            XA_INTEGER,
            32, PropModeReplace,
            (unsigned char *) &value, 1);
}


/* Write an cardinal property (unsigned long) to a window */
int property_set_cardinal(Display *display, Window window,
        const char *property_name, unsigned long value)
{
    Atom prop = XInternAtom(display, property_name, False);
    return XChangeProperty(display, window, prop,
            XA_CARDINAL,
            32, PropModeReplace,
            (unsigned char *) &value, 1);
}


/* Write an pixmap property to a window */
int property_set_pixmap(Display *display, Window window,
        const char *property_name, Pixmap pixmap)
{
    Atom prop = XInternAtom(display, property_name, False);
    return XChangeProperty(display, window, prop,
            XA_PIXMAP,
            32, PropModeReplace,
            (unsigned char *) &pixmap, 1);
}

/* Write an window property to a window */
int property_set_window(Display *display, Window window,
        const char *property_name, Window value)
{
    Atom prop = XInternAtom(display, property_name, False);
    return XChangeProperty(display, window, prop,
            XA_WINDOW,
            32, PropModeReplace,
            (unsigned char *) &value, 1);
}


/* Write an time property to a window */
int property_set_time(Display *display, Window window,
        const char *property_name, Time value)
{
    Atom prop = XInternAtom(display, property_name, False);
    return XChangeProperty(display, window, prop,
            XA_CARDINAL,
            32, PropModeReplace,
            (unsigned char *) &value, 1);
}

                                                         /* Accessors */

/* Read an array of atoms from a specified property of a window */
Atom* property_get_atom_array(Display *display, Window window,
        const char *property_name, unsigned long *nitems)
{
    Atom actual_type;
    int actual_format;
    unsigned long bytes_after;
    unsigned char *prop = NULL;
    Atom *atoms = NULL;

    if (XGetWindowProperty(display, window,
                XInternAtom(display, property_name, False),
                0,
                ~(0L),  /* Get all items */
                False,
                AnyPropertyType,
                &actual_type,
                &actual_format,
                nitems,
                &bytes_after,
                &prop) != Success) {
        return NULL;    /* Error retrieving property */
    }

    /* Validate that the retrieved property is of the atom type and in
     * 32-bit format */
    if (actual_type != XA_ATOM || actual_format != 32) {
        *nitems = 0;
        if (prop) XFree(prop);
        return NULL;    /* Not a valid property */
    }

    /* Allocate space for the array of atoms */
    atoms = (Atom *) prop;  /* 'prop' can be cast to atom pointer since
                               it's in 32-bit format */

    return atoms; /* Memory MUST free this using 'XFree()' */
}


/* Read an atom property from a window */
Atom property_get_atom(Display *display, Window window,
        const char *property_name)
{
    Atom prop = XInternAtom(display, property_name, False);
    Atom actual_type;
    Atom result;
    int actual_format;
    unsigned long nitems, bytes_after;
    Atom *data = NULL;

    int status = XGetWindowProperty(display, window, prop, 0, 1, False,
            XA_ATOM,
            &actual_type, &actual_format, &nitems, &bytes_after,
            (unsigned char **) &data);

    if (status != Success || actual_format != 32 || nitems == 0 ||
            actual_type != XA_ATOM) {
        if (data) {
            XFree(data);
        }
        return None;
    }

    result = data[0];  /* Return the first read atom */
    XFree(data);
    return result;
}

/* Read a string array property from a window */
int property_get_string_array(Display *display, Window window,
        const char *property_name, char ***values,
        unsigned long *nitems)
{
    Atom actual_type;
    int actual_format;
    unsigned long bytes_after;
    unsigned char *data = NULL;
    const char *cur;

    Atom prop = XInternAtom(display, property_name, False);
    int status = XGetWindowProperty(display, window, prop, 0, (~0L),
            False, AnyPropertyType, &actual_type,
            &actual_format, nitems, &bytes_after,
            &data);

    if (status != Success || actual_format != 8 || *nitems == 0 ||
            actual_type != XA_STRING) {
        if (data) {
            XFree(data);
        }
        return -1;
    }

    /* Set string array memory */
    *values = malloc(*nitems * sizeof(char*));
    if (*values == NULL) {
        XFree(data);
        *nitems = 0;    /* Set 'nitems' to 0 to indicate failure */
        return -1;
    }

    cur = (const char *) data;
    for (unsigned long i = 0; i < *nitems; ++i) {
        /* Duplicate string */
        (*values)[i] = safe_strdup(cur);    /* Memory MUST be
                                               deallocated later */
        cur += safe_strlen(cur) + 1;
    }

    XFree(data);
    return 0;
}


/* Read a string property from a window */
char *property_get_string(Display *display, Window window,
        const char *property_name)
{
    Atom prop = XInternAtom(display, property_name, False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;

    int status = XGetWindowProperty(display, window, prop, 0, (~0L),
            False,
            XA_STRING,
            &actual_type, &actual_format, &nitems, &bytes_after,
            &data);

    if (status != Success || actual_format != 8 || nitems == 0 ||
            actual_type != XA_STRING) {
        if (data) {
            XFree(data);
        }
        return NULL;
    }

    return (char *) data;   /* Data memory MUST be deallocated later */
}


/* Read a string property (UTF-8) from a window */
char *property_get_string_utf8(Display *display, Window window,
        const char *property_name)
{
    Atom prop = XInternAtom(display, property_name, False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;

    int status = XGetWindowProperty(display, window, prop, 0, (~0L),
            False,
            XInternAtom(display, "UTF8_STRING", False),
            &actual_type, &actual_format,
            &nitems, &bytes_after,
            &data);

    if (status != Success || actual_format != 8 || nitems == 0 ||
            actual_type != XInternAtom(display, "UTF8_STRING", False)) {
        if (data) {
            XFree(data);
        }
        return NULL;
    }

    return (char *) data;   /* Data memory MUST be deallocated later */
}


/* Read a integer property from a window */
int property_get_int(Display *display, Window window,
        const char *property_name)
{
    Atom prop = XInternAtom(display, property_name, False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    int *data = NULL;
    int result;

    int status = XGetWindowProperty(display, window, prop, 0, 1, False,
            XA_INTEGER,
            &actual_type, &actual_format, &nitems, &bytes_after,
            (unsigned char **) &data);

    if (status != Success || actual_format != 32 || nitems < 1 ||
            actual_type != XA_INTEGER) {
        if (data) {
            XFree(data);
        }
        return -1;  /* Error */
    }

    result = (int) data[0]; /* Return first read int */
    XFree(data);
    return result;
}


/* Read a cardinal property (unsigned long) from a window */
unsigned long property_get_cardinal(Display *display, Window window,
        const char *property_name)
{
    Atom prop = XInternAtom(display, property_name, False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned long *data = NULL;
    unsigned long result;

    int status = XGetWindowProperty(display, window, prop, 0, 1, False,
            XA_CARDINAL,
            &actual_type, &actual_format, &nitems, &bytes_after,
            (unsigned char **) &data);

    if (status != Success || actual_format != 32 || nitems == 0 ||
            actual_type != XA_CARDINAL) {
        if (data) {
            XFree(data);
        }
        return 0;   /* Error */
    }

    result = data[0];   /* Return first read cardinal */
    XFree(data);
    return result;
}


/* Read a pixmap property from a window */
Pixmap property_get_pixmap(Display *display, Window window,
        const char *property_name)
{
    Atom prop = XInternAtom(display, property_name, False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    Pixmap *data = NULL;
    Pixmap result;

    int status = XGetWindowProperty(display, window, prop, 0, 1, False,
            XA_PIXMAP,
            &actual_type, &actual_format, &nitems, &bytes_after,
            (unsigned char **) &data);

    if (status != Success || actual_format != 32 || nitems == 0 ||
            actual_type != XA_PIXMAP) {
        if (data) {
            XFree(data);
        }
        return 0;   /* Error */
    }

    result = data[0];   /* Return the first read pixmap */
    XFree(data);
    return result;
}


/* Read a window property from a window */
Window property_get_window(Display *display, Window window,
        const char *property_name)
{
    Atom prop = XInternAtom(display, property_name, False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    Window *data = NULL;
    Window result;

    int status = XGetWindowProperty(display, window, prop, 0, 1, False,
            XA_WINDOW,
            &actual_type, &actual_format, &nitems, &bytes_after,
            (unsigned char **) &data);

    if (status != Success || actual_format != 32 || nitems == 0 ||
            actual_type != XA_WINDOW) {
        if (data) {
            XFree(data);
        }
        return 0;   /* Error */
    }

    result = data[0];   /* Return the first read window */
    XFree(data);
    return result;
}


/* Read a time property from a window */
Time property_get_time(Display *display, Window window,
        const char *property_name)
{
    Atom prop = XInternAtom(display, property_name, False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    Time *data = NULL;
    Time result;

    int status = XGetWindowProperty(display, window, prop, 0, 1, False,
            XA_CARDINAL,
            &actual_type, &actual_format, &nitems, &bytes_after,
            (unsigned char **) &data);

    if (status != Success || actual_format != 32 || nitems == 0 ||
            actual_type != XA_CARDINAL) {
        if (data) {
            XFree(data);
        }
        return 0;   /* Error */
    }

    result = data[0];
    XFree(data);
    return result;
}
