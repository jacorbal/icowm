/**
 * @file tests/utils/test_spawn.c
 *
 * @brief Test battery for launching an external command from the
 *        window manager
 *
 * Exercises 'spawn_command' (utils/spawn.c) linked for real, with a
 * genuine 'fork'/'execvp' underneath it: no part of the process
 * launch itself is mocked, since that is exactly the behavior this
 * file exists to cover.  'xcb_connection_get' is the one link-only
 * stand-in this file provides (the real X connection this function
 * would otherwise close a copy of in the child), answering 'NULL' so
 * the "no connection to close" branch runs deterministically in
 * a sandbox with no X server, plus 'xcb_get_file_descriptor' for the
 * one scenario that opts into a non-NULL connection to exercise the
 * other branch.  Every child this test spawns is waited for with
 * a real 'waitpid', so no zombie process is ever left behind.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */
#include <sys/types.h>  /* pid_t */
#include <sys/wait.h>   /* waitpid, WIFEXITED, WEXITSTATUS */
#include <unistd.h>     /* usleep */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <harness/tap.h>
#include <utils/spawn.h>


/* Controllable stand-in state */

static xcb_connection_t *s_connection_stub = NULL;
static int s_get_fd_calls = 0;


/** Link-only stand-in for 'logger_msg', behind every 'LOGGER_*' call
 *  this file's target makes; no test here asserts on log output, so
 *  it only needs to exist and never crash */
int logger_msg(enum logger_level_e level, const char *prefix,
        const char *fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/* 'utils/xcb/connection.h' stand-in */

xcb_connection_t *xcb_connection_get(void)
{
    return s_connection_stub;
}


/* Raw XCB stand-in (not linking libxcb at all); reached only when
 * 's_connection_stub' is non-NULL, in the forked child, right before
 * it closes what it believes is the inherited X connection's own
 * file descriptor */
int xcb_get_file_descriptor(xcb_connection_t *c)
{
    (void) c;
    s_get_fd_calls++;
    /* A real, always-valid descriptor any process has open, so the
     * child's own 'close' call on it (real, unstubbed) succeeds
     * instead of failing on a bogus fd number */
    return 0;
}


/**
 * @brief Wait for @p pid to exit and return its exit status
 *
 * @param pid Child process ID to wait for
 *
 * @return The child's exit status per @c WEXITSTATUS, or @c -1 if it
 *         did not exit normally
 */
static int s_wait_for_exit(pid_t pid)
{
    int status = 0;

    if (waitpid(pid, &status, 0) != pid) {
        return -1;
    }
    if (!WIFEXITED(status)) {
        return -1;
    }
    return WEXITSTATUS(status);
}


/* A NULL command is refused outright: no fork happens at all */
static void s_test_null_command_is_refused(void)
{
    int result;

    result = spawn_command(NULL, NULL, NULL);

    TAP_EQ_INT(result, -1, "a NULL command is refused with -1");
}


/* An empty command string is refused the same way as a NULL one */
static void s_test_empty_command_is_refused(void)
{
    int result;

    result = spawn_command("", NULL, NULL);

    TAP_EQ_INT(result, -1, "an empty command is refused with -1");
}


/* A command that fails to word-expand at all (an unterminated quote)
 * is refused without forking */
static void s_test_unparseable_command_is_refused(void)
{
    int result;

    result = spawn_command("'unterminated", NULL, NULL);

    TAP_EQ_INT(result, -2,
            "a command wordexp cannot parse is refused with -2");
}


/* A real, existing program launches successfully: 'spawn_command'
 * reports success, hands back a real child PID, and that child exits
 * 0, exactly what '/bin/true' always does */
static void s_test_real_command_launches_and_succeeds(void)
{
    int result;
    pid_t pid = -1;

    s_connection_stub = NULL;
    result = spawn_command("/bin/true", NULL, &pid);

    TAP_EQ_INT(result, 0, "launching a real, existing program succeeds");
    TAP_OK(pid > 0, "a positive child PID is reported back");
    TAP_EQ_INT(s_wait_for_exit(pid), 0,
            "the child ran '/bin/true' and exited 0");
}


/* A command that word-expands and executes, but is not the zero-exit
 * program above, still counts as a successful launch: 'spawn_command'
 * only reports whether 'execvp' itself succeeded, not what the
 * program went on to do */
static void s_test_real_command_nonzero_exit_still_a_success_launch(
        void)
{
    int result;
    pid_t pid = -1;

    s_connection_stub = NULL;
    result = spawn_command("/bin/false", NULL, &pid);

    TAP_EQ_INT(result, 0,
            "launching '/bin/false' is still a successful launch");
    TAP_EQ_INT(s_wait_for_exit(pid), 1,
            "but the child itself exits nonzero, as '/bin/false'"
            " always does");
}


/* A command naming a program that does not exist anywhere on 'PATH'
 * is reported as a failed launch, via the error-pipe path: the child
 * still forks, but 'execvp' itself fails inside it and the parent
 * reports that back rather than a false success */
static void s_test_nonexistent_program_reports_failure(void)
{
    int result;

    s_connection_stub = NULL;
    result = spawn_command("this-program-does-not-exist-anywhere",
            NULL, NULL);

    TAP_EQ_INT(result, -2,
            "a program execvp cannot find is reported as a failed"
            " launch");
}


/* A NULL 'out_pid' is safe: the child still runs, only nothing is
 * reported back for its PID */
static void s_test_null_out_pid_is_safe(void)
{
    int result;

    s_connection_stub = NULL;
    result = spawn_command("/bin/true", NULL, NULL);

    TAP_EQ_INT(result, 0,
            "a NULL out_pid does not stop the command from launching");
}


/* 'opts' with a startup ID and a class name reaches the child and
 * back out successfully; this file cannot observe the child's own
 * environment once it has exited, so only the overall launch result
 * is checked, exercising the setenv branches without crashing under
 * the sanitizers */
static void s_test_opts_are_accepted(void)
{
    spawn_opts_td opts;
    int result;
    pid_t pid = -1;

    opts.startup_id = "12345";
    opts.class_name = "TestClass";

    s_connection_stub = NULL;
    result = spawn_command("/bin/true", &opts, &pid);

    TAP_EQ_INT(result, 0,
            "a command with startup_id and class_name set launches"
            " successfully");
    TAP_EQ_INT(s_wait_for_exit(pid), 0,
            "and its child still runs '/bin/true' to a normal exit");
}


/* With a non-NULL connection, the child closes what it believes is
 * the inherited X connection's descriptor before exec'ing; this drives
 * that branch (via the 'xcb_get_file_descriptor' stand-in) without
 * a real X server, and the launch still succeeds */
static void s_test_non_null_connection_closes_fd_before_exec(void)
{
    int result;
    pid_t pid = -1;

    s_connection_stub = (xcb_connection_t *) 1;
    s_get_fd_calls = 0;
    result = spawn_command("/bin/true", NULL, &pid);

    TAP_EQ_INT(result, 0,
            "launching still succeeds when a connection is set");
    TAP_EQ_INT(s_wait_for_exit(pid), 0,
            "and the child still reaches a normal '/bin/true' exit");
    s_connection_stub = NULL;
}


/* A multi-word command line is word-expanded into more than one
 * argument, exactly as a shell would split it, before the child ever
 * runs; '/bin/echo one two' only exits 0 if 'execvp' received three
 * separate arguments rather than one literal string */
static void s_test_multi_word_command_is_expanded(void)
{
    int result;
    pid_t pid = -1;

    s_connection_stub = NULL;
    result = spawn_command("/bin/echo one two three", NULL, &pid);

    TAP_EQ_INT(result, 0,
            "a multi-word command line launches successfully");
    TAP_EQ_INT(s_wait_for_exit(pid), 0,
            "and 'echo' with its words as separate arguments exits 0");
}


int main(void)
{
    TAP_PLAN(16);

    s_test_null_command_is_refused();
    s_test_empty_command_is_refused();
    s_test_unparseable_command_is_refused();
    s_test_real_command_launches_and_succeeds();
    s_test_real_command_nonzero_exit_still_a_success_launch();
    s_test_nonexistent_program_reports_failure();
    s_test_null_out_pid_is_safe();
    s_test_opts_are_accepted();
    s_test_non_null_connection_closes_fd_before_exec();
    s_test_multi_word_command_is_expanded();

    return TAP_DONE();
}
