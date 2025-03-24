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

/* Enable features from the POSIX.1-1990 standard */
#define _POSIX_C_SOURCE 199009L /* getopt */

/* System includes */
#include <stdbool.h>    /* bool, false, true */
#include <stdio.h>      /* FILE, fprintf */
#include <stdlib.h>     /* NULL, atoi, srand */
#include <time.h>       /* time */
#include <unistd.h>     /* getopt, getpid */

/* Utils includes */
#include <utils/safestr.h>

/* Project includes */
#include <logger.h>
#include <wm.h>


// TODO: PUT THIS DEFINITIONS ON A FILE
#define ICOWM_AUTHOR "J. A. Corbal"
#define ICOWM_NAME_LONG "Iconizer Window Manager"
#define ICOWM_NAME_SHORT "IcoWM"
#define ICOWM_NAME_PROG "icowm"
#define ICOWM_DESCRIPTION \
    "IcoWM is an austere, ascetic, minimal, and unembellished" \
    " window manager for X11"

#define ICOWM_BUILD_NUMBER "117"
#define ICOWM_BUILD_DATE "20250425T052707"
#define ICOWM_RELEASE "20250425"
#define ICOWM_VERSION "1.0.0"
#define ICOWM_VERSION_CODENAME "'ovelya"
#define ICOWM_LICENSE "ISC License"
#define ICOWM_COPYRIGHT "Copyright (c) 2025"

#ifdef DEBUG
#define ICOWM_DEFAULT_LOGGER_LEVEL_MIN (LOG_TRACE)
#else
#define ICOWM_DEFAULT_LOGGER_LEVEL_MIN (LOG_NOTICE)
#endif /* ! DEBUG */

#define ICOWM_DEFAULT_LOGGER_BEHAVIOR "DEFAULT"

/* Messages I should understand due many decades of 'Star Trek' until
 * they destroyed the franchise, like a phaser set to kill vaporizing my
 * poor human heart!  Those petaQpu'!  ghuy'cha'!  D'kar tel G'denna!
 * Now, everything looks like a starless night of boundless black... */
#define ICOWM_MSG_ON_INIT "Qapla'!"
#define ICOWM_MSG_ON_EXIT "pe'vIl mu'qaDmey tIbach"


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
    fprintf(fp, "%s\n", ICOWM_DESCRIPTION);
    fprintf(fp, "Licensed under "); _show_copyright_str(fp);
    fprintf(fp, "Version "); _show_version_str(fp);
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
    fprintf(fp, "%s -- %s\n", ICOWM_NAME_SHORT, ICOWM_NAME_LONG);
    fprintf(fp, "Usage: %s [<options>]\n", ICOWM_NAME_PROG);

    fprintf(fp, "\nMain options:\n");
    fprintf(fp, "   -d <display>    Set the X display name\n");

    fprintf(fp, "\nLogging:\n");
    fprintf(fp, "   -l <log_file>   Log file (or keyword: 'DEFAULT'," \
            " 'NULL', 'STDOUT', 'STDERR')\n");
    fprintf(fp, "   -L <log_level>  Log verbosity" \
                " (%d:trace; %d:debug; %d:info ... %d=alert; %d=fatal)\n",
                LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_ALERT, LOG_FATAL);
    fprintf(fp, "   -q              Quiet, except on fatal errors" \
                " (equivalent to '-L%d')\n", LOG_MAX_LEVEL);
    fprintf(fp, "   -t              Activate tracking for all" \
                " messages, not only for level %d\n", LOG_TRACE);

    fprintf(fp, "\nOther options:\n");
    fprintf(fp, "   -h              This help\n");
    fprintf(fp, "   -v              Show version and license" \
                " information\n");

    fprintf(fp, "\n");
    fprintf(fp, "By default: logging mode set to '%s'; log level" \
                " severity status set to %d\n",
                ICOWM_DEFAULT_LOGGER_BEHAVIOR,
                ICOWM_DEFAULT_LOGGER_LEVEL_MIN);
    fprintf(fp, "Display: if not given, it defaults to the" \
                " 'DISPLAY' environment variable value\n");
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
    fprintf(fp, "%s is starting...  \"%s\"  :)\n",
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
    fprintf(fp, "%s has stopped...  \"%s\"  :|\n",
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
 * @retval  1 Failed to start window manager
 * @retval  2 Failed to start logger
 * @retval -1 Bad option on @e getopt
 */
int main(int argc, char *const argv[])
{
    char *display_name = NULL;
    char *log_filename = safe_strdup(ICOWM_DEFAULT_LOGGER_BEHAVIOR);
    enum logger_level_e log_level_min = ICOWM_DEFAULT_LOGGER_LEVEL_MIN;
    bool log_is_tracking = false;
    int opt;

    /* Generate a random seed (windows are in a hash table and the seeds
     * for the hashing algorithm use 'rand') */
    srand((unsigned int) (time(NULL) ^ getpid()));

    /* Get user options */
    while ((opt = getopt(argc, argv, "hvd:l:L:qt")) != -1) {
        switch (opt) {
            case 'h':
                _show_help(stdout);
                return 0;
                break;

            case 'v':
                _show_version(stdout);
                return 0;
                break;

            case 'd':
                display_name = safe_strdup(optarg);
                break;

            case 'l':
                /* If user enters as logfile the keywords "STDOUT" or
                 * "STDERR", then the log will be written entirely on
                 * those descriptors, and if the user enters the word
                 * "NULL", the log will be deactivated.  When the
                 * keyword is "DEFAULT", only warning and errors will be
                 * in 'stderr', and debut or information in 'stdout'.
                 * Otherwise, use the log in the specified file. */
                free(log_filename);
                log_filename = safe_strdup(optarg);
                break;

            case 'L':
                log_level_min = (enum logger_level_e) atoi(optarg);
                break;

            case 'q':
                log_level_min = LOG_FATAL;
                break;

            case 't':
                log_is_tracking = true;
                break;

            default:
                _show_help(stderr);
                return -1;
        }
    }

    _show_salutation(stdout);   /* Welcome: be polite, greet */

    /* Start logging */
    if (logger_start(log_filename,
                log_level_min, log_is_tracking) != 0) {
        return 2;
    }

    /* Window manager "magic" */
    if (wm_start(display_name) != 0) {
        logger_stop();
        return 1;
    }

    /* Stop everything */
    wm_stop();
    logger_stop();

    /* Deallocate last things... */
    free(log_filename);
    if (display_name) {
        free(display_name);
    }

    _show_farewell(stdout);     /* Depart: be polite, say goodbye */

    return 0;
}
