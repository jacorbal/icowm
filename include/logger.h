/**
 * @file logger.h
 *
 * @brief Log management declaration
 *
 * 
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
 */

#ifndef LOGGER_H
#define LOGGER_H

#define MAX_LOG_MSG_LENGTH (4096)    /**< Maximum length of a log message */


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
    LOG_FATAL,      /**< Program in unusable: shutdown forced*/
    LOG_MAX_LEVELS,
};


/**
 * @brief Log messages with varying levels of severity
 *
 * This function logs messages with a specified severity level to the
 * given file pointer (e.g., stdout, stderr, or a file).
 *
 * @param fp        Pointer to output stream the log message is written
 * @param min_level Minimum logging level
 * @param cur_level Severity level of the specific message being logged
 * @param fmt       Formatted message to be logged
 *
 * @note Only messages at level @c min_level or higher will be logged
 * @note The message to be logged should be a null-terminated string
 * @note If the level is @e FATAL, the program will perform necessary
 *       cleanup (if possible) and then exit
 * @note Complexity: @e O(n), where @e n is the length of the formatted
 *       string (because of @e vsnprintf)
 *
 * @see logger_level_e
 */
void logger(FILE *fp,
        enum logger_level_e min_level, enum logger_level_e cur_level,
        const char *fmt, ...);


#endif /* ! LOGGER_H */
