/**
 * @file tests/enact/test_send_to_desktop.c
 *
 * @brief Test battery for the focus-on-arrival fix in
 *        'enact_desktop_client_send' (enact/desktop.c)
 *
 * Confirms a client sent to another desktop becomes that desktop's
 * own remembered 'client_active_id' whenever it is focusable,
 * whether the destination was empty or not, and is left alone when
 * it is not.  Every stand-in below exists purely so the linker can
 * resolve 'enact_desktop_client_send' (and its own static helper,
 * 's_enact_desktop_client_send_one'); each test is deliberately
 * built so the branches those stand-ins would otherwise need to
 * behave faithfully for are never actually reached (no currently-
 * visible client to unmap, no EWMH connection to publish through, no
 * transient family, and the client sent is never the source
 * desktop's own active one) -- see each stand-in's own comment for
 * exactly which condition keeps it unreached.
 *
 * @note Could not be compile-verified in the environment this file
 *       was written in (missing XCB development headers); double-
 *       check each stand-in's own signature against its real
 *       declaration if this file fails to build.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>

/* XCB includes */
#include <xcb/xcb.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Local includes */
#include <client.h>
#include <cmds/client/transient.h>
#include <cmds/client/visibility.h>
#include <desktop.h>
#include <enact.h>
#include <handler/internal.h>
#include <harness/tap.h>
#include <ipc.h>
#include <menu/cycle.h>
#include <policy/placement/window.h>
#include <surface.h>
#include <wm.h>


/** Which desktop 'wm_get_client_desktop' below should report as a
 *  client's own current desktop; set by each test right before
 *  calling 'enact_desktop_client_send', since the stub itself only
 *  ever receives the client, never the desktop, as its own
 *  argument. */
static desktop_td *s_stub_client_desktop = NULL;


/** Link-only stand-in for wm_get_surface_by_id (wm.c): always
 *  reports "no such surface", so 's_enact_desktop_client_send_one'
 *  never takes its own "currently visible, unmap it" branch, which
 *  this file's own fixtures have no live XCB connection to survive
 *  correctly */
surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return NULL;
}


/** Link-only stand-in for wm_get_client_desktop (wm.c): reports
 *  whichever desktop the currently running test last set via
 *  's_stub_client_desktop' */
desktop_td *wm_get_client_desktop(const client_td *client)
{
    (void) client;
    return s_stub_client_desktop;
}


/** Link-only stand-in for ccmd_client_transient_top_parent
 *  (cmds/client/basic.c): every client this file builds has no
 *  transient parent of its own, so its own top parent is always
 *  itself, the same as the real function would report for it */
client_td *ccmd_client_transient_top_parent(client_td *client)
{
    return client;
}


/** Link-only stand-in for ccmd_client_transient_family_snapshot
 *  (cmds/client/basic.c): every client this file builds has no
 *  transient family at all, so the real function would report none
 *  either */
client_td **ccmd_client_transient_family_snapshot(const desktop_td *desktop,
        client_td *top, size_t *count_out)
{
    (void) desktop;
    (void) top;
    *count_out = 0u;
    return NULL;
}


/** Link-only stand-in for desktop_action_client_add (desktop.c):
 *  always succeeds; this file's own 'target' desktops always have
 *  room */
int desktop_action_client_add(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;
    return 0;
}


/** Link-only stand-in for desktop_action_client_rem (desktop.c) */
int desktop_action_client_rem(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;
    return 0;
}


/** Link-only stand-in for xcb_flush (libxcb): every connection this
 *  file's fixtures carry is a placeholder, never a live one, so this
 *  never actually reaches the X server either way */
int xcb_flush(xcb_connection_t *c)
{
    (void) c;
    return 1;
}


/** Link-only stand-in for enact_broadcast_client_event
 *  (enact/broadcast.c): this file never inspects IPC traffic */
void enact_broadcast_client_event(client_td *client, uint32_t type)
{
    (void) client;
    (void) type;
}


/** Link-only stand-in for client_focus_fallback (cmds/client/
 *  basic.c): referenced by 's_enact_desktop_client_send_one' itself
 *  (only on its own "client sent away was the source desktop's own
 *  active one" branch, deliberately never taken by any test in this
 *  file), so the linker needs it resolved regardless of whether it
 *  is ever actually reached at runtime */
void client_focus_fallback(desktop_td *desktop, surface_td *surface,
        const client_td *exclude)
{
    (void) desktop;
    (void) surface;
    (void) exclude;
}


/* The stand-ins below exist purely because enact/desktop.c compiles
 * as a single translation unit: every one of its own public
 * functions besides 'enact_desktop_client_send' (enact_desktop_show,
 * '_send_front'/'_back', the iconify-all pair) references at least
 * one of these, so the linker needs all of them resolved regardless
 * of which functions this file's own tests actually call.  None are
 * ever reached at runtime here. */

/** Link-only stand-in for ccmd_client_unmap_decorated
 *  (cmds/client/basic.c) */
void ccmd_client_unmap_decorated(client_td *client,
        xcb_window_t target)
{
    (void) client;
    (void) target;
}


/** Link-only stand-in for cycle_init (menu/cycle.c) */
void cycle_init(xcb_connection_t *connection, surface_td *surface,
        desktop_td *desktop, bool is_icon, int preselect,
        uint16_t modifier, const config_td *cfg)
{
    (void) connection;
    (void) surface;
    (void) desktop;
    (void) is_icon;
    (void) preselect;
    (void) modifier;
    (void) cfg;
}


/** Link-only stand-in for cycle_draw (menu/cycle.c) */
void cycle_draw(xcb_connection_t *connection, const config_td *cfg)
{
    (void) connection;
    (void) cfg;
}


/** Link-only stand-in for desktop_action_client_send_front
 *  (desktop.c) */
int desktop_action_client_send_front(desktop_td *desktop,
        client_td *client)
{
    (void) desktop;
    (void) client;
    return 0;
}


/** Link-only stand-in for desktop_action_client_send_back
 *  (desktop.c) */
int desktop_action_client_send_back(desktop_td *desktop,
        client_td *client)
{
    (void) desktop;
    (void) client;
    return 0;
}


/** Link-only stand-in for desktop_action_clients_iconify_all
 *  (desktop.c) */
int desktop_action_clients_iconify_all(desktop_td *desktop)
{
    (void) desktop;
    return 0;
}


/** Link-only stand-in for desktop_action_clients_deiconify_all
 *  (desktop.c) */
int desktop_action_clients_deiconify_all(desktop_td *desktop)
{
    (void) desktop;
    return 0;
}


/** Link-only stand-in for hi_handle_net_showing_desktop
 *  (handler/ewmhmsg.c) */
void hi_handle_net_showing_desktop(surface_td *surface, bool show)
{
    (void) surface;
    (void) show;
}


/** Link-only stand-in for ipc_broadcast_event (ipc.c) */
void ipc_broadcast_event(uint32_t type, cJSON *fields)
{
    (void) type;
    (void) fields;
}


/** Link-only stand-in for place_window_apply
 *  (policy/placement/window.c) */
void place_window_apply(const wm_td *wm, surface_td *surface,
        client_td *client)
{
    (void) wm;
    (void) surface;
    (void) client;
}


/** Link-only stand-in for place_window_apply_cascade
 *  (policy/placement/window.c) */
void place_window_apply_cascade(const wm_td *wm, surface_td *surface,
        client_td *client)
{
    (void) wm;
    (void) surface;
    (void) client;
}


/** Link-only stand-in for wm_config (wm.c) */
config_td *wm_config(const wm_td *wm)
{
    (void) wm;
    return NULL;
}


/** Link-only stand-in for xcb_change_property (libxcb) */
xcb_void_cookie_t xcb_change_property(xcb_connection_t *c, uint8_t mode,
        xcb_window_t window, xcb_atom_t property, xcb_atom_t type,
        uint8_t format, uint32_t data_len, const void *data)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) mode;
    (void) window;
    (void) property;
    (void) type;
    (void) format;
    (void) data_len;
    (void) data;
    return cookie;
}


/** Link-only stand-in for xcb_map_window (libxcb) */
xcb_void_cookie_t xcb_map_window(xcb_connection_t *c, xcb_window_t window)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) window;
    return cookie;
}


/** Link-only stand-in for xcb_unmap_window (libxcb) */
xcb_void_cookie_t xcb_unmap_window(xcb_connection_t *c,
        xcb_window_t window)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) window;
    return cookie;
}


/**
 * @brief Build a client_td with the given id and focusable flag,
 *        nothing else set: no EWMH connection (so the property-
 *        publish branch is never taken), no transient parent, not
 *        currently the source desktop's own active client
 */
static client_td *s_make_client(uint32_t id, bool focusable)
{
    client_td *client = calloc(1, sizeof(client_td));

    client->id = id;
    client->window = id;
    if (focusable) {
        client->properties.flags |= CLIENT_FLAG_FOCUSABLE;
    }
    return client;
}


static desktop_td *s_make_desktop(uint32_t id)
{
    desktop_td *desktop = calloc(1, sizeof(desktop_td));

    desktop->id = id;
    return desktop;
}


/* A focusable client sent to an empty destination desktop becomes
 * its own remembered active client: the exact fix this whole file
 * exists to confirm */
static void s_test_focusable_client_to_empty_desktop(void)
{
    desktop_td *source = s_make_desktop(0u);
    desktop_td *target = s_make_desktop(1u);
    client_td *client = s_make_client(100u, true);

    /* Deliberately left as 0 (no active client at all), and 'client'
     * is never made 'source''s own active one either, so the
     * 'client_focus_fallback' branch this file has no stand-in for
     * is never reached. */
    source->client_active_id = 0u;
    target->client_active_id = 0u;

    s_stub_client_desktop = source;
    enact_desktop_client_send(source, client, target);

    TAP_EQ_INT((int) target->client_active_id, (int) client->id,
            "focusable client sent to an empty desktop becomes its"
            " own remembered active client");
    TAP_EQ_INT((int) client->desktop_id, (int) target->id,
            "client's own desktop_id is updated to the target");

    free(source);
    free(target);
    free(client);
}


/* A focusable client sent to a desktop that already remembers a
 * different active client overwrites it: arriving is more relevant
 * than whatever was there before */
static void s_test_focusable_client_overwrites_existing_active(void)
{
    desktop_td *source = s_make_desktop(0u);
    desktop_td *target = s_make_desktop(1u);
    client_td *client = s_make_client(100u, true);

    target->client_active_id = 999u;   /* some other, unrelated id */

    s_stub_client_desktop = source;
    enact_desktop_client_send(source, client, target);

    TAP_EQ_INT((int) target->client_active_id, (int) client->id,
            "focusable client overwrites whatever the target"
            " desktop remembered before, even though it was not"
            " empty");

    free(source);
    free(target);
    free(client);
}


/* A non-focusable client sent to an empty desktop leaves
 * 'client_active_id' alone: it never becomes the remembered target
 * only to be silently skipped over later */
static void s_test_non_focusable_client_leaves_active_id_alone(void)
{
    desktop_td *source = s_make_desktop(0u);
    desktop_td *target = s_make_desktop(1u);
    client_td *client = s_make_client(100u, false);

    target->client_active_id = 0u;

    s_stub_client_desktop = source;
    enact_desktop_client_send(source, client, target);

    TAP_EQ_INT((int) target->client_active_id, 0,
            "non-focusable client never becomes the target's own"
            " remembered active client");
    TAP_EQ_INT((int) client->desktop_id, (int) target->id,
            "the client still moves to the target desktop either way");

    free(source);
    free(target);
    free(client);
}


int main(void)
{
    TAP_PLAN(5);

    s_test_focusable_client_to_empty_desktop();
    s_test_focusable_client_overwrites_existing_active();
    s_test_non_focusable_client_leaves_active_id_alone();

    return TAP_DONE();
}
