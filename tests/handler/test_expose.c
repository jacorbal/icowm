/**
 * @file tests/handler/test_expose.c
 *
 * @brief Test battery for handler/expose.c: the X @c EXPOSE event
 *        handler
 *
 * handler_expose is a long, strictly ordered chain of
 * "does this event belong to window kind X; if so, repaint X and
 * return" checks, followed by a managed-client lookup and then a
 * further icon/frame/titlebar dispatch.  Every "owns this window" or
 * "is open" predicate in the chain (drag_is_overlay_window,
 * systray_owns_window, popup_is_open, dialog_info_is_open,
 * notify_desktop_is_open, cycle_is_open, search_is_open,
 * run_owns_window, menu_confirm_dialog_is_open, wincmenu_owns_window,
 * rootmenu_owns_window, winlist_owns_window, iconmenu_owns_window) is
 * a link-only stand-in below, independently controllable per
 * scenario, together with a shared 's_last_repaint' trace recording
 * which of the matching repaint functions actually ran, so both
 * "the right early return fired" and "nothing later in the chain
 * also ran" can be checked in one assertion each.
 *
 * The much larger tail of handler_expose, reached only once a real
 * managed client is found (the icon-window caption repaint, the
 * frame-decoration repaint, and the titlebar-content repaint), is
 * deliberately narrowed out of this file's scope: each of those three
 * branches does its own raw XCB drawing (xcb_change_window_attributes,
 * xcb_create_gc, xcb_poly_fill_rectangle, xcb_copy_area,
 * xcb_offscreen_buffer_create, wmicon_draw, text_renderer_use_font/
 * _set_color, text_draw_string, ri_icon_hints_draw,
 * render_client_decoration_repaint_frame,
 * render_client_titlebar_repaint_content),
 * so testing them meaningfully would mean re-implementing a fake XCB
 * connection just to observe what those already-tested-elsewhere
 * rendering helpers were asked to draw, rather than exercising
 * anything specific to handler_expose's own dispatch logic.  What is
 * covered instead is the client lookup itself: the branch is reached,
 * the correct client is found, and its case (icon window, frame
 * window, titlebar window, none of the above) is correctly
 * identified, verified via s_last_repaint being left at "none" (the
 * lookup returning early with nothing drawn) or by dedicated
 * link-only stand-ins for the always-a-no-op-in-these-fixtures
 * branch guards (client->properties.flags without CLIENT_FLAG_HIDDEN,
 * client->titlebar/frame both zero) that make every one of those
 * three branches themselves return before reaching any raw XCB call.
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

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Render includes */
#include <render/client/decoration.h>
#include <render/client/titlebar.h>
#include <render/icon.h>
#include <render/text.h>
#include <render/wmicon.h>

/* Input includes */
#include <input/mouse/drag.h>
#include <input/mouse/drag/icon.h>

/* Utils includes */
#include <utils/xcb/pixmap.h>

/* Local includes */
#include <handler.h>
#include <harness/tap.h>


enum s_repaint_e {
    S_REPAINT_NONE = 0,
    S_REPAINT_DRAG_OVERLAY,
    S_REPAINT_SYSTRAY,
    S_REPAINT_POPUP,
    S_REPAINT_DIALOG_INFO,
    S_REPAINT_NOTIFY_DESKTOP,
    S_REPAINT_CYCLE,
    S_REPAINT_SEARCH,
    S_REPAINT_RUN,
    S_REPAINT_CONFIRM,
    S_REPAINT_WINCMENU,
    S_REPAINT_ROOTMENU,
    S_REPAINT_WINLIST,
    S_REPAINT_ICONMENU,
    S_REPAINT_ICON
};


/* Which one of the ordered dispatch's repaint functions actually
 * ran, or S_REPAINT_NONE if the chain fell all the way through */
static enum s_repaint_e s_last_repaint;

/* Per-scenario controllable predicate results, all false/0 (i.e.,
 * "does not own/is not open") unless a scenario sets one */
static bool s_is_overlay_window;
static bool s_systray_owns;
static bool s_popup_open;
static xcb_window_t s_popup_win;
static bool s_dialog_info_open;
static xcb_window_t s_dialog_info_win;
static bool s_notify_desktop_open;
static xcb_window_t s_notify_desktop_win;
static bool s_cycle_open;
static xcb_window_t s_cycle_win;
static bool s_search_open;
static xcb_window_t s_search_win;
static bool s_run_owns;
static bool s_confirm_open;
static xcb_window_t s_confirm_win;
static bool s_wincmenu_owns;
static bool s_rootmenu_owns;
static bool s_winlist_owns;
static bool s_iconmenu_owns;
static bool s_iconmenu_target_is_answer;
static client_td *s_lookup_result;


static void s_reset(void)
{
    s_last_repaint = S_REPAINT_NONE;
    s_is_overlay_window = false;
    s_systray_owns = false;
    s_popup_open = false;
    s_popup_win = 0u;
    s_dialog_info_open = false;
    s_dialog_info_win = 0u;
    s_notify_desktop_open = false;
    s_notify_desktop_win = 0u;
    s_cycle_open = false;
    s_cycle_win = 0u;
    s_search_open = false;
    s_search_win = 0u;
    s_run_owns = false;
    s_confirm_open = false;
    s_confirm_win = 0u;
    s_wincmenu_owns = false;
    s_rootmenu_owns = false;
    s_winlist_owns = false;
    s_iconmenu_owns = false;
    s_iconmenu_target_is_answer = false;
    s_lookup_result = NULL;
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


bool drag_is_overlay_window(xcb_window_t window)
{
    (void) window;

    return s_is_overlay_window;
}


void drag_overlay_repaint(xcb_connection_t *connection)
{
    (void) connection;

    s_last_repaint = S_REPAINT_DRAG_OVERLAY;
}


bool systray_owns_window(xcb_window_t window)
{
    (void) window;

    return s_systray_owns;
}


void systray_layout_reflow(void)
{
    s_last_repaint = S_REPAINT_SYSTRAY;
}


bool popup_is_open(void)
{
    return s_popup_open;
}


xcb_window_t popup_window(void)
{
    return s_popup_win;
}


void popup_repaint(xcb_connection_t *connection, const config_td *cfg)
{
    (void) connection;
    (void) cfg;

    s_last_repaint = S_REPAINT_POPUP;
}


bool dialog_info_is_open(void)
{
    return s_dialog_info_open;
}


xcb_window_t dialog_info_window(void)
{
    return s_dialog_info_win;
}


void dialog_info_repaint(xcb_connection_t *connection,
        const config_td *cfg)
{
    (void) connection;
    (void) cfg;

    s_last_repaint = S_REPAINT_DIALOG_INFO;
}


bool notify_desktop_is_open(void)
{
    return s_notify_desktop_open;
}


xcb_window_t notify_desktop_window(void)
{
    return s_notify_desktop_win;
}


void notify_desktop_repaint(xcb_connection_t *connection,
        const config_td *cfg)
{
    (void) connection;
    (void) cfg;

    s_last_repaint = S_REPAINT_NOTIFY_DESKTOP;
}


bool cycle_is_open(void)
{
    return s_cycle_open;
}


xcb_window_t cycle_window(void)
{
    return s_cycle_win;
}


void cycle_force_full_repaint(void)
{
}


void cycle_draw(xcb_connection_t *connection, const config_td *cfg)
{
    (void) connection;
    (void) cfg;

    s_last_repaint = S_REPAINT_CYCLE;
}


client_td *cycle_get_selected_client(void)
{
    return NULL;
}


bool search_is_open(void)
{
    return s_search_open;
}


xcb_window_t search_window(void)
{
    return s_search_win;
}


void search_draw(xcb_connection_t *connection, const config_td *cfg)
{
    (void) connection;
    (void) cfg;

    s_last_repaint = S_REPAINT_SEARCH;
}


bool run_owns_window(xcb_window_t win)
{
    (void) win;

    return s_run_owns;
}


void run_draw(xcb_connection_t *connection, const config_td *cfg)
{
    (void) connection;
    (void) cfg;

    s_last_repaint = S_REPAINT_RUN;
}


bool menu_confirm_dialog_is_open(void)
{
    return s_confirm_open;
}


xcb_window_t menu_confirm_dialog_window(void)
{
    return s_confirm_win;
}


void menu_confirm_dialog_repaint(xcb_connection_t *connection,
        const config_td *cfg)
{
    (void) connection;
    (void) cfg;

    s_last_repaint = S_REPAINT_CONFIRM;
}


bool wincmenu_owns_window(xcb_window_t win)
{
    (void) win;

    return s_wincmenu_owns;
}


void wincmenu_repaint(xcb_window_t win)
{
    (void) win;

    s_last_repaint = S_REPAINT_WINCMENU;
}


bool rootmenu_owns_window(xcb_window_t win)
{
    (void) win;

    return s_rootmenu_owns;
}


void rootmenu_repaint(xcb_window_t win)
{
    (void) win;

    s_last_repaint = S_REPAINT_ROOTMENU;
}


bool winlist_owns_window(xcb_window_t win)
{
    (void) win;

    return s_winlist_owns;
}


void winlist_repaint(xcb_window_t win)
{
    (void) win;

    s_last_repaint = S_REPAINT_WINLIST;
}


bool iconmenu_owns_window(xcb_window_t win)
{
    (void) win;

    return s_iconmenu_owns;
}


void iconmenu_repaint(xcb_window_t win)
{
    (void) win;

    s_last_repaint = S_REPAINT_ICONMENU;
}


/** Recording stand-in for ri_render_client_icon */
void ri_render_client_icon(client_td *client, bool is_current,
        bool force, bool restack)
{
    (void) client;
    (void) is_current;
    (void) force;
    (void) restack;

    s_last_repaint = S_REPAINT_ICON;
}


/** Controllable stand-in for iconmenu_target_is */
bool iconmenu_target_is(const client_td *client)
{
    (void) client;

    return s_iconmenu_target_is_answer;
}


/** Controlled stand-in for lookup_find_client */
client_td *lookup_find_client(list_td *surfaces, xcb_window_t window,
        surface_td **surface, desktop_td **desktop)
{
    (void) surfaces;
    (void) window;

    if (surface != NULL) {
        *surface = NULL;
    }
    if (desktop != NULL) {
        *desktop = NULL;
    }

    return s_lookup_result;
}


/* The stand-ins below back only handler_expose's deep icon/frame/
 * titlebar drawing tail, which this file's scenarios never reach
 * (see the file header comment); they exist solely so the
 * translation unit links, and are never invoked by any TAP_*
 * assertion here */


bool drag_is_active(void)
{
    return false;
}


bool drag_is_icon_drag(void)
{
    return false;
}


client_td *drag_client(void)
{
    return NULL;
}


surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;

    return NULL;
}


xcb_pixmap_t xcb_offscreen_buffer_create(xcb_connection_t *connection,
        uint8_t depth, xcb_drawable_t reference, uint16_t width,
        uint16_t height)
{
    (void) connection;
    (void) depth;
    (void) reference;
    (void) width;
    (void) height;

    return XCB_NONE;
}


uint32_t xcb_generate_id(xcb_connection_t *c)
{
    (void) c;

    return 0u;
}


xcb_void_cookie_t xcb_create_gc(xcb_connection_t *c, xcb_gcontext_t cid,
        xcb_drawable_t drawable, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) cid;
    (void) drawable;
    (void) value_mask;
    (void) value_list;

    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


xcb_void_cookie_t xcb_poly_fill_rectangle(xcb_connection_t *c,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        uint32_t rectangles_len, const xcb_rectangle_t *rectangles)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) drawable;
    (void) gc;
    (void) rectangles_len;
    (void) rectangles;

    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


xcb_void_cookie_t xcb_free_gc(xcb_connection_t *c, xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) gc;

    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


xcb_void_cookie_t xcb_clear_area(xcb_connection_t *c, uint8_t exposures,
        xcb_window_t window, int16_t x, int16_t y, uint16_t width,
        uint16_t height)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) exposures;
    (void) window;
    (void) x;
    (void) y;
    (void) width;
    (void) height;

    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


xcb_void_cookie_t xcb_change_window_attributes(xcb_connection_t *c,
        xcb_window_t window, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) window;
    (void) value_mask;
    (void) value_list;

    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


xcb_void_cookie_t xcb_copy_area(xcb_connection_t *c,
        xcb_drawable_t src_drawable, xcb_drawable_t dst_drawable,
        xcb_gcontext_t gc, int16_t src_x, int16_t src_y, int16_t dst_x,
        int16_t dst_y, uint16_t width, uint16_t height)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) src_drawable;
    (void) dst_drawable;
    (void) gc;
    (void) src_x;
    (void) src_y;
    (void) dst_x;
    (void) dst_y;
    (void) width;
    (void) height;

    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


xcb_void_cookie_t xcb_free_pixmap(xcb_connection_t *c,
        xcb_pixmap_t pixmap)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) pixmap;

    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return NULL;
}


void wmicon_draw(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        xcb_drawable_t drawable, uint16_t area_size,
        uint32_t frame_color, uint32_t bg_color,
        wmicon_cache_td *cache)
{
    (void) connection;
    (void) ewmh;
    (void) window;
    (void) drawable;
    (void) area_size;
    (void) frame_color;
    (void) bg_color;
    (void) cache;
}


int text_renderer_use_font(xcb_connection_t *connection,
        const char *font_name)
{
    (void) connection;
    (void) font_name;

    return 0;
}


void text_renderer_set_color(uint32_t fg, uint32_t bg)
{
    (void) fg;
    (void) bg;
}


void text_draw_string(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        struct position_s pos, const char *text)
{
    (void) connection;
    (void) drawable;
    (void) gc;
    (void) pos;
    (void) text;
}


void ri_icon_hints_draw(xcb_connection_t *connection, client_td *client,
        xcb_drawable_t target, bool is_cycle_sel,
        const struct config_theme_s *theme)
{
    (void) connection;
    (void) client;
    (void) target;
    (void) is_cycle_sel;
    (void) theme;
}


void render_client_decoration_repaint_frame(xcb_connection_t *connection,
        client_td *client, bool use_active_style,
        const struct config_theme_s *theme)
{
    (void) connection;
    (void) client;
    (void) use_active_style;
    (void) theme;
}


void render_client_titlebar_repaint_content(xcb_connection_t *connection,
        client_td *client, bool is_focused, uint16_t inner_w,
        uint16_t title_h, const struct config_theme_s *theme)
{
    (void) connection;
    (void) client;
    (void) is_focused;
    (void) inner_w;
    (void) title_h;
    (void) theme;
}


int main(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;
    config_td config;
    xcb_expose_event_t event;
    list_td surfaces_storage;
    list_td *surfaces = &surfaces_storage;
    client_td client;

    TAP_PLAN(21);

    memset(&config, 0, sizeof(config));
    memset(&client, 0, sizeof(client));

    /* Guard clauses: a null event, a non-zero count (more Expose
     * events still pending for the same region), a null connection,
     * or a null config are all silent no-ops */
    s_reset();
    handler_expose(connection, surfaces, NULL, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_NONE,
            "a null event never triggers any repaint");

    s_reset();
    memset(&event, 0, sizeof(event));
    event.count = 1u;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_NONE,
            "a non-zero count defers to the next Expose in the" \
            " series");

    s_reset();
    event.count = 0u;
    handler_expose(NULL, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_NONE,
            "a null connection is a no-op");

    s_reset();
    handler_expose(connection, surfaces, &event, NULL);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_NONE,
            "a null config is a no-op");

    /* The drag overlay window check runs first: it fires even when
     * every other predicate below would also (incorrectly) match,
     * proving strict dispatch order */
    s_reset();
    s_is_overlay_window = true;
    s_systray_owns = true;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_DRAG_OVERLAY,
            "the drag overlay window is checked before the systray");

    /* Systray window repaint */
    s_reset();
    s_systray_owns = true;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_SYSTRAY,
            "a systray-owned window is repainted via the systray");

    /* Popup: both is_open and the specific window must match */
    s_reset();
    s_popup_open = true;
    s_popup_win = 0x50u;
    event.window = 0x99u;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_NONE,
            "an open popup does not repaint for an unrelated window");

    s_reset();
    s_popup_open = true;
    s_popup_win = 0x50u;
    event.window = 0x50u;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_POPUP,
            "the popup's own window is repainted when it is open");

    /* Info dialog repaint */
    s_reset();
    s_dialog_info_open = true;
    s_dialog_info_win = 0x51u;
    event.window = 0x51u;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_DIALOG_INFO,
            "the info dialog's window is repainted when it is open");

    /* Desktop notify repaint */
    s_reset();
    s_notify_desktop_open = true;
    s_notify_desktop_win = 0x52u;
    event.window = 0x52u;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_NOTIFY_DESKTOP,
            "the desktop notify window is repainted when it is open");

    /* Cycle menu repaint */
    s_reset();
    s_cycle_open = true;
    s_cycle_win = 0x53u;
    event.window = 0x53u;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_CYCLE,
            "the cycle menu window is repainted when it is open");

    /* Search widget repaint */
    s_reset();
    s_search_open = true;
    s_search_win = 0x54u;
    event.window = 0x54u;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_SEARCH,
            "the search widget window is repainted when it is open");

    /* Run box repaint */
    s_reset();
    s_run_owns = true;
    event.window = 0x55u;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_RUN,
            "a run-box-owned window is repainted");

    /* Confirm dialog repaint */
    s_reset();
    s_confirm_open = true;
    s_confirm_win = 0x56u;
    event.window = 0x56u;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_CONFIRM,
            "the confirm dialog window is repainted when it is open");

    /* Window context menu repaint */
    s_reset();
    s_wincmenu_owns = true;
    event.window = 0x57u;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_WINCMENU,
            "a wincmenu-owned window is repainted");

    /* Root desktop menu repaint */
    s_reset();
    s_rootmenu_owns = true;
    event.window = 0x58u;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_ROOTMENU,
            "a rootmenu-owned window is repainted");

    /* Window list menu repaint */
    s_reset();
    s_winlist_owns = true;
    event.window = 0x59u;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_WINLIST,
            "a winlist-owned window is repainted");

    /* Icon context menu repaint */
    s_reset();
    s_iconmenu_owns = true;
    event.window = 0x5bu;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_ICONMENU,
            "an iconmenu-owned window is repainted");

    /* Once every special-window check fails, no managed client owns
     * the window either: falls through to nothing, no repaint at all */
    s_reset();
    s_lookup_result = NULL;
    event.window = 0x5au;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_NONE,
            "an event for a window nothing recognizes triggers no" \
            " repaint at all");

    /* The icon window itself: dispatches straight to
     * 'ri_render_client_icon', the same shared drawing function every
     * other place an icon gets (re)drawn already uses, rather than a
     * separate reimplementation of its own */
    s_reset();
    client.icon_window = 0x111u;
    client.frame = 0x112u;
    client.titlebar = 0x113u;
    client.info.name = "some client";
    s_lookup_result = &client;
    event.window = 0x111u;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_ICON,
            "the icon window dispatches to ri_render_client_icon");

    /* A managed client is found, but it is neither the icon window,
     * the frame, nor the titlebar (e.g., some other client subwindow):
     * still no repaint runs, since none of the tail's three window
     * comparisons match */
    s_reset();
    client.icon_window = 0x111u;
    client.frame = 0x112u;
    client.titlebar = 0x113u;
    client.info.name = "some client";
    s_lookup_result = &client;
    event.window = 0x999u;
    handler_expose(connection, surfaces, &event, &config);
    TAP_EQ_INT((int) s_last_repaint, (int) S_REPAINT_NONE,
            "a managed client whose window matches none of icon," \
            " frame, or titlebar triggers no repaint");

    return TAP_DONE();
}
