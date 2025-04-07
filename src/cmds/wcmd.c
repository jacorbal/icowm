/**
 * @file wcmd.c
 *
 * @brief Implementation on executions over clients using the XCB
 *        interface and updating EWMH and ICCCM hints
 */

/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>

/* Project includes */
#include <actdata.h>
//#include <ewmh/winprop.h>
//#include <ewmh/winprop/state.h>
#include <client.h>

/* Local includes */
#include <cmds/wcmd.h>


static void s_wcmd_raise_window(xcb_connection_t *connection,
        xcb_window_t window)
{
    xcb_configure_window(connection, window,
            XCB_CONFIG_WINDOW_STACK_MODE,
            (const uint32_t[]) { XCB_STACK_MODE_ABOVE });
}


static void s_wcmd_lower_window(xcb_connection_t *connection,
        xcb_window_t window)
{
    xcb_configure_window(connection, window,
            XCB_CONFIG_WINDOW_STACK_MODE,
            (const uint32_t[]) { XCB_STACK_MODE_BELOW });
}



/* Close the client */
void wcmd_client_close(client_td *client)
{
    /* This destroys the client in client->window, but does not
     * deallocates the memory of the client object.  This action is
     * intended to be called by the desktop, therefore, it's
     * responsibility of the desktop to execute this action, and then
     * invoke 'client_destroy' */
    xcb_destroy_window(client->connection, client->window);
}


/* Restore a client to its normal state */
void wcmd_client_restore(client_td *client)
{
    client_geometry_restore(client);

    xcb_map_window(client->connection, client->window);

    client_toggle_hidden(client);
    client->properties.state = CLIENT_STATE_NORMAL;

    client_geometry_save(client);

    // TODO: This has to unminimize, unmaximize, unfullscreen the client
    // TODO: hints
//    ewmh_rem_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_HIDDEN");
}


/* Focus a client */
void wcmd_client_focus(client_td *client)
{
    xcb_set_input_focus(client->connection, XCB_INPUT_FOCUS_PARENT,
                        client->window, XCB_CURRENT_TIME);
    s_wcmd_raise_window(client->connection, client->window);
    xcb_map_window(client->connection, client->window);

    client_focus(client);

    // TODO: hints
}


/* Unfocus the client */
void wcmd_client_unfocus(client_td *client)
{
    client_unfocus(client);

    // TODO: hints
}


/* Move client */
void wcmd_client_move(client_td *client,
        action_data_client_td *client_data)
{
    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {
            (uint32_t) client_data->new_data.geometry.pos.x,
            (uint32_t) client_data->new_data.geometry.pos.y
            });
    client->layout.geometry.cur.pos =
        client_data->new_data.geometry.pos;

    // TODO: hints
}


/* Resize client */
void wcmd_client_resize(client_td *client,
        action_data_client_td *client_data)
{
    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
            (uint32_t) client_data->new_data.geometry.dim.w,
            (uint32_t) client_data->new_data.geometry.dim.h
            });

    client->layout.geometry.cur.dim =
        client_data->new_data.geometry.dim;

    // TODO: hints
}


/* Rename the client */
void wcmd_client_rename(client_td *client,
        action_data_client_td *client_data)
{
    xcb_change_property(client->connection,
            XCB_PROP_MODE_REPLACE,
            client->window,
            XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING,
            8,/*bits*/
            (uint32_t) safe_strlen(client_data->new_data.name),
            client_data->new_data.name);

    // TODO
//    ewmh_alter_net_wm_name(client->connection, client->window,
//    client_data->new_data.name);
}


/* Change the class of the client */
void wcmd_client_reclass(client_td *client,
        action_data_client_td *client_data)
{
    // TODO
}


/* Change the role of the client */
void wcmd_client_rerole(client_td *client,
        action_data_client_td *client_data)
{
    // TODO
}


/* Maximize client horizontally */
void wcmd_client_maximize_horz(client_td *client)
{
    xcb_get_geometry_reply_t *attrs =
        xcb_get_geometry_reply(client->connection,
                xcb_get_geometry(client->connection,
                    client->window), NULL);
    if (!attrs) {
        return;
    }

    //TODO: Deactivate CLIENT_STATE_MAXIMIZED first?
    client_geometry_save(client);

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH,
            (const uint32_t[]) {
            0,
            0,  /* Y */
            attrs->width,   /* Width */
            client->layout.geometry.cur.dim.h   /* Current height */
            });

    client->properties.state = CLIENT_STATE_MAXIMIZED_HORZ;

//    ewmh_add_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_MAXIMIZED_HORZ");

    safe_free((void *) attrs);
}


/* Maximize client vertically */
void wcmd_client_maximize_vert(client_td *client)
{
    xcb_get_geometry_reply_t *attrs =
        xcb_get_geometry_reply(client->connection,
                xcb_get_geometry(client->connection,
                    client->window), NULL);
    if (!attrs) {
        return;
    }

    //TODO: Deactivate CLIENT_STATE_MAXIMIZED first?
    client_geometry_save(client);

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
            0,  /* X */
            0,  /* Y */
            client->layout.geometry.cur.dim.w, /* Current width */
            attrs->height   /* Height */
            });

    client->properties.state = CLIENT_STATE_MAXIMIZED_VERT;

//    ewmh_add_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_MAXIMIZED_VERT");

    safe_free((void *) attrs);
}


/* Maximize client entirely */
void wcmd_client_maximize(client_td *client)
{
    xcb_get_geometry_reply_t *attrs =
        xcb_get_geometry_reply(client->connection,
                xcb_get_geometry(client->connection,
                    client->window), NULL);
    if (!attrs) {
        return;
    }

    client_geometry_save(client);

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
            0,  /* X */
            0,  /* Y */
            attrs->width,   /* Width */
            attrs->height   /* Height */
            });

    client->properties.state = CLIENT_STATE_MAXIMIZED_VERT;

//    ewmh_add_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_MAXIMIZED_HORIZ");
//    ewmh_add_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_MAXIMIZED_VERT");

    safe_free((void *) attrs);
}


/* Iconify client (and minimize it) */
void wcmd_client_iconify(client_td *client)
{
    client_geometry_save(client);

    //unmap === iconify?  iconify => unmap?
    xcb_unmap_window(client->connection, client->window);

    client_set_hidden(client);
    client->properties.state = CLIENT_STATE_ICONIFIED;

    // TODO: Logic to actually iconify!

//    ewmh_add_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_HIDDEN");
}


/* Hide the client (minimize, but not iconify) */
void wcmd_client_hide(client_td *client)
{
    client_geometry_save(client);

    xcb_unmap_window(client->connection, client->window);

    client_set_hidden(client);

//    ewmh_add_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_HIDDEN");
}


/* Show (unhide) the client */
void wcmd_client_unhide(client_td *client)
{
    client_geometry_save(client);

    xcb_map_window(client->connection, client->window);

    client_unset_hidden(client);

//    ewmh_rem_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_HIDDEN");
}


/* Shade client (roll-up), if decorated */
void wcmd_client_shade(client_td *client)
{
    /* Only allow rolling-up if there's client decoration */
    if (!client_is_decorated(client)) {
        return;
    }

    // TODO: Logic to shade the client

    // TODO: This could involve reconfiguring client dimensions and
    // toggling state

    client_set_shade(client);

//    ewmh_add_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_SHADED");
}

/* Unshade client (roll-down), if decorated */
void wcmd_client_unshade(client_td *client)
{
    /* Only allow rolling-down if there's client decoration */
    if (!client_is_decorated(client)) {
        return;
    }

    // TODO: Logic to unshade the client

    // TODO: This could involve reconfiguring client dimensions and
    // toggling state

    client_unset_shade(client);
    client_unset_hidden(client);

//    ewmh_add_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_SHADED");
}


/* Toggle shading */
void wcmd_client_toggle_shade(client_td *client)
{
    client_toggle_shade(client);
}


/* Set client sticky mode */
void wcmd_client_sticky(client_td *client)
{
    client_set_sticky(client);

//    ewmh_add_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_STICKY");
}

/* Remove client sticky mode */
void wcmd_client_unsticky(client_td *client)
{
    client_unset_sticky(client);

//    ewmh_rem_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_STICKY");
}


/* Toggle stickiness */
void wcmd_client_toggle_sticky(client_td *client)
{
    client_toggle_sticky(client);
}


/* Set full screen mode */
void wcmd_client_fullscreen(client_td *client)
{
    xcb_get_geometry_reply_t *attrs =
        xcb_get_geometry_reply(client->connection,
                xcb_get_geometry(client->connection,
                    client->window), NULL);
    if (!attrs) {
        return;
    }

    // TODO: Don't get the desktop dimensions and set the client size

    client_geometry_save(client);

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
            0, /* X */
            0, /* Y */
            attrs->width,  /* Width */
            attrs->height  /* Height */
            });

    client->properties.state = CLIENT_STATE_FULLSCREEN;

    // TODO: if decorated, hid the decoration whilst in this state

//    ewmh_add_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_FULLSCREEN");

    safe_free((void *) attrs);
}


/* Remove full screen mode */
void wcmd_client_unfullscreen(client_td *client)
{
    client_geometry_restore(client);

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
            (uint32_t) client->layout.geometry.cur.dim.w,
            (uint32_t) client->layout.geometry.cur.dim.h
            });

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {
            (uint32_t) client->layout.geometry.cur.pos.x,
            (uint32_t) client->layout.geometry.cur.pos.y
            });

    client->properties.state = CLIENT_STATE_NORMAL;

    // TODO: if decorated, restore the decoration

//    ewmh_rem_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_FULLSCREEN");
}


/* Toggle full screen mode */
void wcmd_client_toggle_fullscreen(client_td *client)
{
    if (client->properties.state == CLIENT_STATE_FULLSCREEN) {
        wcmd_client_unfullscreen(client);
    } else {
        wcmd_client_fullscreen(client);
    }
}



/* Raise the client to the top */
void wcmd_client_raise(client_td *client)
{
    s_wcmd_raise_window(client->connection, client->window);

    // TODO: update the stack?
}


/* Lower the client to the bottom */
void wcmd_client_lower(client_td *client)
{
    s_wcmd_lower_window(client->connection, client->window);
    // TODO: update the stack?
}


/* Put the client in the above layer */
void wcmd_client_layer_above(client_td *client)
{
    s_wcmd_raise_window(client->connection, client->window);
    client->properties.layer = CLIENT_LAYER_ABOVE;
//    ewmh_rem_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_BELOW");
//    ewmh_add_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_ABOVE");
}


/* Put the client in the normal layer */
void wcmd_client_layer_normal(client_td *client)
{
    s_wcmd_raise_window(client->connection, client->window);
    // TODO: update the stack?

    client->properties.layer = CLIENT_LAYER_NORMAL;
//    ewmh_rem_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_BELOW");
//    ewmh_rem_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_ABOVE");
}



/* Put the client in the below layer */
void wcmd_client_layer_below(client_td *client)
{
    s_wcmd_lower_window(client->connection, client->window);
    // TODO: update the stack?

    client->properties.layer = CLIENT_LAYER_BELOW;
//    ewmh_rem_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_ABOVE");
//    ewmh_add_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_BELOW");
}

/* Set client urgency */
void wcmd_client_set_urgent(client_td *client)
{
    client_set_urgent(client);
//    ewmh_add_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_DEMANDS_ATTENTION");
}


/* Clear client urgency */
void wcmd_client_clear_urgent(client_td *client)
{
    client_unset_urgent(client);
//    ewmh_rem_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_DEMANDS_ATTENTION");
}


/* Set icon for the client */
void wcmd_client_set_icon(client_td *client,
        action_data_client_td *client_data)
{
    xcb_change_property(client->connection,
            XCB_PROP_MODE_REPLACE,
            client->window,
            XCB_ATOM_WM_ICON_NAME,
            XCB_ATOM_STRING,
            8, // Longitud en bits
            (unsigned int) safe_strlen(client_data->new_data.icon_name),
            client_data->new_data.icon_name);

    // Update the client properties.  Call any additional functions that
    // might handle update graphics.
    // ewmh_alter_net_wm_icon_name(client->connection, client->window,
    // client_data->new_data.icon_name);
}
