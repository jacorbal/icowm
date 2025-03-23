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
#include <unistd.h>     /* getopt */
#include <stdio.h>      /* FILE, fprintf */
#include <stdlib.h>     /* NULL, atoi, srand */
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

#define ICOWM_DEFAULT_LOGGER_LEVEL_MIN (LOG_NOTICE)
#define ICOWM_DEFAULT_LOGGER_BEHAVIOR "DEFAULT"

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
    fprintf(fp, "   -v              Display version and license" \
            " information\n");
    fprintf(fp, "   -l <log file>   Log file (or keyword: 'DEFAULT'," \
            " 'NULL', 'STDOUT', 'STDERR')\n");
    fprintf(fp, "   -L <log level>  Log verbosity" \
            " (%d:trace; %d:debug; %d:info ... %d=alert; %d=fatal)\n",
            LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_ALERT, LOG_FATAL);
    fprintf(fp, "   -q              Quiet except on fatal errors" \
            " (equivalent to '-L%d')\n", LOG_MAX_LEVEL);
    fprintf(fp, "\n");
    fprintf(fp, "Default: logging mode set to '%s'; log level" \
                " severity status set to %d\n",
                ICOWM_DEFAULT_LOGGER_BEHAVIOR,
                ICOWM_DEFAULT_LOGGER_LEVEL_MIN);
    fprintf(fp, "Log: 'DEFAULT' sends errors to 'stderr', others to" \
                " 'stdout'; 'NULL' disables it\n");
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
 * Start logging, initialize the window manager, enter the event loop to
 * handle incoming X events.  It oversees the life cycle of the window
 * manager and ensures that resources are properly released when the
 * application exits.
 *
 * @return Program status
 * @retval  0 @e Qapla'!
 * @retval  1 Bad option on @e getopt
 * @retval -1 Failed to start window manager
 * @retval -2 Failed to initialize configuration structure
 * @retval -3 Failed to start logger
 */
int main(int argc, char *const argv[])
{
    wm_td *wm;
    config_td *config;
    char *log_filename = strdup(ICOWM_DEFAULT_LOGGER_BEHAVIOR);
    enum logger_level_e log_level_min = ICOWM_DEFAULT_LOGGER_LEVEL_MIN;
    int opt;

    /* Generate a random seed (windows are in a hash table) */

    /* Get user options */
    while ((opt = getopt(argc, argv, "hvl:L:q")) != -1) {
        switch (opt) {
            case 'h':
                _show_help(stdout);
                return 0;
                break;

            case 'v':
                _show_version(stdout);
                return 0;
                break;

            case 'l':
                /* If user enters as logfile the keywords "STDOUT" or
                 * "STDERR", then the log will be written entirely on
                 * those descriptors, and if the user enters the word
                 * "NULL", the log will be deactivated.  When the
                 * keyword is "DEFAULT", only errors will be in
                 * 'stderr', and warnings and information in 'stdout'.
                 * Otherwise, use the log in the specified file. */
                strcpy(log_filename, optarg);
                log_filename = strdup(optarg);
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
        }
    }

    _show_salutation(stdout);   /* Welcome: be polite, greet */

    /* Start logging */
    if (logger_start(log_filename, log_level_min) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return -3;
    }

    config = config_init();
    if (config == NULL) {
        LOGGER_ERROR("Failed to initialize configuration structure",
                L_NARG);
        logger_stop();
        return -2;
    }

    /* Window manager "magic" */
    wm = wm_init(config);
    if (wm == NULL) {
        LOGGER_FATAL("Failed to initialize window manager", L_NARG);
        config_destroy(config);
        logger_stop();
        return -1;
    }

    LOGGER_INFO("%s has started!", ICOWM_NAME_SHORT);
    wm_loop(wm);
    wm_destroy(wm);

    /* Stop logging */
    logger_stop();
    free(log_filename);

    _show_farewell(stdout);     /* Depart: be polite, say goodbye */

    return 0;
}
