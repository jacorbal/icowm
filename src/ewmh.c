/**
 * @file ewmh.c
 *
 * @brief Implementation of EWMH functions
 */

/* System includes */
#include <stddef.h>     /* NULL */
#include <sys/types.h>  /* pid_t */

/* X11 includes */
#include <X11/Xlib.h>   /* Window, Display, Pixmap */
#include <X11/Xatom.h>  /* Atom */

/* Utils includes */
#include <utils/safestr.h>

/* Local includes */
#include <ewmh.h>


/* Set the window name for an EWMH-compliant window */
void ewmh_set_window_name(Display *display, Window window,
        const char *name)
{
    if (name != NULL) {
        XChangeProperty(display, window,
                XInternAtom(display, "_NET_WM_NAME", False),
                XA_STRING, 8,
                PropModeReplace,
                (unsigned char *) name,
                (int) safe_strlen(name));
    }
}


/* Set the class name for an EWMH-compliant window */
void ewmh_set_window_class(Display *display, Window window,
        const char *class_name)
{
    if (class_name != NULL) {
        XChangeProperty(display, window,
                XInternAtom(display, "_NET_WM_CLASS", False),
                XA_STRING, 8,
                PropModeReplace,
                (unsigned char *) class_name,
                (int) safe_strlen(class_name));
    }
}


/* Set the icon name for an EWMH-compliant window */
void ewmh_set_window_icon_name(Display *display, Window window,
        const char *icon_name)
{
    if (icon_name != NULL) {
        XChangeProperty(display, window,
                XInternAtom(display, "_NET_WM_ICON_NAME", False),
                XA_STRING, 8,
                PropModeReplace,
                (unsigned char *) icon_name,
                (int) safe_strlen(icon_name));
    }
}


/* Set the process ID for an EWMH-compliant window */
void ewmh_set_window_pid(Display *display, Window window, pid_t pid)
{
    XChangeProperty(display, window,
            XInternAtom(display, "_NET_WM_PID", False),
            XA_CARDINAL, 32,
            PropModeReplace,
            (unsigned char *) &pid, 1);
}


/* Set the desktop number for an EWMH-compliant window */
void ewmh_set_window_desktop(Display *display, Window window,
        int desktop)
{
    XChangeProperty(display, window,
            XInternAtom(display, "_NET_WM_DESKTOP", False),
            XA_CARDINAL, 32,
            PropModeReplace,
            (unsigned char *) &desktop, 1);
}


/* Set the type for an EWMH-compliant window */
void ewmh_set_window_type(Display *display, Window window,
        const char *type)
{
    Atom atom_type = XInternAtom(display, type, False);
    XChangeProperty(display, window,
            XInternAtom(display, "_NET_WM_WINDOW_TYPE", False),
            XA_ATOM, 32,
            PropModeReplace,
            (unsigned char *) &atom_type, 1);
}


/* Set the icon for an EWMH-compliant window */
void ewmh_set_window_icon(Display *display, Window window, Pixmap *icon,
        int icon_count)
{
    XChangeProperty(display, window,
            XInternAtom(display, "_NET_WM_ICON", False),
            XA_CARDINAL, 32,
            PropModeReplace,
            (unsigned char *) icon,
            icon_count);
}


/* Set the user time for an EWMH-compliant window */
void ewmh_set_window_user_time(Display *display, Window window,
        Time time) {
    XChangeProperty(display, window,
            XInternAtom(display, "_NET_WM_USER_TIME", False),
            XA_CARDINAL, 32,
            PropModeReplace,
            (unsigned char *) &time, 1);
}


/* Set the user time window for an EWMH-compliant window */
void ewmh_set_window_user_time_window(Display *display, Window window,
        Window user_time_window)
{
    XChangeProperty(display, window,
            XInternAtom(display, "_NET_WM_USER_TIME_WINDOW", False),
            XA_WINDOW, 32,
            PropModeReplace,
            (unsigned char *) &user_time_window, 1);
}


/* Set the startup ID for an EWMH-compliant window */
void ewmh_set_window_startup_id(Display *display, Window window,
        const char *startup_id){
    if (startup_id != NULL) {
        XChangeProperty(display, window,
                XInternAtom(display, "_NET_STARTUP_ID", False),
                XA_STRING, 8,
                PropModeReplace,
                (unsigned char *) startup_id,
                (int) safe_strlen(startup_id));
    }
}


/* Set the opacity for an EWMH-compliant window */
void ewmh_set_window_opacity(Display *display, Window window,
        unsigned long int opacity)
{
    /* Validate opacity */
    if (opacity > 0xffffffff) {
        opacity = 0xffffffff;   /* Cap it at maximum opacity */
    }

    XChangeProperty(display, window,
            XInternAtom(display, "_NET_WM_WINDOW_OPACITY", False),
            XA_CARDINAL, 32,
            PropModeReplace,
            (unsigned char *) &opacity, 1);
}


/* Set multiple states for an EWMH-compliant window */
void ewmh_set_window_state_multiple(Display *display, Window window,
        Atom *states, int state_count)
{
    XChangeProperty(display, window,
            XInternAtom(display, "_NET_WM_STATE", False),
            XA_ATOM,
            32,
            PropModeReplace,
            (unsigned char *) states,
            state_count);
}


/* Set a single state for an EWMH-compliant window */
void ewmh_set_window_state(Display *display, Window window,
        const char *state)
{
    Atom atom_state = XInternAtom(display, state, False);
    ewmh_set_window_state_multiple(display, window, &atom_state, 1);
}


/* Unset a single state for an EWMH-compliant window */
void ewmh_unset_window_state(Display *display, Window window,
        const char *state)
{
    Atom atom_state = XInternAtom(display, state, False);

    /* Get current state */
    Atom *current_states;
    unsigned long int n_items;
    int actual_format;
    Atom actual_type;
    Atom new_states[10];        /* Assuming maximum states */
    int new_state_count = 0;

    XGetWindowProperty(display, window,
            XInternAtom(display, "_NET_WM_STATE", False),
            0, 1024, False, XA_ATOM,
            &actual_type, &actual_format,
            &n_items, &n_items, (unsigned char **) &current_states);

    /* Copy only those states that we don't want to eliminate */
    for (unsigned long int i = 0; i < n_items; ++i) {
        if (current_states[i] != atom_state) {
            new_states[new_state_count++] = current_states[i];
        }
    }

    /* Set the new state list */
    ewmh_set_window_state_multiple(display, window,
            new_states, new_state_count);

    XFree(current_states);
}
