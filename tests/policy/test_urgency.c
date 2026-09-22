/**
 * @file tests/policy/test_urgency.c
 *
 * @brief Test battery for the urgent-client attention blink
 *
 * All blink state (s_blink_on, s_last_toggle, s_has_urgent) is
 * private and static to policy/urgency.c itself, with no reset
 * entry point of its own, so this file's tests run as one
 * continuous narrative in a fixed order (see main) rather than as
 * independent cases, exactly mirroring how a real caller would
 * drive it tick by tick.  A short, fixed blink_interval_ms (5) keeps
 * the real time this needs to wait for small without making the
 * sleep-vs-threshold margin unreliable.
 *
 * stage_desktop_get (stage.c) and the three render functions
 * s_repaint_urgent_clients calls are stubbed below as controllable,
 * call-counting stand-ins, the same pattern already used for
 * test_lookup.c and test_resolve.c.  s_any_client_urgent (a
 * different function in the same file) instead walks stage->
 * desktops directly, so that field is also a real cdlist here, with
 * the one desktop inserted into it alongside the s_desktops_by_id
 * lookup array.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* nanosleep */


/* System includes */
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <stage.h>

/* Local includes */
#include <harness/tap.h>
#include <policy/urgency.h>


/**
 * @brief Link-only stand-in for @a stage_desktop_walk_all
 *
 * Walks the stage's own list here rather than linking
 * @c stage/desktops.c, which would bring a desktop's whole teardown
 * along with it and clash with this file's own stand-ins.
 *
 * @param stage Stage whose desktops to visit
 * @param visit   Called once per desktop
 * @param data    Handed to @p visit untouched
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops on
 *       @p stage
 */
void stage_desktop_walk_all(const stage_td *stage,
        stage_desktop_visitor_fn visit, void *data)
{
    cdlist_item_td *node;

    if (stage == NULL || stage->desktops == NULL || visit == NULL) {
        return;
    }

    cdlist_foreach(stage->desktops, node) {
        desktop_td *const desktop = (desktop_td *) cdlist_data(node);

        if (desktop != NULL) {
            visit(desktop, data);
        }
    }
}


static desktop_td *s_desktops_by_id[4];

desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    if (desktop_id >= 4u) {
        return NULL;
    }
    return s_desktops_by_id[desktop_id];
}


static int s_render_client_calls = 0;
static int s_render_icon_calls = 0;
static int s_render_flush_calls = 0;

void desktop_render_one_client(desktop_td *desktop, client_td *client,
        bool is_current)
{
    (void) desktop;
    (void) client;
    (void) is_current;
    s_render_client_calls++;
}

void ri_render_client_icon(desktop_td *desktop, client_td *client,
        bool is_current)
{
    (void) desktop;
    (void) client;
    (void) is_current;
    s_render_icon_calls++;
}

void stage_render_flush(stage_td *stage)
{
    (void) stage;
    s_render_flush_calls++;
}


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
    return ((const client_td *) key1)->id ==
        ((const client_td *) key2)->id;
}


static void s_sleep_ms(long ms)
{
    struct timespec req;

    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&req, NULL);
}


int main(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;
    config_td config;
    list_td *stages;

    TAP_PLAN(12);

    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));

    client.id = 1;
    /* Not hidden: s_repaint_urgent_clients takes the desktop_render_
     * one_client branch, not the iconified/ri_render_client_icon one */
    stage.desktop_count = 1u;
    stage.desktop_cur = 0u;
    desktop.clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    s_desktops_by_id[0] = &desktop;
    stage.desktops = cdlist_init(NULL);
    cdlist_ins_next(stage.desktops, NULL, &desktop);
    config.a11y.urgency.blink_interval_ms = 5u;
    config.a11y.urgency.sound_bell = false;

    stages = list_init(NULL);
    list_ins_next(stages, NULL, &stage);

    /* 1: nothing urgent yet at all */
    TAP_OK(!urgency_blink_is_on(), "starts off, before any tick at all");
    urgency_blink_tick(stages, &config);
    TAP_EQ_INT(urgency_blink_ms_remaining(&config), -1,
            "no urgent client: nothing to wait for");

    /* 3: first tick with an urgent client only arms the timer, does
     * not itself flip the phase */
    ohtbl_insert(desktop.clients, &client);
    client.properties.flags |= CLIENT_FLAG_URGENT;
    urgency_blink_tick(stages, &config);
    TAP_OK(!urgency_blink_is_on(),
            "an urgent client's first tick only arms the timer");
    TAP_OK(urgency_blink_ms_remaining(&config) >= 0,
            "now something to wait for, since a client is urgent");

    /* Wait past the (5ms) interval, then tick again: this is the
     * one that actually flips the phase and repaints */
    s_sleep_ms(20);
    s_render_client_calls = 0;
    s_render_flush_calls = 0;
    urgency_blink_tick(stages, &config);
    TAP_OK(urgency_blink_is_on(),
            "after the interval elapses, the phase flips on");
    TAP_OK(s_render_client_calls == 1,
            "the urgent, visible client was repainted exactly once");
    TAP_OK(s_render_flush_calls == 1,
            "the stage was flushed once after repainting");

    /* Ticking again immediately (no sleep) must not flip the phase
     * back, since the interval has not elapsed again yet */
    urgency_blink_tick(stages, &config);
    TAP_OK(urgency_blink_is_on(),
            "ticking again immediately does not flip the phase back");

    /* The client stops being urgent: the next tick clears the
     * blink state entirely */
    client.properties.flags &= ~(uint32_t) CLIENT_FLAG_URGENT;
    urgency_blink_tick(stages, &config);
    TAP_OK(!urgency_blink_is_on(),
            "once nothing is urgent anymore, the blink turns off");
    TAP_EQ_INT(urgency_blink_ms_remaining(&config), -1,
            "and there is nothing left to wait for");

    /* A NULL config falls back to the built-in interval, and never
     * sounds a bell, without crashing */
    client.properties.flags |= CLIENT_FLAG_URGENT;
    urgency_blink_tick(stages, NULL);
    TAP_OK(true, "a NULL config is handled without crashing");
    TAP_OK(urgency_blink_ms_remaining(NULL) >= 0,
            "ms_remaining also accepts a NULL config");

    ohtbl_destroy(desktop.clients);
    cdlist_destroy(stage.desktops);
    list_destroy(stages);
    s_desktops_by_id[0] = NULL;

    return TAP_DONE();
}
