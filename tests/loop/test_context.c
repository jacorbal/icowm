/**
 * @file tests/loop/test_context.c
 *
 * @brief Test battery for the main loop's per-run context resolver
 *        (loop/context.c)
 *
 * loop_context_init is loop/context.c's only function, and its whole
 * body is genuinely reachable without any live X connection: a null-
 * argument guard clause, then a flat sequence of field assignments,
 * most of them forwarded straight through wm.h's own narrow accessors
 * (wm_stages, wm_config, wm_restricted_memory_mib,
 * wm_randr_base_event, wm_sync_base_event, wm_randr_available,
 * wm_sync_available), none of which itself does anything more than
 * read a field off the real 'struct wm_s'.  wm/instance.c, where every
 * one of those accessors lives, is linked for real below rather than
 * stood in for, exactly so this file proves loop_context_init reads
 * the real singleton through the real accessor path, not through a
 * hand-rolled substitute for it; building a real 'struct wm_s' on the
 * stack, through the 'wm/internal.h' the project itself uses for the
 * very same purpose in tests/wm/test_lifecycle.c, is what makes that
 * possible without wm_start or any of its own X-bound machinery.
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
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <harness/tap.h>
#include <loop/context.h>


/**
 * @brief Build a minimal, real 'struct wm_s' on the stack
 */
static wm_td s_make_wm(void)
{
    wm_td local_wm;

    memset(&local_wm, 0, sizeof(local_wm));
    return local_wm;
}


/* A null context, a null wm, or both together are refused outright,
 * leaving nothing to check on the (nonexistent) output */
static void s_test_null_guards(void)
{
    wm_td local_wm = s_make_wm();
    loop_ctx_td ctx;

    memset(&ctx, 0xAA, sizeof(ctx));

    TAP_OK(!loop_context_init(NULL, &local_wm),
            "loop_context_init on a null context returns false");
    TAP_OK(!loop_context_init(&ctx, NULL),
            "loop_context_init on a null wm returns false");
    TAP_OK(!loop_context_init(NULL, NULL),
            "loop_context_init on both null returns false");
}


/* On a real wm, every cached field is resolved from the singleton
 * through its own accessor, keysyms starts NULL for loop_run to fill
 * in later, and pending_event starts NULL since no event has been
 * looked ahead at yet */
static void s_test_fills_every_field(void)
{
    wm_td local_wm = s_make_wm();
    list_td stages_storage;
    config_td config_storage;
    loop_ctx_td ctx;
    bool result;

    memset(&stages_storage, 0, sizeof(stages_storage));
    memset(&config_storage, 0, sizeof(config_storage));
    memset(&ctx, 0xAA, sizeof(ctx));

    local_wm.stages = &stages_storage;
    local_wm.config = &config_storage;
    local_wm.restricted_memory_mib = 256u;
    local_wm.randr_base_event = 87u;
    local_wm.sync_base_event = 92u;
    local_wm.is_randr_available = true;
    local_wm.is_sync_available = false;

    result = loop_context_init(&ctx, &local_wm);

    TAP_OK(result, "loop_context_init on a real wm returns true");
    TAP_OK(ctx.wm == &local_wm,
            "loop_context_init caches the wm pointer itself");
    TAP_OK(ctx.stages == &stages_storage,
            "loop_context_init resolves stages via wm_stages");
    TAP_OK(ctx.config == &config_storage,
            "loop_context_init resolves config via wm_config");
    TAP_OK(ctx.keysyms == NULL,
            "loop_context_init leaves keysyms NULL for loop_run to"
            " fill in later");
    TAP_OK(ctx.pending_event == NULL,
            "loop_context_init leaves pending_event NULL, since no"
            " lookahead has happened yet");
    TAP_EQ_INT(ctx.restricted_memory_mib, 256,
            "loop_context_init resolves restricted_memory_mib via"
            " wm_restricted_memory_mib");
    TAP_EQ_INT(ctx.randr_base_event, 87,
            "loop_context_init resolves randr_base_event via"
            " wm_randr_base_event");
    TAP_EQ_INT(ctx.sync_base_event, 92,
            "loop_context_init resolves sync_base_event via"
            " wm_sync_base_event");
    TAP_OK(ctx.is_randr_available,
            "loop_context_init resolves is_randr_available via"
            " wm_randr_available");
    TAP_OK(!ctx.is_sync_available,
            "loop_context_init resolves is_sync_available via"
            " wm_sync_available");
}


/* A wm with a NULL stages/config pointer of its own is resolved
 * faithfully too: the accessors return NULL rather than crashing, and
 * loop_context_init simply stores what it was given back */
static void s_test_null_fields_pass_through(void)
{
    wm_td local_wm = s_make_wm();
    loop_ctx_td ctx;
    bool result;

    memset(&ctx, 0xAA, sizeof(ctx));
    local_wm.stages = NULL;
    local_wm.config = NULL;

    result = loop_context_init(&ctx, &local_wm);

    TAP_OK(result,
            "loop_context_init still succeeds when the wm's own"
            " stages/config are NULL");
    TAP_OK(ctx.stages == NULL,
            "loop_context_init passes a NULL stages list through"
            " unchanged");
    TAP_OK(ctx.config == NULL,
            "loop_context_init passes a NULL config through"
            " unchanged");
}


int main(void)
{
    TAP_PLAN(17);

    s_test_null_guards();
    s_test_fills_every_field();
    s_test_null_fields_pass_through();

    return TAP_DONE();
}
