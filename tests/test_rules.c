/**
 * @file tests/test_rules.c
 *
 * @brief Test battery for window matching rule loading
 *
 * rules_apply itself lives in rules/apply.c, not this file (see that
 * file's own doc comment: "Split out of rules.c"), and is left for
 * its own dedicated test given how much heavier its own dependency
 * chain is; this covers rules_init/rules_destroy/rules_load only,
 * the JSON-to-struct parsing this file (rules.c) actually owns.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Local includes */
#include <harness/tap.h>
#include <rules.h>
#include <rules/internal.h>


/**
 * @brief Write 'content' to a fresh temporary directory's own
 *        rules.json, and return that directory's path
 */
static void s_write_rules_file(char *dir_out, size_t dir_out_size,
        const char *content)
{
    static int s_counter = 0;
    char path[512];
    FILE *f;

    snprintf(dir_out, dir_out_size, "/tmp/icowm_test_rules_%d_%d",
            (int) getpid(), s_counter++);
    mkdir(dir_out, 0700);
    snprintf(path, sizeof(path), "%s/rules.json", dir_out);
    f = fopen(path, "w");
    if (f != NULL) {
        fwrite(content, 1, strlen(content), f);
        fclose(f);
    }
}


/* rules_init gives a zeroed, ready-to-load table */
static void s_test_init_and_destroy(void)
{
    rules_td *rules = rules_init();

    TAP_NOT_NULL(rules, "rules_init succeeds");

    rules_destroy(rules);
    rules_destroy(NULL);
    TAP_OK(true, "rules_destroy(NULL) is safe, no crash");
}


/* rules_load with a NULL table fails cleanly */
static void s_test_load_null_table(void)
{
    TAP_EQ_INT(rules_load(NULL, "/tmp"), 1,
            "loading into a NULL table fails");
}


/* A missing or empty rules file is not an error: the table is just
 * left with zero rules, so the window manager can start without one */
static void s_test_load_missing_file_is_fine(void)
{
    rules_td *rules = rules_init();
    char dir[256];
    int rc;

    snprintf(dir, sizeof(dir), "/tmp/icowm_test_rules_missing_%d",
            (int) getpid());
    mkdir(dir, 0700);

    rc = rules_load(rules, dir);

    TAP_EQ_INT(rc, 0, "a missing rules file is not an error");
    TAP_EQ_INT((long) rules->count, 0, "no rules were loaded");

    rules_destroy(rules);
}


/* A single, fully populated rule entry loads every field correctly */
static void s_test_load_full_rule(void)
{
    rules_td *rules = rules_init();
    char dir[256];
    struct rules_rule_s *rule;
    static const char *content =
        "{\"rules\": [{"
        "  \"when\": \"property\","
        "  \"match\": {\"title\": \"My Terminal\", \"transient\": true},"
        "  \"apply\": {"
        "    \"desktop\": 2,"
        "    \"monitor\": 1,"
        "    \"layer\": \"above\","
        "    \"focus\": true,"
        "    \"pinned\": true,"
        "    \"decorated\": false,"
        "    \"iconified\": true,"
        "    \"fullscreen\": false,"
        "    \"maximized\": true,"
        "    \"shaded\": false,"
        "    \"hidden\": true,"
        "    \"opacity\": 80,"
        "    \"position\": {\"x\": 10, \"y\": 20},"
        "    \"size\": {\"width\": 640, \"height\": 480}"
        "  }"
        "}]}";

    s_write_rules_file(dir, sizeof(dir), content);
    rules_load(rules, dir);

    TAP_EQ_INT((long) rules->count, 1, "one rule loaded");
    rule = &rules->rules[0];

    TAP_EQ_INT((long) rule->when, (long) RULES_WHEN_PROPERTY,
            "'when' is parsed correctly");
    TAP_OK(rule->match.has_title, "match.title is present");
    TAP_EQ_STR(rule->match.title[0], "My Terminal",
            "match.title's own value is correct");
    TAP_OK(rule->match.has_transient && rule->match.is_transient,
            "match.transient is parsed as true");

    TAP_OK(rule->apply.has_desktop && rule->apply.desktop == 2u,
            "apply.desktop is parsed correctly");
    TAP_OK(rule->apply.has_monitor && rule->apply.monitor == 1u,
            "apply.monitor is parsed correctly");
    TAP_OK(rule->apply.has_layer, "apply.layer is present");
    TAP_OK(rule->apply.has_focus && rule->apply.is_focused,
            "apply.focus is parsed as true");
    TAP_OK(rule->apply.has_pinned && rule->apply.is_pinned,
            "apply.pinned is parsed as true");
    TAP_OK(rule->apply.has_decoration && !rule->apply.is_decorated,
            "apply.decoration is parsed as false");
    TAP_OK(rule->apply.has_iconified && rule->apply.is_iconified,
            "apply.iconified is parsed as true");
    TAP_OK(rule->apply.has_fullscreen && !rule->apply.is_fullscreen,
            "apply.fullscreen is parsed as false");
    TAP_OK(rule->apply.has_maximized && rule->apply.is_maximized,
            "apply.maximized is parsed as true");
    TAP_OK(rule->apply.has_shaded && !rule->apply.is_shaded,
            "apply.shaded is parsed as false");
    TAP_OK(rule->apply.has_hidden && rule->apply.is_hidden,
            "apply.hidden is parsed as true");
    TAP_OK(rule->apply.has_opacity_active &&
            rule->apply.opacity_active == 80u,
            "a single opacity number sets opacity_active");
    TAP_OK(rule->apply.has_opacity_inactive &&
            rule->apply.opacity_inactive == 80u,
            "...and opacity_inactive to the same value");
    TAP_OK(rule->apply.has_position &&
            !rule->apply.is_position_centered &&
            rule->apply.x == 10 && rule->apply.y == 20,
            "apply.position {x,y} is parsed correctly");
    TAP_OK(rule->apply.has_size && rule->apply.w == 640u &&
            rule->apply.h == 480u,
            "apply.size is parsed correctly");

    rules_destroy(rules);
}


/* A match criterion given as a JSON array of strings loads every
 * value, in order, up to the array itself */
static void s_test_load_match_array(void)
{
    rules_td *rules = rules_init();
    char dir[256];
    struct rules_rule_s *rule;
    static const char *content =
        "{\"rules\": [{"
        "  \"match\": {\"class\": [\"firefox\", \"chromium\"]},"
        "  \"apply\": {\"desktop\": 0}"
        "}]}";

    s_write_rules_file(dir, sizeof(dir), content);
    rules_load(rules, dir);

    rule = &rules->rules[0];
    TAP_EQ_INT((long) rule->match.class_count, 2,
            "both array values were loaded");
    TAP_EQ_STR(rule->match.klass[0], "firefox", "first value correct");
    TAP_EQ_STR(rule->match.klass[1], "chromium", "second value correct");

    rules_destroy(rules);
}


/* "position": "center" sets position_centered rather than x/y */
static void s_test_load_position_center(void)
{
    rules_td *rules = rules_init();
    char dir[256];
    struct rules_rule_s *rule;
    static const char *content =
        "{\"rules\": [{"
        "  \"apply\": {\"position\": \"center\"}"
        "}]}";

    s_write_rules_file(dir, sizeof(dir), content);
    rules_load(rules, dir);

    rule = &rules->rules[0];
    TAP_OK(rule->apply.has_position &&
            rule->apply.is_position_centered,
            "\"center\" sets is_position_centered rather than x/y");

    rules_destroy(rules);
}


/* Separate {"active": .., "inactive": ..} opacity values load
 * independently of one another */
static void s_test_load_opacity_split(void)
{
    rules_td *rules = rules_init();
    char dir[256];
    struct rules_rule_s *rule;
    static const char *content =
        "{\"rules\": [{"
        "  \"apply\": {\"opacity\": {\"active\": 90, \"inactive\": 40}}"
        "}]}";

    s_write_rules_file(dir, sizeof(dir), content);
    rules_load(rules, dir);

    rule = &rules->rules[0];
    TAP_OK(rule->apply.has_opacity_active &&
            rule->apply.opacity_active == 90u,
            "active opacity loaded independently");
    TAP_OK(rule->apply.has_opacity_inactive &&
            rule->apply.opacity_inactive == 40u,
            "inactive opacity loaded independently");

    rules_destroy(rules);
}


/* Opacity values outside 0-100 are clamped into that range, rather
 * than stored as-is or rejected */
static void s_test_opacity_clamping(void)
{
    rules_td *rules = rules_init();
    char dir[256];
    static const char *content =
        "{\"rules\": ["
        "  {\"apply\": {\"opacity\": -50}},"
        "  {\"apply\": {\"opacity\": 500}}"
        "]}";

    s_write_rules_file(dir, sizeof(dir), content);
    rules_load(rules, dir);

    TAP_EQ_INT((long) rules->rules[0].apply.opacity_active, 0,
            "a negative opacity clamps up to 0");
    TAP_EQ_INT((long) rules->rules[1].apply.opacity_active, 100,
            "an over-100 opacity clamps down to 100");

    rules_destroy(rules);
}


/* Multiple rule entries in the same file all load, in order */
static void s_test_load_multiple_rules(void)
{
    rules_td *rules = rules_init();
    char dir[256];
    static const char *content =
        "{\"rules\": ["
        "  {\"apply\": {\"desktop\": 0}},"
        "  {\"apply\": {\"desktop\": 1}},"
        "  {\"apply\": {\"desktop\": 2}}"
        "]}";

    s_write_rules_file(dir, sizeof(dir), content);
    rules_load(rules, dir);

    TAP_EQ_INT((long) rules->count, 3, "all three rules loaded");
    TAP_EQ_INT((long) rules->rules[0].apply.desktop, 0,
            "first rule's own desktop is correct");
    TAP_EQ_INT((long) rules->rules[2].apply.desktop, 2,
            "third rule's own desktop is correct");

    rules_destroy(rules);
}


int main(void)
{
    TAP_PLAN(36);

    s_test_init_and_destroy();
    s_test_load_null_table();
    s_test_load_missing_file_is_fine();
    s_test_load_full_rule();
    s_test_load_match_array();
    s_test_load_position_center();
    s_test_load_opacity_split();
    s_test_opacity_clamping();
    s_test_load_multiple_rules();

    return TAP_DONE();
}
