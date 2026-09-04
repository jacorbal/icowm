/**
 * @file tests/input/mouse/drag/test_snap.c
 *
 * @brief Test battery for edge and peer-window snap math during a drag
 *
 * drag_snap_move and drag_snap_resize (input/mouse/drag/snap.c) read
 * the global drag state, s_drag (input/mouse/drag/internal.h), and
 * walk a real desktop's real stacking order (policy/stacking.c,
 * backed by adt/cdlist.c and desktop/dfind.c) to find candidate
 * windows to snap against, so this file links every one of those real,
 * so the walk this file exercises is the exact same one 'drag.c'
 * itself would trigger.  Storage for s_drag lives in drag.c, which
 * this file never links, so it is defined once here instead, the same
 * way drag.c itself would define it.
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
#include <stdint.h>
#include <string.h>

/* ADT includes */
#include <adt/ohtbl.h>

/* Project includes */
#include <client.h>
#include <desktop.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/snap.h>
#include <policy/stacking.h>


/** Singleton drag state; storage normally lives in drag.c, which this
 *  file never links, so it is defined here instead */
drag_state_td s_drag;


static size_t s_id_hash1(const void *key)
{
    return (size_t) ((const client_td *) key)->id;
}


static size_t s_id_hash2(const void *key)
{
    (void) key;
    return 1u;
}


static bool s_id_match(const void *key1, const void *key2)
{
    return ((const client_td *) key1)->id == ((const client_td *) key2)->id;
}


/** Every client and desktop this file calloc's, freed in one place by
 *  @a s_teardown rather than at each test's own end */
#define MAX_TEST_CLIENTS (16)
static client_td *s_owned_clients[MAX_TEST_CLIENTS];
static int s_owned_clients_used;
static desktop_td *s_owned_desktop;


static desktop_td *s_make_desktop(void)
{
    desktop_td *desktop = calloc(1, sizeof(*desktop));

    desktop->clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    (void) stacking_create(desktop);
    s_owned_desktop = desktop;

    return desktop;
}


static client_td *s_make_client(uint32_t id, int32_t x, int32_t y,
        uint32_t w, uint32_t h)
{
    client_td *client = calloc(1, sizeof(*client));

    client->id = (xcb_window_t) id;
    client->layout.geometry.cur.pos.x = x;
    client->layout.geometry.cur.pos.y = y;
    client->layout.geometry.cur.dim.w = w;
    client->layout.geometry.cur.dim.h = h;
    s_owned_clients[s_owned_clients_used] = client;
    s_owned_clients_used++;

    return client;
}


/* Put a client on the shared stacking order, and the desktop's own
 * table, which the walk needs to confirm a candidate really belongs
 * to this desktop */
static void s_desktop_add_client(desktop_td *desktop, client_td *client)
{
    ohtbl_insert(desktop->clients, client);
    (void) stacking_add(desktop, client);
}


static void s_reset(void)
{
    static client_td dragged;

    memset(&s_drag, 0, sizeof(s_drag));
    memset(&dragged, 0, sizeof(dragged));
    dragged.id = 1u;
    s_drag.client = &dragged;
    s_owned_clients_used = 0;
    s_owned_desktop = NULL;
}


static void s_teardown(void)
{
    if (s_owned_desktop != NULL) {
        stacking_destroy(s_owned_desktop);
        ohtbl_destroy(s_owned_desktop->clients);
        free(s_owned_desktop);
        s_owned_desktop = NULL;
    }

    for (int i = 0; i < s_owned_clients_used; ++i) {
        free(s_owned_clients[i]);
    }
    s_owned_clients_used = 0;
}


/* NULL x or y makes drag_snap_move a no-op, not a crash */
static void s_test_move_null_pointers_no_crash(void)
{
    int32_t x = 10;
    int32_t y = 10;

    s_reset();

    drag_snap_move(NULL, &y, 100u, 100u);
    drag_snap_move(&x, NULL, 100u, 100u);

    TAP_EQ_INT(x, 10, "x untouched when y is NULL");
    TAP_EQ_INT(y, 10, "y untouched when x is NULL");

    s_teardown();
}


/* No window snap distance and no screen snap distance: coordinates
 * pass through completely unchanged */
static void s_test_move_no_snap_configured_is_identity(void)
{
    int32_t x = 37;
    int32_t y = 53;
    desktop_td *desktop;
    client_td *other;

    s_reset();
    desktop = s_make_desktop();
    other = s_make_client(2u, 200u, 200u, 50u, 50u);
    s_desktop_add_client(desktop, other);
    s_drag.desktop = desktop;
    s_drag.snap_window = 0u;
    s_drag.snap_screen = 0u;

    drag_snap_move(&x, &y, 100u, 100u);

    TAP_EQ_INT(x, 37, "snap_window/snap_screen both 0: x unchanged");
    TAP_EQ_INT(y, 53, "and y unchanged");

    s_teardown();
}


/* A window whose left edge sits just within snap distance of the
 * dragged window's right edge pulls the dragged window's right edge
 * flush against it */
static void s_test_move_snaps_right_edge_to_neighbor_left_edge(void)
{
    int32_t x = 90;
    int32_t y = 100;
    desktop_td *desktop;
    client_td *other;

    s_reset();
    desktop = s_make_desktop();
    /* Dragged window (100x100) at (90,100): right edge at 190, top
     * 100, bottom 200.  Neighbor's left edge at 195, overlapping the
     * dragged window's vertical span exactly: within 10px snap. */
    other = s_make_client(2u, 195u, 100u, 50u, 100u);
    s_desktop_add_client(desktop, other);
    s_drag.desktop = desktop;
    s_drag.snap_window = 10u;

    drag_snap_move(&x, &y, 100u, 100u);

    TAP_EQ_INT(x, 95,
            "right edge (was 190) snaps flush to neighbor's left edge"
            " (195), moving x from 90 to 95");
    TAP_EQ_INT(y, 100, "y is untouched: no vertical candidate applies");

    s_teardown();
}


/* A neighbor entirely outside the vertical overlap range never
 * contributes a horizontal snap candidate, even if its left edge is
 * numerically close */
static void s_test_move_ignores_neighbor_without_axis_overlap(void)
{
    int32_t x = 90;
    int32_t y = 100;
    desktop_td *desktop;
    client_td *other;

    s_reset();
    desktop = s_make_desktop();
    /* Same left edge as the snapping case above (195), but its whole
     * vertical span (500..600) is far from the dragged window's
     * (100..200), well past the snap_window of 10 */
    other = s_make_client(2u, 195u, 500u, 50u, 100u);
    s_desktop_add_client(desktop, other);
    s_drag.desktop = desktop;
    s_drag.snap_window = 10u;

    drag_snap_move(&x, &y, 100u, 100u);

    TAP_EQ_INT(x, 90,
            "no vertical overlap with the neighbor: no horizontal snap"
            " applied at all");

    s_teardown();
}


/* The dragged client itself is skipped by the walk, never snapping
 * against its own edges */
static void s_test_move_never_snaps_against_self(void)
{
    int32_t x = 90;
    int32_t y = 100;
    desktop_td *desktop;

    s_reset();
    desktop = s_make_desktop();
    /* s_drag.client (id 1) placed on the desktop at a position that
     * would otherwise be a perfect snap candidate for itself */
    s_drag.client->layout.geometry.cur.pos.x = 195;
    s_drag.client->layout.geometry.cur.pos.y = 100;
    s_drag.client->layout.geometry.cur.dim.w = 50u;
    s_drag.client->layout.geometry.cur.dim.h = 100u;
    s_desktop_add_client(desktop, s_drag.client);
    s_drag.desktop = desktop;
    s_drag.snap_window = 10u;

    drag_snap_move(&x, &y, 100u, 100u);

    TAP_EQ_INT(x, 90, "the dragged client is excluded from its own"
            " snap candidates");

    s_teardown();
}


/* A hidden neighbor is skipped by the walk, since it is not something
 * the user can actually see an edge of */
static void s_test_move_ignores_hidden_neighbor(void)
{
    int32_t x = 90;
    int32_t y = 100;
    desktop_td *desktop;
    client_td *hidden;

    s_reset();
    desktop = s_make_desktop();
    hidden = s_make_client(2u, 195u, 100u, 50u, 100u);
    hidden->properties.flags |= CLIENT_FLAG_HIDDEN;
    s_desktop_add_client(desktop, hidden);
    s_drag.desktop = desktop;
    s_drag.snap_window = 10u;

    drag_snap_move(&x, &y, 100u, 100u);

    TAP_EQ_INT(x, 90, "a hidden neighbor never contributes a snap"
            " candidate");

    s_teardown();
}


/* An iconified neighbor is skipped by the walk for the same reason a
 * hidden one is */
static void s_test_move_ignores_iconified_neighbor(void)
{
    int32_t x = 90;
    int32_t y = 100;
    desktop_td *desktop;
    client_td *iconified;

    s_reset();
    desktop = s_make_desktop();
    iconified = s_make_client(2u, 195u, 100u, 50u, 100u);
    iconified->properties.state |= CLIENT_STATE_ICONIFIED;
    s_desktop_add_client(desktop, iconified);
    s_drag.desktop = desktop;
    s_drag.snap_window = 10u;

    drag_snap_move(&x, &y, 100u, 100u);

    TAP_EQ_INT(x, 90, "an iconified neighbor never contributes a snap"
            " candidate");

    s_teardown();
}


/* A candidate exactly at the snap distance still snaps: the range
 * check is inclusive, not a strict less-than */
static void s_test_move_snap_at_exact_distance_still_applies(void)
{
    int32_t x = 90;
    int32_t y = 100;
    desktop_td *desktop;
    client_td *other;

    s_reset();
    desktop = s_make_desktop();
    /* Dragged right edge at 190; neighbor's left edge at 200 (exactly
     * 10px away): snap_window of 10 should still apply, inclusive */
    other = s_make_client(2u, 200u, 100u, 50u, 100u);
    s_desktop_add_client(desktop, other);
    s_drag.desktop = desktop;
    s_drag.snap_window = 10u;

    drag_snap_move(&x, &y, 100u, 100u);

    TAP_EQ_INT(x, 100, "exactly-10px gap still counts as within"
            " snap distance (inclusive boundary)");

    s_teardown();
}


/* A candidate one pixel past the snap distance never applies */
static void s_test_move_snap_one_past_distance_does_not_apply(void)
{
    int32_t x = 90;
    int32_t y = 100;
    desktop_td *desktop;
    client_td *other;

    s_reset();
    desktop = s_make_desktop();
    /* Dragged right edge at 190; neighbor's left edge at 201 (11px
     * away): one past a snap_window of 10 */
    other = s_make_client(2u, 201u, 100u, 50u, 100u);
    s_desktop_add_client(desktop, other);
    s_drag.desktop = desktop;
    s_drag.snap_window = 10u;

    drag_snap_move(&x, &y, 100u, 100u);

    TAP_EQ_INT(x, 90, "11px gap against a snap_window of 10 does not"
            " snap");

    s_teardown();
}


/* Among several candidates, the closest delta on each axis wins,
 * regardless of stacking order */
static void s_test_move_closest_candidate_wins(void)
{
    int32_t x = 90;
    int32_t y = 100;
    desktop_td *desktop;
    client_td *far_one;
    client_td *near_one;

    s_reset();
    desktop = s_make_desktop();
    /* Dragged right edge at 190.  far_one's left edge at 199 (9px
     * away); near_one's left edge at 194 (4px away): the closer one
     * must win even though it is added second */
    far_one = s_make_client(2u, 199u, 100u, 50u, 100u);
    near_one = s_make_client(3u, 194u, 100u, 50u, 100u);
    s_desktop_add_client(desktop, far_one);
    s_desktop_add_client(desktop, near_one);
    s_drag.desktop = desktop;
    s_drag.snap_window = 10u;

    drag_snap_move(&x, &y, 100u, 100u);

    TAP_EQ_INT(x, 94, "the smaller-magnitude delta (4, not 9) wins");

    s_teardown();
}


/* Screen-edge snapping pulls the dragged window's left edge flush
 * against a monitor work area's left edge */
static void s_test_move_snaps_to_monitor_left_edge(void)
{
    int32_t x = 5;
    int32_t y = 50;
    desktop_td *desktop;

    s_reset();
    desktop = s_make_desktop();
    desktop->monitor_workarea_count = 1u;
    desktop->monitor_workareas[0].pos.x = 0;
    desktop->monitor_workareas[0].pos.y = 0;
    desktop->monitor_workareas[0].dim.w = 1920u;
    desktop->monitor_workareas[0].dim.h = 1080u;
    s_drag.desktop = desktop;
    s_drag.snap_screen = 10u;

    drag_snap_move(&x, &y, 100u, 100u);

    TAP_EQ_INT(x, 0, "left edge at x=5 snaps flush to the monitor's"
            " left edge (0), within a 10px screen snap");

    s_teardown();
}


/* A monitor whose cross-axis span does not overlap the dragged
 * window's own cross-axis span is never a candidate, even if its
 * edge coordinate would otherwise be numerically close */
static void s_test_move_ignores_monitor_without_cross_overlap(void)
{
    int32_t x = 5;
    int32_t y = 2000;
    desktop_td *desktop;

    s_reset();
    desktop = s_make_desktop();
    /* Monitor's work area spans y=[0,1080); dragged window sits at
     * y=2000..2100, nowhere near it */
    desktop->monitor_workarea_count = 1u;
    desktop->monitor_workareas[0].pos.x = 0;
    desktop->monitor_workareas[0].pos.y = 0;
    desktop->monitor_workareas[0].dim.w = 1920u;
    desktop->monitor_workareas[0].dim.h = 1080u;
    s_drag.desktop = desktop;
    s_drag.snap_screen = 10u;

    drag_snap_move(&x, &y, 100u, 100u);

    TAP_EQ_INT(x, 5, "no cross-axis overlap with the only monitor:"
            " no screen snap applied");

    s_teardown();
}


/* drag_snap_resize: NULL pointers make it a no-op, not a crash */
static void s_test_resize_null_pointers_no_crash(void)
{
    int32_t x = 10;
    int32_t y = 10;
    uint32_t w = 100u;
    uint32_t h = 100u;

    s_reset();

    drag_snap_resize(NULL, &y, &w, &h);
    drag_snap_resize(&x, NULL, &w, &h);
    drag_snap_resize(&x, &y, NULL, &h);
    drag_snap_resize(&x, &y, &w, NULL);

    TAP_EQ_INT(x, 10, "x untouched across every NULL-argument call");
    TAP_EQ_INT(w, 100, "width untouched too");

    s_teardown();
}


/* Resize with the right edge anchored (dragging from the left): the
 * left edge snapping against a neighbor moves x and shrinks width
 * together, keeping the right edge fixed */
static void s_test_resize_anchor_right_moves_x_and_width(void)
{
    int32_t x = 100;
    int32_t y = 100;
    uint32_t w = 100u;
    uint32_t h = 100u;
    desktop_td *desktop;
    client_td *other;

    s_reset();
    desktop = s_make_desktop();
    /* Dragged window's left edge at 100; neighbor's right edge at 95
     * (5px away): within a 10px snap_window */
    other = s_make_client(2u, 45u, 100u, 50u, 100u);
    s_desktop_add_client(desktop, other);
    s_drag.desktop = desktop;
    s_drag.snap_window = 10u;
    s_drag.is_anchor_right = true;

    drag_snap_resize(&x, &y, &w, &h);

    TAP_EQ_INT(x, 95, "left edge snaps flush to the neighbor's right"
            " edge (95)");
    TAP_EQ_INT(w, 105, "width grows to keep the right edge (200)"
            " fixed while x moves left by 5");

    s_teardown();
}


/* Resize with the right edge free (dragging from the right, the
 * default, non-anchored case): only width changes, x stays fixed */
static void s_test_resize_anchor_left_only_changes_width(void)
{
    int32_t x = 100;
    int32_t y = 100;
    uint32_t w = 100u;
    uint32_t h = 100u;
    desktop_td *desktop;
    client_td *other;

    s_reset();
    desktop = s_make_desktop();
    /* Dragged window's right edge at 200; neighbor's left edge at 205
     * (5px away): within a 10px snap_window */
    other = s_make_client(2u, 205u, 100u, 50u, 100u);
    s_desktop_add_client(desktop, other);
    s_drag.desktop = desktop;
    s_drag.snap_window = 10u;
    s_drag.is_anchor_right = false;

    drag_snap_resize(&x, &y, &w, &h);

    TAP_EQ_INT(x, 100, "x is untouched: the left edge is not the one"
            " being dragged");
    TAP_EQ_INT(w, 105, "width grows so the right edge reaches the"
            " neighbor's left edge (205)");

    s_teardown();
}


/* A resize snap that would shrink a window below the minimum
 * dimension is clamped there by geom_dim_clamp, never going smaller
 * or wrapping negative */
static void s_test_resize_clamps_width_at_minimum(void)
{
    int32_t x = 100;
    int32_t y = 100;
    uint32_t w = 5u;
    uint32_t h = 100u;
    desktop_td *desktop;
    client_td *other;

    s_reset();
    desktop = s_make_desktop();
    /* Dragged window (5 wide) at x=100, right edge at 105; neighbor's
     * left edge at 102, well inside the window's own current span:
     * this would drive width negative without clamping */
    other = s_make_client(2u, 102u, 100u, 50u, 100u);
    s_desktop_add_client(desktop, other);
    s_drag.desktop = desktop;
    s_drag.snap_window = 10u;
    s_drag.is_anchor_right = false;

    drag_snap_resize(&x, &y, &w, &h);

    TAP_OK(w >= 1u, "width never wraps to a huge unsigned value or"
            " goes negative, clamped to at least the minimum");

    s_teardown();
}


/* Resize screen-edge snapping on the bottom edge (anchor_bottom
 * false, so height is free to grow) pulls the window's bottom flush
 * against the monitor's bottom work area edge */
static void s_test_resize_snaps_to_monitor_bottom_edge(void)
{
    int32_t x = 100;
    int32_t y = 100;
    uint32_t w = 100u;
    uint32_t h = 975u;
    desktop_td *desktop;

    s_reset();
    desktop = s_make_desktop();
    desktop->monitor_workarea_count = 1u;
    desktop->monitor_workareas[0].pos.x = 0;
    desktop->monitor_workareas[0].pos.y = 0;
    desktop->monitor_workareas[0].dim.w = 1920u;
    desktop->monitor_workareas[0].dim.h = 1080u;
    s_drag.desktop = desktop;
    s_drag.snap_screen = 10u;
    s_drag.is_anchor_bottom = false;

    drag_snap_resize(&x, &y, &w, &h);

    TAP_EQ_INT(h, 980, "bottom edge (was 1075) snaps flush to the"
            " monitor's bottom work area edge (1080)");

    s_teardown();
}


int main(void)
{
    TAP_PLAN(23);

    s_test_move_null_pointers_no_crash();
    s_test_move_no_snap_configured_is_identity();
    s_test_move_snaps_right_edge_to_neighbor_left_edge();
    s_test_move_ignores_neighbor_without_axis_overlap();
    s_test_move_never_snaps_against_self();
    s_test_move_ignores_hidden_neighbor();
    s_test_move_ignores_iconified_neighbor();
    s_test_move_snap_at_exact_distance_still_applies();
    s_test_move_snap_one_past_distance_does_not_apply();
    s_test_move_closest_candidate_wins();
    s_test_move_snaps_to_monitor_left_edge();
    s_test_move_ignores_monitor_without_cross_overlap();
    s_test_resize_null_pointers_no_crash();
    s_test_resize_anchor_right_moves_x_and_width();
    s_test_resize_anchor_left_only_changes_width();
    s_test_resize_clamps_width_at_minimum();
    s_test_resize_snaps_to_monitor_bottom_edge();

    return TAP_DONE();
}
