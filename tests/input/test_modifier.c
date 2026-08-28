/**
 * @file tests/input/test_modifier.c
 *
 * @brief Test battery for modifier-token to XCB-mask resolution
 *
 * im_parse_modifier_token first expands a configured alias (modc,
 * mods, modl, mod1-mod5) into whatever string the configuration
 * actually holds for it, then maps that resolved name to an XCB
 * modifier mask.  Both steps are covered here: aliasing through a
 * configured value, and every direct name/synonym the second step
 * itself recognizes.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <config.h>
#include <harness/tap.h>
#include <input/modifier.h>


static void s_make_config(config_td *config)
{
    memset(config, 0, sizeof(*config));
    strncpy(config->bindings.modc, "control",
            sizeof(config->bindings.modc) - 1);
    strncpy(config->bindings.mods, "shift",
            sizeof(config->bindings.mods) - 1);
    strncpy(config->bindings.modl, "lock",
            sizeof(config->bindings.modl) - 1);
    strncpy(config->bindings.mod1, "alt",
            sizeof(config->bindings.mod1) - 1);
    strncpy(config->bindings.mod4, "super",
            sizeof(config->bindings.mod4) - 1);
}


/* Every direct modifier name/synonym the second step recognizes,
 * bypassing alias resolution entirely (a NULL config) */
static void s_test_direct_names_and_synonyms(void)
{
    config_td *no_config = NULL;

    TAP_EQ_INT(im_parse_modifier_token(no_config, "mod1"),
            XCB_MOD_MASK_1, "mod1");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "alt"),
            XCB_MOD_MASK_1, "alt is a synonym for mod1");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "mod2"),
            XCB_MOD_MASK_2, "mod2");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "num_lock"),
            XCB_MOD_MASK_2, "num_lock is a synonym for mod2");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "num-lock"),
            XCB_MOD_MASK_2, "num-lock (hyphen) is too");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "mod3"),
            XCB_MOD_MASK_3, "mod3");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "mod4"),
            XCB_MOD_MASK_4, "mod4");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "super"),
            XCB_MOD_MASK_4, "super is a synonym for mod4");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "win"),
            XCB_MOD_MASK_4, "win is too");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "mod5"),
            XCB_MOD_MASK_5, "mod5");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "hyper"),
            XCB_MOD_MASK_5, "hyper is a synonym for mod5");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "ctrl"),
            XCB_MOD_MASK_CONTROL, "ctrl");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "control"),
            XCB_MOD_MASK_CONTROL, "control is a synonym for ctrl");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "shift"),
            XCB_MOD_MASK_SHIFT, "shift");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "lock"),
            XCB_MOD_MASK_LOCK, "lock");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "caps_lock"),
            XCB_MOD_MASK_LOCK, "caps_lock is a synonym for lock");
    TAP_EQ_INT(im_parse_modifier_token(no_config, "caps-lock"),
            XCB_MOD_MASK_LOCK, "caps-lock (hyphen) is too");
}


/* strcasecmp is used throughout: mixed case must match exactly the
 * same as lowercase */
static void s_test_case_insensitive(void)
{
    TAP_EQ_INT(im_parse_modifier_token(NULL, "SHIFT"),
            XCB_MOD_MASK_SHIFT, "all-uppercase still matches");
    TAP_EQ_INT(im_parse_modifier_token(NULL, "Super"),
            XCB_MOD_MASK_4, "mixed case still matches");
    TAP_EQ_INT(im_parse_modifier_token(NULL, "Caps_Lock"),
            XCB_MOD_MASK_LOCK, "mixed case with an underscore" \
            " still matches");
}


/* An alias (modc/mods/modl/mod1-mod5) resolves through the
 * configured string first, then that resolved name maps to a mask,
 * exactly the same as if the resolved name had been passed directly */
static void s_test_alias_resolves_through_config(void)
{
    config_td config;

    s_make_config(&config);

    TAP_EQ_INT(im_parse_modifier_token(&config, "modc"),
            XCB_MOD_MASK_CONTROL,
            "'modc' resolves through config to 'control'");
    TAP_EQ_INT(im_parse_modifier_token(&config, "mods"),
            XCB_MOD_MASK_SHIFT,
            "'mods' resolves through config to 'shift'");
    TAP_EQ_INT(im_parse_modifier_token(&config, "modl"),
            XCB_MOD_MASK_LOCK,
            "'modl' resolves through config to 'lock'");
    TAP_EQ_INT(im_parse_modifier_token(&config, "mod1"),
            XCB_MOD_MASK_1,
            "'mod1' resolves through config to 'alt', still mod1's" \
            " own mask");
    TAP_EQ_INT(im_parse_modifier_token(&config, "mod4"),
            XCB_MOD_MASK_4,
            "'mod4' resolves through config to 'super', still" \
            " mod4's own mask");
    TAP_EQ_INT(im_parse_modifier_token(&config, "MODC"),
            XCB_MOD_MASK_CONTROL,
            "alias lookup is itself case-insensitive too");
}


/* An alias with no matching config entry (mod2/mod3/mod5 left empty
 * by s_make_config) resolves to an empty string, which then matches
 * nothing at all */
static void s_test_unconfigured_alias_resolves_to_nothing(void)
{
    config_td config;

    s_make_config(&config);

    TAP_EQ_INT(im_parse_modifier_token(&config, "mod2"), 0,
            "'mod2' with no configured value resolves to an empty" \
            " string, matching nothing");
}


/* Guard clauses: NULL token, an empty string, and a name that
 * matches nothing at all */
static void s_test_guards_and_unknown(void)
{
    config_td config;

    s_make_config(&config);

    TAP_EQ_INT(im_parse_modifier_token(&config, NULL), 0,
            "a NULL token returns 0, no crash");
    TAP_EQ_INT(im_parse_modifier_token(&config, ""), 0,
            "an empty string returns 0");
    TAP_EQ_INT(im_parse_modifier_token(&config, "not-a-modifier"), 0,
            "an unrecognized name returns 0");
}


int main(void)
{
    TAP_PLAN(30);

    s_test_direct_names_and_synonyms();
    s_test_case_insensitive();
    s_test_alias_resolves_through_config();
    s_test_unconfigured_alias_resolves_to_nothing();
    s_test_guards_and_unknown();

    return TAP_DONE();
}
