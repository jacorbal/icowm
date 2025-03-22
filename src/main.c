/**
 * @file main.c
 *
 * @brief Main entry point
 *
 * @author J. A. Corbal <jacorbal@gmail.com>
 *
 * @version 1.0.0 release 20250425 ("'ovelya") build 117.20250425T052707
 * @copyright Copyright (c) 2025, J. A. Corbal.
 *            ISC License <https://opensource.org/license/isc-license-txt>
 */

/* Enable features from the POSIX.1-2008 standard */
#define _POSIX_C_SOURCE 200809L


/* System includes */
#include <getopt.h>     /* getopt */
#include <stdbool.h>    /* bool, false, true */
#include <stdio.h>      /* FILE, fprintf */
#include <stdlib.h>     /* NULL, atoi */
#include <string.h>     /* strdup */

/* Project includes */
#include <event.h>
#include <logger.h>
#include <wm.h>


// TODO: PUT THIS DEFINITIONS ON A FILE
#define ICOWM_AUTHOR "J. A. Corbal"
#define ICOWM_NAME_LONG "Iconizer Window Manager"
#define ICOWM_NAME_SHORT "IcoWM"
#define ICOWM_NAME_PROG "icowm"

#define ICOWM_BUILD_NUMBER "117"
#define ICOWM_BUILD_DATE "20250425T052707"
#define ICOWM_RELEASE "20250425"
#define ICOWM_VERSION "1.0.0"
#define ICOWM_VERSION_CODENAME "'ovelya"
#define ICOWM_LICENSE "ISC License"
#define ICOWM_COPYRIGHT "Copyright (c) 2025"

/* Messages I should understand due many decades of 'Star Trek' until
 * they destroyed the franchise, like a phaser set to kill vaporizing my
 * poor human heart!  Those petaQpu'!  ghuy'cha'!  D'kar tel G'denna!
 * Now, everything looks like a starless night of boundless black... */
#define ICOWM_MSG_ON_INIT "Qapla'!"
#define ICOWM_MSG_ON_EXIT "pe'vIl mu'qaDmey tIbach"
#define ICOWM_MSG_OF_THE_VERSION "qaStaHvIS Hu'vam chay' bIpIvneS SoH?"


/**
 * @brief Print copyright string
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static void _show_copyright_str(FILE *fp)
{
    fprintf(fp, "'%s'; %s, %s\n",
            ICOWM_LICENSE, ICOWM_COPYRIGHT, ICOWM_AUTHOR);
}


/**
 * @brief Print version string
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static void _show_version_str(FILE *fp)
{
    fprintf(fp, "%s release %s (\"%s\") build %s.%s\n",
            ICOWM_VERSION, ICOWM_RELEASE, ICOWM_VERSION_CODENAME,
            ICOWM_BUILD_NUMBER, ICOWM_BUILD_DATE);
}


/**
 * @brief Show current version complete information
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static void _show_version(FILE *fp)
{
    fprintf(fp, "%s -- %s\n", ICOWM_NAME_SHORT, ICOWM_NAME_LONG);
    fprintf(fp, "Licensed under "); _show_copyright_str(fp);
    fprintf(fp, "Version "); _show_version_str(fp);
    fprintf(fp, "MOTV: \"%s\"\n", ICOWM_MSG_OF_THE_VERSION);
}


/**
 * @brief Display help on screen
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static void _show_help(FILE *fp)
{
    fprintf(fp, "Usage: %s [<options>]\n", ICOWM_NAME_PROG);
    fprintf(fp, "   -h              This help\n");
    fprintf(fp, "   -v              Version information\n");
    fprintf(fp, "   -c <config_dir> Configuration base directory\n");
    fprintf(fp, "   -l <log file>   Log file (also 'null'," \
            "'stdout' or 'stderr')\n");
    fprintf(fp, "   -L <log level>  Log verbosity (%d..%d)\n",
            LOG_MIN_LEVEL, LOG_MAX_LEVEL);
    fprintf(fp, "   -q              Quiet (equivalent to '-L%d')\n",
            LOG_MAX_LEVEL);
    fprintf(fp, "\n");
    fprintf(fp, "Default values:\n");
    // TODO: Use variables here
    fprintf(fp, "   Configuration directory: %s\n", "~/.icowm");
    fprintf(fp, "   Log file: %s\n", "stdout");
    fprintf(fp, "   Log level: %d\n", LOG_INFO);
}


/**
 * @brief Show greetings line to present the program when invoked
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static void _show_salutation(FILE *fp)
{
    fprintf(fp, "%s -- ", ICOWM_NAME_SHORT);
    _show_version_str(fp);
    fprintf(fp, "%s is ready to start: \"%s\"  :)\n",
            ICOWM_NAME_SHORT, ICOWM_MSG_ON_INIT);
}


/**
 * @brief Show farewell line to dismiss the program when finished
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static void _show_farewell(FILE *fp)
{
    fprintf(fp, "%s exited successfully: \"%s\"  :|\n",
            ICOWM_NAME_SHORT, ICOWM_MSG_ON_EXIT);
}


/* Main entry */
/**
 * Initializes the window manager, enters the event loop to handle
 * incoming X events.  It oversees the life cycle of the window manager
 * and ensures that resources are properly released when the application
 * exits.
 *
 * @return 0 on successful execution, or otherwise
 */
int main(int argc, char *const argv[])
{
    wm_td *wm;
    config_td *config;
    char *log_file = NULL;
    FILE *log_fp = stdout;
    bool log_file_needs_closing = false;
    enum logger_level_e log_level_min = LOG_INFO;
    int opt;
//    char *config_dir = NULL;

//    config_dir = strdup("~/.config");     // XDG, etc.

    while ((opt = getopt(argc, argv, "hvc:l:L:q")) != -1) {
        switch (opt) {
            case 'h':
                _show_help(stdout);
                return 0;
                break;

            case 'v':
                _show_version(stdout);
                return 0;
                break;

            case 'c':
//                config_dir = optarg;
                break;

            case 'l':
                log_file = optarg;
                /* If user enters as logfile the words "stdout" or
                 * "stderr", then the log will be at those descriptors,
                 * and if the user enters the word "null", the log will
                 * be deactivated.  Otherwise, use the log in the
                 * specified file. */
                if (strcmp(log_file, "stdout") == 0) {
                    log_fp = stdout;
                } else if (strcmp(log_file, "stderr") == 0) {
                    log_fp = stderr;
                } else if (strcmp(log_file, "null") == 0) {
                    log_fp = NULL;
                } else {
                    log_fp = fopen(log_file, "a+t");
                    if (log_fp == NULL) {
                        fprintf(stderr,
                            "Failed to open file: '%s'\n", log_file);
                        return -3;
                    }
                    log_file_needs_closing = true;
                }
                break;

            case 'L':
                log_level_min = (enum logger_level_e) atoi(optarg);
                break;

            case 'q':
                log_level_min = LOG_FATAL;
                break;

            default:
                _show_help(stderr);
                return 1;
                break;
        }
    }

    _show_salutation(stdout);   /* Welcome: be polite, greet */

    /* Start logging */
    if (logger_start(log_fp, log_level_min) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return -3;
    }

    config = config_init();
    if (config == NULL) {
        logger_msg(LOG_ERROR,
                "Failed to initialize configuration structure");
        logger_stop();
        return -2;
    }

    wm = wm_init(config);
    if (wm == NULL) {
        logger_msg(LOG_FATAL, "Failed to initialize window manager");
        config_destroy(config);
        logger_stop();
        return -1;
    }
    logger_msg(LOG_INFO, "%s has started!", ICOWM_NAME_SHORT);

    wm_loop(wm);
    wm_destroy(wm);

    /* Stop loggging */
    logger_stop();
    if (log_file_needs_closing && log_fp != NULL) {
        fclose(log_fp);
    }

    _show_farewell(stdout);     /* Depart: be polite, say goodbye */

    return 0;
}
