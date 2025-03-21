/*
 * @file logger.c
 *
 * @brief Logger implementation
 */

/* System includes */
#include <stdarg.h>     /* va_list, va_start, va_end */
#include <stdio.h>      /* FILE, fflush, fprintf, snprintf, vsnprintf */
#include <stdlib.h>     /* free, malloc */
#include <string.h>     /* strcpy, strlen */
#include <time.h>       /* localtime, strftime, time, tm */

/* Local includes */
#include <logger.h>


/* Though variable static dost often lurk near,
 * In shadows of scope, few e’er call thee their own,
 * Thy global existence, to none dost bring fear,
 * A sentinel watching, though thou art alone. */
static logger_td *logger = NULL;    /**< Global logger singleton pointer */


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
int logger_start(FILE *fp, const enum logger_level_e level_min)
{
    logger = malloc(sizeof(logger_td));
    if (logger == NULL) {
        return 1;
    }

    logger->level_min = level_min;
    logger->fp = fp;

    if (fp == NULL || fp == stdout || fp == stderr) {
        logger->buffer = NULL;
    } else {
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
        _logger_buffer_flush(logger->buffer, logger->fp); 
        free(logger->buffer->messages);
        free(logger->buffer);
    }
    free(logger);
}


/* Log messages with varying levels of severity */
int logger_msg(enum logger_level_e level, const char *fmt, ...)
{
    time_t now;
    struct tm *tm_info;
    char timestamp[100];
    const char *level_str;
    char msg[LOGGER_MAX_MSG_LENGTH];
    va_list args;
    int len;


    /* If the logger is not set, do nothing */
    if (logger == NULL || logger->fp == NULL) {
        return -1;
    }

    /* Ignore the logging if the level is not high enough */
    if (level < logger->level_min) {
        return 0;
    }

    /* Get the timestamp */
    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp),
            "%Y-%m-%d %H:%M:%S %Z", tm_info);

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
        default:            level_str = "UNKNOWN";  break;
    }

    /* Format the message */
    va_start(args, fmt);
    len = snprintf(msg, sizeof(msg), "[%s] [%s]: ",
            timestamp, level_str);
    len = vsnprintf(msg + len, sizeof(msg) - (size_t) len, fmt, args);
    va_end(args);

    /* If no buffer is used, just print it */
    if (logger->buffer == NULL) {
        fprintf(logger->fp, "%s\n", msg);
        return len;
    }

    /* Check if there's enough space in buffer, or flush it */
    if (logger->buffer->count >= LOGGER_MAX_MESSAGES) {
        _logger_buffer_flush(logger->buffer, logger->fp); 
    }

    /* Copy message to buffer */
    logger->buffer->messages[logger->buffer->count] = 
            malloc(strlen(msg) + 1);
    strcpy(logger->buffer->messages[logger->buffer->count], msg);
    logger->buffer->count++;

    /* Flush the buffer when error to make sure it's on the logfile */
    if (level > LOG_WARNING) {
        _logger_buffer_flush(logger->buffer, logger->fp); 
    }

/*
    if (level == LOG_FATAL) {
    }
*/

    return len;
}
