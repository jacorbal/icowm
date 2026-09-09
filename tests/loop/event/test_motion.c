/**
 * @file tests/loop/event/test_motion.c
 *
 * @brief Test battery for the main loop's pointer motion event
 *        handler (loop/event/motion.c)
 *
 * loop_event_motion_notify's null guards, the file-local s_loop_
 * event_motion_target's priority-ordered branching across every one
 * of its seven outcomes, the switch-case dispatch that follows it,
 * and the file-local s_loop_event_motion_collapse's event-queue
 * draining loop are all genuine, file-local logic worth exercising in
 * their own right; none of place_manual_is_active, wincmenu_is_open,
 * rootmenu_is_open, winlist_is_open, search_is_open/search_window,
 * drag_is_active/drag_update, drag_background_is_active/_update, or
 * any of the seven per-target handler functions belongs to
 * loop/event/motion.c itself, so every one is a link-only,
 * call-recording stand-in below, and xcb_poll_for_event itself is
 * stood in too, handing back a scenario-queued sequence of fake
 * events one at a time exactly as libxcb's own real queue would,
 * without needing a live X connection to actually queue anything on.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/drag/background.h>
#include <loop/event.h>


/* Controllable results for the priority-branching stand-ins below */
static bool s_place_manual_is_active;
static bool s_wincmenu_is_open;
static bool s_rootmenu_is_open;
static bool s_winlist_is_open;
static bool s_iconmenu_is_open;
static bool s_search_is_open;
static xcb_window_t s_search_window;
static bool s_drag_is_active;
static bool s_drag_background_is_active;

/* Recording for the per-target handler stand-ins below */
static int s_place_manual_handle_motion_calls;
static int s_wincmenu_handle_motion_calls;
static int s_rootmenu_handle_motion_calls;
static int s_winlist_handle_motion_calls;
static int s_iconmenu_handle_motion_calls;
static int s_search_handle_motion_calls;
static int s_mouse_handle_motion_hover_calls;
static int s_mouse_viewport_edge_check_calls;
static int s_drag_update_calls;
static struct position_s s_drag_update_last_position;
static int s_drag_background_update_calls;
static struct position_s s_drag_background_update_last_position;

/* A scenario-queued sequence of fake events for xcb_poll_for_event to
 * hand back one at a time, and how many were consumed */
static xcb_generic_event_t *s_poll_queue[8];
static int s_poll_queue_len;
static int s_poll_queue_next;
static int s_poll_for_event_calls;


static void s_reset(void)
{
    s_place_manual_is_active = false;
    s_wincmenu_is_open = false;
    s_rootmenu_is_open = false;
    s_winlist_is_open = false;
    s_iconmenu_is_open = false;
    s_search_is_open = false;
    s_search_window = 0;
    s_drag_is_active = false;
    s_drag_background_is_active = false;
    s_place_manual_handle_motion_calls = 0;
    s_wincmenu_handle_motion_calls = 0;
    s_rootmenu_handle_motion_calls = 0;
    s_winlist_handle_motion_calls = 0;
    s_iconmenu_handle_motion_calls = 0;
    s_search_handle_motion_calls = 0;
    s_mouse_handle_motion_hover_calls = 0;
    s_mouse_viewport_edge_check_calls = 0;
    s_drag_update_calls = 0;
    s_drag_update_last_position.x = 0;
    s_drag_update_last_position.y = 0;
    s_drag_background_update_calls = 0;
    s_drag_background_update_last_position.x = 0;
    s_drag_background_update_last_position.y = 0;
    memset(s_poll_queue, 0, sizeof(s_poll_queue));
    s_poll_queue_len = 0;
    s_poll_queue_next = 0;
    s_poll_for_event_calls = 0;
}


/** Link-only stand-in for xcb_connection_get
 *  (utils/xcb/connection.c) */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/** Link-only stand-in for xcb_poll_for_event (libxcb): hands back
 *  whatever a scenario queued in 's_poll_queue', one entry at a time,
 *  then NULL once exhausted, exactly like a real event queue running
 *  dry */
xcb_generic_event_t *xcb_poll_for_event(xcb_connection_t *c)
{
    (void) c;
    s_poll_for_event_calls++;
    if (s_poll_queue_next >= s_poll_queue_len) {
        return NULL;
    }
    return s_poll_queue[s_poll_queue_next++];
}


/** Link-only stand-in for place_manual_is_active
 *  (policy/placement/manual.c) */
bool place_manual_is_active(void)
{
    return s_place_manual_is_active;
}


/** Link-only stand-in for place_manual_handle_motion
 *  (policy/placement/manual.c) */
void place_manual_handle_motion(xcb_connection_t *connection,
        struct position_s pos)
{
    (void) connection;
    (void) pos;
    s_place_manual_handle_motion_calls++;
}


/** Link-only stand-in for wincmenu_is_open
 *  (menu/context/wincmenu.c) */
bool wincmenu_is_open(void)
{
    return s_wincmenu_is_open;
}


/** Link-only stand-in for wincmenu_handle_motion
 *  (menu/context/wincmenu.c) */
void wincmenu_handle_motion(xcb_window_t win, int x, int y)
{
    (void) win;
    (void) x;
    (void) y;
    s_wincmenu_handle_motion_calls++;
}


/** Link-only stand-in for rootmenu_is_open
 *  (menu/context/rootmenu.c) */
bool rootmenu_is_open(void)
{
    return s_rootmenu_is_open;
}


/** Link-only stand-in for rootmenu_handle_motion
 *  (menu/context/rootmenu.c) */
void rootmenu_handle_motion(xcb_window_t win, int x, int y)
{
    (void) win;
    (void) x;
    (void) y;
    s_rootmenu_handle_motion_calls++;
}


/** Link-only stand-in for winlist_is_open (menu/context/winlist.c) */
bool winlist_is_open(void)
{
    return s_winlist_is_open;
}


/** Link-only stand-in for winlist_handle_motion
 *  (menu/context/winlist.c) */
void winlist_handle_motion(xcb_window_t win, int x, int y)
{
    (void) win;
    (void) x;
    (void) y;
    s_winlist_handle_motion_calls++;
}


/** Link-only stand-in for iconmenu_is_open (menu/context/iconmenu.c) */
bool iconmenu_is_open(void)
{
    return s_iconmenu_is_open;
}


/** Link-only stand-in for iconmenu_handle_motion
 *  (menu/context/iconmenu.c) */
void iconmenu_handle_motion(xcb_window_t win, int x, int y)
{
    (void) win;
    (void) x;
    (void) y;
    s_iconmenu_handle_motion_calls++;
}


/** Link-only stand-in for search_is_open (menu/search.c) */
bool search_is_open(void)
{
    return s_search_is_open;
}


/** Link-only stand-in for search_window (menu/search.c) */
xcb_window_t search_window(void)
{
    return s_search_window;
}


/** Link-only stand-in for search_handle_motion (menu/search.c) */
void search_handle_motion(int16_t x, int16_t y)
{
    (void) x;
    (void) y;
    s_search_handle_motion_calls++;
}


/** Link-only stand-in for drag_is_active (input/mouse/drag.c) */
bool drag_is_active(void)
{
    return s_drag_is_active;
}


/** Link-only stand-in for drag_update (input/mouse/drag.c) */
void drag_update(xcb_connection_t *connection, struct position_s pos)
{
    (void) connection;
    s_drag_update_calls++;
    s_drag_update_last_position = pos;
}


/** Link-only stand-in for drag_background_is_active
 *  (input/mouse/drag/background.c) */
bool drag_background_is_active(void)
{
    return s_drag_background_is_active;
}


/** Link-only stand-in for drag_background_update
 *  (input/mouse/drag/background.c) */
void drag_background_update(xcb_connection_t *connection,
        struct position_s pos)
{
    (void) connection;
    s_drag_background_update_calls++;
    s_drag_background_update_last_position = pos;
}


/** Link-only stand-in for mouse_handle_motion_hover
 *  (input/mouse/hover.c) */
void mouse_handle_motion_hover(xcb_connection_t *connection,
        list_td *surfaces, const xcb_motion_notify_event_t *me)
{
    (void) connection;
    (void) surfaces;
    (void) me;
    s_mouse_handle_motion_hover_calls++;
}


/** Link-only stand-in for mouse_viewport_edge_check
 *  (input/mouse/viewport/edge.c) */
void mouse_viewport_edge_check(list_td *surfaces, xcb_window_t root,
        int16_t root_x, int16_t root_y)
{
    (void) surfaces;
    (void) root;
    (void) root_x;
    (void) root_y;
    s_mouse_viewport_edge_check_calls++;
}


/**
 * @brief Build a loop context with every field left null/zero
 */
static loop_ctx_td s_make_ctx(void)
{
    loop_ctx_td ctx;

    memset(&ctx, 0, sizeof(ctx));
    return ctx;
}


/**
 * @brief Allocate a real motion-notify event on the heap for the
 *        collapse loop to genuinely be able to free
 */
static xcb_generic_event_t *s_make_motion_event(int16_t root_x,
        int16_t root_y)
{
    xcb_motion_notify_event_t *me = malloc(sizeof(*me));

    memset(me, 0, sizeof(*me));
    me->response_type = XCB_MOTION_NOTIFY;
    me->root_x = root_x;
    me->root_y = root_y;
    me->event_x = root_x;
    me->event_y = root_y;
    return (xcb_generic_event_t *) me;
}


/**
 * @brief Allocate a real, non-motion event on the heap
 */
static xcb_generic_event_t *s_make_other_event(uint8_t response_type)
{
    xcb_generic_event_t *ev = malloc(sizeof(*ev));

    memset(ev, 0, sizeof(*ev));
    ev->response_type = response_type;
    return ev;
}


/* loop_event_motion_notify is a safe no-op given a null ctx, a null
 * event pointer, or a pointer to a null event, never reaching
 * xcb_poll_for_event or drag_update at all */
static void s_test_null_guards(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t *null_event = NULL;
    xcb_generic_event_t *real_event = s_make_motion_event(1, 1);

    s_reset();
    loop_event_motion_notify(NULL, &real_event);
    loop_event_motion_notify(&ctx, NULL);
    loop_event_motion_notify(&ctx, &null_event);

    TAP_EQ_INT(s_poll_for_event_calls, 0,
            "loop_event_motion_notify never polls for more events on"
            " a null ctx, a null event pointer, or a pointer to a"
            " null event");
    TAP_EQ_INT(s_drag_update_calls, 0,
            "loop_event_motion_notify never updates the drag position"
            " on any of those three null cases");

    free(real_event);
}


/* With no queued follow-up event, the collapse loop polls exactly
 * once, finds nothing, and the single motion event is fed to
 * drag_update and, with nothing else active, to hover tracking */
static void s_test_no_queued_events_falls_to_hover(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t *event = s_make_motion_event(10, 20);

    s_reset();

    loop_event_motion_notify(&ctx, &event);

    TAP_EQ_INT(s_poll_for_event_calls, 1,
            "with nothing queued, the collapse loop polls exactly"
            " once before giving up");
    TAP_EQ_INT(s_drag_update_calls, 1,
            "the surviving motion event's position is fed to"
            " drag_update exactly once");
    TAP_EQ_INT(s_drag_update_last_position.x, 10,
            "drag_update receives the event's own root_x");
    TAP_EQ_INT(s_drag_update_last_position.y, 20,
            "drag_update receives the event's own root_y");
    TAP_EQ_INT(s_mouse_handle_motion_hover_calls, 1,
            "with nothing else active, the event falls through to"
            " plain hover tracking exactly once");
    TAP_EQ_INT(s_mouse_viewport_edge_check_calls, 1,
            "and the edge-pan check runs alongside it, exactly once");
    TAP_EQ_INT(s_drag_background_update_calls, 1,
            "the background-pan position is also fed unconditionally,"
            " exactly once, alongside the plain drag's own");

    free(event);
}


/* A run of consecutive queued motion events collapses down to just
 * the newest one: every superseded event is freed along the way
 * (ASan/UBSan would flag a double free or a leak here), and the final
 * surviving position is the last queued event's own */
static void s_test_collapse_consecutive_motion_events(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t *event = s_make_motion_event(1, 1);

    s_reset();
    s_poll_queue[0] = s_make_motion_event(2, 2);
    s_poll_queue[1] = s_make_motion_event(3, 3);
    s_poll_queue[2] = s_make_motion_event(4, 4);
    s_poll_queue_len = 3;

    loop_event_motion_notify(&ctx, &event);

    TAP_EQ_INT(s_poll_for_event_calls, 4,
            "the collapse loop polls once past every queued motion"
            " event, plus the final poll that finds the queue empty");
    TAP_EQ_INT(s_drag_update_last_position.x, 4,
            "the position fed onward is exactly the newest, last"
            " queued event's own root_x");
    TAP_EQ_INT(s_drag_update_last_position.y, 4,
            "the position fed onward is exactly the newest, last"
            " queued event's own root_y");

    free(event);
}


/* A run of queued motion events that ends in a non-motion event
 * collapses the motion run, but keeps the non-motion event rather
 * than dropping it, storing it in the ctx's own pending-event slot for
 * the very next pass to handle */
static void s_test_collapse_stops_at_non_motion_event(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t *event = s_make_motion_event(1, 1);
    xcb_generic_event_t *non_motion = s_make_other_event(
            XCB_ENTER_NOTIFY);

    s_reset();
    s_poll_queue[0] = s_make_motion_event(2, 2);
    s_poll_queue[1] = non_motion;
    s_poll_queue_len = 2;

    loop_event_motion_notify(&ctx, &event);

    TAP_EQ_INT(s_poll_for_event_calls, 2,
            "the collapse loop stops polling the moment a non-motion"
            " event is found, rather than draining the whole queue");
    TAP_OK(ctx.pending_event == non_motion,
            "the non-motion event that ended the run is kept, in the"
            " ctx's own pending-event slot, for the next pass");
    TAP_EQ_INT(s_drag_update_last_position.x, 2,
            "the surviving motion position is the last motion event"
            " collapsed to, ahead of the non-motion event that ended"
            " the run");

    free(event);
    free(non_motion);
}


/* A synthetic motion event (top bit of response_type set) is still
 * treated as a motion event for collapsing purposes: only the low
 * seven bits are compared against XCB_MOTION_NOTIFY */
static void s_test_collapse_treats_synthetic_motion_as_motion(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t *event = s_make_motion_event(1, 1);
    xcb_generic_event_t *synthetic_motion = s_make_motion_event(9, 9);

    synthetic_motion->response_type = (uint8_t)
            (XCB_MOTION_NOTIFY | 0x80u);

    s_reset();
    s_poll_queue[0] = synthetic_motion;
    s_poll_queue_len = 1;

    loop_event_motion_notify(&ctx, &event);

    TAP_EQ_INT(s_drag_update_last_position.x, 9,
            "a synthetic motion event still collapses into the run,"
            " becoming the surviving position");

    free(event);
}


/* Priority order, highest first: an active manual placement wins over
 * every menu, every menu, and any drag, even with every single flag
 * turned on at once */
static void s_test_target_priority_manual_wins_all(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t *event = s_make_motion_event(0, 0);

    s_reset();
    s_place_manual_is_active = true;
    s_wincmenu_is_open = true;
    s_rootmenu_is_open = true;
    s_winlist_is_open = true;
    s_search_is_open = true;
    s_drag_is_active = true;

    loop_event_motion_notify(&ctx, &event);

    TAP_EQ_INT(s_place_manual_handle_motion_calls, 1,
            "an active manual placement takes the event ahead of"
            " every other flag, all of which were also set");
    TAP_EQ_INT(s_wincmenu_handle_motion_calls, 0,
            "the window context menu never sees the event while"
            " manual placement is active");

    free(event);
}


/* Priority order: the window context menu wins over the root menu,
 * the window list, search, and drag, once manual placement is out of
 * the running */
static void s_test_target_priority_wincmenu_wins_rest(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t *event = s_make_motion_event(0, 0);

    s_reset();
    s_wincmenu_is_open = true;
    s_rootmenu_is_open = true;
    s_winlist_is_open = true;
    s_search_is_open = true;
    s_drag_is_active = true;

    loop_event_motion_notify(&ctx, &event);

    TAP_EQ_INT(s_wincmenu_handle_motion_calls, 1,
            "the window context menu takes the event once manual"
            " placement is inactive, ahead of the root menu, the"
            " window list, search, and drag");
    TAP_EQ_INT(s_rootmenu_handle_motion_calls, 0,
            "the root menu never sees the event while the window"
            " context menu is open");

    free(event);
}


/* Priority order: the root menu wins over the window list, search,
 * and drag */
static void s_test_target_priority_rootmenu_wins_rest(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t *event = s_make_motion_event(0, 0);

    s_reset();
    s_rootmenu_is_open = true;
    s_winlist_is_open = true;
    s_search_is_open = true;
    s_drag_is_active = true;

    loop_event_motion_notify(&ctx, &event);

    TAP_EQ_INT(s_rootmenu_handle_motion_calls, 1,
            "the root menu takes the event ahead of the window list,"
            " search, and drag");
    TAP_EQ_INT(s_winlist_handle_motion_calls, 0,
            "the window list never sees the event while the root"
            " menu is open");

    free(event);
}


/* Priority order: the window list wins over search and drag */
static void s_test_target_priority_winlist_wins_rest(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t *event = s_make_motion_event(0, 0);

    s_reset();
    s_winlist_is_open = true;
    s_search_is_open = true;
    s_drag_is_active = true;

    loop_event_motion_notify(&ctx, &event);

    TAP_EQ_INT(s_winlist_handle_motion_calls, 1,
            "the window list takes the event ahead of search and"
            " drag");
    TAP_EQ_INT(s_search_handle_motion_calls, 0,
            "search never sees the event while the window list is"
            " open");

    free(event);
}


/* Priority order: the icon menu wins over search and drag, but the
 * window list still wins over the icon menu */
static void s_test_target_priority_iconmenu_wins_rest(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t *event = s_make_motion_event(0, 0);

    s_reset();
    s_winlist_is_open = true;
    s_iconmenu_is_open = true;

    loop_event_motion_notify(&ctx, &event);

    TAP_EQ_INT(s_winlist_handle_motion_calls, 1,
            "the window list takes the event ahead of the icon menu");
    TAP_EQ_INT(s_iconmenu_handle_motion_calls, 0,
            "the icon menu never sees the event while the window"
            " list is open");

    free(event);

    s_reset();
    event = s_make_motion_event(0, 0);
    s_iconmenu_is_open = true;
    s_search_is_open = true;
    s_drag_is_active = true;

    loop_event_motion_notify(&ctx, &event);

    TAP_EQ_INT(s_iconmenu_handle_motion_calls, 1,
            "the icon menu takes the event ahead of search and drag,"
            " once the window list is closed");
    TAP_EQ_INT(s_search_handle_motion_calls, 0,
            "search never sees the event while the icon menu is"
            " open");

    free(event);
}


/* Search only wins if it is open AND the event actually names its own
 * window, either as the event field or as the child field; open
 * search with neither match falls through to drag instead */
static void s_test_target_priority_search_requires_window_match(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t *event_matching_event_field =
            s_make_motion_event(0, 0);
    xcb_generic_event_t *event_matching_child_field =
            s_make_motion_event(0, 0);
    xcb_generic_event_t *event_no_match = s_make_motion_event(0, 0);
    xcb_motion_notify_event_t *me;

    s_search_is_open = true;
    s_search_window = 55;

    s_reset();
    s_search_is_open = true;
    s_search_window = 55;
    s_drag_is_active = true;
    me = (xcb_motion_notify_event_t *) event_matching_event_field;
    me->event = 55;
    me->child = 0;
    loop_event_motion_notify(&ctx, &event_matching_event_field);
    TAP_EQ_INT(s_search_handle_motion_calls, 1,
            "search wins when the motion event's own 'event' field"
            " matches search's window");

    s_reset();
    s_search_is_open = true;
    s_search_window = 55;
    s_drag_is_active = true;
    me = (xcb_motion_notify_event_t *) event_matching_child_field;
    me->event = 0;
    me->child = 55;
    loop_event_motion_notify(&ctx, &event_matching_child_field);
    TAP_EQ_INT(s_search_handle_motion_calls, 1,
            "search also wins when only the motion event's 'child'"
            " field matches search's window");

    s_reset();
    s_search_is_open = true;
    s_search_window = 55;
    s_drag_is_active = true;
    me = (xcb_motion_notify_event_t *) event_no_match;
    me->event = 1;
    me->child = 2;
    loop_event_motion_notify(&ctx, &event_no_match);
    TAP_EQ_INT(s_search_handle_motion_calls, 0,
            "search is skipped when open but neither window field"
            " matches its own window");
    TAP_EQ_INT(s_place_manual_handle_motion_calls, 0,
            "an unmatched search still is not manual placement's"
            " concern");

    free(event_matching_event_field);
    free(event_matching_child_field);
    free(event_no_match);
}


/* With every menu closed and search either closed or unmatched, an
 * active drag claims the event as S_MOTION_TARGET_NONE and calls no
 * per-target handler at all, since drag_update already ran
 * unconditionally beforehand */
static void s_test_target_priority_drag_claims_none(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t *event = s_make_motion_event(3, 4);

    s_reset();
    s_drag_is_active = true;

    loop_event_motion_notify(&ctx, &event);

    TAP_EQ_INT(s_drag_update_calls, 1,
            "an active drag still receives the position through the"
            " unconditional drag_update call");
    TAP_EQ_INT(s_mouse_handle_motion_hover_calls, 0,
            "an active drag claiming the event means hover tracking"
            " is never separately called");
    TAP_EQ_INT(s_mouse_viewport_edge_check_calls, 0,
            "nor is the edge-pan check, hover's own sibling call");
    TAP_EQ_INT(s_place_manual_handle_motion_calls, 0,
            "an active drag claiming the event means manual placement"
            " is never separately called either");

    free(event);
}


/* With every menu closed and search either closed or unmatched, an
 * active background pan also claims the event as S_MOTION_TARGET_NONE
 * on its own, even with the plain drag flag left off, since the two
 * are checked with a plain 'or' */
static void s_test_target_priority_background_drag_claims_none(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t *event = s_make_motion_event(5, 6);

    s_reset();
    s_drag_background_is_active = true;

    loop_event_motion_notify(&ctx, &event);

    TAP_EQ_INT(s_drag_background_update_calls, 1,
            "an active background pan still receives the position"
            " through the unconditional drag_background_update call");
    TAP_EQ_INT(s_mouse_handle_motion_hover_calls, 0,
            "an active background pan claiming the event means hover"
            " tracking is never separately called");
    TAP_EQ_INT(s_mouse_viewport_edge_check_calls, 0,
            "nor is the edge-pan check, hover's own sibling call");

    free(event);
}


int main(void)
{
    TAP_PLAN(39);

    s_test_null_guards();
    s_test_no_queued_events_falls_to_hover();
    s_test_collapse_consecutive_motion_events();
    s_test_collapse_stops_at_non_motion_event();
    s_test_collapse_treats_synthetic_motion_as_motion();
    s_test_target_priority_manual_wins_all();
    s_test_target_priority_wincmenu_wins_rest();
    s_test_target_priority_rootmenu_wins_rest();
    s_test_target_priority_winlist_wins_rest();
    s_test_target_priority_iconmenu_wins_rest();
    s_test_target_priority_search_requires_window_match();
    s_test_target_priority_drag_claims_none();
    s_test_target_priority_background_drag_claims_none();

    return TAP_DONE();
}
