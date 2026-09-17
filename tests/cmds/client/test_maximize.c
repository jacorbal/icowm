/**
 * @file tests/cmds/client/test_maximize.c
 *
 * @brief Test battery for the per-axis maximize state machine
 *        (cmds/client/maximize.c)
 *
 * 'ccmd_client_maximize'/'_horz'/'_vert', 'ccmd_client_refill_
 * maximized', 'ccmd_client_demote_axis_state', and 'ccmd_client_
 * promote_axis_state' are exercised through the real, file-static
 * 's_ccmd_client_maximize_dir' and 's_ccmd_maximize_precheck', which
 * they are the only way to reach.  'ccmd_client_resolve_workarea' is
 * a test-controlled stand-in answering a fixed rectangle a test
 * registers first, so every case can pick apart the maximize math
 * without any real desktop/monitor lookup running underneath it;
 * 'ccmd_screen_dim' is a link-only stand-in refusing outright,
 * reached only on the fallback path once 'ccmd_client_resolve_
 * workarea' itself refuses, which no test here needs since a fixed
 * workarea is always registered.  'ccmd_client_apply_geometry' is a
 * recording stand-in, capturing what was actually asked for so a
 * test can check it directly rather than needing a live X
 * connection.  'client_decoration_layout_sync', 'ccmd_client_
 * restore', 'ccmd_client_unshade', 'ccmd_client_sync_states', 'wm_
 * get_client_desktop', 'wm_request_client_redraw', 'client_send_
 * synthetic_configure_notify', 'xcb_connection_get', and 'ccmd_
 * target_win' are link-only stand-ins: each is a side effect this
 * file's assertions do not need to observe directly, the geometry
 * and state-flag changes already being visible on 'client_td'
 * itself.
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

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <client.h>
#include <cmds/client/maximize.h>
#include <desktop.h>
#include <harness/tap.h>
#include <stage.h>


/** Fixed rectangle @a ccmd_client_resolve_workarea answers with,
 *  registered by @a s_set_workarea; 'is_ok' controls whether it
 *  succeeds or refuses outright */
static int32_t s_wa_x;
static int32_t s_wa_y;
static uint16_t s_wa_w;
static uint16_t s_wa_h;
static bool s_wa_ok;


/**
 * @brief Test-controlled stand-in for @a ccmd_client_resolve_workarea
 * @note Complexity: @e O(1)
 */
bool ccmd_client_resolve_workarea(client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y,
        uint16_t *restrict out_w, uint16_t *restrict out_h)
{
    (void) client;

    if (!s_wa_ok) {
        return false;
    }

    if (out_x != NULL) {
        *out_x = s_wa_x;
    }
    if (out_y != NULL) {
        *out_y = s_wa_y;
    }
    *out_w = s_wa_w;
    *out_h = s_wa_h;

    return true;
}


static void s_set_workarea(int32_t x, int32_t y, uint16_t w, uint16_t h)
{
    s_wa_x = x;
    s_wa_y = y;
    s_wa_w = w;
    s_wa_h = h;
    s_wa_ok = true;
}


/**
 * @brief Link-only stand-in for @a ccmd_screen_dim
 *
 * Reached only on the fallback path once 'ccmd_client_resolve_
 * workarea' itself refuses, which no test here needs since every
 * test registers a fixed workarea through @a s_set_workarea first
 *
 * @note Complexity: @e O(1)
 */
bool ccmd_screen_dim(const client_td *client, uint16_t *restrict out_w,
        uint16_t *restrict out_h)
{
    (void) client;
    (void) out_w;
    (void) out_h;

    return false;
}


/**
 * @brief Link-only stand-in for @a ccmd_target_win
 * @note Complexity: @e O(1)
 */
xcb_window_t ccmd_target_win(client_td *client)
{
    (void) client;
    return (xcb_window_t) 1;
}


/** The last geometry @a ccmd_client_apply_geometry recorded */
static uint16_t s_apply_mask;
static int32_t s_apply_x;
static int32_t s_apply_y;
static uint32_t s_apply_w;
static uint32_t s_apply_h;
static int s_apply_count;


/**
 * @brief Recording stand-in for @a ccmd_client_apply_geometry
 * @note Complexity: @e O(1)
 */
void ccmd_client_apply_geometry(client_td *client,
        xcb_window_t target, uint16_t mask,
        int32_t x, int32_t y, uint32_t w, uint32_t h,
        uint32_t border_width)
{
    (void) client;
    (void) target;
    (void) border_width;

    s_apply_mask = mask;
    s_apply_x = x;
    s_apply_y = y;
    s_apply_w = w;
    s_apply_h = h;
    s_apply_count++;
}


/**
 * @brief Link-only stand-in for @a client_decoration_layout_sync
 *
 * Reached only when 'client->frame != 0'; every client built here
 * has 'frame == 0' (undecorated), which also keeps 'client_border_
 * width' returning 0 unconditionally, since a real theme lookup
 * needs a live 'config_td' this file never builds
 *
 * @note Complexity: @e O(1)
 */
void client_decoration_layout_sync(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_restore
 * @note Complexity: @e O(1)
 */
void ccmd_client_restore(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_unshade
 * @note Complexity: @e O(1)
 */
void ccmd_client_unshade(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_sync_states
 * @note Complexity: @e O(1)
 */
void ccmd_client_sync_states(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a wm_get_client_desktop
 *
 * Reached only to resolve whether the client passed in is currently
 * active, purely for border-width accounting; a client whose
 * 'frame' is 0 already forces 'client_border_width' through its
 * 'client->config == NULL' branch instead, so this being always
 * @c NULL changes nothing any test here checks
 *
 * @note Complexity: @e O(1)
 */
desktop_td *wm_get_client_desktop(const client_td *client)
{
    (void) client;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a wm_request_client_redraw
 * @note Complexity: @e O(1)
 */
void wm_request_client_redraw(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a xcb_connection_get
 *
 * Reached only to hand a live connection to @a client_send_
 * synthetic_configure_notify, itself a link-only stand-in that never
 * dereferences what it is given
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/**
 * @brief Link-only stand-in for @a client_send_synthetic_configure_
 *        notify
 * @note Complexity: @e O(1)
 */
void client_send_synthetic_configure_notify(xcb_connection_t *connection,
        const client_td *client)
{
    (void) connection;
    (void) client;
}


static void s_reset(void)
{
    s_wa_ok = false;
    s_wa_x = 0;
    s_wa_y = 0;
    s_wa_w = 0;
    s_wa_h = 0;
    s_apply_mask = 0u;
    s_apply_x = 0;
    s_apply_y = 0;
    s_apply_w = 0u;
    s_apply_h = 0u;
    s_apply_count = 0;
}


/* A resizable, non-modal, normal client with a fresh 800x600
 * workarea maximizes to fill it entirely, on the first call */
static void s_test_maximize_both_fresh(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_RESIZABLE;
    s_set_workarea(10, 20, 800, 600);

    ccmd_client_maximize(&client);

    TAP_OK(client_is_maximized_any(&client),
            "a fresh maximize sets the maximized state");
    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 10,
            "left edge matches the workarea");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 20,
            "top edge matches the workarea");
    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 800,
            "width matches the workarea");
    TAP_EQ_INT(client.layout.geometry.cur.dim.h, 600,
            "height matches the workarea");
}


/* Maximizing a client already fully maximized toggles it back to its
 * saved, pre-maximize geometry instead */
static void s_test_maximize_both_toggles_off(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_RESIZABLE;
    client.layout.geometry.old.pos.x = 50;
    client.layout.geometry.old.pos.y = 60;
    client.layout.geometry.old.dim.w = 300;
    client.layout.geometry.old.dim.h = 200;
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED;
    s_set_workarea(0, 0, 1920, 1080);

    ccmd_client_maximize(&client);

    TAP_OK(!client_is_maximized_any(&client),
            "toggling an already fully maximized client demotes it");
    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 50,
            "restored to its saved x");
    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 300,
            "restored to its saved width");
}


/* A modal client is never maximizable, so the call is a silent no-op */
static void s_test_modal_client_refused(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_RESIZABLE | CLIENT_FLAG_MODAL;
    s_set_workarea(0, 0, 800, 600);

    ccmd_client_maximize(&client);

    TAP_OK(!client_is_maximized_any(&client),
            "a modal client is never maximized");
    TAP_EQ_INT(s_apply_count, 0,
            "no geometry is even applied for a refused client");
}


/* A non-resizable client is never maximizable either */
static void s_test_non_resizable_client_refused(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    s_set_workarea(0, 0, 800, 600);

    ccmd_client_maximize(&client);

    TAP_OK(!client_is_maximized_any(&client),
            "a non-resizable client is never maximized");
}


/* A null client is a silent no-op, never a crash */
static void s_test_null_client_is_noop(void)
{
    s_reset();
    s_set_workarea(0, 0, 800, 600);

    ccmd_client_maximize(NULL);
    ccmd_client_maximize_horz(NULL);
    ccmd_client_maximize_vert(NULL);
    ccmd_client_refill_maximized(NULL);
    ccmd_client_demote_axis_state(NULL, 1);
    ccmd_client_promote_axis_state(NULL, 1);

    TAP_OK(true, "every entry point tolerates a null client without"
            " crashing");
}


/* Maximizing horizontally alone leaves the vertical axis exactly as
 * it already was */
static void s_test_maximize_horz_only(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_RESIZABLE;
    client.layout.geometry.cur.pos.y = 77;
    client.layout.geometry.cur.dim.h = 111;
    s_set_workarea(0, 0, 1000, 800);

    ccmd_client_maximize_horz(&client);

    TAP_OK(client_is_maximized_horz(&client),
            "the horizontal axis is now maximized");
    TAP_OK(!client_is_maximized_vert(&client),
            "the vertical axis is untouched");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 77,
            "vertical position is left exactly as it was");
    TAP_EQ_INT(client.layout.geometry.cur.dim.h, 111,
            "vertical size is left exactly as it was");
    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 1000,
            "horizontal size now fills the workarea");
}


/* With the vertical axis already maximized, maximizing horizontally
 * too completes it to a full maximize rather than leaving two
 * independent single-axis states */
static void s_test_maximize_horz_completes_full(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_RESIZABLE;
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED_VERT;
    s_set_workarea(5, 6, 640, 480);

    ccmd_client_maximize_horz(&client);

    TAP_OK(client_is_maximized_horz(&client),
            "horizontal is now maximized too");
    TAP_OK(client_is_maximized_vert(&client),
            "vertical stays maximized");
    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 5,
            "horizontal position now comes from the workarea");
    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 640,
            "horizontal size now comes from the workarea");
}


/* Demoting one axis alone, with the other still maximized, restores
 * only that axis's saved geometry and leaves the other exactly as it
 * currently sits */
static void s_test_demote_single_axis(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_RESIZABLE;
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;
    client.layout.geometry.old.pos.x = 15;
    client.layout.geometry.old.dim.w = 250;
    client.layout.geometry.cur.pos.y = 33;
    client.layout.geometry.cur.dim.h = 444;
    s_set_workarea(0, 0, 1024, 768);

    ccmd_client_maximize_horz(&client);

    TAP_OK(!client_is_maximized_horz(&client),
            "demoting the maximized horizontal axis clears its state");
    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 15,
            "horizontal position restored from the saved geometry");
    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 250,
            "horizontal size restored from the saved geometry");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 33,
            "vertical position untouched by the horizontal demote");
}


/* An iconified client is restored before it is maximized, and a
 * shaded one is unshaded, both as a precondition rather than a
 * refusal */
static void s_test_iconified_and_shaded_precheck(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_RESIZABLE;
    client.properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    s_set_workarea(0, 0, 800, 600);

    ccmd_client_maximize(&client);

    TAP_OK(client_is_maximized_any(&client),
            "an iconified client is still maximized after being"
            " restored as a precondition");
}


/* A fullscreen client is refused outright: fullscreen and maximize
 * are mutually exclusive states here */
static void s_test_fullscreen_client_refused(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_RESIZABLE;
    client.properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;
    s_set_workarea(0, 0, 800, 600);

    ccmd_client_maximize(&client);

    TAP_OK(!client_is_maximized_any(&client),
            "a fullscreen client is never additionally maximized");
    TAP_EQ_INT(s_apply_count, 0, "no geometry is applied for it");
}


/* A locked client is refused outright too */
static void s_test_locked_client_refused(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_RESIZABLE;
    client.properties.flags |= CLIENT_FLAG_LOCKED;
    s_set_workarea(0, 0, 800, 600);

    ccmd_client_maximize(&client);

    TAP_OK(!client_is_maximized_any(&client),
            "a locked client is never maximized");
}


/* A maximum size hint clamps the frame size a maximize applies, never
 * stretching past what the client itself declared */
static void s_test_size_hint_clamp(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_RESIZABLE;
    client.hints_icccm.size.is_valid = true;
    client.hints_icccm.size.max.w = 400u;
    client.hints_icccm.size.max.h = 300u;
    s_set_workarea(0, 0, 1920, 1080);

    ccmd_client_maximize(&client);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 400,
            "width is clamped to the declared maximum");
    TAP_EQ_INT(client.layout.geometry.cur.dim.h, 300,
            "height is clamped to the declared maximum");
}


/* A workarea that cannot be resolved at all (and no screen fallback
 * either) leaves the client untouched instead of maximizing against
 * garbage geometry */
static void s_test_no_workarea_and_no_fallback_is_noop(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.flags |= CLIENT_FLAG_RESIZABLE;
    s_wa_ok = false;

    ccmd_client_maximize(&client);

    TAP_OK(!client_is_maximized_any(&client),
            "with no resolvable workarea and no screen fallback,"
            " nothing is maximized");
    TAP_EQ_INT(s_apply_count, 0, "and no geometry is ever applied");
}


/* ccmd_client_refill_maximized is a no-op on a client that is not
 * currently maximized on any axis */
static void s_test_refill_noop_when_not_maximized(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 5;
    client.layout.geometry.cur.pos.y = 6;
    s_set_workarea(999, 999, 100, 100);

    ccmd_client_refill_maximized(&client);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 5,
            "an unmaximized client's geometry is left untouched");
    TAP_EQ_INT(s_apply_count, 0, "and no geometry is ever applied");
}


/* ccmd_client_refill_maximized re-fills a maximized client's
 * geometry against a workarea that has since changed, touching only
 * the axis (or axes) actually maximized */
static void s_test_refill_updates_maximized_axis(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;
    client.layout.geometry.cur.pos.x = 0;
    client.layout.geometry.cur.dim.w = 800;
    client.layout.geometry.cur.pos.y = 42;
    client.layout.geometry.cur.dim.h = 99;
    /* The workarea has since grown */
    s_set_workarea(0, 0, 1600, 900);

    ccmd_client_refill_maximized(&client);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 1600,
            "the maximized horizontal axis is refilled to the new"
            " workarea width");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 42,
            "the untouched vertical axis keeps its own position");
    TAP_EQ_INT(client.layout.geometry.cur.dim.h, 99,
            "the untouched vertical axis keeps its own size");
}


/* ccmd_client_demote_axis_state clears only the named axis's state
 * bit, touching no geometry at all */
static void s_test_demote_axis_state_bit_only(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED;

    ccmd_client_demote_axis_state(&client, 1);

    TAP_OK(!client_is_maximized_horz(&client),
            "demoting the horizontal axis state clears its own bit");
    TAP_OK(client_is_maximized_vert(&client),
            "the vertical axis bit is untouched");
    TAP_EQ_INT(s_apply_count, 0,
            "no geometry is applied, this is a pure state change");
}


/* ccmd_client_promote_axis_state sets only the named axis's state
 * bit, the exact inverse of the demote above */
static void s_test_promote_axis_state_bit_only(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));

    ccmd_client_promote_axis_state(&client, 2);

    TAP_OK(client_is_maximized_vert(&client),
            "promoting the vertical axis state sets its own bit");
    TAP_OK(!client_is_maximized_horz(&client),
            "the horizontal axis bit is untouched");
}


int main(void)
{
    TAP_PLAN(43);

    s_test_maximize_both_fresh();
    s_test_maximize_both_toggles_off();
    s_test_modal_client_refused();
    s_test_non_resizable_client_refused();
    s_test_null_client_is_noop();
    s_test_maximize_horz_only();
    s_test_maximize_horz_completes_full();
    s_test_demote_single_axis();
    s_test_iconified_and_shaded_precheck();
    s_test_fullscreen_client_refused();
    s_test_locked_client_refused();
    s_test_size_hint_clamp();
    s_test_no_workarea_and_no_fallback_is_noop();
    s_test_refill_noop_when_not_maximized();
    s_test_refill_updates_maximized_axis();
    s_test_demote_axis_state_bit_only();
    s_test_promote_axis_state_bit_only();

    return TAP_DONE();
}
