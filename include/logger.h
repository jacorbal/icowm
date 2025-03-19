/**
 * @file logger.h
 *
 * @brief Log management declaration

 * A singleton pointer to the logger structure allows to log in the same
 * place since the logger starts until it ends working with the same
 * configuration.  The logger uses a buffer to store the formatted
 * messages before flushing them (unless the system descriptors such as
 * @c stdout or @c stderr are used).  This is build to increase
 * performance at the cost of a extra bit of memory, so the log is
 * written in chunks if everything is working properly, but it will
 * always flush the messages on any error.
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
#include <stdio.h>      /* FILE */


#define LOGGER_MAX_MESSAGES (16)    /**< Messages on buffer before flush */
#define LOGGER_MAX_MSG_LENGTH (128) /**< Maximum length of a log message */


/**
 * @brief Logger levels
 */
enum logger_level_e {
    LOG_TRACE,      /**< Tracing every single action */
    LOG_DEBUG,      /**< Detailed information for debugging purposes */
    LOG_INFO,       /**< Record of the normal operation */
    LOG_NOTICE,     /**< Normal but significant information */
    LOG_WARNING,    /**< Potential issues that may lead to errors */
    LOG_ERROR,      /**< Conditions on operations, not the program */
    LOG_CRITICAL,   /**< Conditions that may lead to failure of program */
    LOG_ALERT,      /**< Immediate action is necessary */
    LOG_FATAL,      /**< Program in unusable: shutdown forced */
    LOG_MAX_LEVELS = LOG_FATAL,
};


/**
 * @brief Logger structure with buffer
 */
typedef struct {
    FILE *fp;                       /**< Pointer to output stream */
    enum logger_level_e level_min;  /**< Minimum logging level */

    struct logger_buffer_s {
        char **messages;            /**< Array of messages in buffer */
        unsigned int count;         /**< Number of messages in buffer */
    } *buffer;                      /**< Log buffer to store messages */
} logger_td;


/* Public interface */
/**
 * @brief Initializes a new logger
 *
 * @param fp        Pointer to output stream the log message is written
 * @param level_min Minimum logging level
 *
 * @return Status of the operation
 * @retval 0 Success
 * @retval 1 Error and could not allocate memory
 *
 * @note Complexity: @e O(1)
 *
 * @see logger_level_e
 */
int logger_start(FILE *fp, const enum logger_level_e level_min);

/**
 * @brief Deallocates memory used by this logger instance
 *
 * @note Complexity: @e O(1)
 */
void logger_stop(void);

/**
 * @brief Log formatted messages with varying levels of severity
 *
 * @param level  Severity of this message
 * @param fmt    Formatted message to be logged
 *
 * This function logs messages with a specified severity level to the
 * given file pointer by the logger instance
 *
 * @return Number of characters printed (excluding the null byte used to
 *         end output to strings)
 *
 * @note The message to be logged should be a null-terminated string
 * @note Only messages at level @c min_level or higher will be logged
 * @note Complexity: @e O(n), where @e n is the length of the formatted
 *       string (because of @e vsnprintf)
 */
int logger_msg(enum logger_level_e level, const char *fmt, ...);


#endif  /* ! LOGGER_H */
