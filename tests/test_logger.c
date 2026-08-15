/**
 * @file tests/test_logger.c
 *
 * @brief Test battery for the logger
 *
 * The logger is a process-wide singleton, so every test here ends
 * with its own logger_stop, leaving a clean slate for the next one
 * to call logger_start on, exactly as any real caller must.
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
#include <unistd.h>

/* Local includes */
#include <harness/tap.h>
#include <logger.h>


/**
 * @brief Return a fresh temporary file path for one test's own log
 *        output
 */
static void s_temp_log_path(char *path_out, size_t path_out_size)
{
    static int s_counter = 0;

    snprintf(path_out, path_out_size, "/tmp/icowm_test_logger_%d_%d.log",
            (int) getpid(), s_counter++);
}


/**
 * @brief Read an entire file's contents into a caller-provided buffer
 *
 * @return Number of bytes read, or 0 if the file could not be opened
 */
static size_t s_read_file(const char *path, char *buf, size_t buf_size)
{
    FILE *f = fopen(path, "r");
    size_t n;

    if (f == NULL) {
        return 0;
    }
    n = fread(buf, 1, buf_size - 1, f);
    buf[n] = '\0';
    fclose(f);
    return n;
}


/* A message logged at or above the minimum level, without tracking
 * enabled, is written without the caller-function prefix, and its
 * own formatted content is present verbatim */
static void s_test_basic_message_is_written(void)
{
    char path[256];
    char content[1024];
    int rc_start;

    s_temp_log_path(path, sizeof(path));
    rc_start = logger_start(path, LOG_DEBUG, false);
    TAP_EQ_INT(rc_start, 0, "logger_start on a real file succeeds");

    logger_msg(LOG_INFO, "some_func", "the answer is %d", 42);
    logger_stop();

    s_read_file(path, content, sizeof(content));
    TAP_OK(strstr(content, "(INFO):") != NULL,
            "the level tag appears in the flushed output");
    TAP_OK(strstr(content, "the answer is 42") != NULL,
            "the formatted message content appears verbatim");
    TAP_NULL(strstr(content, "<some_func>"),
            "without tracking, the caller-function prefix is absent");

    unlink(path);
}


/* With is_tracking on, every message (at any level) includes the
 * caller-supplied prefix, wrapped in angle brackets */
static void s_test_tracking_includes_prefix(void)
{
    char path[256];
    char content[1024];

    s_temp_log_path(path, sizeof(path));
    logger_start(path, LOG_DEBUG, true);

    logger_msg(LOG_INFO, "my_function", "tracked message");
    logger_stop();

    s_read_file(path, content, sizeof(content));
    TAP_OK(strstr(content, "<my_function>") != NULL,
            "tracking on: the caller-function prefix is present");

    unlink(path);
}


/* Messages below the configured minimum level are silently dropped,
 * never reaching the output file at all */
static void s_test_below_minimum_level_is_dropped(void)
{
    char path[256];
    char content[1024];
    int rc_msg;

    s_temp_log_path(path, sizeof(path));
    logger_start(path, LOG_WARNING, false);

    rc_msg = logger_msg(LOG_DEBUG, "func", "should not appear");
    logger_stop();

    TAP_EQ_INT(rc_msg, 0,
            "a below-minimum message returns 0, not an error");
    s_read_file(path, content, sizeof(content));
    TAP_NULL(strstr(content, "should not appear"),
            "a below-minimum message never reaches the output file");

    unlink(path);
}


/* Messages at or above the configured minimum level are still
 * written, alongside the ones that get dropped */
static void s_test_at_minimum_level_is_written(void)
{
    char path[256];
    char content[1024];

    s_temp_log_path(path, sizeof(path));
    logger_start(path, LOG_WARNING, false);

    logger_msg(LOG_DEBUG, "func", "dropped");
    logger_msg(LOG_WARNING, "func", "kept");
    logger_stop();

    s_read_file(path, content, sizeof(content));
    TAP_OK(strstr(content, "kept") != NULL,
            "an at-minimum-level message is written");
    TAP_NULL(strstr(content, "dropped"),
            "a below-minimum message alongside it is still dropped");

    unlink(path);
}


/* logger_msg before any logger_start (or after logger_stop) fails
 * cleanly instead of crashing */
static void s_test_msg_without_start_fails(void)
{
    int rc = logger_msg(LOG_INFO, "func", "nobody is listening");

    TAP_EQ_INT(rc, -1,
            "logging with no active logger returns -1, no crash");
}


/* The "NULL" filename keyword deactivates the logger entirely:
 * logger_start itself still succeeds, but every subsequent message
 * is silently discarded */
static void s_test_null_keyword_deactivates_logger(void)
{
    int rc_start;
    int rc_msg;

    rc_start = logger_start("NULL", LOG_DEBUG, false);
    TAP_EQ_INT(rc_start, 0,
            "logger_start with the \"NULL\" keyword still succeeds");

    rc_msg = logger_msg(LOG_INFO, "func", "into the void");
    TAP_EQ_INT(rc_msg, -1,
            "a message under a deactivated logger is dropped (-1)");

    logger_stop();
}


/* logger_start called a second time, without an intervening
 * logger_stop, fails rather than silently replacing the running
 * singleton */
static void s_test_double_start_fails(void)
{
    char path[256];
    int rc_first;
    int rc_second;

    s_temp_log_path(path, sizeof(path));
    rc_first = logger_start(path, LOG_DEBUG, false);
    rc_second = logger_start(path, LOG_DEBUG, false);

    TAP_EQ_INT(rc_first, 0, "the first logger_start succeeds");
    TAP_EQ_INT(rc_second, -1,
            "a second logger_start before stop fails, singleton" \
            " already running");

    logger_stop();
    unlink(path);
}


/* logger_stop with no logger currently running reports that no
 * operation was performed, rather than crashing */
static void s_test_stop_without_start_reports_no_op(void)
{
    int rc = logger_stop();

    TAP_EQ_INT(rc, 1,
            "stopping with nothing running reports 'no operation'" \
            " (1)");
}


int main(void)
{
    TAP_PLAN(15);

    s_test_basic_message_is_written();
    s_test_tracking_includes_prefix();
    s_test_below_minimum_level_is_dropped();
    s_test_at_minimum_level_is_written();
    s_test_msg_without_start_fails();
    s_test_null_keyword_deactivates_logger();
    s_test_double_start_fails();
    s_test_stop_without_start_reports_no_op();

    return TAP_DONE();
}
