/**
 * @file tests/rules/test_apply.c
 *
 * @brief Test battery for merging and applying a matched rule's
 *        actions to a client
 *
 * rules_apply (rules/apply.c) is the merge-and-dispatch engine that
 * sits directly on top of rules/match.c's ri_when_matches and
 * ri_client_matches, its dependencia hermana already covered by
 * tests/rules/test_match.c, so this file links that same real source
 * (plus logger.c and safestr.c, which it in turn needs) to exercise
 * the exact matching path rules_apply itself calls, rather than
 * duplicating its own copy of that logic.  Everything past matching,
 * every enact_client and ccmd_client action, focus_apply,
 * ipc_broadcast_event, cJSON's two calls, client_size_constrain, and
 * client_decoration_layout_sync, is a heavy side-effecting primitive
 * belonging to other subsystems entirely (XCB requests, IPC sockets,
 * JSON trees), so each is a recording stand-in here instead, letting
 * every scenario below assert on rules_apply's own merge order and
 * dispatch logic directly.  wm_rules and wm_config are accessor
 * stand-ins over a wm_td this file never actually builds (the type is
 * opaque outside wm.c): any non-null pointer serves as the opaque
 * handle, with the two accessors simply returning whichever
 * rules_td or config_td each scenario set up beforehand instead of
 * actually dereferencing it.
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

/* JSON includes */
#include <cjson/cJSON.h>

/* Command includes */
#include <cmds/client/flags.h>
#include <cmds/client/move.h>
#include <cmds/client/state.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <enact.h>
#include <ipc.h>
#include <policy/focus.h>
#include <stage.h>
#include <wm.h>

/* Local includes */
#include <harness/tap.h>
#include <rules.h>
#include <rules/internal.h>


/** A non-null opaque handle standing in for a real wm_td, which this
 *  file never actually builds, since the type is opaque outside wm.c
 *  itself; wm_rules and wm_config below never dereference it, only
 *  ignore it and return their own fixtures, so a dummy address is
 *  enough */
static int s_fake_wm_storage;
static wm_td *const s_fake_wm = (wm_td *) &s_fake_wm_storage;

/** rules_td/config_td the wm_rules/wm_config stand-ins hand back */
static rules_td s_rules;
static config_td s_config;

/** Call counters and last-seen arguments, reset by s_reset before
 *  each scenario */
static int s_call_enact_client_layer_above;
static int s_call_enact_client_layer_below;
static int s_call_enact_client_layer_normal;
static int s_call_enact_client_pin;
static int s_call_enact_client_unpin;
static int s_call_enact_client_stick;
static int s_call_enact_client_unstick;
static int s_call_enact_client_toggle_decorate;
static int s_call_ccmd_client_set_opacity_active;
static uint8_t s_last_opacity_active;
static int s_call_ccmd_client_set_opacity_inactive;
static uint8_t s_last_opacity_inactive;
static int s_call_enact_client_fullscreen;
static int s_call_enact_client_unfullscreen;
static int s_call_enact_client_maximize;
static int s_call_enact_client_shade;
static int s_call_enact_client_unshade;
static int s_call_enact_client_hide;
static int s_call_enact_client_unhide;
static int s_call_enact_client_iconify;
static int s_call_enact_client_restore;
static int s_call_enact_desktop_client_send;
static int s_call_ccmd_client_apply_geometry;
static uint16_t s_last_geometry_mask;
static int32_t s_last_geometry_x;
static int32_t s_last_geometry_y;
static uint32_t s_last_geometry_w;
static uint32_t s_last_geometry_h;
static int s_call_client_size_constrain;
static int s_call_client_decoration_layout_sync;
static int s_call_focus_apply;
static int s_call_ipc_broadcast_event;
static uint32_t s_last_ipc_event_type;
static desktop_td *s_desktop_get_result;


/**
 * @brief Recording stand-in for @a wm_rules
 *
 * @note Complexity: @e O(1)
 */
rules_td *wm_rules(const wm_td *wm)
{
    (void) wm;
    return &s_rules;
}


/**
 * @brief Recording stand-in for @a wm_config
 *
 * @note Complexity: @e O(1)
 */
config_td *wm_config(const wm_td *wm)
{
    (void) wm;
    return &s_config;
}


/**
 * @brief Link-only stand-in for @a wm_stages, never exercised for
 *        its return value by any scenario below
 *
 * @note Complexity: @e O(1)
 */
list_td *wm_stages(const wm_td *wm)
{
    (void) wm;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a stage_desktop_get
 *
 * @note Complexity: @e O(1)
 */
desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;
    return s_desktop_get_result;
}


/**
 * @brief Recording stand-in for @a enact_desktop_client_send
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_client_send(const desktop_td *desktop,
        client_td *client, desktop_td *target)
{
    (void) desktop;
    (void) client;
    (void) target;
    s_call_enact_desktop_client_send++;
}


/**
 * @brief Recording stand-in for @a enact_client_layer_above
 *
 * @note Complexity: @e O(1)
 */
void enact_client_layer_above(client_td *client)
{
    (void) client;
    s_call_enact_client_layer_above++;
}


/**
 * @brief Recording stand-in for @a enact_client_layer_below
 *
 * @note Complexity: @e O(1)
 */
void enact_client_layer_below(client_td *client)
{
    (void) client;
    s_call_enact_client_layer_below++;
}


/**
 * @brief Recording stand-in for @a enact_client_layer_normal
 *
 * @note Complexity: @e O(1)
 */
void enact_client_layer_normal(client_td *client)
{
    (void) client;
    s_call_enact_client_layer_normal++;
}


/**
 * @brief Recording stand-in for @a enact_client_pin
 *
 * @note Complexity: @e O(1)
 */
void enact_client_pin(client_td *client)
{
    (void) client;
    s_call_enact_client_pin++;
}


/**
 * @brief Recording stand-in for @a enact_client_unpin
 *
 * @note Complexity: @e O(1)
 */
void enact_client_unpin(client_td *client)
{
    (void) client;
    s_call_enact_client_unpin++;
}


/**
 * @brief Recording stand-in for @a enact_client_stick
 *
 * @note Complexity: @e O(1)
 */
void enact_client_stick(client_td *client)
{
    (void) client;
    s_call_enact_client_stick++;
}


/**
 * @brief Recording stand-in for @a enact_client_unstick
 *
 * @note Complexity: @e O(1)
 */
void enact_client_unstick(client_td *client)
{
    (void) client;
    s_call_enact_client_unstick++;
}


/**
 * @brief Recording stand-in for @a enact_client_toggle_decorate
 *
 * @note Complexity: @e O(1)
 */
void enact_client_toggle_decorate(client_td *client)
{
    (void) client;
    s_call_enact_client_toggle_decorate++;
}


/**
 * @brief Recording stand-in for @a ccmd_client_set_opacity_active
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_set_opacity_active(client_td *client, uint8_t percent)
{
    (void) client;
    s_call_ccmd_client_set_opacity_active++;
    s_last_opacity_active = percent;
}


/**
 * @brief Recording stand-in for @a ccmd_client_set_opacity_inactive
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_set_opacity_inactive(client_td *client, uint8_t percent)
{
    (void) client;
    s_call_ccmd_client_set_opacity_inactive++;
    s_last_opacity_inactive = percent;
}


/**
 * @brief Recording stand-in for @a enact_client_fullscreen
 *
 * @note Complexity: @e O(1)
 */
void enact_client_fullscreen(client_td *client)
{
    client->properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;
    s_call_enact_client_fullscreen++;
}


/**
 * @brief Recording stand-in for @a enact_client_unfullscreen
 *
 * @note Complexity: @e O(1)
 */
void enact_client_unfullscreen(client_td *client)
{
    client->properties.state &= (uint16_t) ~CLIENT_STATE_FULLSCREEN;
    s_call_enact_client_unfullscreen++;
}


/**
 * @brief Recording stand-in for @a enact_client_maximize
 *
 * @note Complexity: @e O(1)
 */
void enact_client_maximize(client_td *client)
{
    client->properties.state ^= (uint16_t) CLIENT_STATE_MAXIMIZED;
    s_call_enact_client_maximize++;
}


/**
 * @brief Link-only stand-in for @a enact_client_maximize_horz, never
 *        exercised by any scenario below
 *
 * @note Complexity: @e O(1)
 */
void enact_client_maximize_horz(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a enact_client_maximize_vert, never
 *        exercised by any scenario below
 *
 * @note Complexity: @e O(1)
 */
void enact_client_maximize_vert(client_td *client)
{
    (void) client;
}


/**
 * @brief Recording stand-in for @a enact_client_shade
 *
 * @note Complexity: @e O(1)
 */
void enact_client_shade(client_td *client)
{
    (void) client;
    s_call_enact_client_shade++;
}


/**
 * @brief Recording stand-in for @a enact_client_unshade
 *
 * @note Complexity: @e O(1)
 */
void enact_client_unshade(client_td *client)
{
    (void) client;
    s_call_enact_client_unshade++;
}


/**
 * @brief Recording stand-in for @a enact_client_hide
 *
 * @note Complexity: @e O(1)
 */
void enact_client_hide(client_td *client)
{
    (void) client;
    s_call_enact_client_hide++;
}


/**
 * @brief Recording stand-in for @a enact_client_unhide
 *
 * @note Complexity: @e O(1)
 */
void enact_client_unhide(client_td *client)
{
    (void) client;
    s_call_enact_client_unhide++;
}


/**
 * @brief Recording stand-in for @a enact_client_iconify
 *
 * @note Complexity: @e O(1)
 */
void enact_client_iconify(client_td *client)
{
    client->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    s_call_enact_client_iconify++;
}


/**
 * @brief Recording stand-in for @a enact_client_restore
 *
 * @note Complexity: @e O(1)
 */
void enact_client_restore(client_td *client)
{
    client->properties.state &= (uint16_t) ~CLIENT_STATE_ICONIFIED;
    s_call_enact_client_restore++;
}


/**
 * @brief Recording stand-in for @a ccmd_client_apply_geometry
 *
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
    s_call_ccmd_client_apply_geometry++;
    s_last_geometry_mask = mask;
    s_last_geometry_x = x;
    s_last_geometry_y = y;
    s_last_geometry_w = w;
    s_last_geometry_h = h;
}


/**
 * @brief Recording stand-in for @a client_size_constrain: passes width
 *        and height through unconstrained, since ICCCM size hints are
 *        outside this file's target
 *
 * @note Complexity: @e O(1)
 */
void client_size_constrain(const client_td *client,
        uint32_t *width, uint32_t *height)
{
    (void) client;
    (void) width;
    (void) height;
    s_call_client_size_constrain++;
}


/**
 * @brief Recording stand-in for @a client_decoration_layout_sync
 *
 * @note Complexity: @e O(1)
 */
void client_decoration_layout_sync(client_td *client)
{
    (void) client;
    s_call_client_decoration_layout_sync++;
}


/**
 * @brief Stand-in for @a ccmd_client_is_on_screen: every client here is
 *        on screen
 * @note Complexity: @e O(1)
 */
bool ccmd_client_is_on_screen(const client_td *client)
{
    (void) client;
    return true;
}


/**
 * @brief Recording stand-in for @a focus_apply
 *
 * @note Complexity: @e O(1)
 */
void focus_apply(list_td *stages, stage_td *stage,
        desktop_td *desktop, client_td *client, bool raise,
        const config_td *cfg)
{
    (void) stages;
    (void) stage;
    (void) desktop;
    (void) client;
    (void) raise;
    (void) cfg;
    s_call_focus_apply++;
}


/**
 * @brief Recording stand-in for @a ipc_broadcast_event: never frees
 *        @p fields, since the two cJSON calls that would have
 *        allocated it are themselves no-op stand-ins below
 *
 * @note Complexity: @e O(1)
 */
void ipc_broadcast_event(uint64_t type, cJSON *fields)
{
    (void) fields;
    s_call_ipc_broadcast_event++;
    s_last_ipc_event_type = type;
}


/**
 * @brief No-op stand-in for @a cJSON_CreateObject: apply.c only ever
 *        passes its result to @a cJSON_AddNumberToObject and
 *        @a ipc_broadcast_event, both of which tolerate @c NULL, so
 *        this file need not link real cJSON at all
 *
 * @note Complexity: @e O(1)
 */
cJSON *cJSON_CreateObject(void)
{
    return NULL;
}


/**
 * @brief No-op stand-in for @a cJSON_AddNumberToObject, never actually
 *        reached since @a cJSON_CreateObject above always returns
 *        @c NULL
 *
 * @note Complexity: @e O(1)
 */
cJSON *cJSON_AddNumberToObject(cJSON *object, const char *name,
        double number)
{
    (void) object;
    (void) name;
    (void) number;
    return NULL;
}


/** Every client_td this file needs, built once by s_reset */
static client_td s_client;
static stage_td s_stage;
static desktop_td s_desktop;


static void s_reset(void)
{
    memset(&s_rules, 0, sizeof(s_rules));
    memset(&s_config, 0, sizeof(s_config));
    memset(&s_client, 0, sizeof(s_client));
    memset(&s_stage, 0, sizeof(s_stage));
    memset(&s_desktop, 0, sizeof(s_desktop));

    s_client.id = 1u;
    s_client.window = 100u;
    s_client.properties.flags = (uint32_t) CLIENT_FLAG_FOCUSABLE;
    s_client.layout.geometry.cur.dim.w = 300u;
    s_client.layout.geometry.cur.dim.h = 200u;

    s_stage.id = 0u;
    s_stage.properties.dim.w = 1920u;
    s_stage.properties.dim.h = 1080u;

    s_desktop.id = 0u;

    s_desktop_get_result = NULL;

    s_call_enact_client_layer_above = 0;
    s_call_enact_client_layer_below = 0;
    s_call_enact_client_layer_normal = 0;
    s_call_enact_client_pin = 0;
    s_call_enact_client_unpin = 0;
    s_call_enact_client_stick = 0;
    s_call_enact_client_unstick = 0;
    s_call_enact_client_toggle_decorate = 0;
    s_call_ccmd_client_set_opacity_active = 0;
    s_last_opacity_active = 0u;
    s_call_ccmd_client_set_opacity_inactive = 0;
    s_last_opacity_inactive = 0u;
    s_call_enact_client_fullscreen = 0;
    s_call_enact_client_unfullscreen = 0;
    s_call_enact_client_maximize = 0;
    s_call_enact_client_shade = 0;
    s_call_enact_client_unshade = 0;
    s_call_enact_client_hide = 0;
    s_call_enact_client_unhide = 0;
    s_call_enact_client_iconify = 0;
    s_call_enact_client_restore = 0;
    s_call_enact_desktop_client_send = 0;
    s_call_ccmd_client_apply_geometry = 0;
    s_last_geometry_mask = 0u;
    s_last_geometry_x = 0;
    s_last_geometry_y = 0;
    s_last_geometry_w = 0u;
    s_last_geometry_h = 0u;
    s_call_client_size_constrain = 0;
    s_call_client_decoration_layout_sync = 0;
    s_call_focus_apply = 0;
    s_call_ipc_broadcast_event = 0;
    s_last_ipc_event_type = 0u;
}


/** Append one match-anything rule with the given apply block */
static void s_add_rule(enum rules_when_e when, struct rules_apply_s apply)
{
    struct rules_rule_s *rule = &s_rules.rules[s_rules.count];

    memset(rule, 0, sizeof(*rule));
    rule->when = when;
    /* rule->match left fully zeroed: no active criteria, matching
     * every client, same as tests/rules/test_match.c's
     * s_test_no_criteria_matches_anything already established for
     * ri_client_matches itself */
    rule->apply = apply;
    s_rules.count++;
}


/* NULL wm is a no-op, returning false without touching anything */
static void s_test_null_wm_returns_false(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    bool result;

    s_reset();

    result = rules_apply(NULL, &s_client, &stage, &desktop,
            RULES_TRIGGER_MAP);

    TAP_OK(!result, "a NULL wm returns false");
}


/* NULL client is a no-op, returning false */
static void s_test_null_client_returns_false(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    bool result;

    s_reset();

    result = rules_apply(s_fake_wm, NULL, &stage, &desktop,
            RULES_TRIGGER_MAP);

    TAP_OK(!result, "a NULL client returns false");
}


/* NULL stage_io, desktop_io, or the pointer they point to being
 * NULL, are each a no-op */
static void s_test_null_io_pointers_return_false(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    stage_td *null_stage = NULL;
    desktop_td *null_desktop = NULL;

    s_reset();

    TAP_OK(!rules_apply(s_fake_wm, &s_client, NULL, &desktop,
                RULES_TRIGGER_MAP),
            "a NULL stage_io itself returns false");
    TAP_OK(!rules_apply(s_fake_wm, &s_client, &stage, NULL,
                RULES_TRIGGER_MAP),
            "a NULL desktop_io itself returns false");
    TAP_OK(!rules_apply(s_fake_wm, &s_client, &null_stage, &desktop,
                RULES_TRIGGER_MAP),
            "a NULL *stage_io returns false");
    TAP_OK(!rules_apply(s_fake_wm, &s_client, &stage, &null_desktop,
                RULES_TRIGGER_MAP),
            "a NULL *desktop_io returns false");
}


/* No rules loaded at all: no match, returns false */
static void s_test_no_rules_returns_false(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;

    s_reset();

    TAP_OK(!rules_apply(s_fake_wm, &s_client, &stage, &desktop,
                RULES_TRIGGER_MAP),
            "an empty rules table never matches: returns false");
}


/* A rule whose apply block sets nothing (every has_* false) matches
 * but changes nothing, so no event is broadcast and the return is
 * false */
static void s_test_empty_apply_block_changes_nothing(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;
    bool result;

    s_reset();
    memset(&apply, 0, sizeof(apply));
    s_add_rule(RULES_WHEN_BOTH, apply);

    result = rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_MAP);

    TAP_OK(!result, "a rule that matches but sets nothing changes"
            " nothing: false");
    TAP_EQ_INT(s_call_ipc_broadcast_event, 0,
            "and never broadcasts IPC_EVENT_CLIENT_RULE_APPLIED");
}


/* RULES_WHEN_MAP only matches a RULES_TRIGGER_MAP call; a property
 * trigger sees no match at all */
static void s_test_when_map_only_matches_map_trigger(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    memset(&apply, 0, sizeof(apply));
    apply.has_layer = true;
    apply.layer = (uint16_t) CLIENT_LAYER_ABOVE;
    s_add_rule(RULES_WHEN_MAP, apply);

    TAP_OK(rules_apply(s_fake_wm, &s_client, &stage, &desktop,
                RULES_TRIGGER_MAP),
            "WHEN_MAP rule matches a map trigger");

    s_reset();
    s_add_rule(RULES_WHEN_MAP, apply);

    TAP_OK(!rules_apply(s_fake_wm, &s_client, &stage, &desktop,
                RULES_TRIGGER_PROPERTY),
            "the same WHEN_MAP rule does not match a property trigger");
}


/* has_layer above/below/normal each dispatch to their own enact call,
 * exactly one of the three ever firing */
static void s_test_layer_dispatches_to_the_right_enact_call(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    memset(&apply, 0, sizeof(apply));
    apply.has_layer = true;

    s_reset();
    apply.layer = (uint16_t) CLIENT_LAYER_ABOVE;
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_layer_above, 1,
            "CLIENT_LAYER_ABOVE calls enact_client_layer_above once");
    TAP_EQ_INT(s_call_enact_client_layer_below, 0,
            "and never enact_client_layer_below");

    s_reset();
    apply.layer = (uint16_t) CLIENT_LAYER_BELOW;
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_layer_below, 1,
            "CLIENT_LAYER_BELOW calls enact_client_layer_below once");

    s_reset();
    apply.layer = (uint16_t) CLIENT_LAYER_NORMAL;
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_layer_normal, 1,
            "CLIENT_LAYER_NORMAL calls enact_client_layer_normal once");
}


/* !has_layer never calls any of the three layer enact functions at
 * all */
static void s_test_no_layer_field_calls_nothing(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    memset(&apply, 0, sizeof(apply));
    apply.has_pinned = true;
    apply.is_pinned = true;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_call_enact_client_layer_above +
            s_call_enact_client_layer_below +
            s_call_enact_client_layer_normal, 0,
            "no has_layer: none of the 3 layer functions fire");
}


/* has_pinned true pins the client, false unpins it */
static void s_test_pinned_pins_or_unpins(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    memset(&apply, 0, sizeof(apply));
    apply.has_pinned = true;

    s_reset();
    apply.is_pinned = true;
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_pin, 1, "is_pinned true: pins once");
    TAP_EQ_INT(s_call_enact_client_unpin, 0, "and never unpins");

    s_reset();
    apply.is_pinned = false;
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_unpin, 1,
            "is_pinned false: unpins once");
}


/* has_sticky true sticks the client, false unsticks it; not to be
 * confused with has_pinned above, a fully independent flag */
static void s_test_sticky_sticks_or_unsticks(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    memset(&apply, 0, sizeof(apply));
    apply.has_sticky = true;

    s_reset();
    apply.is_sticky = true;
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_stick, 1,
            "is_sticky true: sticks once");
    TAP_EQ_INT(s_call_enact_client_unstick, 0, "and never unsticks");

    s_reset();
    apply.is_sticky = false;
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_unstick, 1,
            "is_sticky false: unsticks once");
}


/* has_decoration only toggles decoration when the requested state
 * actually differs from the client's current one, never redundantly */
static void s_test_decoration_toggles_only_on_mismatch(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    memset(&apply, 0, sizeof(apply));
    apply.has_decoration = true;
    apply.is_decorated = false;

    /* client_is_decorated(w) reads (w)->properties.flags &
     * CLIENT_FLAG_DECORATED; left unset by s_reset, so the client
     * starts out undecorated already */
    s_reset();
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_toggle_decorate, 0,
            "asking for undecorated on an already-undecorated client:"
            " no toggle call");

    apply.is_decorated = true;
    s_reset();
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_toggle_decorate, 1,
            "asking for decorated on an undecorated client: toggles"
            " once");
}


/* has_opacity_active/inactive forward their exact percent values */
static void s_test_opacity_forwards_exact_values(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    memset(&apply, 0, sizeof(apply));
    apply.has_opacity_active = true;
    apply.opacity_active = 75u;
    apply.has_opacity_inactive = true;
    apply.opacity_inactive = 30u;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_last_opacity_active, 75,
            "active opacity forwarded exactly as given");
    TAP_EQ_INT(s_last_opacity_inactive, 30,
            "inactive opacity forwarded exactly as given");
}


/* RULES_TRIGGER_MAP defers iconified/fullscreen/maximized/shaded/
 * hidden into the client's own has_rule and is_rule fields instead of
 * calling any enact function directly */
static void s_test_map_trigger_defers_state_instead_of_enacting(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    memset(&apply, 0, sizeof(apply));
    apply.has_iconified = true;
    apply.is_iconified = true;
    apply.has_maximized = true;
    apply.is_maximized = true;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_MAP);

    TAP_OK(s_client.has_rule_iconified && s_client.is_rule_iconified,
            "iconified request recorded on the client for later");
    TAP_OK(s_client.has_rule_maximized && s_client.is_rule_maximized,
            "maximized request recorded on the client for later");
    TAP_EQ_INT(s_call_enact_client_iconify + s_call_enact_client_maximize,
            0, "neither enact function actually ran yet");
}


/* RULES_TRIGGER_PROPERTY applies fullscreen/maximized directly through
 * enact, unlike RULES_TRIGGER_MAP */
static void s_test_property_trigger_applies_state_directly(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    memset(&apply, 0, sizeof(apply));
    apply.has_fullscreen = true;
    apply.is_fullscreen = true;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_call_enact_client_fullscreen, 1,
            "a property-triggered fullscreen request enacts directly");
    TAP_OK(!s_client.has_rule_fullscreen,
            "and never touches the map-deferred bookkeeping fields"
            " at all");
}


/* has_maximized only toggles when the requested state actually
 * differs from the client's current one */
static void s_test_maximized_toggles_only_on_mismatch(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    memset(&apply, 0, sizeof(apply));
    apply.has_maximized = true;
    apply.is_maximized = false;

    s_reset();
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_maximize, 0,
            "asking for un-maximized on an already-restored client:"
            " no toggle");

    apply.is_maximized = true;
    s_reset();
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_maximize, 1,
            "asking for maximized on a restored client: toggles once");
}


/* has_shaded true shades an already-decorated, non-fullscreen,
 * non-about-to-iconify client */
static void s_test_shaded_applies_when_eligible(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    s_client.properties.flags |= (uint32_t) CLIENT_FLAG_DECORATED;
    memset(&apply, 0, sizeof(apply));
    apply.has_shaded = true;
    apply.is_shaded = true;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_call_enact_client_shade, 1,
            "a decorated, non-fullscreen, non-iconifying client"
            " shades");
}


/* has_shaded true loses to fullscreen taking precedence: no shade
 * call at all */
static void s_test_shaded_loses_to_fullscreen(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    s_client.properties.flags |= (uint32_t) CLIENT_FLAG_DECORATED;
    memset(&apply, 0, sizeof(apply));
    apply.has_shaded = true;
    apply.is_shaded = true;
    apply.has_fullscreen = true;
    apply.is_fullscreen = true;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_call_enact_client_shade, 0,
            "fullscreen requested alongside shaded: shaded is ignored");
    TAP_EQ_INT(s_call_enact_client_fullscreen, 1,
            "fullscreen itself still applies");
}


/* has_shaded true loses to a client not decorated */
static void s_test_shaded_loses_to_undecorated(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    /* CLIENT_FLAG_DECORATED deliberately left unset */
    memset(&apply, 0, sizeof(apply));
    apply.has_shaded = true;
    apply.is_shaded = true;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_call_enact_client_shade, 0,
            "an undecorated client never shades, regardless of the"
            " rule");
}


/* has_shaded true loses to an iconified request in the very same
 * apply block */
static void s_test_shaded_loses_to_iconified(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    s_client.properties.flags |= (uint32_t) CLIENT_FLAG_DECORATED;
    memset(&apply, 0, sizeof(apply));
    apply.has_shaded = true;
    apply.is_shaded = true;
    apply.has_iconified = true;
    apply.is_iconified = true;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_call_enact_client_shade, 0,
            "iconified requested alongside shaded: shaded is ignored");
    TAP_EQ_INT(s_call_enact_client_iconify, 1,
            "iconified itself still applies");
}


/* has_hidden true/false dispatch to hide/unhide respectively */
static void s_test_hidden_dispatches_hide_or_unhide(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    memset(&apply, 0, sizeof(apply));
    apply.has_hidden = true;

    s_reset();
    apply.is_hidden = true;
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_hide, 1, "is_hidden true: hides once");

    s_reset();
    apply.is_hidden = false;
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_unhide, 1,
            "is_hidden false: unhides once");
}


/* has_iconified only calls iconify/restore when the client's current
 * iconified state actually differs from what was requested */
static void s_test_iconified_toggles_only_on_mismatch(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    memset(&apply, 0, sizeof(apply));
    apply.has_iconified = true;

    s_reset();
    apply.is_iconified = false;
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_iconify + s_call_enact_client_restore,
            0, "already-restored client asked to un-iconify: no call"
            " at all");

    s_reset();
    apply.is_iconified = true;
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_iconify, 1,
            "a restored client asked to iconify: iconifies once");

    s_reset();
    s_client.properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    apply.is_iconified = false;
    s_add_rule(RULES_WHEN_BOTH, apply);
    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);
    TAP_EQ_INT(s_call_enact_client_restore, 1,
            "an iconified client asked to restore: restores once");
}


/* has_desktop resolves the target desktop via stage_desktop_get and
 * moves the client through enact_desktop_client_send, updating
 * *desktop_io on success */
static void s_test_desktop_moves_client_and_updates_io(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    desktop_td target;
    struct rules_apply_s apply;

    s_reset();
    memset(&target, 0, sizeof(target));
    target.id = 3u;
    s_desktop_get_result = &target;
    memset(&apply, 0, sizeof(apply));
    apply.has_desktop = true;
    apply.desktop = 3u;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_call_enact_desktop_client_send, 1,
            "a resolvable different desktop moves the client once");
    TAP_OK(desktop == &target,
            "*desktop_io is updated to point at the new desktop");
}


/* has_desktop targeting the client's own current desktop is a no-op:
 * never calls enact_desktop_client_send */
static void s_test_desktop_same_target_is_noop(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    s_desktop_get_result = &s_desktop;
    memset(&apply, 0, sizeof(apply));
    apply.has_desktop = true;
    apply.desktop = 0u;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_call_enact_desktop_client_send, 0,
            "the target desktop is already the current one: no move");
}


/* has_desktop targeting a nonexistent desktop falls back to desktop
 * 0 when it exists */
static void s_test_desktop_falls_back_to_zero(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    desktop_td fallback;
    struct rules_apply_s apply;

    s_reset();
    memset(&fallback, 0, sizeof(fallback));
    fallback.id = 0u;
    /* stage_desktop_get stand-in always returns the same result
     * regardless of the id asked for, which is fine here: the rule
     * targets a nonexistent desktop, so both lookups (target, then
     * the desktop-0 fallback) resolve to the same fallback desktop */
    s_desktop_get_result = &fallback;
    memset(&apply, 0, sizeof(apply));
    apply.has_desktop = true;
    apply.desktop = 99u;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_OK(desktop == &fallback,
            "an out-of-range desktop falls back to desktop 0");
}


/* has_size alone resizes without touching position at all */
static void s_test_size_only_sets_width_and_height(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    memset(&apply, 0, sizeof(apply));
    apply.has_size = true;
    apply.w = 640u;
    apply.h = 480u;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_last_geometry_w, 640, "width applied exactly");
    TAP_EQ_INT(s_last_geometry_h, 480, "height applied exactly");
    TAP_OK((s_last_geometry_mask &
                (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y)) == 0,
            "the position bits are not set in the mask at all");
}


/* has_position with is_position_centered false places the client at
 * an exact x/y, clamping a negative y to 0 */
static void s_test_explicit_position_clamps_negative_y(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    memset(&apply, 0, sizeof(apply));
    apply.has_position = true;
    apply.is_position_centered = false;
    apply.x = 50;
    apply.y = -20;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_last_geometry_x, 50, "x applied exactly as given");
    TAP_EQ_INT(s_last_geometry_y, 0,
            "a negative y is clamped to 0, never applied negative");
    TAP_OK(s_client.has_rule_position_locked,
            "has_rule_position_locked is set once a position is"
            " applied");
}


/* is_position_centered true centers the client within the whole
 * stage when no monitor is targeted */
static void s_test_centered_position_uses_stage_dimensions(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    memset(&apply, 0, sizeof(apply));
    apply.has_position = true;
    apply.is_position_centered = true;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    /* stage is 1920x1080, client (unresized here) is 300x200:
     * centered x = (1920-300)/2 = 810, y = (1080-200)/2 = 440 */
    TAP_EQ_INT(s_last_geometry_x, 810,
            "centered x uses the stage's own width");
    TAP_EQ_INT(s_last_geometry_y, 440,
            "centered y uses the stage's own height");
}


/* has_monitor alone (no explicit position) centers by default on the
 * targeted monitor, offset by that monitor's own top-left corner */
static void s_test_monitor_alone_centers_by_default(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    s_stage.monitor_count = 2u;
    s_stage.monitors[1] = (monitor_td) { 2000, 100, 800u, 600u };
    memset(&apply, 0, sizeof(apply));
    apply.has_monitor = true;
    apply.monitor = 1u;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    /* monitor 1 is 800x600 at (2000,100); client 300x200:
     * local center (800-300)/2=250, (600-200)/2=200, plus the
     * monitor's own offset: x=2250, y=300 */
    TAP_EQ_INT(s_last_geometry_x, 2250,
            "monitor-relative centered x includes the monitor's"
            " own x offset");
    TAP_EQ_INT(s_last_geometry_y, 300,
            "monitor-relative centered y includes the monitor's"
            " own y offset");
}


/* has_monitor out of range falls back to monitor 0 rather than
 * crashing or reading past the array */
static void s_test_monitor_out_of_range_falls_back_to_zero(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    s_stage.monitor_count = 1u;
    s_stage.monitors[0] = (monitor_td) { 0, 0, 1920u, 1080u };
    memset(&apply, 0, sizeof(apply));
    apply.has_monitor = true;
    apply.monitor = 7u;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    /* monitor 0 is the full 1920x1080 stage at (0,0): same as the
     * whole-stage centering case */
    TAP_EQ_INT(s_last_geometry_x, 810,
            "an out-of-range monitor falls back to monitor 0's own"
            " centered x");
}


/* A fullscreen client's geometry is left completely untouched, even
 * with an explicit has_position/has_size in the merged apply block */
static void s_test_fullscreen_client_skips_geometry(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    s_client.properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;
    memset(&apply, 0, sizeof(apply));
    apply.has_position = true;
    apply.x = 10;
    apply.y = 10;
    apply.has_size = true;
    apply.w = 50u;
    apply.h = 50u;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_call_ccmd_client_apply_geometry, 0,
            "a fullscreen client's geometry request is skipped"
            " entirely");
}


/* A fully maximized client's geometry is left untouched too, the
 * same as a fullscreen one, rather than a re-evaluated rule
 * silently un-maximizing it */
static void s_test_maximized_client_skips_geometry(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    s_client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED;
    memset(&apply, 0, sizeof(apply));
    apply.has_position = true;
    apply.x = 10;
    apply.y = 10;
    apply.has_size = true;
    apply.w = 50u;
    apply.h = 50u;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_call_ccmd_client_apply_geometry, 0,
            "a fully maximized client's geometry request is skipped"
            " entirely");
}


/* A client maximized on just one axis skips its geometry request
 * too: the rule cannot tell which axis a "position"/"size" is meant
 * for, so both stay skipped rather than only the free one */
static void s_test_maximized_horz_client_skips_geometry(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    s_client.properties.state |=
        (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;
    memset(&apply, 0, sizeof(apply));
    apply.has_position = true;
    apply.x = 10;
    apply.y = 10;
    apply.has_size = true;
    apply.w = 50u;
    apply.h = 50u;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_call_ccmd_client_apply_geometry, 0,
            "a horizontally maximized client's geometry request is"
            " skipped entirely too");
}


/* has_focus true with is_focused true on a focusable client calls
 * focus_apply exactly once */
static void s_test_focus_applies_when_focusable(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    memset(&apply, 0, sizeof(apply));
    apply.has_focus = true;
    apply.is_focused = true;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_call_focus_apply, 1,
            "a focusable client requesting focus calls focus_apply"
            " once");
}


/* At map time, a rule's focus is only recorded, to be decided once the
 * window is mapped: focus_apply is not called then, and a rule saying
 * false is kept, so it can stand in for 'focus-new' */
static void s_test_focus_deferred_at_map(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    s_client.has_rule_focus = false;
    s_client.is_rule_focus = true;
    memset(&apply, 0, sizeof(apply));
    apply.has_focus = true;
    apply.is_focused = false;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_MAP);

    TAP_OK(s_call_focus_apply == 0 && s_client.has_rule_focus &&
            !s_client.is_rule_focus,
            "at map, a rule's focus=false is recorded for after mapping"
            " and focus_apply is not called");

    s_client.has_rule_focus = false;
}


/* has_focus true on a client without CLIENT_FLAG_FOCUSABLE never
 * calls focus_apply */
static void s_test_focus_skips_unfocusable_client(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;

    s_reset();
    s_client.properties.flags = 0u;
    memset(&apply, 0, sizeof(apply));
    apply.has_focus = true;
    apply.is_focused = true;
    s_add_rule(RULES_WHEN_BOTH, apply);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_call_focus_apply, 0,
            "a non-focusable client never gets focus_apply, even when"
            " the rule asks for it");
}


/* Two rules whose apply blocks each set a different field merge
 * together rather than one replacing the other */
static void s_test_two_rules_merge_disjoint_fields(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply_a;
    struct rules_apply_s apply_b;

    s_reset();
    memset(&apply_a, 0, sizeof(apply_a));
    apply_a.has_pinned = true;
    apply_a.is_pinned = true;
    memset(&apply_b, 0, sizeof(apply_b));
    apply_b.has_hidden = true;
    apply_b.is_hidden = true;
    s_add_rule(RULES_WHEN_BOTH, apply_a);
    s_add_rule(RULES_WHEN_BOTH, apply_b);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_call_enact_client_pin, 1,
            "the first rule's pinned field still applies");
    TAP_EQ_INT(s_call_enact_client_hide, 1,
            "the second rule's hidden field applies too, alongside it");
}


/* A later rule's value for the same field overrides an earlier one's,
 * last-write-wins */
static void s_test_later_rule_overrides_earlier_same_field(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply_a;
    struct rules_apply_s apply_b;

    s_reset();
    memset(&apply_a, 0, sizeof(apply_a));
    apply_a.has_opacity_active = true;
    apply_a.opacity_active = 10u;
    memset(&apply_b, 0, sizeof(apply_b));
    apply_b.has_opacity_active = true;
    apply_b.opacity_active = 90u;
    s_add_rule(RULES_WHEN_BOTH, apply_a);
    s_add_rule(RULES_WHEN_BOTH, apply_b);

    (void) rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_EQ_INT(s_last_opacity_active, 90,
            "the second, later rule's value for the same field wins");
    TAP_EQ_INT(s_call_ccmd_client_set_opacity_active, 1,
            "the merge means only one opacity call happens, not two");
}


/* A successful apply that changed at least one field broadcasts
 * IPC_EVENT_CLIENT_RULE_APPLIED and returns true */
static void s_test_successful_apply_broadcasts_and_returns_true(void)
{
    stage_td *stage = &s_stage;
    desktop_td *desktop = &s_desktop;
    struct rules_apply_s apply;
    bool result;

    s_reset();
    memset(&apply, 0, sizeof(apply));
    apply.has_pinned = true;
    apply.is_pinned = true;
    s_add_rule(RULES_WHEN_BOTH, apply);

    result = rules_apply(s_fake_wm, &s_client, &stage, &desktop,
            RULES_TRIGGER_PROPERTY);

    TAP_OK(result, "a rule that actually changes something returns"
            " true");
    TAP_EQ_INT(s_call_ipc_broadcast_event, 1,
            "and broadcasts exactly one IPC event");
    TAP_EQ_INT((long) s_last_ipc_event_type,
            (long) IPC_EVENT_CLIENT_RULE_APPLIED,
            "the broadcast event type is IPC_EVENT_CLIENT_RULE_APPLIED");
}


int main(void)
{
    TAP_PLAN(72);

    s_test_null_wm_returns_false();
    s_test_null_client_returns_false();
    s_test_null_io_pointers_return_false();
    s_test_no_rules_returns_false();
    s_test_empty_apply_block_changes_nothing();
    s_test_when_map_only_matches_map_trigger();
    s_test_layer_dispatches_to_the_right_enact_call();
    s_test_no_layer_field_calls_nothing();
    s_test_pinned_pins_or_unpins();
    s_test_sticky_sticks_or_unsticks();
    s_test_decoration_toggles_only_on_mismatch();
    s_test_opacity_forwards_exact_values();
    s_test_map_trigger_defers_state_instead_of_enacting();
    s_test_property_trigger_applies_state_directly();
    s_test_maximized_toggles_only_on_mismatch();
    s_test_shaded_applies_when_eligible();
    s_test_shaded_loses_to_fullscreen();
    s_test_shaded_loses_to_undecorated();
    s_test_shaded_loses_to_iconified();
    s_test_hidden_dispatches_hide_or_unhide();
    s_test_iconified_toggles_only_on_mismatch();
    s_test_desktop_moves_client_and_updates_io();
    s_test_desktop_same_target_is_noop();
    s_test_desktop_falls_back_to_zero();
    s_test_size_only_sets_width_and_height();
    s_test_explicit_position_clamps_negative_y();
    s_test_centered_position_uses_stage_dimensions();
    s_test_monitor_alone_centers_by_default();
    s_test_monitor_out_of_range_falls_back_to_zero();
    s_test_fullscreen_client_skips_geometry();
    s_test_maximized_client_skips_geometry();
    s_test_maximized_horz_client_skips_geometry();
    s_test_focus_applies_when_focusable();
    s_test_focus_deferred_at_map();
    s_test_focus_skips_unfocusable_client();
    s_test_two_rules_merge_disjoint_fields();
    s_test_later_rule_overrides_earlier_same_field();
    s_test_successful_apply_broadcasts_and_returns_true();

    return TAP_DONE();
}
