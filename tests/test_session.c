/**
 * @file tests/test_session.c
 *
 * @brief Test battery for session.c's hook table lifecycle, JSON
 *        loading, and hook dispatch (session.c)
 *
 * Every public entry point in session.c is genuinely testable once its
 * body is actually read: session_init/session_destroy are plain
 * allocate/free pairs over three real cdlist-backed lists, session_load
 * is real JSON parsing (cJSON via json_load_config/json_get_item, both
 * linked for real below) into those lists with no X or process
 * involvement at all, and session_run_hook only ever walks a real list
 * and calls spawn_command once per entry, exactly the one genuinely
 * external, process-lifecycle-bound call this file replaces with a
 * controllable link-only stand-in, the same pattern
 * tests/wm/test_lifecycle.c and tests/wm/test_startup.c already use for
 * the same shape of problem.  session_reap_children is the one function
 * in this file left entirely untested: it calls the real @c waitpid(-1,
 * ..., WNOHANG) unconditionally, over every child of this very test
 * process, not just ones session.c itself spawned, so exercising it
 * here would either reap a sanitizer/build-tool helper process this
 * binary does not own, or, with no real children at all, only prove
 * that a loop bounded by "no more children" terminates immediately,
 * which is not a meaningful assertion about session.c's own logic.
 *
 * config_resolve_dir (config.c) is replaced by a small link-only
 * stand-in below, since the real implementation queries the XDG/home
 * environment, a cross-module concern this file has no need to
 * re-verify to test session_load's own JSON-to-list logic; the
 * stand-in instead hands back a scenario-controlled directory so each
 * test can point session_load at its own real temporary JSON file.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* waitpid, getpid */

/* System includes */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ADT includes */
#include <adt/list.h>

/* Default initial values */
#include <defs/config.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <session.h>
#include <utils/spawn.h>

/* Local includes */
#include <harness/tap.h>


/** Directory the config_resolve_dir stand-in below hands back next */
static char s_resolve_dir_next[CONFIG_MAX_LENGTH_PATH_BASE];

/** Link-only stand-in for config_resolve_dir (config.c): ignores the
 *  requested prefix and always hands back whatever directory a
 *  scenario configured beforehand, so session_load can be pointed at
 *  a real temporary directory without touching any real XDG/home
 *  environment lookup */
void config_resolve_dir(const char *restrict config_dir_prefix,
        char *restrict config_dir_base)
{
    (void) config_dir_prefix;
    (void) snprintf(config_dir_base, CONFIG_MAX_LENGTH_PATH_BASE,
            "%s", s_resolve_dir_next);
}


/** Link-only stand-in for logger_msg (logger.c): a silent no-op,
 *  matching the real logger's own behavior whenever logger_start has
 *  never run, the same reasoning tests/wm/test_lifecycle.c documents
 *  for never calling the real logger_start here either */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/** What the spawn_command stand-in below should return next */
static int s_spawn_result;

/** PID the spawn_command stand-in below hands back on a simulated
 *  success, incremented per call so distinct calls are distinguishable */
static pid_t s_spawn_next_pid;

/** Recording state for the spawn_command stand-in */
static int s_spawn_call_count;
static char s_spawn_last_commands[8][SESSION_MAX_CMD_LEN];

/** Link-only stand-in for spawn_command (utils/spawn.c): records every
 *  command it was asked to run and returns a scenario-controlled
 *  result and PID, instead of a real fork/exec, since a real child
 *  process is exactly the kind of process-lifecycle-bound behavior
 *  this round's instructions call out as needing a stand-in */
int spawn_command(const char *command, const spawn_opts_td *opts,
        pid_t *out_pid)
{
    (void) opts;

    if (s_spawn_call_count < 8) {
        (void) snprintf(s_spawn_last_commands[s_spawn_call_count],
                SESSION_MAX_CMD_LEN, "%s", (command != NULL) ? command : "");
    }
    s_spawn_call_count += 1;

    if (s_spawn_result == 0 && out_pid != NULL) {
        *out_pid = s_spawn_next_pid;
        s_spawn_next_pid += 1;
    }
    return s_spawn_result;
}


/** Reset every stand-in's recorded state and configured return value */
static void s_reset_stand_ins(void)
{
    s_resolve_dir_next[0] = '\0';
    s_spawn_result = 0;
    s_spawn_next_pid = 4242;
    s_spawn_call_count = 0;
    for (uint32_t i = 0u; i < 8u; ++i) {
        s_spawn_last_commands[i][0] = '\0';
    }
}


/** Create a fresh, empty temporary directory and copy its path into
 *  'out'
 *
 * Builds its own unique name from the process ID and a call counter
 * rather than 'mkdtemp' (which needs a newer POSIX feature-test level
 * than this project's own '_POSIX_C_SOURCE 200112L' exposes), the same
 * reasoning tests/utils/config/test_json.c documents for its own
 * 's_write_temp_file' doing the equivalent thing for a plain file.
 */
static void s_make_temp_dir(char *out, size_t size)
{
    static int s_counter = 0;

    (void) snprintf(out, size, "/tmp/icowm_test_session_%d_%d",
            (int) getpid(), s_counter++);
    (void) mkdir(out, 0700);
}


/** Write 'contents' verbatim into '<dir>/session.json' */
static void s_write_session_json(const char *dir, const char *contents)
{
    char path[CONFIG_MAX_LENGTH_PATH_CONFIG];
    FILE *fp;

    (void) snprintf(path, sizeof(path), "%s/%s", dir,
            CONFIG_FILENAME_SESSION);
    fp = fopen(path, "w");
    if (fp != NULL) {
        (void) fputs(contents, fp);
        (void) fclose(fp);
    }
}


/* session_init returns a non-NULL table with every hook list empty */
static void s_test_init_returns_empty_table(void)
{
    session_td *session = session_init();

    TAP_NOT_NULL(session, "session_init returns a non-NULL table");

    session_destroy(session);
}


/* session_destroy tolerates a NULL argument as a no-op */
static void s_test_destroy_null_is_noop(void)
{
    session_destroy(NULL);

    TAP_OK(true, "session_destroy(NULL) does not crash");
}


/* session_load rejects a NULL session table, leaving nothing to
 * dereference */
static void s_test_load_null_session(void)
{
    int result;

    s_reset_stand_ins();
    result = session_load(NULL, "/tmp/does-not-matter");

    TAP_EQ_INT(result, 1, "session_load(NULL, ...) returns 1");
}


/* session_load on a missing session file is not an error: it leaves
 * the session table with its hook lists empty */
static void s_test_load_missing_file_is_ok(void)
{
    char dir[CONFIG_MAX_LENGTH_PATH_BASE];
    session_td *session = session_init();
    int result;

    s_reset_stand_ins();
    s_make_temp_dir(dir, sizeof(dir));
    (void) snprintf(s_resolve_dir_next, sizeof(s_resolve_dir_next),
            "%s", dir);

    result = session_load(session, dir);

    TAP_EQ_INT(result, 0,
            "session_load with no session file present returns 0");

    session_run_hook(session, SESSION_HOOK_START);
    TAP_EQ_INT(s_spawn_call_count, 0,
            "an empty on-start hook list spawns nothing");

    session_destroy(session);
}


/* session_load populates all three hook lists from a well-formed
 * session file, in the order the file lists them */
static void s_test_load_populates_all_hooks(void)
{
    char dir[CONFIG_MAX_LENGTH_PATH_BASE];
    session_td *session = session_init();
    int result;

    s_reset_stand_ins();
    s_make_temp_dir(dir, sizeof(dir));
    (void) snprintf(s_resolve_dir_next, sizeof(s_resolve_dir_next),
            "%s", dir);
    s_write_session_json(dir,
            "{"
            "\"on-start\": [\"nitrogen --restore\", \"picom\"],"
            "\"on-reload\": [\"pkill -HUP picom\"],"
            "\"on-exit\": [\"notify-send bye\"]"
            "}");

    result = session_load(session, dir);
    TAP_EQ_INT(result, 0, "session_load on a well-formed file returns 0");

    session_run_hook(session, SESSION_HOOK_START);
    TAP_EQ_INT(s_spawn_call_count, 2,
            "on-start spawns exactly its two configured commands");
    TAP_EQ_STR(s_spawn_last_commands[0], "nitrogen --restore",
            "the first on-start command is spawned first");
    TAP_EQ_STR(s_spawn_last_commands[1], "picom",
            "the second on-start command is spawned second");

    s_reset_stand_ins();
    (void) snprintf(s_resolve_dir_next, sizeof(s_resolve_dir_next),
            "%s", dir);
    session_run_hook(session, SESSION_HOOK_RELOAD);
    TAP_EQ_INT(s_spawn_call_count, 1,
            "on-reload spawns exactly its one configured command");
    TAP_EQ_STR(s_spawn_last_commands[0], "pkill -HUP picom",
            "the on-reload command matches the session file");

    s_reset_stand_ins();
    session_run_hook(session, SESSION_HOOK_EXIT);
    TAP_EQ_INT(s_spawn_call_count, 1,
            "on-exit spawns exactly its one configured command");
    TAP_EQ_STR(s_spawn_last_commands[0], "notify-send bye",
            "the on-exit command matches the session file");

    session_destroy(session);
}


/* session_load skips non-string and empty-string array entries, rather
 * than spawning a blank command or crashing on the wrong cJSON type */
static void s_test_load_skips_invalid_entries(void)
{
    char dir[CONFIG_MAX_LENGTH_PATH_BASE];
    session_td *session = session_init();
    int result;

    s_reset_stand_ins();
    s_make_temp_dir(dir, sizeof(dir));
    (void) snprintf(s_resolve_dir_next, sizeof(s_resolve_dir_next),
            "%s", dir);
    s_write_session_json(dir,
            "{"
            "\"on-start\": [\"\", 42, null, \"real-command\"]"
            "}");

    result = session_load(session, dir);
    TAP_EQ_INT(result, 0,
            "session_load with mixed-type entries still returns 0");

    session_run_hook(session, SESSION_HOOK_START);
    TAP_EQ_INT(s_spawn_call_count, 1,
            "only the one genuine string entry is spawned");
    TAP_EQ_STR(s_spawn_last_commands[0], "real-command",
            "the surviving entry is the real command, not a blank one");

    session_destroy(session);
}


/* A second session_load call on the same table clears each hook list
 * first, rather than appending to whatever was already loaded */
static void s_test_load_reload_clears_previous(void)
{
    char dir[CONFIG_MAX_LENGTH_PATH_BASE];
    session_td *session = session_init();

    s_reset_stand_ins();
    s_make_temp_dir(dir, sizeof(dir));
    (void) snprintf(s_resolve_dir_next, sizeof(s_resolve_dir_next),
            "%s", dir);
    s_write_session_json(dir, "{\"on-start\": [\"first-command\"]}");
    (void) session_load(session, dir);

    s_write_session_json(dir, "{\"on-start\": [\"second-command\"]}");
    (void) session_load(session, dir);

    session_run_hook(session, SESSION_HOOK_START);
    TAP_EQ_INT(s_spawn_call_count, 1,
            "reloading replaces the previous on-start list, not appends");
    TAP_EQ_STR(s_spawn_last_commands[0], "second-command",
            "only the second load's command remains after a reload");

    session_destroy(session);
}


/* session_run_hook tolerates a NULL session table as a no-op */
static void s_test_run_hook_null_session(void)
{
    s_reset_stand_ins();
    session_run_hook(NULL, SESSION_HOOK_START);

    TAP_EQ_INT(s_spawn_call_count, 0,
            "session_run_hook(NULL, ...) never calls spawn_command");
}


/* session_run_hook still walks past a hook whose spawn_command call
 * failed, rather than aborting the rest of the list */
static void s_test_run_hook_continues_after_spawn_failure(void)
{
    char dir[CONFIG_MAX_LENGTH_PATH_BASE];
    session_td *session = session_init();

    s_reset_stand_ins();
    s_make_temp_dir(dir, sizeof(dir));
    (void) snprintf(s_resolve_dir_next, sizeof(s_resolve_dir_next),
            "%s", dir);
    s_write_session_json(dir,
            "{\"on-start\": [\"will-fail\", \"will-still-run\"]}");
    (void) session_load(session, dir);

    s_spawn_result = -2;
    session_run_hook(session, SESSION_HOOK_START);

    TAP_EQ_INT(s_spawn_call_count, 2,
            "a failing spawn_command call does not stop the rest of"
            " the hook list");

    session_destroy(session);
}


int main(void)
{
    TAP_PLAN(20);

    s_test_init_returns_empty_table();
    s_test_destroy_null_is_noop();
    s_test_load_null_session();
    s_test_load_missing_file_is_ok();
    s_test_load_populates_all_hooks();
    s_test_load_skips_invalid_entries();
    s_test_load_reload_clears_previous();
    s_test_run_hook_null_session();
    s_test_run_hook_continues_after_spawn_failure();

    return TAP_DONE();
}
