/**
 * @file logger.h
 *
 * @brief Log management declaration
 *
 * A singleton pointer to the logger structure allows to log in the same
 * place since the logger starts until it ends working with the same
 * configuration.  The logger uses a buffer to store the formatted
 * messages before flushing them (unless the system descriptors such as
 * @c stdout or @c stderr are used).  This is built to increase
 * performance by minimizing the number of flushes at the cost of
 * a extra bit of memory, so the log is written in chunks if everything
 * is working properly, but it will always flush the messages on any
 * error.
 *
 * Unless specified the opposite, tracking, debug or information logs
 * are displayed on @c stdout, and warnings or errors on @c stderr.
 * This is achieved by using the keyword "DEFAULT" as the file name when
 * initializing the logger.
 *
 * @note The initial logic behind the different log levels came from an
 *       answer on Stack Overflow (answered on Nov 12, 2020)
 * <https://stackoverflow.com/questions/2031163/when-to-use-the-different-log-levels#answer-64806781>:
 *
 * @verbatim
 *      +-------+                                         ||
 *      | TRACE |<--------Y---.                           VV
 *      +-------+             |                           /\    For whom
 *                           /\   Do I need to           /  \   am I
 *      +-------+           /  \  log states of <-devs.--\  /   writing
 *      | DEBUG |<--------N-\  /  variables?              \/    the log
 *      +-------+            \/                            |    line?
 *                                  .-------system ops.----'
 *                                  |
 *                                  v
 *                                 /\        Do I log because
 *      +-------+                 /  \       of an unwanted
 *      | INFO. |<--------------N-\  /-Y--.  state?
 *      +-------+                  \/     |
 *                                        v
 *                                       /\       Can the process
 *      +-------+                       /  \      continue with the
 *      | WARN. |<-------------------Y- \  /-N-.  unwanted state?
 *      +-------+                        \/    |
 *                                             v
 *      +-------+                             /\    Can the program
 *      | ERROR |<-------------------------Y-/  \   continue with the
 *      +-------+                            \  /   unwanted state?
 *                                            \/
 *      +-------+                              |
 *      | FATAL |<-------------------------N---'
 *      +-------+
 * @endverbatim
 *
 * @defgroup logger Logger
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef LOGGER_H
#define LOGGER_H

/* System includes */
#include <stdbool.h>
#include <stdio.h>      /* FILE */


/* Define 'LOGGER_OS_HAS_TM_GMTOFF' as 1 if the system is a BSD-derived
 * or macOS system, which typically supports 'tm_gmtoff' in 'struct tm'.
 *
 * Checks for UNIX-like systems that are not Linux (e.g., FreeBSD,
 * OpenBSD, NetBSD, DragonFly BSD), which generally include 'tm_gmtoff'.
 * It also covers other UNIX systems like Tru64 ('__osf__'), Irix
 * ('__sgi__'), and Solaris ('__sun__'), which also
 * support 'tm_gmtoff', as well as macOS ('__APPLE__').
 *
 * For Linux systems ('__linux__'), 'struct tm' does not include
 * 'tm_gmtoff', so the macro 'LOGGER_OS_HAS_TM_GMTOFF' is set to 0.
 * For all other systems, the macro defaults to 0 as a fallback. */
#if (defined(__unix__) && !defined(__linux__)) || \
    defined(__APPLE__) || defined(__FreeBSD__) || \
    defined(__NetBSD__) || defined(__OpenBSD__) || \
    defined(__osf__) || defined(__sgi__) || defined(__sun__)
#define LOGGER_OS_HAS_TM_GMTOFF (1)
#else
#define LOGGER_OS_HAS_TM_GMTOFF (0)
#endif

/* This is used as "there are no arguments" argument so the variadic
 * macros do not complain.
 *
 * Yes, '((void *) 0)' is 'NULL', and it's also included in 'stddef.h'
 * which is inarguably called by 'stdio.h', so '#define L_NARG NULL'
 * should be enough, but I like it this way. */
#define L_NARG ((void *) 0)             /**< "No arguments" modifier for
                                             variadic macros in log
                                             messages */

#define LOGGER_MAX_LENGTH_MSG (256)     /**< Max. length of a log message */
#define LOGGER_FLUSH_THRESHOLD (16)     /**< Number of messages stored in
                                             buffer before flushing */

#define LOGGER_TIMESTAMP_USEC (1 << 0)  /**< Flag to add microseconds
                                             to timestamp */
#define LOGGER_TIMESTAMP_TZ (1 << 1)    /**< Flag to add timezone offset
                                             to timestamp */

/**
 * @brief Logger levels
 */
enum logger_level_e {
    LOG_MIN_LEVEL = 0,
    LOG_TRACE = LOG_MIN_LEVEL,  /**< Tracing every single action */
    LOG_DEBUG,                  /**< Information for debugging purposes */
    LOG_INFO,                   /**< Record of the normal operation */
    LOG_NOTICE,                 /**< Normal yet significant information */
    LOG_WARNING,                /**< Issues that may lead to errors */
    LOG_ERROR,                  /**< Conditions on operations */
    LOG_CRITICAL,               /**< Conditions that may lead to failure */
    LOG_ALERT,                  /**< Immediate action is necessary */
    LOG_FATAL,                  /**< Program in unusable: shutdown */
    LOG_MAX_LEVEL = LOG_FATAL,
};


/**
 * @brief Logger structure with buffer
 *
 * Provides a mechanism for logging messages within the application,
 * including support for different logging levels and buffered message
 * storage.  The logger utilizes a buffer to temporarily store up to
 * @c LOGGER_FLUSH_THRESHOLD (16) messages, each with a maximum length
 * of @c LOGGER_MAX_LENGTH_MSG (160 characters).  This approach minimizes
 * direct file I/O operations by allowing the logger to flush messages
 * in chunks, thereby improving performance while still ensuring message
 * delivery in the event of an error.
 *
 * The @p level_min field defines the minimum severity level of messages
 * that will be logged, allowing for fine-grained control over what
 * information is recorded.
 *
 * Nested within are the @p file and @p buffer structures, where the
 * @p file structure manages file pointers for output and error messages
 * and tracks if the output file needs to be closed.  The @p buffer
 * structure holds an array of logged messages, facilitating temporary
 * storage for efficient message management before they are written to
 * the output.
 */
typedef struct {
    enum logger_level_e level_min;  /**< Minimum logging level */
    bool is_tracking;               /**< Track even if not in 'LOG_TRACE' */

    struct file_s {
        FILE *fp_out;               /**< Pointer to output stream */
        FILE *fp_err;               /**< Pointer to error stream */
        bool is_open;               /**< Does the file need closing? */
    } file;

    struct logger_buffer_s {
        char **messages;            /**< Array of messages in buffer */
        unsigned int count;         /**< No. of messages in buffer */
    } *buffer;                      /**< Log buffer to store messages
                                         temporarily before flushing */
} logger_td;


/* Public interface */
/**
 * @brief Initializes a new logger
 *
 * The logging behavior is as follows:
 *  - If filename is keyword:
 *      - "NULL", the logger will be deactivated;
 *      - "STDOUT", all logs will be written to @c stdout;
 *      - "STDERR", all logs will be written to @c stderr;
 *      - "DEFAULT", debug and information logs are sent to @c stdout,
 *        warning and error logs are sent to @c stderr.
 *  - For any other name, the logger will open that filename to append
 *    newer information
 *
 * If it's a file, log entries are written to a buffer until it reaches
 * its capacity.  Once the buffer is full or an error occurs, the
 * contents are flushed to the log file, which is opened in append mode.
 * The buffer is then cleared and reset for future log entries.
 *
 * @param filename    Filename where to output log messages, or keyword
 * @param level_min   Minimum logging level
 * @param is_tracking If @c true, the caller function is traced always,
 *                    otherwise, track only on @c LOG_TRACE level
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Could not allocate memory
 * @retval  2 Failed to open file
 * @retval -1 Singleton was already initialized; no action taken
 *
 * @pre @p level_min must be a valid value in the range @c LOG_MIN_LEVEL
 *      and @c LOG_MAX_LEVEL (closed interval)
 *
 * @note Complexity: @e O(1)
 *
 * @see @c logger_level_e
 */
int logger_start(const char *filename,
        enum logger_level_e level_min, bool is_tracking);

/**
 * @brief Deallocate memory used by this logger instance
 *
 * @return Status of the operation
 * @return  0 Success
 * @return  1 No operation has been performed
 *
 * @note Complexity: @e O(n), where @e n is the number of messages to be
 *       deallocated from the buffer
 */
int logger_stop(void);

/**
 * @brief Log formatted messages with varying levels of severity
 *
 * @param level  Severity of this message
 * @param prefix Prefix to display before the message
 * @param fmt    Formatted message to be logged
 *
 * Logs messages with a specified severity level to the given file
 * pointer by the logger instance.  If the severity level is
 * @c LOG_TRACE, then the caller function will be prepended before the
 * message, otherwise is ignored.
 *
 * @return Number of characters printed (excluding the null byte used to
 *         end output to strings), or a negative value if an output
 *         error is encountered
 *
 * @note The message to be logged should be a null-terminated string
 * @note Only messages at level @p min_level or higher will be logged
 * @note Complexity: @e O(n), where @e n is the length of the formatted
 *       string (because of @a vsnprintf)
 */
int logger_msg(enum logger_level_e level, const char *prefix,
        const char *fmt, ...);

/**
 * @brief Set the logger to always show the calling function in messages
 */
void logger_tracking_on(void);

/**
 * @brief Set the logger to never show the calling function in messages
 *        except in those of level @c LOG_TRACE
 */
void logger_tracking_off(void);

/**
 * @brief Logger helper macro for various severity levels
 *
 * Macro that calls the @a LOGGER_* macros with the appropriate level
 *
 * @param level Message level
 * @param msg   Message format string, or @c NULL if no additional
 *              arguments are needed
 *
 * @see @a logger_msg
 */
#define LOGGER(level, msg, ...) \
    logger_msg(level, __func__, (const char *) msg, __VA_ARGS__)

/**
 * @defgroup logger_macros Logger macros
 * @ingroup logger
 *
 * Macros that evaluate to the logger message sender severity for
 * simplicity of the code.
 *
 * @see @a LOGGER
 * @{
 */
#define LOGGER_TRACE(msg, ...) LOGGER(LOG_TRACE, msg, __VA_ARGS__)
#define LOGGER_DEBUG(msg, ...) LOGGER(LOG_DEBUG, msg, __VA_ARGS__)
#define LOGGER_INFO(msg, ...) LOGGER(LOG_INFO, msg, __VA_ARGS__)
#define LOGGER_NOTICE(msg, ...) LOGGER(LOG_NOTICE, msg, __VA_ARGS__)
#define LOGGER_WARNING(msg, ...) LOGGER(LOG_WARNING, msg, __VA_ARGS__)
#define LOGGER_ERROR(msg, ...) LOGGER(LOG_ERROR, msg, __VA_ARGS__)
#define LOGGER_CRITICAL(msg, ...) LOGGER(LOG_CRITICAL, msg, __VA_ARGS__)
#define LOGGER_ALERT(msg, ...) LOGGER(LOG_ALERT, msg, __VA_ARGS__)
#define LOGGER_FATAL(msg, ...) LOGGER(LOG_FATAL, msg, __VA_ARGS__)
/** @} */


#endif  /* ! LOGGER_H */
