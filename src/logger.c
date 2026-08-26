/**
 * @file logger.c
 *
 * @brief Logger implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* localtime_r */


/* System includes */
#include <stdarg.h>     /* va_list, va_start, va_end */
#include <stdbool.h>
#include <pthread.h>    /* pthread_mutex_t, pthread_mutex_lock */
#include <stdio.h>      /* FILE, fflush, fileno, fprintf, snprintf,
                           vsnprintf */
#include <stdlib.h>     /* NULL, free, malloc, size_t */
#include <string.h>     /* strlen */
#include <sys/time.h>   /* gettimeofday */
#include <time.h>       /* localtime, strftime, time, tm */
#include <unistd.h>     /* STDERR_FILENO, ssize_t, write */

/* Utils includes */
#include <utils/safe/safestr.h>

/* Local includes */
#include <logger.h>


static logger_td *logger = NULL;    /**< Pointer to the singleton
                                         instance of the logger */

/** Synchronize logger shared state */
static pthread_mutex_t logger_mutex = PTHREAD_MUTEX_INITIALIZER;


/**
 * @brief Flush the log messages to one or two files and reset the
 *        buffer
 *
 * Prints all buffered messages to @p fp_a and, when @p fp_b is non-NULL
 * and differs from @p fp_a, also to @p fp_b.  Each message is freed
 * after being written to both destinations so the buffer is always left
 * empty regardless of how many output files are active.
 *
 * @param logger_buffer Pointer to the buffer structure
 * @param fp_a          Primary output file (must not be null)
 * @param fp_b          Secondary output file, or @c NULL if unused
 *
 * @note Complexity: @e O(n), where @e n is the number of messages in
 *       the buffer
 */
static void s_logger_buffer_flush(struct logger_buffer_s *logger_buffer,
        FILE *fp_a, FILE *fp_b)
{
    bool const dual = (fp_b != NULL && fp_b != fp_a);

    if (fp_a == NULL) {
        return;
    }

    (void) fflush(fp_a);
    if (dual) {
        (void) fflush(fp_b);
    }

    for (unsigned int i = 0; i < logger_buffer->count; ++i) {
        fprintf(fp_a, "%s\n", logger_buffer->messages[i]);
        if (dual) {
            fprintf(fp_b, "%s\n", logger_buffer->messages[i]);
        }
        free(logger_buffer->messages[i]);
    }

    (void) fflush(fp_a);
    if (dual) {
        (void) fflush(fp_b);
    }

    logger_buffer->count = 0;
}


/**
 * @brief Format current timestamp with microseconds and timezone offset
 *
 * Retrieves the current time, including microseconds, and formats it
 * into the provided buffer as a human-readable timestamp with timezone
 * offset in the form `YYYY-MM-DD HH:MM:SS.UUUUUU +/-HHMM`.
 *
 * @param buffer    Pointer to the character array where the formatted
 *                  timestamp will be stored
 * @param buffer_sz Size of the buffer in bytes
 *
 * @note Format: `YYYY-MM-DD HH:MM:SS.UUUUUU +/-HHMM`
 * @note Uses POSIX global variable @p timezone to determine timezone
 *       offset adjusting for daylight saving time using @e tm_isdst
 *       from @a localtime
 * @note Complexity: @e O(1)
 *
 * @see @a LOGGER_TIMESTAMP_USEC, @a LOGGER_TIMESTAMP_TZ
 */
static void s_timestamp_fmt(char *buffer, size_t buffer_sz)
{
    struct timeval tv;
    struct tm tm_info;
    char tz_sign;
    int tz_hours = 0;
    int tz_minutes = 0;
    long tz_offset_seconds;
#if !LOGGER_OS_HAS_TM_GMTOFF
#ifdef __linux__
    extern long timezone;   /* Linux 'glibc' */
#endif  /* !__linux__ */
#endif

    gettimeofday(&tv, NULL);
    if (localtime_r(&tv.tv_sec, &tm_info) == NULL) {
        (void) snprintf(buffer, buffer_sz, "Timestamp error");
        return;
    }

    /* Calculate timezone offset */
#if LOGGER_OS_HAS_TM_GMTOFF
    tz_offset_seconds = tm_info.tm_gmtoff;  /* BSD, macOS */
#else
#ifdef __linux__
    /* Check if daylight saving time is in effect:
     *   - 'tm_isdst'  > 0: summer; daylight saving time
     *   - 'tm_isdst' == 0: winter; standard time
     *   - 'tm_isdst'  < 0: info. not available */
    tz_offset_seconds = -timezone;          /* Linux 'glibc' */
    if (tm_info.tm_isdst > 0) {
        tz_offset_seconds += 3600;
    }
#else
    /* Other systems without 'tm_gmtoff' nor 'timezone' */
    tz_offset_seconds = 0;
#endif  /* ! __linux__ */
#endif  /* ! LOGGER_OS_HAS_TM_GMTOFF */

    /* Determine timezone sign offset and convert to "HHMM" format */
    tz_sign = '+';
    if (tz_offset_seconds < 0) {
        tz_sign = '-';
        tz_offset_seconds = -tz_offset_seconds;
    }
    tz_hours = (int) tz_offset_seconds / 3600;
    tz_minutes = ((int) tz_offset_seconds % 3600) / 60;

    (void) snprintf(buffer, buffer_sz,
            "%04d-%02d-%02d %02d:%02d:%02d.%06ld %c%02d%02d",
            tm_info.tm_year + 1900,
            tm_info.tm_mon + 1,
            tm_info.tm_mday,
            tm_info.tm_hour,
            tm_info.tm_min,
            tm_info.tm_sec,
            tv.tv_usec,
            tz_sign,
            tz_hours,
            tz_minutes);
}


/* Initialize logger */
int logger_start(const char *filename,
        const enum logger_level_e level_min, bool is_tracking)
{
    pthread_mutex_lock(&logger_mutex);

    if (logger == NULL) {
        logger = malloc(sizeof(logger_td));
        if (logger == NULL) {
            fprintf(stderr, "Failed to allocate memory for logger\n");
            pthread_mutex_unlock(&logger_mutex);
            return 1;
        }

        /* Validate and correct the minimum log level value */
        logger->level_min =
            (level_min < LOG_MIN_LEVEL) ? LOG_MIN_LEVEL :
                (level_min > LOG_MAX_LEVEL) ? LOG_MAX_LEVEL :
                    level_min;

        /* Tracking: always or only on 'LOG_TRACE' level */
        logger->is_tracking = is_tracking;

        /* Set the file stream and its buffer if necessary */
        logger->file.is_open = false;
        logger->buffer = NULL;

        /* If file name could be:
         *    - "NULL", deactivate the logger
         *    - "STDOUT", write always to 'stdout'
         *    - "STDERR", write always to 'stderr'
         *    - "DEFAULT", write all messages to:
         *      - 'stdout' if 'severity < LOG_ERROR'
         *      - 'stderr' if 'severity >= LOG_ERROR'
         * else, open the file name and write to it */
        if (safe_strcmp(filename, "DEFAULT") == 0) {
            logger->file.fp_out = stdout;
            logger->file.fp_err = stderr;
        } else if (safe_strcmp(filename, "STDOUT") == 0) {
            logger->file.fp_out = stdout;
            logger->file.fp_err = stdout;
        } else if (safe_strcmp(filename, "STDERR") == 0) {
            logger->file.fp_out = stderr;
            logger->file.fp_err = stderr;
        } else if (safe_strcmp(filename, "NULL") == 0 ) {
            logger->file.fp_out = NULL;
            logger->file.fp_err = NULL;
        } else {
            logger->file.fp_err = NULL;
            logger->file.fp_out = fopen(filename, "a+t");
            if (logger->file.fp_out == NULL) {
                fprintf(stderr, "Failed to open file to log: '%s'\n",
                        filename);
                free(logger);
                pthread_mutex_unlock(&logger_mutex);
                return 2;
            }
            logger->file.is_open = true;

            /* Initialize the log message buffer */
            logger->buffer = malloc(sizeof(struct logger_buffer_s));
            if (logger->buffer == NULL) {
                fclose(logger->file.fp_out);
                free(logger);
                pthread_mutex_unlock(&logger_mutex);
                return 1;
            }

            logger->buffer->messages = malloc(LOGGER_FLUSH_THRESHOLD *
                    sizeof(char *));
            if (logger->buffer->messages == NULL) {
                fclose(logger->file.fp_out);
                free(logger->buffer);
                free(logger);
                pthread_mutex_unlock(&logger_mutex);
                return 1;
            }

            logger->buffer->count = 0;
        }

        pthread_mutex_unlock(&logger_mutex);
        return 0;
    }

    pthread_mutex_unlock(&logger_mutex);
    return -1;
}


/* Free allocated memory */
/* Write out whatever is buffered, from a signal handler */
void logger_emergency_flush(void)
{
    int fd;

    if (logger == NULL || logger->buffer == NULL ||
            logger->buffer->count == 0u) {
        return;
    }

    /* The stream's own descriptor rather than the stream: 'fprintf'
     * and 'fflush' are not async-signal-safe, and this runs from a
     * handler for a signal that has already left the process in an
     * undefined state.  'write' is on the guaranteed-safe list. */
    fd = (logger->file.fp_out != NULL)
        ? fileno(logger->file.fp_out) : STDERR_FILENO;
    if (fd < 0) {
        fd = STDERR_FILENO;
    }

    /* 'logger_mutex' is deliberately not taken.  A handler that
     * blocked on a mutex the interrupted code already holds would
     * hang the process instead of letting it die, which is the one
     * outcome worse than losing the log.  Reading the buffer
     * unlocked is safe enough here only because this program runs a
     * single thread: the signal interrupted that thread, so nothing
     * is concurrently writing, and the worst case is a final message
     * that was half-formatted when the signal arrived. */
    for (unsigned int i = 0u; i < logger->buffer->count; ++i) {
        const char *const msg = logger->buffer->messages[i];
        ssize_t written;

        if (msg == NULL) {
            continue;
        }
        written = write(fd, msg, strlen(msg));
        (void) written;
        written = write(fd, "\n", 1u);
        (void) written;
    }

    /* Not freed and not zeroed: the process is about to re-raise the
     * signal and die, and touching the allocator from a handler is
     * exactly what this function exists to avoid. */
}


int logger_stop(void)
{
    pthread_mutex_lock(&logger_mutex);

    if (logger == NULL) {
        pthread_mutex_unlock(&logger_mutex);
        return 1;
    }

    /* Free buffer */
    if (logger->buffer) {
        s_logger_buffer_flush(logger->buffer, logger->file.fp_out,
                logger->file.fp_err);
        free(logger->buffer->messages);
        free(logger->buffer);
    } else {
        if (logger->file.fp_out) {
            (void) fflush(logger->file.fp_out);
        }
        if (logger->file.fp_err &&
                logger->file.fp_err != logger->file.fp_out) {
            (void) fflush(logger->file.fp_err);
        }
    }

    /* Close the file if it was open.  The 'fp_out != NULL' check is
     * a defensive second guard alongside 'is_open': the two fields
     * are always kept in sync today, but 'fclose(NULL)' is undefined
     * behavior, so this does not rely solely on that invariant
     * holding across every future code path */
    if (logger->file.is_open && logger->file.fp_out != NULL) {
        fclose(logger->file.fp_out);
    }

    free(logger);
    logger = NULL;  /* Reset the singleton instance pointer to 'NULL' */

    pthread_mutex_unlock(&logger_mutex);
    return 0;
}


/* Log messages with varying levels of severity */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    char timestamp[64];
    const char *level_str = NULL;
    char msg[LOGGER_MAX_LENGTH_MSG];
    va_list args;
    int len;
    int retval = 0;

    pthread_mutex_lock(&logger_mutex);
    do {
        int len_fmt;

        /* If the logger is not set, do nothing */
        if (logger == NULL || logger->file.fp_out == NULL) {
            retval = -1;
            break;
        }

        /* Ignore logging if the level is not high enough */
        if (level < logger->level_min) {
            retval = 0;
            break;
        }

        /* Get the timestamp */
        s_timestamp_fmt(timestamp, sizeof(timestamp));

        /* Set the level string to output */
        switch (level) {
            case LOG_TRACE:     level_str = "TRACE";    break;
            case LOG_DEBUG:     level_str = "DEBUG";    break;
            case LOG_INFO:      level_str = "INFO";     break;
            case LOG_NOTICE:    level_str = "NOTICE";   break;
            case LOG_WARNING:   level_str = "WARNING";  break;
            case LOG_ERROR:     level_str = "ERROR";    break;
            case LOG_CRITICAL:  level_str = "CRITICAL"; break;
            case LOG_ALERT:     level_str = "ALERT";    break;
            case LOG_FATAL:     level_str = "FATAL";    break;
        }

        /* Format first part message */
        if (logger->level_min == LOG_TRACE || logger->is_tracking) {
            len = snprintf(msg, sizeof(msg), "[%s] (%s) <%s>: ",
                    timestamp, level_str, prefix);
        } else {
            len = snprintf(msg, sizeof(msg), "[%s] (%s): ",
                    timestamp, level_str);
        }

        /* Handle possible errors */
        if (len < 0 || (size_t) len >= sizeof(msg)) {
            retval = -1;
            break;
        }

        /* Format additional message */
        va_start(args, fmt);
        len_fmt = vsnprintf(msg + len, sizeof(msg) - (size_t) len,
                fmt, args);
        va_end(args);
        if (len_fmt < 0) {
            /* Fail if 'vsnprintf' did not complete successfully */
            retval = -1;
            break;
        }

        /* Calculate final length, accounting for potential truncation.
         * Note that 'vsnprintf' reports the length the string WOULD
         * have had if 'msg' were large enough, so 'len' must be checked
         * against 'sizeof(msg)' BEFORE it is used to index 'msg';
         * otherwise the null-termination write below could land past
         * the end of the buffer */
        len += len_fmt;     /* Update total length */

        /* Truncate the message if necessary */
        if ((size_t) len >= sizeof(msg)) {
            len = sizeof(msg) - 4;
            msg[len] = '\0';
            safe_strncat(msg, "...", sizeof(msg));
        } else {
            msg[len] = '\0';    /* Ensure null termination */
        }

        /* If no buffer is used, just print it */
        if (logger->buffer == NULL) {
            if (level < LOG_WARNING) {
                fprintf(logger->file.fp_out, "%s\n", msg);
            } else {
                fprintf(logger->file.fp_err, "%s\n", msg);
            }
            retval = len;
            break;
        }

        /* Check if there's enough space in buffer, or flush it */
        if (logger->buffer->count >= LOGGER_FLUSH_THRESHOLD) {
            s_logger_buffer_flush(logger->buffer, logger->file.fp_out,
                    logger->file.fp_err);
        }

        /* Allocate memory for the message */
        logger->buffer->messages[logger->buffer->count] =
                malloc(safe_strlen(msg) + 1);
        if (logger->buffer->messages[logger->buffer->count] == NULL) {
            /* Failure to allocate memory */
            retval = -2;
            break;
        }

        /* Copy message to buffer */
        safe_strncpy(logger->buffer->messages[logger->buffer->count],
                msg, safe_strlen(msg) + 1);
        logger->buffer->count++;

        /* Flush the buffer on warning-or-above so it is not lost to
         * an unflushed buffer if the process terminates shortly
         * after: an unexpected client (or icowm's own) crash is
         * exactly the kind of event this range of severities exists
         * to record, and exactly the moment losing it to buffering
         * would matter most. */
        if (level >= LOG_WARNING) {
            s_logger_buffer_flush(logger->buffer, logger->file.fp_out,
                    logger->file.fp_err);
        }

        retval = len;
    } while (false);

    pthread_mutex_unlock(&logger_mutex);
    return retval;
}
