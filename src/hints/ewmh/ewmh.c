/**
 * @file hints/ewmh/ewmh.c
 *
 * @brief Implementation of basic routines to fetch and alter EWMH data
 */

/* System includes */
#include <stdint.h>     /* uint32_t */
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* memcpy, strlen */

/* X11 includes */
#include <X11/Xlib.h>   /* Display, Window */
#include <X11/Xatom.h>  /* Atom, XA_* */

/* Local includes */
#include <hints/ewmh/ewmh.h>


/* Get a string property from a window */
char *ewmh_fetch_string_property(Display *display,
        Window window, const char *prop)
{
    Atom property = XInternAtom(display, prop, False);
    Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_value = NULL;

    if (XGetWindowProperty(display, window, property, 0,
                HINT_EWMH_MAX_PROPERTY_SIZE, False,
                AnyPropertyType, &actual_type, &actual_format,
                &nitems, &bytes_after, &prop_value) != Success) {
        /*
        fprintf(stderr,
                "Failed to get property %s from window\n", prop);
        */
        return NULL;
    }

    if (actual_type == utf8_string && prop_value != NULL) {
        char *ret = malloc(nitems + 1);
        if (ret) {
            memcpy(ret, prop_value, nitems);
            ret[nitems] = '\0';
        }
        XFree(prop_value);
        return ret;
    }

    XFree(prop_value);
    return NULL;
}


/* Get a string property from a window */
uint32_t ewmh_fetch_uint_property(Display *display,
        Window window, const char *prop)
{
    Atom property = XInternAtom(display, prop, False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_value = NULL;
    uint32_t ret = 0;

    if (XGetWindowProperty(display, window, property, 0, 1, False,
                AnyPropertyType, &actual_type, &actual_format,
                &nitems, &bytes_after, &prop_value) != Success) {
        /*
        fprintf(stderr,
                "Failed to get property %s from window\n", prop);
        */
        return 0;
    }

    if (prop_value != NULL && nitems > 0) {
        ret = *((uint32_t *)prop_value);
    }

    XFree(prop_value);
    return ret;
}


/* Set an unsigned integer property on a window */
void ewmh_alter_uint_property(Display *display,
        Window window, const char *prop, uint32_t value)
{
    Atom property = XInternAtom(display, prop, False);

    XChangeProperty(display, window, property, XA_CARDINAL,
                    32, PropModeReplace,
                    (unsigned char *) &value, 1);
}


/* Set a string property on a window */
void ewmh_alter_string_property(Display *display,
        Window window, const char *prop, const char *value)
{
    Atom property = XInternAtom(display, prop, False);

    XChangeProperty(display, window,
            property, XInternAtom(display, "UTF8_STRING", False),
            8, PropModeReplace,
            (unsigned char *) value,
            strlen(value));
}
