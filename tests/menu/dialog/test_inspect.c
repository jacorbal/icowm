/**
 * @file tests/menu/dialog/test_inspect.c
 *
 * @brief Test battery for the window property inspector dialog
 *
 * 'dialog_inspect_show' composes every row from a real 'client_td'
 * fixture through the genuine 'client/predicates.h' macros (no
 * stubbing needed, since those are pure bit tests over a struct this
 * file already controls directly), then hands the finished rows to
 * 'menu_message_dialog_show_pairs', which is the one recording
 * stand-in below: how those rows are laid out and drawn is message.c's
 * own, separately tested, concern.  'stage_monitor_for_point' is
 * likewise a recording stand-in: it is 'src/stage/monitors.c' 's own
 * logic, a whole other module, not anything inspect.c itself defines.
 * 'dialog_pair_append_blank' is a tiny, pure, link-only reproduction of
 * message.c's own two-line helper, the same way test_confirm.c
 * reproduces 'dlgutil_u16max' rather than linking message.c's whole
 * (separately tested) file for it.
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
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <adt/cdlist.h>
#include <client.h>
#include <client/predicates.h>
#include <config.h>
#include <defs/dialog.h>
#include <stage.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/dialog/inspect.h>
#include <menu/dialog/message.h>


/** Fake, non-null XCB connection handle, standing in for a live one
 *  wherever inspect.c merely forwards it onward without ever
 *  dereferencing it itself */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;

/** Recording stand-ins' own call counters and captured arguments,
 *  reset by s_reset between scenarios; each row's label/value text is
 *  copied into its own fixed buffer rather than kept as the caller's
 *  pointer, since inspect.c's own row storage ('struct
 *  s_inspect_ctx_s', file-static to inspect.c) goes out of scope the
 *  instant 'dialog_inspect_show' returns */
static int s_call_show_pairs;
static char s_captured_labels
        [DIALOG_MSG_MAX_LINES][DIALOG_MSG_LINE_MAX_LENGTH];
static char s_captured_values
        [DIALOG_MSG_MAX_LINES][DIALOG_MSG_LINE_MAX_LENGTH];
static bool s_captured_has_value[DIALOG_MSG_MAX_LINES];
static bool s_captured_has_label[DIALOG_MSG_MAX_LINES];
static uint8_t s_captured_count;
static menu_msg_level_e s_captured_level;

/** Monitor this file's own stage_monitor_for_point stand-in answers */
static monitor_td s_stub_monitor;
static int s_call_monitor_for_point;


/**
 * @brief Reset every stand-in's recorded call state between scenarios
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    s_call_show_pairs = 0;
    memset(s_captured_labels, 0, sizeof(s_captured_labels));
    memset(s_captured_values, 0, sizeof(s_captured_values));
    memset(s_captured_has_value, 0, sizeof(s_captured_has_value));
    memset(s_captured_has_label, 0, sizeof(s_captured_has_label));
    s_captured_count = 0u;
    s_captured_level = MENU_MSG_LEVEL_NONE;
    memset(&s_stub_monitor, 0, sizeof(s_stub_monitor));
    s_stub_monitor.w = 1920u;
    s_stub_monitor.h = 1080u;
    s_call_monitor_for_point = 0;
}


/**
 * @brief Link-only stand-in for @a dialog_pair_append_blank
 *
 * A tiny, pure helper genuinely defined in menu/dialog/message.c
 * rather than inspect.c itself; reproduced here rather than linking
 * that whole (separately tested) file, the same way
 * tests/menu/dialog/test_confirm.c reproduces 'dlgutil_u16max' rather
 * than linking all of menu/dialog.c for one two-line function.
 *
 * @note Complexity: @e O(1)
 */
void dialog_pair_append_blank(struct dialog_pair_s *pairs, uint8_t *count)
{
    if (*count >= (uint8_t) DIALOG_MSG_MAX_LINES) {
        return;
    }

    pairs[*count].label = NULL;
    pairs[*count].value = NULL;
    (*count)++;
}


/**
 * @brief Recording stand-in for @a menu_message_dialog_show_pairs
 *
 * Copies every row rather than keeping the caller's pointer, since
 * inspect.c's own row storage ('struct s_inspect_ctx_s', file-static
 * to inspect.c) goes out of scope the instant 'dialog_inspect_show'
 * returns.
 *
 * @note Complexity: @e O(n), where @e n is @p count
 */
void menu_message_dialog_show_pairs(xcb_connection_t *connection,
        stage_td *stage, const config_td *config,
        const struct dialog_pair_s *pairs, size_t count,
        menu_msg_level_e level)
{
    size_t i;

    (void) connection;
    (void) stage;
    (void) config;
    s_call_show_pairs++;
    s_captured_count = (uint8_t) count;
    s_captured_level = level;
    for (i = 0u; i < count && i < DIALOG_MSG_MAX_LINES; i++) {
        s_captured_has_label[i] = (pairs[i].label != NULL);
        s_captured_has_value[i] = (pairs[i].value != NULL);
        if (pairs[i].label != NULL) {
            (void) strncpy(s_captured_labels[i], pairs[i].label,
                    DIALOG_MSG_LINE_MAX_LENGTH - 1u);
        }
        if (pairs[i].value != NULL) {
            (void) strncpy(s_captured_values[i], pairs[i].value,
                    DIALOG_MSG_LINE_MAX_LENGTH - 1u);
        }
    }
}


/**
 * @brief Test-controlled stand-in for @a stage_monitor_for_point
 * @note Complexity: @e O(1)
 */
monitor_td stage_monitor_for_point(const stage_td *stage,
        struct position_s point)
{
    (void) stage;
    (void) point;
    s_call_monitor_for_point++;
    return s_stub_monitor;
}


/**
 * @brief Find a captured row by its exact label
 *
 * @return The row's value (possibly NULL, for a heading), or NULL if
 *         no captured row has that label
 *
 * @note Complexity: @e O(n), where @e n is the captured row count
 */
static const char *s_find_value(const char *label)
{
    uint8_t i;

    for (i = 0u; i < s_captured_count; i++) {
        if (s_captured_has_label[i] &&
                strcmp(s_captured_labels[i], label) == 0) {
            return (s_captured_has_value[i]) ? s_captured_values[i] : NULL;
        }
    }
    return NULL;
}


/**
 * @brief Build a minimal, real client fixture: an ordinary, decorated,
 *        resizable, focused, unmaximized, non-modal top-level window
 * @note Complexity: @e O(1)
 */
static void s_make_client(client_td *client)
{
    memset(client, 0, sizeof(*client));
    client->info.name = "xterm";
    client->info.class_name[0] = "XTerm";
    client->info.class_name[1] = "xterm";
    client->info.role_name = "";
    client->properties.type = CLIENT_TYPE_NORMAL;
    client->properties.layer = CLIENT_LAYER_NORMAL;
    client->properties.flags = (uint16_t)
        (CLIENT_FLAG_FOCUSABLE | CLIENT_FLAG_DECORATED |
         CLIENT_FLAG_RESIZABLE | CLIENT_FLAG_FOCUSED);
    client->process.pid = 0;
    client->desktop_id = 2u;
    client->screen_id = 0u;
    client->layout.geometry.cur.pos.x = 100;
    client->layout.geometry.cur.pos.y = 50;
    client->layout.geometry.cur.dim.w = 640u;
    client->layout.geometry.cur.dim.h = 480u;
    client->layout.frame_extents.left = 2u;
    client->layout.frame_extents.top = 20u;
    client->layout.frame_extents.right = 2u;
    client->layout.frame_extents.bottom = 2u;
    client->hints_icccm.size.min.w = 1u;
    client->hints_icccm.size.min.h = 1u;
    client->hints_icccm.size.max.w = 0u;
    client->hints_icccm.size.max.h = 0u;
    client->transient_parent = NULL;
    client->transients = NULL;
    client->window = (xcb_window_t) 0x100u;
    client->frame = (xcb_window_t) 0x101u;
    client->titlebar = XCB_WINDOW_NONE;
    client->icon_window = XCB_WINDOW_NONE;
}


/**
 * @brief Build a minimal, real stage fixture
 * @note Complexity: @e O(1)
 */
static void s_make_stage(stage_td *stage)
{
    memset(stage, 0, sizeof(*stage));
}


/* dialog_inspect_show is a no-op on every guarded NULL argument */
static void s_test_null_guards(void)
{
    stage_td stage;
    config_td config;
    client_td client;

    s_make_stage(&stage);
    memset(&config, 0, sizeof(config));
    s_make_client(&client);

    s_reset();
    dialog_inspect_show(NULL, &stage, &config, &client);
    TAP_EQ_INT(s_call_show_pairs, 0,
            "inspect_show: NULL connection is a no-op");

    s_reset();
    dialog_inspect_show(s_fake_connection, NULL, &config, &client);
    TAP_EQ_INT(s_call_show_pairs, 0,
            "inspect_show: NULL stage is a no-op");

    s_reset();
    dialog_inspect_show(s_fake_connection, &stage, NULL, &client);
    TAP_EQ_INT(s_call_show_pairs, 0,
            "inspect_show: NULL config is a no-op");

    s_reset();
    dialog_inspect_show(s_fake_connection, &stage, &config, NULL);
    TAP_EQ_INT(s_call_show_pairs, 0,
            "inspect_show: NULL client is a no-op");
}


/* An ordinary client's identity, placement, and window rows are
 * composed from its own fields */
static void s_test_identity_and_placement_rows(void)
{
    stage_td stage;
    config_td config;
    client_td client;

    s_make_stage(&stage);
    memset(&config, 0, sizeof(config));
    s_make_client(&client);
    s_reset();

    dialog_inspect_show(s_fake_connection, &stage, &config, &client);

    TAP_EQ_INT(s_call_show_pairs, 1,
            "inspect_show: shows the dialog exactly once");
    TAP_EQ_INT((int) s_captured_level, (int) MENU_MSG_LEVEL_INFO,
            "inspect_show: shows the pairs at level INFO");
    TAP_EQ_STR(s_find_value("Title"), "xterm",
            "inspect_show: Title row shows the client's window name");
    TAP_EQ_STR(s_find_value("Class"), "XTerm / xterm",
            "inspect_show: Class row shows both class-name components");
    TAP_EQ_STR(s_find_value("Type"), "normal",
            "inspect_show: Type row names an ordinary client \"normal\"");
    TAP_EQ_STR(s_find_value("Desktop"), "2",
            "inspect_show: Desktop row shows the client's desktop index"
            " for an unpinned client, with no \"pinned\" annotation");
    TAP_EQ_STR(s_find_value("Frame extents"), "2, 20, 2, 2",
            "inspect_show: Frame extents row lists left/top/right/bottom"
            " in that order");
    TAP_EQ_STR(s_find_value("Layer"), "normal",
            "inspect_show: Layer row names the normal layer \"normal\"");
    TAP_EQ_STR(s_find_value("Client"), "0x100",
            "inspect_show: Client row shows the client window ID in hex");
    TAP_EQ_STR(s_find_value("Frame"), "0x101",
            "inspect_show: Frame row shows the frame window ID in hex");
}


/* A pinned client's desktop row is annotated, and a process with no
 * live PID omits its row entirely, while one with a positive PID
 * includes it */
static void s_test_pinned_and_process_rows(void)
{
    stage_td stage;
    config_td config;
    client_td client;

    s_make_stage(&stage);
    memset(&config, 0, sizeof(config));
    s_make_client(&client);
    client.properties.flags |= (uint16_t) CLIENT_FLAG_PIN;
    s_reset();

    dialog_inspect_show(s_fake_connection, &stage, &config, &client);

    TAP_EQ_STR(s_find_value("Desktop"), "2 (pinned to all)",
            "inspect_show: pinned client's Desktop row is annotated");
    TAP_NULL(s_find_value("Process"),
            "inspect_show: a client with pid <= 0 omits the Process row"
            " entirely");

    s_make_client(&client);
    client.process.pid = 4242;
    s_reset();
    dialog_inspect_show(s_fake_connection, &stage, &config, &client);
    TAP_EQ_STR(s_find_value("Process"), "4242",
            "inspect_show: a client with a positive pid shows its"
            " Process row");
}


/* Every state flag this file sets is reflected in the "Is" row's
 * comma-joined list, and every one left unset shows up in "Is not"
 * instead */
static void s_test_state_flags_partition_is_and_is_not(void)
{
    stage_td stage;
    config_td config;
    client_td client;
    const char *is_list;
    const char *is_not_list;

    s_make_stage(&stage);
    memset(&config, 0, sizeof(config));
    s_make_client(&client);
    client.properties.flags = (uint16_t)
        (CLIENT_FLAG_FOCUSED | CLIENT_FLAG_DECORATED |
         CLIENT_FLAG_RESIZABLE);
    client.properties.state = (uint16_t) CLIENT_STATE_ICONIFIED;
    s_reset();

    dialog_inspect_show(s_fake_connection, &stage, &config, &client);

    is_list = s_find_value("Is");
    is_not_list = s_find_value("Is not");

    TAP_OK(is_list != NULL && strstr(is_list, "focused") != NULL,
            "inspect_show: a focused client lists \"focused\" in Is");
    TAP_OK(is_list != NULL && strstr(is_list, "decorated") != NULL,
            "inspect_show: a decorated client lists \"decorated\" in Is");
    TAP_OK(is_not_list != NULL && strstr(is_not_list, "modal") != NULL,
            "inspect_show: a non-modal client lists \"modal\" in Is not");
    TAP_OK(is_not_list != NULL &&
            strstr(is_not_list, "fullscreen") != NULL,
            "inspect_show: a non-fullscreen client lists \"fullscreen\""
            " in Is not");
    TAP_OK(is_list != NULL && strstr(is_list, "iconified") != NULL,
            "inspect_show: an iconified client lists \"iconified\" in"
            " Is");
    TAP_OK(is_not_list != NULL && strstr(is_not_list, "sticky") != NULL,
            "inspect_show: a non-sticky client lists \"sticky\" in Is"
            " not");
}


/* A client with no maximum size hint shows "unlimited" rather than a
 * literal 0x0 */
static void s_test_unlimited_max_size(void)
{
    stage_td stage;
    config_td config;
    client_td client;

    s_make_stage(&stage);
    memset(&config, 0, sizeof(config));
    s_make_client(&client);
    client.hints_icccm.size.max.w = 0u;
    client.hints_icccm.size.max.h = 0u;
    s_reset();

    dialog_inspect_show(s_fake_connection, &stage, &config, &client);

    TAP_EQ_STR(s_find_value("Maximum"), "unlimited",
            "inspect_show: a zero max size hint shows \"unlimited\"");

    s_make_client(&client);
    client.hints_icccm.size.max.w = 800u;
    client.hints_icccm.size.max.h = 600u;
    s_reset();
    dialog_inspect_show(s_fake_connection, &stage, &config, &client);
    TAP_EQ_STR(s_find_value("Maximum"), "800x600",
            "inspect_show: a genuine max size hint is shown as WxH");
}


/* A client with no transient parent shows "(none)"; one with a
 * transient parent shows that parent's window ID and title, and its
 * own Transients count reflects its real transient children list */
static void s_test_transient_relations_rows(void)
{
    stage_td stage;
    config_td config;
    client_td client;
    client_td parent;
    cdlist_td *children;

    s_make_stage(&stage);
    memset(&config, 0, sizeof(config));
    s_make_client(&client);
    s_reset();

    dialog_inspect_show(s_fake_connection, &stage, &config, &client);
    TAP_EQ_STR(s_find_value("Transient for"), "(none)",
            "inspect_show: a client with no transient parent shows"
            " \"(none)\"");
    TAP_EQ_STR(s_find_value("Transients"), "0",
            "inspect_show: a client with a NULL transients list shows"
            " zero transients");

    s_make_client(&parent);
    parent.info.name = "editor";
    parent.window = (xcb_window_t) 0x55u;
    s_make_client(&client);
    client.transient_parent = &parent;
    children = cdlist_init(NULL);
    (void) cdlist_ins_next(children, NULL, &client);
    parent.transients = children;
    s_reset();

    dialog_inspect_show(s_fake_connection, &stage, &config, &client);
    TAP_EQ_STR(s_find_value("Transient for"), "0x55 (editor)",
            "inspect_show: a transient client shows its parent's window"
            " ID and title");

    cdlist_destroy(children);
}


int main(void)
{
    TAP_PLAN(28);

    s_test_null_guards();
    s_test_identity_and_placement_rows();
    s_test_pinned_and_process_rows();
    s_test_state_flags_partition_is_and_is_not();
    s_test_unlimited_max_size();
    s_test_transient_relations_rows();

    return TAP_DONE();
}
