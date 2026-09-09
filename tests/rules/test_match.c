/**
 * @file tests/rules/test_match.c
 *
 * @brief Test battery for the window matching rules engine
 *
 * ri_client_matches takes a real client_td; every field it actually
 * reads (info.class_name[0]/[1], info.role_name, info.name,
 * properties.type, transient_for) is a plain pointer or integer, so a
 * client_td built in memory with s_make_client below (zeroed, then
 * only those fields set) exercises the real function without needing
 * an X11 connection at all.
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

/* Project includes */
#include <client.h>

/* Local includes */
#include <rules/internal.h>
#include <harness/tap.h>


/**
 * @brief Build a zeroed client_td with only the fields
 *        ri_client_matches actually reads set
 */
static void s_make_client(client_td *client, const char *instance,
        const char *klass, const char *role, const char *title,
        enum client_type_e type, bool is_transient)
{
    memset(client, 0, sizeof(*client));
    client->info.class_name[0] = (char *) instance;
    client->info.class_name[1] = (char *) klass;
    client->info.role_name = (char *) role;
    client->info.name = (char *) title;
    client->properties.type = (uint16_t) type;
    client->transient_for =
        (is_transient) ? (xcb_window_t) 42u : XCB_WINDOW_NONE;
}


/**
 * @brief Build a zeroed rules_match_s with no criteria active at all
 */
static void s_make_empty_match(struct rules_match_s *match)
{
    memset(match, 0, sizeof(*match));
}


/* ri_when_matches: RULES_WHEN_BOTH always matches, regardless of
 * trigger; RULES_WHEN_MAP/PROPERTY each match only their own trigger */
static void s_test_when_matches(void)
{
    TAP_OK(ri_when_matches(RULES_WHEN_BOTH, RULES_TRIGGER_MAP),
            "WHEN_BOTH matches a map trigger");
    TAP_OK(ri_when_matches(RULES_WHEN_BOTH, RULES_TRIGGER_PROPERTY),
            "WHEN_BOTH matches a property trigger too");
    TAP_OK(ri_when_matches(RULES_WHEN_MAP, RULES_TRIGGER_MAP),
            "WHEN_MAP matches a map trigger");
    TAP_OK(!ri_when_matches(RULES_WHEN_MAP, RULES_TRIGGER_PROPERTY),
            "WHEN_MAP does not match a property trigger");
    TAP_OK(ri_when_matches(RULES_WHEN_PROPERTY, RULES_TRIGGER_PROPERTY),
            "WHEN_PROPERTY matches a property trigger");
    TAP_OK(!ri_when_matches(RULES_WHEN_PROPERTY, RULES_TRIGGER_MAP),
            "WHEN_PROPERTY does not match a map trigger");
}


/* No active criteria at all matches every client */
static void s_test_no_criteria_matches_anything(void)
{
    client_td client;
    struct rules_match_s match;

    s_make_client(&client, "firefox", "Firefox", "browser", "Mozilla",
            CLIENT_TYPE_NORMAL, false);
    s_make_empty_match(&match);

    TAP_OK(ri_client_matches(&match, &client),
            "no active criteria: matches unconditionally");
}


/* A single glob criterion (instance) matches or fails to match as
 * expected */
static void s_test_instance_glob(void)
{
    client_td client;
    struct rules_match_s match;

    s_make_client(&client, "firefox-bin", "Firefox", "", "",
            CLIENT_TYPE_NORMAL, false);
    s_make_empty_match(&match);
    match.has_instance = true;
    match.instance_count = 1u;
    strcpy(match.instance[0], "firefox*");

    TAP_OK(ri_client_matches(&match, &client),
            "'firefox*' matches instance 'firefox-bin'");

    strcpy(match.instance[0], "chrome*");
    TAP_OK(!ri_client_matches(&match, &client),
            "'chrome*' does not match instance 'firefox-bin'");
}


/* Multiple alternative values for one criterion: matches if any one
 * of them matches ("or" within the field) */
static void s_test_multiple_alternatives_is_or(void)
{
    client_td client;
    struct rules_match_s match;

    s_make_client(&client, "xterm", "XTerm", "", "", CLIENT_TYPE_NORMAL,
            false);
    s_make_empty_match(&match);
    match.has_instance = true;
    match.instance_count = 3u;
    strcpy(match.instance[0], "firefox*");
    strcpy(match.instance[1], "chrome*");
    strcpy(match.instance[2], "xterm*");

    TAP_OK(ri_client_matches(&match, &client),
            "matches the 3rd alternative, having failed the first 2");
}


/* Several criteria present at once: the client must match every one
 * of them ("and" across fields), not just any single one */
static void s_test_multiple_fields_is_and(void)
{
    client_td client;
    struct rules_match_s match;

    s_make_client(&client, "firefox", "Firefox", "", "Mozilla Firefox",
            CLIENT_TYPE_NORMAL, false);
    s_make_empty_match(&match);
    match.has_instance = true;
    match.instance_count = 1u;
    strcpy(match.instance[0], "firefox");
    match.has_title = true;
    match.title_count = 1u;
    strcpy(match.title[0], "*Firefox*");

    TAP_OK(ri_client_matches(&match, &client),
            "both instance and title match: overall match");

    strcpy(match.title[0], "*Chrome*");
    TAP_OK(!ri_client_matches(&match, &client),
            "instance matches but title does not: overall no match");
}


/* A recognized type name matches only its own corresponding
 * client_type_e value */
static void s_test_type_matching(void)
{
    client_td client;
    struct rules_match_s match;

    s_make_client(&client, "", "", "", "", CLIENT_TYPE_DIALOG, false);
    s_make_empty_match(&match);
    match.has_type = true;
    match.type_count = 1u;
    strcpy(match.type[0], "dialog");

    TAP_OK(ri_client_matches(&match, &client),
            "'dialog' matches a CLIENT_TYPE_DIALOG client");

    strcpy(match.type[0], "normal");
    TAP_OK(!ri_client_matches(&match, &client),
            "'normal' does not match a CLIENT_TYPE_DIALOG client");
}


/* has_transient checks whether the client actually has a transient-for
 * window against the rule's own expected true/false value */
static void s_test_transient_matching(void)
{
    client_td transient_client;
    client_td normal_client;
    struct rules_match_s match_wants_transient;
    struct rules_match_s match_wants_not_transient;

    s_make_client(&transient_client, "", "", "", "", CLIENT_TYPE_DIALOG,
            true);
    s_make_client(&normal_client, "", "", "", "", CLIENT_TYPE_NORMAL,
            false);

    s_make_empty_match(&match_wants_transient);
    match_wants_transient.has_transient = true;
    match_wants_transient.is_transient = true;

    s_make_empty_match(&match_wants_not_transient);
    match_wants_not_transient.has_transient = true;
    match_wants_not_transient.is_transient = false;

    TAP_OK(ri_client_matches(&match_wants_transient, &transient_client),
            "rule wants transient, client is transient: matches");
    TAP_OK(!ri_client_matches(&match_wants_transient, &normal_client),
            "rule wants transient, client is not: no match");
    TAP_OK(ri_client_matches(&match_wants_not_transient, &normal_client),
            "rule wants non-transient, client is not transient: matches");
    TAP_OK(!ri_client_matches(&match_wants_not_transient,
                &transient_client),
            "rule wants non-transient, client is transient: no match");
}


/* NULL pointer fields (instance/class/role/title never set on this
 * client, e.g. WM_CLASS never provided) are treated as empty
 * strings, not a crash and not an automatic mismatch by themselves */
static void s_test_null_client_fields_treated_as_empty(void)
{
    client_td client;
    struct rules_match_s match;

    memset(&client, 0, sizeof(client));
    /* every info.* field left NULL, and properties.type/transient_for
     * left at their zeroed defaults */

    s_make_empty_match(&match);
    match.has_instance = true;
    match.instance_count = 1u;
    strcpy(match.instance[0], "*");  /* matches any string, incl. "" */

    TAP_OK(ri_client_matches(&match, &client),
            "a NULL instance field is treated as \"\", not a crash");

    strcpy(match.instance[0], "somename");
    TAP_OK(!ri_client_matches(&match, &client),
            "a NULL instance field (== \"\") does not match a"
            " specific non-empty pattern");
}


/* ri_parse_layer: NULL, every recognized name, and an unrecognized
 * one all resolve correctly */
static void s_test_parse_layer(void)
{
    TAP_EQ_INT(ri_parse_layer(NULL), CLIENT_LAYER_NORMAL,
            "NULL defaults to normal");
    TAP_EQ_INT(ri_parse_layer("above"), CLIENT_LAYER_ABOVE,
            "'above' recognized");
    TAP_EQ_INT(ri_parse_layer("below"), CLIENT_LAYER_BELOW,
            "'below' recognized");
    TAP_EQ_INT(ri_parse_layer("normal"), CLIENT_LAYER_NORMAL,
            "'normal' recognized");
    TAP_EQ_INT(ri_parse_layer("bogus"), CLIENT_LAYER_NORMAL,
            "an unrecognized name defaults to normal");
}


int main(void)
{
    TAP_PLAN(25);

    s_test_when_matches();
    s_test_no_criteria_matches_anything();
    s_test_instance_glob();
    s_test_multiple_alternatives_is_or();
    s_test_multiple_fields_is_and();
    s_test_type_matching();
    s_test_transient_matching();
    s_test_null_client_fields_treated_as_empty();
    s_test_parse_layer();

    return TAP_DONE();
}
