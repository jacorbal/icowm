/**
 * @file tests/handler/test_randr.c
 *
 * @brief Test battery for handler/randr.c: the XRandR extension
 *        event handler
 *
 * handler_randr_event reaches a real, stack-built 'struct wm_s'
 * (wm/internal.h, safe to include; see the same rationale used in
 * tests/handler/test_crossing.c) through the narrow accessor
 * functions in src/wm/instance.c, linked for real since it is nothing
 * but side-effect-free field reads.  wm_outdate_stage and
 * wm_outdate_desktop (render/outdate.h) are 'static inline' and thus
 * exercised for real too, needing no stand-in of their own.
 *
 * Every other collaborator (lookup_stage_for_root, stage_resize,
 * stage_monitor_refresh_all/_workareas, stage_client_reflow_all,
 * stage_action_randr_apply_profiles, systray_handle_stage_resize,
 * keyboard_load) is cross-module and a link-only stand-in below,
 * recording what it was called with so each branch's real effect can
 * be checked without needing any of those modules' own, much larger
 * dependency trees.
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
#include <xcb/randr.h>

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <stage.h>
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <handler/randr.h>
#include <harness/tap.h>


/* Recording state for every link-only stand-in below, reset by
 * s_reset before each scenario */
static stage_td *s_lookup_result;
static int s_call_stage_resize;
static uint32_t s_resize_width;
static uint32_t s_resize_height;
static int s_call_refresh_monitors;
static int s_call_refresh_workareas;
static int s_call_clients_reflow;
static int s_call_apply_randr_profiles;
static int s_call_systray_resize;
static int s_call_keyboard_load;


static void s_reset(void)
{
    s_lookup_result = NULL;
    s_call_stage_resize = 0;
    s_resize_width = 0u;
    s_resize_height = 0u;
    s_call_refresh_monitors = 0;
    s_call_refresh_workareas = 0;
    s_call_clients_reflow = 0;
    s_call_apply_randr_profiles = 0;
    s_call_systray_resize = 0;
    s_call_keyboard_load = 0;
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


/** Controlled stand-in for lookup_stage_for_root */
stage_td *lookup_stage_for_root(list_td *stages, xcb_window_t root)
{
    (void) stages;
    (void) root;

    return s_lookup_result;
}


/** Link-only stand-in for stage_resize */
void stage_resize(stage_td *stage, uint32_t width, uint32_t height)
{
    (void) stage;

    s_call_stage_resize++;
    s_resize_width = width;
    s_resize_height = height;
}


/** Link-only stand-in for stage_monitor_refresh_all */
void stage_monitor_refresh_all(stage_td *stage)
{
    (void) stage;

    s_call_refresh_monitors++;
}


/** Link-only stand-in for stage_workarea_refresh_all */
void stage_workarea_refresh_all(stage_td *stage)
{
    (void) stage;

    s_call_refresh_workareas++;
}


/** Link-only stand-in for stage_client_reflow_all */
void stage_client_reflow_all(stage_td *stage)
{
    (void) stage;

    s_call_clients_reflow++;
}


/** Link-only stand-in for stage_action_randr_apply_profiles */
bool stage_action_randr_apply_profiles(stage_td *stage,
        bool force)
{
    (void) stage;
    (void) force;

    s_call_apply_randr_profiles++;

    return false;
}


/** Link-only stand-in for stage_desktop_walk_all: no test in this file
 *  gives a stage any desktops, so the walk is always a no-op; kept
 *  only so the translation unit links */
void stage_desktop_walk_all(const stage_td *stage,
        stage_desktop_visitor_fn visit, void *data)
{
    (void) stage;
    (void) visit;
    (void) data;
}


/** Link-only stand-in for systray_handle_stage_resize */
void systray_handle_stage_resize(wm_td *wm)
{
    (void) wm;

    s_call_systray_resize++;
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


/** Build a minimal, real 'struct wm_s' on the stack */
static wm_td s_make_wm(list_td *stages, bool randr_available)
{
    wm_td local_wm;

    memset(&local_wm, 0, sizeof(local_wm));
    local_wm.stages = stages;
    local_wm.is_randr_available = randr_available;
    local_wm.randr_base_event = 80u;

    return local_wm;
}


int main(void)
{
    wm_td wm;
    stage_td stage;
    xcb_randr_screen_change_notify_event_t screen_event;
    xcb_randr_notify_event_t notify_event;
    list_td dummy_stages_storage;
    list_td *dummy_stages = &dummy_stages_storage;

    TAP_PLAN(16);

    /* Guard clauses: a null wm, null event, null stages, or RandR
     * being unavailable are all silent no-ops */
    s_reset();
    handler_randr_event(NULL, NULL);
    TAP_EQ_INT(s_call_stage_resize, 0,
            "a null wm never resizes any stage");

    memset(&screen_event, 0, sizeof(screen_event));
    screen_event.response_type = 80u + XCB_RANDR_SCREEN_CHANGE_NOTIFY;
    s_reset();
    wm = s_make_wm(dummy_stages, true);
    handler_randr_event(&wm, NULL);
    TAP_EQ_INT(s_call_stage_resize, 0,
            "a null event never resizes any stage");

    s_reset();
    wm = s_make_wm(NULL, true);
    handler_randr_event(&wm, (xcb_generic_event_t *) &screen_event);
    TAP_EQ_INT(s_call_stage_resize, 0,
            "a null stages list never resizes any stage");

    s_reset();
    wm = s_make_wm(dummy_stages, false);
    handler_randr_event(&wm, (xcb_generic_event_t *) &screen_event);
    TAP_EQ_INT(s_call_stage_resize, 0,
            "RandR unavailable means the event is ignored entirely");

    /* ScreenChangeNotify, but lookup_stage_for_root finds no
     * matching stage: nothing else runs */
    s_reset();
    s_lookup_result = NULL;
    wm = s_make_wm(dummy_stages, true);
    memset(&screen_event, 0, sizeof(screen_event));
    screen_event.response_type = 80u + XCB_RANDR_SCREEN_CHANGE_NOTIFY;
    screen_event.width = 1920u;
    screen_event.height = 1080u;
    handler_randr_event(&wm, (xcb_generic_event_t *) &screen_event);
    TAP_EQ_INT(s_call_stage_resize, 0,
            "an unmatched root window is a no-op for" \
            " ScreenChangeNotify");

    /* ScreenChangeNotify with a real matching stage: resized to the
     * event's dimensions, monitors/workareas/reflow all refreshed,
     * systray and keyboard bindings reloaded, and both mm dimensions
     * and rotation copied across from the event */
    s_reset();
    memset(&stage, 0, sizeof(stage));
    s_lookup_result = &stage;
    wm = s_make_wm(dummy_stages, true);
    memset(&screen_event, 0, sizeof(screen_event));
    screen_event.response_type = 80u + XCB_RANDR_SCREEN_CHANGE_NOTIFY;
    screen_event.width = 1920u;
    screen_event.height = 1080u;
    screen_event.mwidth = 520u;
    screen_event.mheight = 290u;
    screen_event.rotation = 3u;
    handler_randr_event(&wm, (xcb_generic_event_t *) &screen_event);
    TAP_EQ_INT(s_call_stage_resize, 1,
            "a matching stage is resized exactly once");
    TAP_EQ_INT((int) s_resize_width, 1920,
            "the stage is resized to the event's width");
    TAP_EQ_INT((int) s_resize_height, 1080,
            "the stage is resized to the event's height");
    TAP_EQ_INT((int) stage.properties.dim_mm.w, 520,
            "the stage's physical width in mm is copied from" \
            " the event");
    TAP_EQ_INT((int) stage.properties.dim_mm.h, 290,
            "the stage's physical height in mm is copied from" \
            " the event");
    TAP_EQ_INT(stage.randr.rotation, 3,
            "the stage's rotation is copied from the event");
    TAP_OK(stage.randr.is_known,
            "the stage's RandR identity is marked known");
    TAP_EQ_INT(s_call_refresh_monitors, 1,
            "monitors are refreshed exactly once for the matched" \
            " stage");
    TAP_EQ_INT(s_call_systray_resize, 1,
            "the systray is notified of the resize exactly once");
    TAP_EQ_INT(s_call_keyboard_load, 1,
            "keyboard bindings are reloaded exactly once");

    /* A generic RandR Notify event whose subCode is not one of the
     * three that trigger a refresh (e.g., PROVIDER_CHANGE): a no-op,
     * proving the subCode gate itself, not just CRTC/OUTPUT paths */
    s_reset();
    wm = s_make_wm(dummy_stages, true);
    memset(&notify_event, 0, sizeof(notify_event));
    notify_event.response_type = 80u + XCB_RANDR_NOTIFY;
    notify_event.subCode = XCB_RANDR_NOTIFY_PROVIDER_CHANGE;
    handler_randr_event(&wm, (xcb_generic_event_t *) &notify_event);
    TAP_EQ_INT(s_call_keyboard_load, 0,
            "a subCode outside CRTC/OUTPUT/OUTPUT_PROPERTY never" \
            " reloads keyboard bindings");

    return TAP_DONE();
}
