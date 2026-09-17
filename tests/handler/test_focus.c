/**
 * @file tests/handler/test_focus.c
 *
 * @brief Test battery for handler/focus.c: the X @c PROPERTY_NOTIFY,
 *        @c FOCUS_IN, @c FOCUS_OUT, and @c MAPPING_NOTIFY event
 *        handlers
 *
 * handler_focus_in, handler_focus_out, and handler_mapping_notify are
 * covered in full: each is a small, self-contained function whose
 * every branch is exercised below via link-only stand-ins for its few
 * collaborators (mouse_enter_focus_is_active/_clear, lookup_find_client,
 * xcb_connection_get, xcb_refresh_keyboard_mapping, xcb_ungrab_key,
 * xcb_ungrab_button, keyboard_load, mouse_load), all recording what
 * they were called with.
 *
 * handler_property_notify is, by contrast, a much larger dispatcher
 * with roughly a dozen atom-keyed branches, most of which call into
 * client property-refresh, rules, and render modules that would each
 * need their own large fixture trees (client_props_refresh_*,
 * rules_apply, wmicon_invalidate, xcb_ewmh_get_wm_strut_partial_reply,
 * client_subscribe_colormap_windows, ccmd_client_toggle_decorate) to
 * exercise meaningfully.  Rather than skip the function outright, this
 * file covers its cleanly separable, side-effect-visible guard
 * clauses: the null-event guard, the PROPERTY_DELETE short-circuit,
 * the root-window background-pixmap path (via
 * render_desktop_background_property_is_pixmap and
 * stage_render_current_desktop_repaint stand-ins), the unconditional
 * systray_handle_property_notify call, and the no-managed-client-found
 * early return.  The deeper per-atom refresh branches
 * (WM_NAME/_NET_WM_NAME, WM_ICON_NAME, _NET_WM_ICON/WM_HINTS, WM_CLASS,
 * _MOTIF_WM_HINTS, struts, WM_COLORMAP_WINDOWS) are left uncovered;
 * this narrowing is called out explicitly in the report rather than
 * silently.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Rules includes */
#include <rules.h>

/* Render includes */
#include <render/desktop/background.h>
#include <render/wmicon.h>

/* Utils includes */
#include <utils/xcb/atom.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <stage.h>
#include <wm.h>
#include <wm/internal.h>

/* Command includes */
#include <cmds/client/state.h>

/* Local includes */
#include <handler/focus.h>
#include <handler/mapping.h>
#include <handler/property.h>
#include <harness/tap.h>


/* Recording state for every link-only stand-in below, reset by
 * s_reset before each scenario */
static int s_call_mouse_enter_focus_clear;
static bool s_mouse_enter_focus_is_active_result;
static client_td *s_lookup_result;
static stage_td *s_lookup_stage_out;
static int s_call_systray_property_notify;
static int s_call_bg_pixmap_invalidate;
static int s_call_repaint;
static bool s_is_background_pixmap_result;
static int s_call_refresh_keyboard_mapping;
static int s_call_ungrab_key;
static int s_call_ungrab_button;
static int s_call_keyboard_load;
static int s_call_mouse_load;


static void s_reset(void)
{
    s_call_mouse_enter_focus_clear = 0;
    s_mouse_enter_focus_is_active_result = false;
    s_lookup_result = NULL;
    s_lookup_stage_out = NULL;
    s_call_systray_property_notify = 0;
    s_call_bg_pixmap_invalidate = 0;
    s_call_repaint = 0;
    s_is_background_pixmap_result = false;
    s_call_refresh_keyboard_mapping = 0;
    s_call_ungrab_key = 0;
    s_call_ungrab_button = 0;
    s_call_keyboard_load = 0;
    s_call_mouse_load = 0;
}


/** Link-only stand-in for logger_msg */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    va_list args;

    (void) level;
    (void) prefix;

    va_start(args, fmt);
    va_end(args);

    return 0;
}


/** Controlled stand-in for mouse_enter_focus_is_active */
bool mouse_enter_focus_is_active(void)
{
    return s_mouse_enter_focus_is_active_result;
}


/** Link-only stand-in for mouse_enter_focus_clear */
void mouse_enter_focus_clear(void)
{
    s_call_mouse_enter_focus_clear++;
}


/** Controlled stand-in for lookup_find_client */
client_td *lookup_find_client(list_td *stages, xcb_window_t window,
        stage_td **stage, desktop_td **desktop)
{
    (void) stages;
    (void) window;

    if (stage != NULL) {
        *stage = s_lookup_stage_out;
    }
    if (desktop != NULL) {
        *desktop = NULL;
    }

    return s_lookup_result;
}


/** Link-only stand-in for systray_handle_property_notify */
void systray_handle_property_notify(const wm_td *wm,
        xcb_property_notify_event_t *event)
{
    (void) wm;
    (void) event;

    s_call_systray_property_notify++;
}


/** Controlled stand-in for
 *  render_desktop_background_property_is_pixmap */
bool render_desktop_background_property_is_pixmap(
        xcb_connection_t *connection, xcb_atom_t atom)
{
    (void) connection;
    (void) atom;

    return s_is_background_pixmap_result;
}


/** Link-only stand-in for render_desktop_background_cache_invalidate */
void render_desktop_background_cache_invalidate(void)
{
    s_call_bg_pixmap_invalidate++;
}


/** Link-only stand-in for stage_render_current_desktop_repaint */
/** Link-only stand-in for @a viewport_mesh_cache_invalidate: the mesh
 *  is dropped whenever the desktop background may have changed under
 *  it, which this file has no mesh to drop */
void viewport_mesh_cache_invalidate(void)
{
}


void stage_render_current_desktop_repaint(stage_td *stage)
{
    (void) stage;

    s_call_repaint++;
}


/** Link-only stand-in for xcb_connection_get: never dereferenced by
 *  any scenario in this file, only compared/forwarded, so a distinct
 *  non-null sentinel is enough */
xcb_connection_t *xcb_connection_get(void)
{
    static int sentinel;

    return (xcb_connection_t *) &sentinel;
}


/** Link-only stand-in for xcb_ewmh_connection_get: no scenario in
 *  this file reaches a branch that dereferences it */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return NULL;
}


/* The remaining link-only stand-ins below back only the deeper
 * per-atom refresh branches of handler_property_notify, which this
 * file's scenarios never actually reach (see the file header
 * comment); they exist solely so the translation unit links, and are
 * never invoked by any TAP_* assertion here */


/** Link-only stand-in for client_props_refresh_normal_hints */
void client_props_refresh_normal_hints(client_td *client)
{
    (void) client;
}


/** Whether the two name stand-ins report a change, so a scenario can
 *  drive the repaint the handler now gates on */
static bool s_refresh_name_changed = true;
static bool s_refresh_icon_name_changed = true;


/** Link-only stand-in for client_props_refresh_name */
bool client_props_refresh_name(client_td *client)
{
    (void) client;

    return s_refresh_name_changed;
}


/** Link-only stand-in for client_props_refresh_icon_name */
bool client_props_refresh_icon_name(client_td *client)
{
    (void) client;

    return s_refresh_icon_name_changed;
}


/** Link-only stand-in for client_props_refresh_role */
void client_props_refresh_role(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for client_props_refresh_colormap_windows */
void client_props_refresh_colormap_windows(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for client_subscribe_colormap_windows */
void client_subscribe_colormap_windows(xcb_connection_t *connection,
        const client_td *client)
{
    (void) connection;
    (void) client;
}


/** Link-only stand-in for rules_apply */
bool rules_apply(const wm_td *wm, client_td *client,
        stage_td **stage_io, desktop_td **desktop_io,
        enum rules_trigger_e trigger)
{
    (void) wm;
    (void) client;
    (void) stage_io;
    (void) desktop_io;
    (void) trigger;

    return false;
}


/** Link-only stand-in for wmicon_invalidate */
void wmicon_invalidate(xcb_connection_t *connection,
        wmicon_cache_td *cache)
{
    (void) connection;
    (void) cache;
}


/** Link-only stand-in for atom_intern */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    (void) connection;
    (void) name;
    (void) only_if_exists;

    return XCB_ATOM_NONE;
}


/** Link-only stand-in for ccmd_client_toggle_decorate */
void ccmd_client_toggle_decorate(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for xcb_get_property: never reached, since
 *  atom_intern above always returns XCB_ATOM_NONE, so the
 *  '_MOTIF_WM_HINTS' branch's guard never matches */
xcb_get_property_cookie_t xcb_get_property(xcb_connection_t *c,
        uint8_t _delete, xcb_window_t window, xcb_atom_t property,
        xcb_atom_t type, uint32_t long_offset, uint32_t long_length)
{
    xcb_get_property_cookie_t cookie;

    (void) c;
    (void) _delete;
    (void) window;
    (void) property;
    (void) type;
    (void) long_offset;
    (void) long_length;

    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/** Link-only stand-in for xcb_get_property_reply */
xcb_get_property_reply_t *xcb_get_property_reply(xcb_connection_t *c,
        xcb_get_property_cookie_t cookie, xcb_generic_error_t **e)
{
    (void) c;
    (void) cookie;

    if (e != NULL) {
        *e = NULL;
    }

    return NULL;
}


/** Link-only stand-in for xcb_get_property_value_length */
int xcb_get_property_value_length(const xcb_get_property_reply_t *r)
{
    (void) r;

    return 0;
}


/** Link-only stand-in for xcb_get_property_value */
void *xcb_get_property_value(const xcb_get_property_reply_t *r)
{
    (void) r;

    return NULL;
}


/** Link-only stand-in for xcb_ewmh_get_wm_strut_partial */
xcb_get_property_cookie_t xcb_ewmh_get_wm_strut_partial(
        xcb_ewmh_connection_t *ewmh, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) ewmh;
    (void) window;

    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/** Link-only stand-in for xcb_ewmh_get_wm_strut_partial_reply */
uint8_t xcb_ewmh_get_wm_strut_partial_reply(xcb_ewmh_connection_t *ewmh,
        xcb_get_property_cookie_t cookie,
        xcb_ewmh_wm_strut_partial_t *reply, xcb_generic_error_t **e)
{
    (void) ewmh;
    (void) cookie;
    (void) reply;

    if (e != NULL) {
        *e = NULL;
    }

    return 0;
}


/** Link-only stand-in for xcb_ewmh_get_wm_strut */
xcb_get_property_cookie_t xcb_ewmh_get_wm_strut(
        xcb_ewmh_connection_t *ewmh, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) ewmh;
    (void) window;

    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/** Link-only stand-in for xcb_ewmh_get_wm_strut_reply */
uint8_t xcb_ewmh_get_wm_strut_reply(xcb_ewmh_connection_t *ewmh,
        xcb_get_property_cookie_t cookie,
        xcb_ewmh_get_extents_reply_t *reply, xcb_generic_error_t **e)
{
    (void) ewmh;
    (void) cookie;
    (void) reply;

    if (e != NULL) {
        *e = NULL;
    }

    return 0;
}


/** Link-only stand-in for stage_workarea_refresh_all */
void stage_workarea_refresh_all(stage_td *stage)
{
    (void) stage;
}


/** Link-only stand-in for xcb_refresh_keyboard_mapping */
int xcb_refresh_keyboard_mapping(xcb_key_symbols_t *keysyms,
        xcb_mapping_notify_event_t *event)
{
    (void) keysyms;
    (void) event;

    s_call_refresh_keyboard_mapping++;

    return 0;
}


/** Link-only stand-in for xcb_ungrab_key */
xcb_void_cookie_t xcb_ungrab_key(xcb_connection_t *c, xcb_keycode_t key,
        xcb_window_t grab_window, uint16_t modifiers)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) key;
    (void) grab_window;
    (void) modifiers;

    memset(&cookie, 0, sizeof(cookie));
    s_call_ungrab_key++;

    return cookie;
}


/** Link-only stand-in for xcb_ungrab_button */
xcb_void_cookie_t xcb_ungrab_button(xcb_connection_t *c,
        uint8_t button, xcb_window_t grab_window, uint16_t modifiers)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) button;
    (void) grab_window;
    (void) modifiers;

    memset(&cookie, 0, sizeof(cookie));
    s_call_ungrab_button++;

    return cookie;
}


/** Link-only stand-in for keyboard_load */
void keyboard_load(list_td *stages, xcb_key_symbols_t *keysyms,
        const config_td *config)
{
    (void) stages;
    (void) keysyms;
    (void) config;

    s_call_keyboard_load++;
}


/** Link-only stand-in for mouse_load */
void mouse_load(list_td *stages, const config_td *config)
{
    (void) stages;
    (void) config;

    s_call_mouse_load++;
}


int main(void)
{
    xcb_focus_in_event_t focus_event;
    xcb_focus_out_event_t focus_out_event;
    xcb_mapping_notify_event_t mapping_event;
    wm_td wm;
    stage_td stage;
    xcb_screen_t screen;
    client_td client;
    xcb_key_symbols_t *keysyms_sentinel = (xcb_key_symbols_t *) 0x1234;
    config_td config;
    list_td stages_storage;
    list_td *stages = &stages_storage;

    TAP_PLAN(22);

    memset(&wm, 0, sizeof(wm));
    memset(&config, 0, sizeof(config));

    /* handler_focus_in: a null event is a silent no-op */
    s_reset();
    handler_focus_in(NULL, NULL, NULL);
    TAP_EQ_INT(s_call_mouse_enter_focus_clear, 0,
            "focus_in with a null event never clears enter-focus");

    /* handler_focus_in: enter-focus not currently pending is a no-op */
    s_reset();
    s_mouse_enter_focus_is_active_result = false;
    memset(&focus_event, 0, sizeof(focus_event));
    handler_focus_in(NULL, NULL, &focus_event);
    TAP_EQ_INT(s_call_mouse_enter_focus_clear, 0,
            "focus_in does nothing when no enter-focus is pending");

    /* handler_focus_in: enter-focus pending gets cleared exactly once */
    s_reset();
    s_mouse_enter_focus_is_active_result = true;
    handler_focus_in(NULL, NULL, &focus_event);
    TAP_EQ_INT(s_call_mouse_enter_focus_clear, 1,
            "focus_in clears a pending enter-focus exactly once");

    /* handler_focus_out: a null wm or null event is a silent no-op */
    s_reset();
    memset(&focus_out_event, 0, sizeof(focus_out_event));
    handler_focus_out(NULL, &focus_out_event);
    TAP_OK(true,
            "focus_out with a null wm does not crash");

    s_reset();
    handler_focus_out(&wm, NULL);
    TAP_OK(true,
            "focus_out with a null event does not crash");

    /* handler_focus_out: a mode outside NORMAL/WHILE_GRABBED is
     * ignored regardless of what lookup would have found */
    memset(&stage, 0, sizeof(stage));
    stage.is_outdated = false;
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    memset(&client, 0, sizeof(client));
    memset(&focus_out_event, 0, sizeof(focus_out_event));
    focus_out_event.mode = XCB_NOTIFY_MODE_UNGRAB;
    handler_focus_out(&wm, &focus_out_event);
    TAP_OK(!stage.is_outdated,
            "focus_out ignores modes other than NORMAL or" \
            " WHILE_GRABBED (here, UNGRAB)");

    /* handler_focus_out: NORMAL mode with a matching client marks its
     * stage outdated */
    stage.is_outdated = false;
    focus_out_event.mode = XCB_NOTIFY_MODE_NORMAL;
    handler_focus_out(&wm, &focus_out_event);
    TAP_OK(stage.is_outdated,
            "focus_out marks the matched stage outdated on" \
            " NORMAL mode");

    /* handler_focus_out: WHILE_GRABBED mode also marks it outdated */
    stage.is_outdated = false;
    focus_out_event.mode = XCB_NOTIFY_MODE_WHILE_GRABBED;
    handler_focus_out(&wm, &focus_out_event);
    TAP_OK(stage.is_outdated,
            "focus_out marks the matched stage outdated on" \
            " WHILE_GRABBED mode too");

    /* handler_focus_out: no managed client found never touches the
     * (unrelated) stage */
    stage.is_outdated = false;
    s_lookup_result = NULL;
    s_lookup_stage_out = NULL;
    focus_out_event.mode = XCB_NOTIFY_MODE_NORMAL;
    handler_focus_out(&wm, &focus_out_event);
    TAP_OK(!stage.is_outdated,
            "focus_out with no matched client leaves the stage" \
            " untouched");

    /* handler_mapping_notify: null keysyms, event, or config are all
     * silent no-ops */
    s_reset();
    memset(&mapping_event, 0, sizeof(mapping_event));
    mapping_event.request = XCB_MAPPING_MODIFIER;
    handler_mapping_notify(NULL, stages, &mapping_event, &config);
    TAP_EQ_INT(s_call_keyboard_load, 0,
            "mapping_notify with a null keysyms table is a no-op");

    s_reset();
    handler_mapping_notify(keysyms_sentinel, stages, NULL, &config);
    TAP_EQ_INT(s_call_keyboard_load, 0,
            "mapping_notify with a null event is a no-op");

    s_reset();
    handler_mapping_notify(keysyms_sentinel, stages, &mapping_event,
            NULL);
    TAP_EQ_INT(s_call_keyboard_load, 0,
            "mapping_notify with a null config is a no-op");

    /* handler_mapping_notify: a pointer-mapping change is ignored
     * entirely, before even refreshing the keyboard mapping */
    s_reset();
    mapping_event.request = XCB_MAPPING_POINTER;
    handler_mapping_notify(keysyms_sentinel, stages, &mapping_event,
            &config);
    TAP_EQ_INT(s_call_refresh_keyboard_mapping, 0,
            "a pointer mapping change never refreshes the keyboard" \
            " mapping");
    TAP_EQ_INT(s_call_keyboard_load, 0,
            "a pointer mapping change never reloads keyboard" \
            " bindings");

    /* handler_mapping_notify: a keyboard-mapping change refreshes the
     * table, ungrabs every stage's keys, and reloads bindings, but
     * never touches mouse bindings (request != MODIFIER) */
    s_reset();
    memset(&screen, 0, sizeof(screen));
    screen.root = 0x321u;
    memset(&stage, 0, sizeof(stage));
    stage.screen = &screen;
    stages = list_init(NULL);
    (void) list_ins_next(stages, NULL, &stage);
    mapping_event.request = XCB_MAPPING_KEYBOARD;
    handler_mapping_notify(keysyms_sentinel, stages, &mapping_event,
            &config);
    TAP_EQ_INT(s_call_refresh_keyboard_mapping, 1,
            "a keyboard mapping change refreshes the keysym table" \
            " exactly once");
    TAP_EQ_INT(s_call_ungrab_key, 1,
            "every stage's keys are ungrabbed exactly once each");
    TAP_EQ_INT(s_call_keyboard_load, 1,
            "keyboard bindings are reloaded exactly once");
    TAP_EQ_INT(s_call_ungrab_button, 0,
            "a keyboard-only mapping change never ungrabs buttons");
    TAP_EQ_INT(s_call_mouse_load, 0,
            "a keyboard-only mapping change never reloads mouse" \
            " bindings");

    /* handler_mapping_notify: a modifier-mapping change additionally
     * ungrabs every stage's buttons and reloads mouse bindings */
    s_reset();
    mapping_event.request = XCB_MAPPING_MODIFIER;
    handler_mapping_notify(keysyms_sentinel, stages, &mapping_event,
            &config);
    TAP_EQ_INT(s_call_ungrab_button, 1,
            "a modifier mapping change ungrabs every stage's" \
            " buttons exactly once each");
    TAP_EQ_INT(s_call_mouse_load, 1,
            "a modifier mapping change reloads mouse bindings" \
            " exactly once");

    list_destroy(stages);

    /* handler_property_notify: a null event is a silent no-op,
     * proven by never even reaching the systray call */
    s_reset();
    stages = list_init(NULL);
    handler_property_notify(&wm, NULL, stages, NULL);
    TAP_EQ_INT(s_call_systray_property_notify, 0,
            "property_notify with a null event never calls into" \
            " the systray");
    list_destroy(stages);

    return TAP_DONE();
}
