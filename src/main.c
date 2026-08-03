/**
 * @file main.c
 *
 * @brief Main entry point, i.e., the mystical enchanting gateway where
 *        the program awakens, and the symphony of logic begins to play
 *
 * @author J. A. Corbal <jacorbal@gmail.com>
 *
 * @date Tue Feb 24 11:42:50 UTC 2026
 *
 * @version 0.1.0 ("'ovelya")
 * @copyright Copyright (c) 2026, J. A. Corbal.
 *            ISC License <https://opensource.org/license/isc-license-txt>
 *
 * @note Compiled according to the ISO/IEC 9899:1999 (C99) standard;
 *       conforms to POSIX.1-2001
 * @note Built with `gcc` 14.2.0 and `clang` 19.1.7
 */
/*                 ____       _      ____  ___
 *                /  _/______| | /| / /  |/  /
 *               _/ // __/ _ \ |/ |/ / /|_/ /
 *              /___/\__/\___/__/|__/_/  /_/
 *               Iconifying Window Manager
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* Enable features from the POSIX.1-2001 standard */
#define _POSIX_C_SOURCE 200112L /* getopt */


/* System includes */
#include <stdbool.h>
#include <stdio.h>      /* FILE, fprintf */
#include <stdlib.h>     /* NULL, atoi, getenv, srand */
#include <time.h>       /* time */
#include <unistd.h>     /* optarg, getopt, getpid */

/* Utils includes */
#include <utils/safe/safemem.h>
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/config.h>
#include <defs/main.h>

/* Project includes */
#include <logger.h>
#include <wm.h>


/**
 * @brief Print copyright string
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static inline void s_show_copyright_str(FILE *fp)
{
    fprintf(fp, "'%s'; %s, %s\n", LICENSE, COPYRIGHT, AUTHOR);
}


/**
 * @brief Print version string
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static inline void s_show_version_str(FILE *fp)
{
    fprintf(fp, "%s+%d.%s (\"%s\"); release %s\n",
            PROJECT_VERSION,
            BUILD_NUMBER,
            BUILD_TIMESTAMP,
            PROJECT_VERSION_CODENAME,
            RELEASE_DATE);
}


/**
 * @brief Show current version complete information
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static inline void s_show_version(FILE *fp)
{
    fprintf(fp, "%s %s\n", PROJECT_NAME_SHORT, ICOWM_DESCRIPTION);
    fprintf(fp, "Licensed under "); s_show_copyright_str(fp);
    fprintf(fp, "Version "); s_show_version_str(fp);
}


/**
 * @brief Display help on screen
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static inline void s_show_help(FILE *fp)
{
    const char *config_xdg_config_home = getenv("XDG_CONFIG_HOME");
    const char *config_home = getenv("HOME");

    /* Show name and usage */
    fprintf(fp, "%s -- %s\n", PROJECT_NAME_SHORT, PROJECT_NAME_LONG);
    fprintf(fp, "Usage: %s [<options>]\n", PROJECT_NAME_PROG);

    /* Show options by category */
    fprintf(fp, "\nMain options:\n");
    fprintf(fp, "   -d <display>    Set X server display (e.g.: ':0')\n");
    fprintf(fp, "   -c <config_dir> Set configuration directory\n");
    fprintf(fp, "\nLogging:\n");
    fprintf(fp, "   -L <log_level>  Set log verbosity level (%d-%d)\n",
            LOG_MIN_LEVEL, LOG_MAX_LEVEL);
    fprintf(fp, "   -l <log_file>   Log file (or keyword: 'DEFAULT'," \
                " 'STDOUT', 'STDERR', 'NULL')\n");
    fprintf(fp, "   -q              Quiet mode; filter to fatal errors" \
                " only (identical to '-L%d')\n", LOG_MAX_LEVEL);
    fprintf(fp, "   -t              Enable function tracking for all" \
                " messages, not just level %d\n", LOG_TRACE);

    fprintf(fp, "\nOther options:\n");
    fprintf(fp, "   -h              Show this help information, and exit\n");
    fprintf(fp, "   -v              Show version and license" \
                " information, and exit\n");

    fprintf(fp, "\n");

    /* Show information for display */
    fprintf(fp, "Display: if not specified, the X server defaults to" \
                " the 'DISPLAY' env. variable\n");

    /* Show default configuration directory values */
    fprintf(fp, "Configuration directory is set to ");
    if (config_xdg_config_home) {
        fprintf(fp, "'%s/%s'\n", config_xdg_config_home, CONFIG_DIR_BASE);
    } else if (config_home) {
        fprintf(fp, "'%s/.%s'\n", config_home, CONFIG_DIR_BASE);
    } else {
        fprintf(fp, "'%s'\n", CONFIG_DIR_BASE);
    }

    /* Show default logging information */
    fprintf(fp, "Logging mode is set to '%s'; log level" \
                " verbose status is set to %d\n",
            ICOWM_DEFAULT_LOGGER_BEHAVIOR,
            ICOWM_DEFAULT_LOGGER_LEVEL_MIN);
    fprintf(fp, "Log: 'DEFAULT' sends errors to 'stderr' & info to" \
                " 'stdout'; 'NULL' disables it\n");
    fprintf(fp, "Log verbosity levels:" \
                " | %d:trace; %d:debug; %d:info; %d:notice; %d:warning;\n" \
                "                     " \
                " | %d:error; %d:critical; %d:alert; %d:fatal\n",
            LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_NOTICE, LOG_WARNING,
            LOG_ERROR, LOG_CRITICAL, LOG_ALERT, LOG_FATAL);
}


/**
 * @brief Show where the logger is writing the messages in
 *        a human-readable fashion, and the logging level in use
 *
 * @param fp       File pointer to the stream where to write the output
 * @param filename Filename that is either the log file, or a keyword
 * @param level    Current level the logger is working on
 *
 * @note Complexity: @e O(1)
 */
static inline void s_show_logger_destination(FILE *fp,
        const char *filename, const enum logger_level_e level)
{
    if (filename == NULL) {
        fprintf(fp, "Nowhere to write the log!\n");
        return;
    }

    if (safe_strcmp(filename, "NULL") == 0) {
        fprintf(fp, "Log deactivated!\n");
    } else {
        fprintf(fp, "Logging ");
        if (safe_strcmp(filename, "DEFAULT") == 0) {
            fprintf(fp, "warnings & errors to 'stderr'," \
                        " and information to 'stdout' ");
        } else if (safe_strcmp(filename, "STDOUT") == 0) {
            fprintf(fp, "everything to 'stdout' ");
        } else if (safe_strcmp(filename, "STDERR") == 0) {
            fprintf(fp, "everything to 'stderr' ");
        } else {
            fprintf(fp, "in chunks to the innocuous file '%s' ",
                    filename);
        }
        fprintf(fp, "with level %u\n", level);
    }
}


/**
 * @brief Show greetings line to present the program when invoked
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static inline void s_show_salutation(FILE *fp)
{
    fprintf(fp, "%s -- ", PROJECT_NAME_SHORT);
    s_show_version_str(fp);
    fprintf(fp, "%s is starting...  \"%s\"  :)\n",
            PROJECT_NAME_SHORT, ICOWM_MSG_ON_INIT);
}


/**
 * @brief Show farewell line to dismiss the program when finished
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static inline void s_show_farewell(FILE *fp)
{
    fprintf(fp, "%s has stopped...  \"%s\"  :|\n",
            PROJECT_NAME_SHORT, ICOWM_MSG_ON_EXIT);
}


/**
 * @brief Free up to three dynamically allocated buffers
 *
 * Convenience helper to release multiple pointers using @c safe_free,
 * commonly used on early exits to avoid code duplication.
 *
 * @param s1 First buffer pointer (may be null)
 * @param s2 Second buffer pointer (may be null)
 * @param s3 Third buffer pointer (may be null)
 *
 * @note Each pointer is passed by address to @c safe_free and is
 *       expected to be set to @c NULL after deallocation.
 * @note Complexity: @e O(1)
 */
static inline void s_deallocate_buffers(char **s1, char **s2, char **s3)
{
    safe_free_var((void **) s1,
            (void **) s2,
            (void **) s3,
            SAFE_FREE_VAR_END);
}


/**
 * @brief Replace an option-owned string without leaking the previous one
 *
 * @param dst Pointer to the owned string slot
 * @param src Replacement string
 *
 * @return 0 on success, 1 if duplication fails
 */
static int s_replace_option_string(char **dst, const char *src)
{
    char *copy;

    if (dst == NULL || src == NULL) {
        return 1;
    }

    copy = safe_strdup(src);
    if (copy == NULL) {
        return 1;
    }

    safe_free((void **) dst);
    *dst = copy;
    return 0;
}


/* Main entry */
/**
 * Start logging, initialize the window manager, enter the event loop to
 * handle incoming X events.  It oversees the life cycle of the window
 * manager and ensures that resources are properly released when the
 * application exits.
 *
 * @return Program status
 * @retval  0 @e Qapla'!
 * @retval  1 Failed to start window manager
 * @retval  2 Failed to start logger
 * @retval -1 Bad option on @e getopt
 */
int main(int argc, char *const argv[])
{
    char *display_name = NULL;
    char *config_dir = NULL;
    char *log_filename = safe_strdup(ICOWM_DEFAULT_LOGGER_BEHAVIOR);
    enum logger_level_e log_level_min = ICOWM_DEFAULT_LOGGER_LEVEL_MIN;
    bool log_is_tracking = false;
    bool verbose = true;
    int opt;

    /* Generate a random seed (windows are in a hash table and the seeds
     * for the hashing algorithm use 'rand') */
    srand((unsigned int) (time(NULL) ^ getpid()));

    /* Get user options */
    while ((opt = getopt(argc, argv, "hvd:c:l:L:qt")) != -1) {
        int opt_level;

        switch (opt) {
            case 'h':
                s_show_help(stdout);
                s_deallocate_buffers(&log_filename,
                                     &display_name,
                                     &config_dir);
                return 0;

            case 'v':
                s_show_version(stdout);
                s_deallocate_buffers(&log_filename,
                                     &display_name,
                                     &config_dir);
                return 0;

            case 'd':
                if (s_replace_option_string(&display_name, optarg) != 0) {
                    fprintf(stderr,
                            "Failed to store display option value\n");
                    s_deallocate_buffers(&log_filename,
                                         &display_name,
                                         &config_dir);
                    return 1;
                }
                break;

            case 'c':
                if (s_replace_option_string(&config_dir, optarg) != 0) {
                    fprintf(stderr,
                            "Failed to store configuration directory\n");
                    s_deallocate_buffers(&log_filename,
                                         &display_name,
                                         &config_dir);
                    return 1;
                }
                break;

            case 'l':
                if (s_replace_option_string(&log_filename, optarg) != 0) {
                    fprintf(stderr,
                            "Failed to store log destination\n");
                    s_deallocate_buffers(&log_filename,
                                         &display_name,
                                         &config_dir);
                    return 2;
                }
                break;

            case 'L':
                opt_level = atoi(optarg);
                if (opt_level >= LOG_MIN_LEVEL &&
                    opt_level <= LOG_MAX_LEVEL) {
                    log_level_min = (enum logger_level_e) opt_level;
                } else {
                    fprintf(stderr, "Log level out of range:" \
                                    " using default level %d\n",
                                    ICOWM_DEFAULT_LOGGER_LEVEL_MIN);
                }
                break;

            case 'q':
                log_level_min = LOG_FATAL;
                verbose = false;
                break;

            case 't':
                log_is_tracking = true;
                break;

            default:
                s_show_help(stderr);
                s_deallocate_buffers(&log_filename,
                                     &display_name,
                                     &config_dir);
                return -1;
        }
    }

    /* Welcome: be polite, greet */
    s_show_salutation(stdout);

    /* Start logging */
    if (logger_start(log_filename,
                log_level_min, log_is_tracking) != 0) {
        s_deallocate_buffers(&log_filename, &display_name, &config_dir);
        return 2;
    }
    if (verbose) {
        s_show_logger_destination(stdout, log_filename, log_level_min);
    }

    /* Window manager "magic" */
    LOGGER_INFO("Starting up window manager", L_NARG);
    if (wm_start(display_name, config_dir) != 0) {
        logger_stop();
        s_deallocate_buffers(&log_filename, &display_name, &config_dir);
        return 1;
    }

    /* Stop everything */
    wm_stop();
    LOGGER_INFO("Shutting down window manager", L_NARG);
    logger_stop();

    /* Deallocate last things... */
    s_deallocate_buffers(&log_filename, &display_name, &config_dir);

    /* Depart: be polite, say goodbye */
    s_show_farewell(stdout);

    return 0;
}
