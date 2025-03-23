/**
 * @file logger.h
 *
 * @brief Log management declaration
 *
 * A singleton pointer to the logger structure allows to log in the same
 * place since the logger starts until it ends working with the same
 * configuration.  The logger uses a buffer to store the formatted
 * messages before flushing them (unless the system descriptors such as
 * @c stdout or @c stderr are used).  This is build to increase
 * performance by minimizing the number of flushes at the cost of
 * a extra bit of memory, so the log is written in chunks if everything
 * is working properly, but it will always flush the messages on any
 * error.
 *
 * The initial logic behind the different log levels came from an answer
 * on Stack Overflow
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
 *                                  .-------system ops.----´
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
 *      | FATAL |<-------------------------N---´
 *      +-------+
 * @endverbatim
 */

#ifndef LOGGER_H
#define LOGGER_H

/* System includes */
#include <stdbool.h>    /* bool */
#include <stdio.h>      /* FILE */


/* Yes, '((void *) 0)' is 'NULL', and it's also included in 'stddef.h'
 * which is called by 'stdio.h', so '#define L_NARG NULL' should be
 * enough, but I like it this way. */
#define L_NARG ((void *) 0)         /**< No arguments modifier, when the
                                         log message has no arguments,
                                         but because of variadic macros,
                                         at least one argument should be
                                         there */

// TODO: Add this to a config file?
#define LOGGER_MAX_MESSAGES (16)    /**< Messages on buffer before flush */
#define LOGGER_MAX_MSG_LENGTH (160) /**< Maximum length of a log message */


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
 * This structure provides a mechanism for logging messages within the
 * application, including support for different logging levels and
 * buffered message storage.
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
 * the output.  I hope this approach helps to improve the performance of
 * logging operations by minimizing direct file I/O.
 */
typedef struct {
    enum logger_level_e level_min;  /**< Minimum logging level */

    struct file_s {
        FILE *fp_out;               /**< Pointer to output stream */
        FILE *fp_err;               /**< Pointer to error stream */
        bool is_open;               /**< Does the file need closing? */
    } file;

    struct logger_buffer_s {
        char **messages;            /**< Array of messages in buffer */
        unsigned int count;         /**< Number of messages in buffer */
    } *buffer;                      /**< Log buffer to store messages */
} logger_td;


/* Public interface */
/**
 * @brief Initializes a new logger
 *
 * @param filename  Filename where to output log messages, or keyword
 * @param level_min Minimum logging level
 *
 * This function initializes the logger based on the specified filename.
 * The logging behavior is as follows:
 *  - If filename is keyword:
 *      - "NULL", the logger will be deactivated.
 *      - "STDOUT", all logs will be written to @c stdout
 *      - "STDERR", all logs will be written to @c stderr
 *      - "DEFAULT", normal severity logs are sent to @c stdout, and
 *        error severity logs are sent to @c stderr.
 *  - For any other name, the logger will open this file for appending
 *
 * If it's a file, log entries are written to a buffer until it reaches
 * its capacity.  Once the buffer is full or an error occurs, the
 * contents are flushed to the log file, which is opened in append mode.
 * The buffer is then cleared and reset for future log entries.
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Could not allocate memory
 * @retval  2 Failed to open file
 *
 * @pre @p level_min must be a valid value in the range @e LOG_MIN_LEVEL
 *      and @e LOG_MAX_LEVEL (closed interval)
 *
 * @note Complexity: @e O(1)
 *
 * @see logger_level_e
 */
int logger_start(const char *filename,
        const enum logger_level_e level_min);

/**
 * @brief Deallocates memory used by this logger instance
 *
 * @note Complexity: @e O(n), where @e n is the number of messages to be
 *       deallocated from the buffer
 */
void logger_stop(void);

/**
 * @brief Log formatted messages with varying levels of severity
 *
 * @param level  Severity of this message
 * @param prefix Prefix to display before the message
 * @param fmt    Formatted message to be logged
 *
 * This function logs messages with a specified severity level to the
 * given file pointer by the logger instance
 *
 * @return Number of characters printed (excluding the null byte used to
 *         end output to strings)
 *
 * @note The message to be logged should be a null-terminated string
 * @note Only messages at level @p min_level or higher will be logged
 * @note Complexity: @e O(n), where @e n is the length of the formatted
 *       string (because of @e vsnprintf)
 */
int logger_msg(enum logger_level_e level, const char *prefix,
        const char *fmt, ...);

/**
 * @brief Logger helper macro for various severity levels
 *
 * These macro call the @c LOGGER_* macros with the appropriate level
 *
 * @param level Message level
 * @param msg   Message format string, or @c NULL if no additional
 *              arguments are needed
 *
 * @see logger_msg
 */
#define _LOGGER(level, msg, ...) \
    logger_msg(level, __func__, msg, ##__VA_ARGS__)

/**
 * @defgroup Logger_Macros Macros that evaluate to the logger message
 *                         sender by severity for simplicity of the code
 *
 * @see _LOGGER
 * @{
 */
#define LOGGER_TRACE(msg, ...) _LOGGER(LOG_TRACE, msg, ##__VA_ARGS__)
#define LOGGER_DEBUG(msg, ...) _LOGGER(LOG_DEBUG, msg, ##__VA_ARGS__)
#define LOGGER_INFO(msg, ...) _LOGGER(LOG_INFO, msg, ##__VA_ARGS__)
#define LOGGER_NOTICE(msg, ...) _LOGGER(LOG_NOTICE, msg, ##__VA_ARGS__)
#define LOGGER_WARNING(msg, ...) _LOGGER(LOG_WARNING, msg, ##__VA_ARGS__)
#define LOGGER_ERROR(msg, ...) _LOGGER(LOG_ERROR, msg, ##__VA_ARGS__)
#define LOGGER_CRITICAL(msg, ...) _LOGGER(LOG_CRITICAL, msg, ##__VA_ARGS__)
#define LOGGER_ALERT(msg, ...) _LOGGER(LOG_ALERT, msg, ##__VA_ARGS__)
#define LOGGER_FATAL(msg, ...) _LOGGER(LOG_FATAL, msg, ##__VA_ARGS__)
/** @} */


#endif  /* ! LOGGER_H */
