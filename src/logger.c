/*
 * @file logger.c
 */

/* System includes */
#include <stdarg.h>     /* va_list, va_start, va_end */
#include <stdio.h>      /* FILE, fprintf, vsnprintf */
#include <stdlib.h>     /* exit */
#include <string.h>     /* strlen */
#include <time.h>       /* ctime, time */

/* Local includes */
#include <logger.h>


/* Log messages with varying levels of severity */
void logger(FILE *fp,
        enum logger_level_e min_level, enum logger_level_e cur_level,
        const char *fmt, ...)
{
    time_t now;
    char *timestamp;
    const char *level_str;
    char msg[MAX_LOG_MSG_LENGTH];

    /* Ignore the logging if the level is not high enough */
    if (cur_level < min_level) {
        return;
    }

    time(&now);
    timestamp = ctime(&now);
    timestamp[strlen(timestamp) - 1] = '\0';    /* Remove newline */

    switch (cur_level) {
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

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    /* Print the log level */
    fprintf(fp, "[%s] [%s]: %s\n", timestamp, level_str, msg);

    /* Clean and exit abruptly when 'FATAL' error */
    if (cur_level == LOG_FATAL) {
        // TODO: Cleaning
        exit(EXIT_FAILURE);
    }
}
