/**
 * @file wcmd.c
 *
 * @brief Implementation on executions over windows using the X11
 *        interface
 */

/* X11 includes */
#include <X11/Xlib.h>   /* Window, Pixmap */

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>

/* Project includes */
#include <actdata.h>
#include <window.h>

/* Local includes */
#include <cmds/wcmd.h>


/**/
void wcmd_window_close(window_td *window)
{
    /* This destroys the window in window->xwindow, but does not
     * deallocates the memory of the window object.  This action is
     * intended to be called by the desktop, therefore, it's
     * responsibility of the desktop to execute this action, and then
     * invoke 'window_destroy' */
    XDestroyWindow(window->display, window->xwindow);
}


/**/
void wcmd_window_restore(window_td *window)
{
    window_geometry_restore(window);
    XMapWindow(window->display, window->xwindow);
    window->properties.state = WINDOW_STATE_NORMAL;
    window_geometry_save(window);
}


/**/
void wcmd_window_focus(window_td *window)
{
    XSetInputFocus(window->display,
            window->xwindow, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(window->display, window->xwindow);
    XMapRaised(window->display, window->xwindow);
    window_focus(window);
}


/**/
void wcmd_window_unfocus(window_td *window)
{
    window_unfocus(window);
}


/**/
void wcmd_window_move(window_td *window,
        action_data_window_td *window_data)
{
    XResizeWindow(window->display, window->xwindow,
            window_data->new_data.geometry.dim.w,
            window_data->new_data.geometry.dim.h);
    window->properties.geometry_cur.dim =
        window_data->new_data.geometry.dim;
}

