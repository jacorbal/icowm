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
//#include <ewmh/winprop.h>
//#include <ewmh/winprop/state.h>
#include <window.h>

/* Local includes */
#include <cmds/wcmd.h>


/* Close the window */
void wcmd_window_close(window_td *window)
{
    /* This destroys the window in window->xwindow, but does not
     * deallocates the memory of the window object.  This action is
     * intended to be called by the desktop, therefore, it's
     * responsibility of the desktop to execute this action, and then
     * invoke 'window_destroy' */
    XDestroyWindow(window->display, window->xwindow);
}


/* Restore a window to its normal state */
void wcmd_window_restore(window_td *window)
{
    window_geometry_restore(window);

    XMapWindow(window->display, window->xwindow);

    window->properties.flags &= (unsigned int) ~WINDOW_FLAG_HIDDEN;
    window->properties.state = WINDOW_STATE_NORMAL;

    window_geometry_save(window);

    // TODO: This has to unminimize, unmaximize, unfullscreen the window
    // TODO: hints
//    ewmh_rem_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_HIDDEN");
}


/* Focus a window */
void wcmd_window_focus(window_td *window)
{
    XSetInputFocus(window->display,
            window->xwindow, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(window->display, window->xwindow);
    XMapRaised(window->display, window->xwindow);

    window_focus(window);

    // TODO: hints
}


/* Unfocus the window */
void wcmd_window_unfocus(window_td *window)
{
    window_unfocus(window);

    // TODO: hints
}


/* Move window */
void wcmd_window_move(window_td *window,
        action_data_window_td *window_data)
{
    XMoveWindow(window->display, window->xwindow,
            window_data->new_data.geometry.pos.x,
            window_data->new_data.geometry.pos.y);
    window->properties.geometry_cur.pos =
        window_data->new_data.geometry.pos;

    // TODO: hints
}


/* Resize window */
void wcmd_window_resize(window_td *window,
        action_data_window_td *window_data)
{
    XResizeWindow(window->display, window->xwindow,
            window_data->new_data.geometry.dim.w,
            window_data->new_data.geometry.dim.h);

    window->properties.geometry_cur.dim =
        window_data->new_data.geometry.dim;

    // TODO: hints
}


/* Rename the window */
void wcmd_window_rename(window_td *window,
        action_data_window_td *window_data)
{
    // TODO
    XStoreName(window->display, window->xwindow,
            window_data->new_data.name);
//    ewmh_alter_net_wm_name(window->display, window->xwindow,
//    window_data->new_data.name);
}


/* Change the class of the window */
void wcmd_window_reclass(window_td *window,
        action_data_window_td *window_data)
{
    // TODO
}


/* Maximize window horizontally */
void wcmd_window_maximize_horz(window_td *window)
{
    XWindowAttributes attrs; // TODO: don't get 'attrs', use desktop workarea

    //TODO: Deactivate WINDOW_STATE_MAXIMIZED first?
    window_geometry_save(window);

    XGetWindowAttributes(window->display, window->xwindow,
            &attrs);
    XMoveResizeWindow(window->display, window->xwindow, 0, 0,
            (unsigned int) attrs.screen->width,
            (unsigned int) window->properties.geometry_cur.dim.h);

    window->properties.state = WINDOW_STATE_MAXIMIZED_HORZ;

//    ewmh_add_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_MAXIMIZED_HORZ");
}


/* Maximize window vertically */
void wcmd_window_maximize_vert(window_td *window)
{
    XWindowAttributes attrs; // TODO: don't get 'attrs', use desktop workarea

    //TODO: Deactivate WINDOW_STATE_MAXIMIZED first?
    window_geometry_save(window);

    XGetWindowAttributes(window->display, window->xwindow,
            &attrs);
    XMoveResizeWindow(window->display, window->xwindow, 0, 0,
            window->properties.geometry_cur.dim.w,
            (unsigned int) attrs.screen->width);

    window->properties.state = WINDOW_STATE_MAXIMIZED_VERT;

//    ewmh_add_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_MAXIMIZED_VERT");

}


/* Maximize window entirely */
void wcmd_window_maximize(window_td *window)
{
    XWindowAttributes attrs; // TODO: don't get 'attrs', use desktop workarea

    window_geometry_save(window);

    XGetWindowAttributes(window->display, window->xwindow,
            &attrs);
    XMoveResizeWindow(window->display, window->xwindow, 0, 0,
            (unsigned int) attrs.screen->width,
            (unsigned int) attrs.screen->height);

    window->properties.state = WINDOW_STATE_MAXIMIZED_VERT;

//    ewmh_add_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_MAXIMIZED_HORIZ");
//    ewmh_add_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_MAXIMIZED_VERT");

}


/* Iconify window (and minimize it) */
void wcmd_window_iconify(window_td *window)
{
    window_geometry_save(window);

    XIconifyWindow(window->display, window->xwindow,
            (int) window->screen_id);

    window->properties.flags |= WINDOW_FLAG_HIDDEN;
    window->properties.state = WINDOW_STATE_ICONIFIED;

    // TODO: Logic to actually iconify!

//    ewmh_add_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_HIDDEN");
}


/* Hide the window (minimize, but not iconify) */
void wcmd_window_hide(window_td *window)
{
    window_geometry_save(window);

    XIconifyWindow(window->display, window->xwindow,
            (int) window->screen_id);

    window->properties.flags |= WINDOW_FLAG_HIDDEN;

//    ewmh_add_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_HIDDEN");
}


/* Show (unhide) the window */
void wcmd_window_unhide(window_td *window)
{
    window_geometry_save(window);

    XIconifyWindow(window->display, window->xwindow,
            (int) window->screen_id);

    window->properties.flags &= (unsigned int) ~WINDOW_FLAG_HIDDEN;

//    ewmh_rem_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_HIDDEN");
}


/* Shade window (roll-up), if decorated */
void wcmd_window_shade(window_td *window)
{
    /* Only allow rolling-up if there's window decoration */
    if (!(window->properties.flags & WINDOW_FLAG_DECORATED)) {
        return;
    }

    // TODO: Logic to shade the window

    // TODO: This could involve reconfiguring window dimensions and
    // toggling state

    window->properties.flags |= WINDOW_FLAG_STICKY;
    //    window->properties.state = WINDOW_STATE_SHADED;

//    ewmh_add_net_wm_state(window->display, window->xwindow, "_NET_WM_STATE_SHADED");
}

/* Unshade window (roll-down), if decorated */
void wcmd_window_unshade(window_td *window)
{
    /* Only allow rolling-down if there's window decoration */
    if (!(window->properties.flags & WINDOW_FLAG_DECORATED)) {
        return;
    }

    // TODO: Logic to unshade the window

    // TODO: This could involve reconfiguring window dimensions and
    // toggling state

    window->properties.flags &= (unsigned int) ~WINDOW_FLAG_SHADED;

//    ewmh_add_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_SHADED");
}

/* Toggle shading */
void wcmd_window_toggle_shade(window_td *window)
{
    if (window->properties.flags & WINDOW_FLAG_SHADED) {
        wcmd_window_unshade(window);
    } else {
        wcmd_window_shade(window);
    }
}


/* Set window sticky mode */
void wcmd_window_sticky(window_td *window)
{
    window->properties.flags |= WINDOW_FLAG_STICKY;

//    ewmh_add_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_STICKY");
}

/* Remove window sticky mode */
void wcmd_window_unsticky(window_td *window)
{
    window->properties.flags &= (unsigned int) ~WINDOW_FLAG_STICKY;

//    ewmh_rem_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_STICKY");
}


/* Toggle stickiness */
void wcmd_window_toggle_sticky(window_td *window)
{
    if (window->properties.flags & WINDOW_FLAG_STICKY) {
        wcmd_window_unsticky(window);
    } else {
        wcmd_window_sticky(window);
    }
}


/* Set full screen mode */
void wcmd_window_fullscreen(window_td *window)
{
    // TODO: Don't get the desktop dimensions and set the window size
    XWindowAttributes attrs;
    XGetWindowAttributes(window->display, window->xwindow, &attrs);

    window_geometry_save(window);

    XMoveResizeWindow(window->display, window->xwindow,
            0, 0,
            (unsigned int) attrs.screen->width,
            (unsigned int) attrs.screen->height);

    window->properties.state = WINDOW_STATE_FULLSCREEN;

    // TODO: if decorated, hid the decoration whilst in this state

//    ewmh_add_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_FULLSCREEN");
}


/* Remove full screen mode */
void wcmd_window_unfullscreen(window_td *window)
{
    window_geometry_restore(window);

    XResizeWindow(window->display, window->xwindow,
            window->properties.geometry_cur.dim.w,
            window->properties.geometry_cur.dim.h);

    XMoveWindow(window->display, window->xwindow,
            window->properties.geometry_cur.pos.x,
            window->properties.geometry_cur.pos.y);

    window->properties.state = WINDOW_STATE_NORMAL;

    // TODO: if decorated, restore the decoration

//    ewmh_rem_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_FULLSCREEN");
}


/* Toggle full screen mode */
void wcmd_window_toggle_fullscreen(window_td *window)
{
    if (window->properties.state == WINDOW_STATE_FULLSCREEN) {
        wcmd_window_unfullscreen(window);
    } else {
        wcmd_window_fullscreen(window);
    }
}


/* Raise the window to the top */
void wcmd_window_raise(window_td *window)
{
    XRaiseWindow(window->display, window->xwindow);
    // TODO: update the stack?
}


/* Lower the window to the bottom */
void wcmd_window_lower(window_td *window)
{
    XLowerWindow(window->display, window->xwindow);
    // TODO: update the stack?
}


/* Put the window in the above layer */
void wcmd_window_layer_above(window_td *window)
{
    XRaiseWindow(window->display, window->xwindow);
    window->properties.layer = WINDOW_LAYER_ABOVE;
//    ewmh_rem_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_BELOW");
//    ewmh_add_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_ABOVE");
}


/* Put the window in the normal layer */
void wcmd_window_layer_normal(window_td *window)
{
    XRaiseWindow(window->display, window->xwindow);
    // TODO: update the stack?

    window->properties.layer = WINDOW_LAYER_NORMAL;
//    ewmh_rem_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_BELOW");
//    ewmh_rem_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_ABOVE");
}



/* Put the window in the below layer */
void wcmd_window_layer_below(window_td *window)
{
    XLowerWindow(window->display, window->xwindow);
    // TODO: update the stack?

    window->properties.layer = WINDOW_LAYER_BELOW;
//    ewmh_rem_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_ABOVE");
//    ewmh_add_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_BELOW");
}

/* Set window urgency */
void wcmd_window_set_urgent(window_td *window)
{
    window->properties.flags |= WINDOW_FLAG_URGENT;
//    ewmh_add_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_DEMANDS_ATTENTION");
}


/* Clear window urgency */
void wcmd_window_clear_urgent(window_td *window)
{
    window->properties.flags &= (unsigned int) ~WINDOW_FLAG_URGENT;
//    ewmh_rem_net_wm_state(window->display, window->xwindow,
//    "_NET_WM_STATE_DEMANDS_ATTENTION");
}


/* Set icon for the window */
void wcmd_window_set_icon(window_td *window,
        action_data_window_td *window_data)
{
    XSetIconName(window->display, window->xwindow,
            window_data->new_data.icon_name);

    // Update the window properties.  Call any additional functions that
    // might handle update graphics.
    // ewmh_alter_net_wm_icon_name(window->display, window->xwindow,
    // window_data->new_data.icon_name);
}
