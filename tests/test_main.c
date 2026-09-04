/**
 * @file tests/test_main.c
 *
 * @brief Test battery for main.c's argv-parsing and flag-validation
 *        guard clauses
 *
 * main.c's own 'main' is, on the surface, "mostly argv parsing and
 * a real X connection/event-loop handoff", but reading its actual
 * body shows every option branch either terminates before ever
 * reaching 'wm_start' (the '-h', '-v', and default/unknown-option
 * cases, plus the '-C' lint path and the '-M' range check) or feeds a
 * plain string/number straight through to a real cross-module call
 * ('wm_start', 'logger_start', 'config_lint_run') this file replaces
 * with a controllable link-only stand-in below, exactly the pattern
 * 'tests/wm/test_startup.c' and 'tests/wm/test_lifecycle.c' already
 * use for the same shape of problem.  Nothing here ever reaches a real
 * X connection, a real log file, or a real 'execvp': every stand-in
 * below is a harmless, recording no-op, so every scenario exercises
 * exactly 'main' itself: its option dispatch, its early-return guard
 * clauses, and the values it threads through to the functions it
 * calls, never anything downstream of those calls.
 *
 * 'main' itself is called directly, several times over, once per
 * scenario, each with its own 'argv'; 'getopt' keeps its scan
 * position in the file-scope 'optind', so it is reset to 1 before
 * every call, the documented, portable way to reuse 'getopt' more than
 * once in the same process.  Two things 'main' does are deliberately
 * never exercised here: the 'execvp' restart path (a real process
 * image replacement, only reached when the link-only 'wm_restart_
 * requested' stand-in below is made to return true, which no scenario
 * here does, since asserting anything after a successful 'execvp'
 * would require a child process to observe it in, not a plain
 * assertion in this same process) and the allocation-failure branches
 * of 's_replace_option_string' inside the '-d'/'-c' cases (only
 * reachable when 'safe_strdup' itself fails to allocate, which is not
 * something a scenario here can force without faking 'malloc' itself,
 * a class of stand-in none of the four patterns this round's
 * instructions point at attempts).
 *
 * main.c's own 'main' cannot keep that name in this test binary,
 * since this file needs its own 'main' to drive the TAP harness; the
 * compile command below therefore builds src/main.c with
 * '-Dmain=icowm_main', a preprocessor rename applied only to that
 * translation unit, exactly the way this file's own 'int
 * icowm_main(int argc, char *const argv[])' declaration expects,
 * without editing src/main.c itself in any way.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* Enable 'optind' reset and 'getopt' itself */
#define _POSIX_C_SOURCE 200112L

/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Default initial values */
#include <defs/main.h>
#include <defs/memguard.h>

/* Project includes */
#include <config.h>
#include <config/lint.h>
#include <logger.h>
#include <wm.h>

/* Local includes */
#include <harness/tap.h>


/** main.c's own entry point, renamed to this at compile time via
 *  '-Dmain=icowm_main' so this file can supply its own 'main' to
 *  drive the TAP harness instead */
int icowm_main(int argc, char *const argv[]);


/** Recording state for the config_resolve_dir stand-in */
static char s_resolve_dir_last_prefix[CONFIG_MAX_LENGTH_PATH_BASE];
static int s_resolve_dir_call_count;

/** Link-only stand-in for config_resolve_dir (config.c): records the
 *  prefix it was asked to resolve, and hands back a fixed, harmless
 *  path, rather than touching any real XDG/home environment lookup */
void config_resolve_dir(const char *restrict config_dir_prefix,
        char *restrict config_dir_base)
{
    s_resolve_dir_call_count += 1;
    if (config_dir_prefix != NULL) {
        (void) snprintf(s_resolve_dir_last_prefix,
                sizeof(s_resolve_dir_last_prefix), "%s", config_dir_prefix);
    } else {
        s_resolve_dir_last_prefix[0] = '\0';
    }
    (void) snprintf(config_dir_base, CONFIG_MAX_LENGTH_PATH_BASE,
            "%s", "/tmp/icowm-test-config-dir");
}


/** What the config_lint_run stand-in below should return next */
static int s_lint_run_result;

/** Recording state for the config_lint_run stand-in */
static char s_lint_run_last_dir[CONFIG_MAX_LENGTH_PATH_BASE];
static int s_lint_run_call_count;

/** Link-only stand-in for config_lint_run (config/lint.c): records the
 *  resolved directory it was handed and returns whatever a scenario
 *  configured beforehand, instead of really reading configuration
 *  files from disk */
int config_lint_run(const char *config_dir)
{
    s_lint_run_call_count += 1;
    (void) snprintf(s_lint_run_last_dir, sizeof(s_lint_run_last_dir),
            "%s", (config_dir != NULL) ? config_dir : "");
    return s_lint_run_result;
}


/** What the logger_start stand-in below should return next */
static int s_logger_start_result;

/** Recording state for the logger_start stand-in */
static int s_logger_start_call_count;
static char s_logger_start_last_filename[256];
static enum logger_level_e s_logger_start_last_level;
static bool s_logger_start_last_tracking;

/** Link-only stand-in for logger_start (logger.c): records the
 *  arguments it was handed and returns whatever a scenario configured
 *  beforehand, rather than opening any real log destination */
int logger_start(const char *filename, enum logger_level_e level_min,
        bool is_tracking)
{
    s_logger_start_call_count += 1;
    (void) snprintf(s_logger_start_last_filename,
            sizeof(s_logger_start_last_filename), "%s",
            (filename != NULL) ? filename : "");
    s_logger_start_last_level = level_min;
    s_logger_start_last_tracking = is_tracking;
    return s_logger_start_result;
}


/** Recording state for the logger_stop stand-in */
static int s_logger_stop_call_count;

/** Link-only stand-in for logger_stop (logger.c): a harmless recording
 *  no-op, since no scenario here ever started a real logger to stop */
int logger_stop(void)
{
    s_logger_stop_call_count += 1;
    return 0;
}


/** Recording state for the logger_msg stand-in */
static int s_logger_msg_call_count;

/** Link-only stand-in for logger_msg (logger.c): a silent no-op,
 *  matching the real logger's own behavior whenever logger_start has
 *  never actually run, the same reasoning tests/wm/test_lifecycle.c
 *  documents for never calling the real logger_start here either */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    s_logger_msg_call_count += 1;
    return 0;
}


/** What the wm_start stand-in below should return next */
static int s_wm_start_result;

/** Recording state for the wm_start stand-in */
static int s_wm_start_call_count;
static char s_wm_start_last_display[256];
static char s_wm_start_last_config_dir[256];
static uint32_t s_wm_start_last_mib;
static bool s_wm_start_last_ipc_disabled;
static bool s_wm_start_last_replace_requested;

/** Link-only stand-in for wm_start (wm.c): records every argument
 *  main passed through and returns whatever a scenario configured
 *  beforehand, rather than dialing a real xcb_connect and running the
 *  whole startup sequence */
int wm_start(const char *restrict display_name,
        const char *restrict config_dir_prefix,
        uint32_t restricted_memory_mib, bool ipc_disabled,
        bool replace_requested)
{
    s_wm_start_call_count += 1;
    (void) snprintf(s_wm_start_last_display,
            sizeof(s_wm_start_last_display), "%s",
            (display_name != NULL) ? display_name : "");
    (void) snprintf(s_wm_start_last_config_dir,
            sizeof(s_wm_start_last_config_dir), "%s",
            (config_dir_prefix != NULL) ? config_dir_prefix : "");
    s_wm_start_last_mib = restricted_memory_mib;
    s_wm_start_last_ipc_disabled = ipc_disabled;
    s_wm_start_last_replace_requested = replace_requested;
    return s_wm_start_result;
}


/** Recording state for the wm_stop stand-in */
static int s_wm_stop_call_count;

/** Link-only stand-in for wm_stop (wm.c): a harmless recording no-op,
 *  since no scenario here ever brings up a real singleton to tear
 *  down */
int wm_stop(void)
{
    s_wm_stop_call_count += 1;
    return 0;
}


/** What the wm_restart_requested stand-in below should return next */
static bool s_wm_restart_requested_result;

/** Link-only stand-in for wm_restart_requested (wm.c): every scenario
 *  here leaves this false, so 'main' never reaches its real 'execvp'
 *  call, which would replace this very test process's image and never
 *  return control to it */
bool wm_restart_requested(void)
{
    return s_wm_restart_requested_result;
}


/** Reset every stand-in's recorded state and configured return value
 *  to a fresh, known baseline before each scenario */
static void s_reset_stand_ins(void)
{
    s_resolve_dir_last_prefix[0] = '\0';
    s_resolve_dir_call_count = 0;
    s_lint_run_result = 0;
    s_lint_run_last_dir[0] = '\0';
    s_lint_run_call_count = 0;
    s_logger_start_result = 0;
    s_logger_start_call_count = 0;
    s_logger_start_last_filename[0] = '\0';
    s_logger_start_last_level = LOG_MIN_LEVEL;
    s_logger_start_last_tracking = false;
    s_logger_stop_call_count = 0;
    s_logger_msg_call_count = 0;
    s_wm_start_result = 0;
    s_wm_start_call_count = 0;
    s_wm_start_last_display[0] = '\0';
    s_wm_start_last_config_dir[0] = '\0';
    s_wm_start_last_mib = 0u;
    s_wm_start_last_ipc_disabled = false;
    s_wm_start_last_replace_requested = false;
    s_wm_stop_call_count = 0;
    s_wm_restart_requested_result = false;
}


/* '-h' shows help and returns 0 without ever reaching logger_start or
 * wm_start */
static void s_test_help_flag(void)
{
    char *argv[] = { "icowm", "-h", NULL };
    int result;

    s_reset_stand_ins();
    optind = 1;
    result = icowm_main(2, argv);

    TAP_EQ_INT(result, 0, "'-h' returns 0");
    TAP_EQ_INT(s_wm_start_call_count, 0,
            "'-h' never reaches wm_start");
    TAP_EQ_INT(s_logger_start_call_count, 0,
            "'-h' never reaches logger_start");
}


/* '-v' shows version information and returns 0 without ever reaching
 * logger_start or wm_start */
static void s_test_version_flag(void)
{
    char *argv[] = { "icowm", "-v", NULL };
    int result;

    s_reset_stand_ins();
    optind = 1;
    result = icowm_main(2, argv);

    TAP_EQ_INT(result, 0, "'-v' returns 0");
    TAP_EQ_INT(s_wm_start_call_count, 0,
            "'-v' never reaches wm_start");
    TAP_EQ_INT(s_logger_start_call_count, 0,
            "'-v' never reaches logger_start");
}


/* An unrecognized option makes getopt itself return '?', which main's
 * 'default' case turns into a help dump on stderr and a -1 exit,
 * again never reaching logger_start or wm_start */
static void s_test_unknown_option(void)
{
    char *argv[] = { "icowm", "-Z", NULL };
    int result;

    s_reset_stand_ins();
    optind = 1;
    result = icowm_main(2, argv);

    TAP_EQ_INT(result, -1, "an unrecognized option returns -1");
    TAP_EQ_INT(s_wm_start_call_count, 0,
            "an unrecognized option never reaches wm_start");
}


/* '-M' below the compiled-in floor is rejected outright, returning -1
 * without ever reaching logger_start or wm_start */
static void s_test_memguard_below_floor(void)
{
    char *argv[] = { "icowm", "-M", "1", NULL };
    int result;

    s_reset_stand_ins();
    optind = 1;
    result = icowm_main(3, argv);

    TAP_EQ_INT(result, -1,
            "'-M' below MEMGUARD_MIN_CEILING_MIB returns -1");
    TAP_EQ_INT(s_wm_start_call_count, 0,
            "a rejected '-M' value never reaches wm_start");
}


/* '-M' at exactly the compiled-in floor is accepted and threaded
 * through to wm_start unchanged */
static void s_test_memguard_at_floor(void)
{
    char *argv[] = { "icowm", "-M",
        "14" /* MEMGUARD_MIN_CEILING_MIB, spelled literally so this
                assertion still catches a change to the constant
                itself, not just mirror it */, NULL };
    int result;

    s_reset_stand_ins();
    optind = 1;
    result = icowm_main(3, argv);

    TAP_EQ_INT(result, 0, "'-M 14' at the floor lets main proceed");
    TAP_EQ_INT(s_wm_start_call_count, 1,
            "an accepted '-M' value reaches wm_start exactly once");
    TAP_EQ_INT((int) s_wm_start_last_mib,
            (int) MEMGUARD_MIN_CEILING_MIB,
            "the accepted ceiling is threaded through unchanged");
}


/* An out-of-range '-L' level falls back to the compiled-in default
 * rather than rejecting the whole invocation */
static void s_test_log_level_out_of_range(void)
{
    char *argv[] = { "icowm", "-L", "999", NULL };
    int result;

    s_reset_stand_ins();
    optind = 1;
    result = icowm_main(3, argv);

    TAP_EQ_INT(result, 0,
            "an out-of-range '-L' does not abort the invocation");
    TAP_EQ_INT((int) s_logger_start_last_level,
            (int) ICOWM_DEFAULT_LOGGER_LEVEL_MIN,
            "an out-of-range '-L' falls back to the default level");
}


/* A well-formed run threads '-d', '-c', '-s', and '-r' straight
 * through to wm_start, and reports success when the stand-ins agree */
static void s_test_full_success_path(void)
{
    char *argv[] = { "icowm", "-d", ":7", "-c", "/tmp/icowm-cfg",
        "-s", "-r", NULL };
    int result;

    s_reset_stand_ins();
    optind = 1;
    result = icowm_main(7, argv);

    TAP_EQ_INT(result, 0, "a well-formed run returns 0");
    TAP_EQ_INT(s_wm_start_call_count, 1,
            "a well-formed run reaches wm_start exactly once");
    TAP_EQ_STR(s_wm_start_last_display, ":7",
            "'-d' is threaded through to wm_start unchanged");
    TAP_EQ_STR(s_wm_start_last_config_dir, "/tmp/icowm-cfg",
            "'-c' is threaded through to wm_start unchanged");
    TAP_OK(s_wm_start_last_ipc_disabled,
            "'-s' threads ipc_disabled=true through to wm_start");
    TAP_OK(s_wm_start_last_replace_requested,
            "'-r' threads replace_requested=true through to wm_start");
    TAP_EQ_INT(s_wm_stop_call_count, 1,
            "a successful run reaches wm_stop exactly once");
}


/* A logger_start failure makes main return 2 immediately, never
 * reaching wm_start at all */
static void s_test_logger_start_failure(void)
{
    char *argv[] = { "icowm", NULL };
    int result;

    s_reset_stand_ins();
    s_logger_start_result = 1;
    optind = 1;
    result = icowm_main(1, argv);

    TAP_EQ_INT(result, 2, "a logger_start failure returns 2");
    TAP_EQ_INT(s_wm_start_call_count, 0,
            "a logger_start failure never reaches wm_start");
}


/* A wm_start failure makes main return 1, having already stopped the
 * logger but never called wm_stop (there is nothing running to stop) */
static void s_test_wm_start_failure(void)
{
    char *argv[] = { "icowm", NULL };
    int result;

    s_reset_stand_ins();
    s_wm_start_result = 1;
    optind = 1;
    result = icowm_main(1, argv);

    TAP_EQ_INT(result, 1, "a wm_start failure returns 1");
    TAP_EQ_INT(s_wm_stop_call_count, 0,
            "a wm_start failure never reaches wm_stop");
    TAP_EQ_INT(s_logger_stop_call_count, 1,
            "a wm_start failure still stops the logger exactly once");
}


/* '-C' takes the lint path: it resolves the configuration directory,
 * runs the linter, and returns 0 on a clean result without ever
 * starting the logger or wm_start */
static void s_test_lint_clean(void)
{
    char *argv[] = { "icowm", "-C", "-c", "/tmp/icowm-lint-dir", NULL };
    int result;

    s_reset_stand_ins();
    s_lint_run_result = 0;
    optind = 1;
    result = icowm_main(4, argv);

    TAP_EQ_INT(result, 0, "'-C' with a clean lint result returns 0");
    TAP_EQ_INT(s_lint_run_call_count, 1,
            "'-C' reaches config_lint_run exactly once");
    TAP_EQ_STR(s_resolve_dir_last_prefix, "/tmp/icowm-lint-dir",
            "'-C' resolves the '-c' prefix before linting");
    TAP_EQ_INT(s_logger_start_call_count, 0,
            "'-C' never starts the logger");
    TAP_EQ_INT(s_wm_start_call_count, 0,
            "'-C' never reaches wm_start");
}


/* '-C' returns 1 when the linter itself finds unknown keys */
static void s_test_lint_unknown_keys(void)
{
    char *argv[] = { "icowm", "-C", NULL };
    int result;

    s_reset_stand_ins();
    s_lint_run_result = 3;
    optind = 1;
    result = icowm_main(2, argv);

    TAP_EQ_INT(result, 1,
            "'-C' with unknown keys found returns 1");
}


int main(void)
{
    TAP_PLAN(33);

    s_test_help_flag();
    s_test_version_flag();
    s_test_unknown_option();
    s_test_memguard_below_floor();
    s_test_memguard_at_floor();
    s_test_log_level_out_of_range();
    s_test_full_success_path();
    s_test_logger_start_failure();
    s_test_wm_start_failure();
    s_test_lint_clean();
    s_test_lint_unknown_keys();

    return TAP_DONE();
}
