/**
 * @file logger.c
 *
 * @brief Logger implementation
 */

/* System includes */
#include <stdbool.h>
#include <stdarg.h>     /* va_list, va_start, va_end */
#include <stdio.h>      /* FILE, fflush, fprintf, snprintf, vsnprintf */
#include <stdlib.h>     /* NULL, free, malloc, size_t */
#include <time.h>       /* localtime, strftime, time, tm */

/* Utils includes */
#include <utils/safestr.h>

/* Local includes */
#include <logger.h>


static logger_td *logger = NULL;    /**< Pointer to the singleton
                                         instance of the logger */


/**
 * @brief Flush the log messages and reset the buffer
 *
 * @param logger_buffer Pointer to the buffer structure
 * @param fp            File descriptor where to flush the stream
 *
 * @note Complexity: @e O(n), where @e n is the number of messages in
 *       the buffer
 */
static void s_logger_buffer_flush(struct logger_buffer_s *logger_buffer,
        FILE *fp)
{
    fflush(fp);

    for (unsigned int i = 0; i < logger_buffer->count; ++i) {
        fprintf(fp, "%s\n", logger_buffer->messages[i]);
        free(logger_buffer->messages[i]);
    }

    logger_buffer->count = 0;
}


/* Initialize logger */
int logger_start(const char *filename,
        const enum logger_level_e level_min, bool is_tracking)
{
    if (logger == NULL) {
        logger = malloc(sizeof(logger_td));
        if (logger == NULL) {
            fprintf(stderr, "Failed to allocate memory for logger\n");
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
         *      - "NULL", deactivate the logger;
         *      - "STDOUT", write always to 'stdout';
         *      - "STDERR", write always to 'stderr';
         *      - "DEFAULT", write all messages to 'stdout' if
         *        'severity < LOG_ERROR', and to 'stderr' if
         *        'severity >= LOG_ERROR';
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
                return 2;
            }
            logger->file.is_open = true;

            /* Initialize the log message buffer */
            logger->buffer = malloc(sizeof(struct logger_buffer_s));
            if (logger->buffer == NULL) {
                free(logger);
                return 1;
            }

            logger->buffer->messages = malloc(LOGGER_FLUSH_THRESHOLD *
                    sizeof(char *));
            if (logger->buffer->messages == NULL) {
                free(logger->buffer);
                free(logger);
                return 1;
            }

            logger->buffer->count = 0;
        }

        return 0;
    }

    return -1;
}


/* Free allocated memory */
int logger_stop(void)
{
    if (logger == NULL) {
        return 1;
    }

    if (logger->buffer) {
        s_logger_buffer_flush(logger->buffer, logger->file.fp_out);
        free(logger->buffer->messages);
        free(logger->buffer);
    } else {
        if (logger->file.fp_out) {
            fflush(logger->file.fp_out);
        }
        if (logger->file.fp_err &&
                logger->file.fp_err != logger->file.fp_out) {
            fflush(logger->file.fp_err);
        }
    }

    /* Close the file if it was open */
    if (logger->file.is_open) {
        fclose(logger->file.fp_out);
    }

    free(logger);
    logger = NULL;  /* Reset the singleton instance pointer to 'NULL' */

    return 0;
}


/* Log messages with varying levels of severity */
int logger_msg(enum logger_level_e level, const char *prefix,
        const char *fmt, ...)
{
    time_t now = time(NULL);
    struct tm *tm_info;
    char timestamp[100];
    const char *level_str = NULL;
    char msg[LOGGER_MAX_LENGTH_MSG];
    va_list args;
    int len, len_fmt;

    /* If the logger is not set, do nothing */
    if (logger == NULL || logger->file.fp_out == NULL) {
        return -1;
    }

    /* Ignore logging if the level is not high enough */
    if (level < logger->level_min) {
        return 0;
    }

    /* Get the timestamp */
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp),
            "%Y-%m-%d %H:%M:%S %Z", tm_info);

    /* Set the level string to output */
    switch (level) {
        case LOG_TRACE:     level_str = "TRACE";    break;  /* debugs */
        case LOG_DEBUG:     level_str = "DEBUG";    break;
        case LOG_INFO:      level_str = "INFO";     break;  /* infos. */
        case LOG_NOTICE:    level_str = "NOTICE";   break;
        case LOG_WARNING:   level_str = "WARNING";  break;  /* warns. */
        case LOG_ERROR:     level_str = "ERROR";    break;  /* errors */
        case LOG_CRITICAL:  level_str = "CRITICAL"; break;
        case LOG_ALERT:     level_str = "ALERT";    break;
        case LOG_FATAL:     level_str = "FATAL";    break;  /* CRASH! */
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
        return -1;
    }

    /* Format additional message */
    va_start(args, fmt);
    len_fmt = vsnprintf(msg + len, sizeof(msg) - (size_t) len, fmt, args);
    va_end(args);
    if (len_fmt < 0) {
        /* Fail if 'vsnprintf' did not complete successfully */
        return -1;
    }

    /* Calculate final length, accounting for potential truncation */
    len += len_fmt;     /* Update total length */
    msg[len] = '\0';    /* Ensure null termination */

    /* Truncate the message if necessary */
    if ((size_t) len >= sizeof(msg)) {
        len = sizeof(msg) - 4;
        msg[len] = '\0';
        safe_strcat(msg, "..."); /* Append "..." if truncation */
    }

    /* If no buffer is used, just print it */
    if (logger->buffer == NULL) {
        if (level < LOG_WARNING) {
            fprintf(logger->file.fp_out, "%s\n", msg);
        } else {
            fprintf(logger->file.fp_err, "%s\n", msg);
        }
        return len;
    }

    /* Check if there's enough space in buffer, or flush it */
    if (logger->buffer->count >= LOGGER_FLUSH_THRESHOLD) {
        s_logger_buffer_flush(logger->buffer, logger->file.fp_out);
        if (logger->file.fp_out != logger->file.fp_err) {
            s_logger_buffer_flush(logger->buffer, logger->file.fp_err);
        }
    }

    /* Allocate memory for the message */
    logger->buffer->messages[logger->buffer->count] =
            malloc(safe_strlen(msg) + 1);
    if (logger->buffer->messages[logger->buffer->count] == NULL) {
        /* Failure to allocate memory */
        return -2;
    }

    /* Copy message to buffer */
    safe_strcpy(logger->buffer->messages[logger->buffer->count], msg);
    logger->buffer->count++;

    /* Flush the buffer on error to make sure it's on the logfile */
    if (level > LOG_WARNING) {
        s_logger_buffer_flush(logger->buffer, logger->file.fp_out);
        if (logger->file.fp_out != logger->file.fp_err) {
            s_logger_buffer_flush(logger->buffer, logger->file.fp_err);
        }
    }

/*
    if (level == LOG_FATAL) {
    }
*/

    return len;
}


/* Set the logger to always track */
void logger_tracking_on(void)
{
    if (logger != NULL && !logger->is_tracking) {
        logger->is_tracking = true;
    }
}


/* Set the logger to never track except in 'LOG_TRACE' level */
void logger_tracking_off(void)
{
    if (logger != NULL && logger->is_tracking) {
        logger->is_tracking = false;
    }
}
