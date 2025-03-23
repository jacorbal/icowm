/**
 * @file logger.c
 *
 * @brief Logger implementation
 */

/* System includes */
#include <stdarg.h>     /* va_list, va_start, va_end */
#include <stdbool.h>    /* false, true */
#include <stdio.h>      /* FILE, fflush, fprintf, snprintf, vsnprintf */
#include <stdlib.h>     /* NULL, free, malloc, size_t */
#include <string.h>     /* strcpy, strlen */
#include <time.h>       /* localtime, strftime, time, tm */

/* Local includes */
#include <logger.h>


/* Though variable static dost often lurk near,
 * In shadows of scope, few e’er call thee their own,
 * Thy global existence, to none dost bring fear,
 * A sentinel watching, though thou art alone. */
static logger_td *logger = NULL;    /**< Logger singleton pointer */


/**
 * @brief Flush the log messages and reset the buffer
 *
 * @param logger_buffer Pointer to the buffer structure
 * @param fp            File descriptor where to flush the stream
 *
 * @note Complexity: @e O(n), where @e n is the number of messages in
 *       the buffer
 */
static void _logger_buffer_flush(struct logger_buffer_s *logger_buffer,
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
        const enum logger_level_e level_min)
{
    logger = malloc(sizeof(logger_td));
    if (logger == NULL) {
        return 1;
    }

    /* Validate and correct if necessary the minimum log level value */
    logger->level_min = (level_min < LOG_MIN_LEVEL) ? LOG_MIN_LEVEL :
                        (level_min > LOG_MAX_LEVEL) ? LOG_MAX_LEVEL :
                         level_min;

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
    if (strcmp(filename, "DEFAULT") == 0) { /* log to stdout & stderr */
        logger->file.fp_out = stdout;
        logger->file.fp_err = stderr;
    } else if (strcmp(filename, "STDOUT") == 0) {   /* log in stdout */
        logger->file.fp_out = stdout;
        logger->file.fp_err = stdout;
    } else if (strcmp(filename, "STDERR") == 0) {   /* log in stderr */
        logger->file.fp_out = stderr;
        logger->file.fp_err = stderr;
    } else if (strcmp(filename, "NULL") == 0 ) {    /* log deactivated */
        logger->file.fp_out = NULL;
        logger->file.fp_err = NULL;
    } else {                                        /* log in file */
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

        logger->buffer->messages = malloc(LOGGER_MAX_MESSAGES *
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


/* Free allocated memory */
void logger_stop(void)
{
    if (logger->buffer) {
        _logger_buffer_flush(logger->buffer, logger->file.fp_out);
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
}


/* Log messages with varying levels of severity */
int logger_msg(enum logger_level_e level, const char *prefix,
        const char *fmt, ...)
{
    time_t now = time(NULL);
    struct tm *tm_info;
    char timestamp[100];
    const char *level_str;
    char msg[LOGGER_MAX_MSG_LENGTH];
    va_list args;
    int len;

    /* If the logger is not set, do nothing */
    if (logger == NULL || logger->file.fp_out == NULL) {
        return -1;
    }

    /* Ignore the logging if the level is not high enough */
    if (level < logger->level_min) {
        return 0;
    }

    /* Get the timestamp */
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp),
            "%Y-%m-%d %H:%M:%S %Z", tm_info);

    switch (level) {
        case LOG_TRACE:     level_str = "TRACE";    break;  /* INFO. */
        case LOG_DEBUG:     level_str = "DEBUG";    break;
        case LOG_INFO:      level_str = "INFO";     break;
        case LOG_NOTICE:    level_str = "NOTICE";   break;
        case LOG_WARNING:   level_str = "WARNING";  break;  /* WARN. */
        case LOG_ERROR:     level_str = "ERROR";    break;  /* ERROR */
        case LOG_CRITICAL:  level_str = "CRITICAL"; break;
        case LOG_ALERT:     level_str = "ALERT";    break;
        case LOG_FATAL:     level_str = "FATAL";    break;
        default:            level_str = "UNKNOWN";  break;
    }

    /* Format the message */
    va_start(args, fmt);
    len = snprintf(msg, sizeof(msg), "[%s] [%s] (%s): ",
            timestamp, level_str, prefix);
    len = vsnprintf(msg + len, sizeof(msg) - (size_t) len, fmt, args);
    va_end(args);

    /* If no buffer is used, just print it */
    if (logger->buffer == NULL) {
        if (level < LOG_ERROR) {
            fprintf(logger->file.fp_out, "%s\n", msg);
        } else {
            fprintf(logger->file.fp_err, "%s\n", msg);
        }
        return len;
    }

    /* Check if there's enough space in buffer, or flush it */
    if (logger->buffer->count >= LOGGER_MAX_MESSAGES) {
        _logger_buffer_flush(logger->buffer, logger->file.fp_out);
        if (logger->file.fp_out != logger->file.fp_err) {
            _logger_buffer_flush(logger->buffer, logger->file.fp_err);
        }
    }

    /* Copy message to buffer */
    logger->buffer->messages[logger->buffer->count] =
            malloc(strlen(msg) + 1);
    strcpy(logger->buffer->messages[logger->buffer->count], msg);
    logger->buffer->count++;

    /* Flush the buffer on error to make sure it's on the logfile */
    if (level > LOG_WARNING) {
        _logger_buffer_flush(logger->buffer, logger->file.fp_out);
        if (logger->file.fp_out != logger->file.fp_err) {
            _logger_buffer_flush(logger->buffer, logger->file.fp_err);
        }
    }

/*
    if (level == LOG_FATAL) {
    }
*/

    return len;
}
