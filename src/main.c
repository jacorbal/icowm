/**
 * file main.c
 *
 * @brief Main entry point
 *
 * @author J. A. Corbal <jacorbal@gmail.com>
 * @copyright (c) 2025, J. A. Corbal
 * @license ISC License <https://opensource.org/license/isc-license-txt>
 * @version 0.1.0 release 20250425 ("'ovelya") build 117.20250425T052707
 *
 * @note
 * "It is not the responsibility of programmers to discover whether the
 *  users, the systems, and the code they encounter on their journey are
 *  in these circumstances and suffer this distress for their flaws, or
 *  for their merits: the programmer's sole responsibility is to help
 *  them as entities in need, having eyes only for their challenges, not
 *  for their mistakes."
 */
/**
 * TO THE GOOD HEED OF THY DISCERNING AND ILLUSTRIOUS READER
 *
 * Behold this programme, wrought with stalwart 'vi' ('nvi'      - o -
 * v1.81.6, if thou dost desire precision), crafted in stout     - - o
 * GNU/Linux; yet amidst this, my boundless and                  o o o
 * unconditional love for ev'ry manifold iteration of BSD
 * doth ever remain.  'Twas expressed upon the magnificent
 * portable Sony Vaio TZ[21WN/B], my steadfast companion
 * since thereabout the year two-thousand and seven.
 * A voyager through realms both nigh and far, it bore
 * witness to my travails, as I embraced the haunting
 * specter of sleepless nights, seeking solace in code's
 * warm embrace.  For this very programme, the hours
 * twenty-four, fragmented 'neath five suns, were sacrificed
 * as offerings to the sacred art of creation of this first
 * completed version, where each keystroke upon its faithful
 * keys became a whisper in the hush of night, each line an
 * echo of my restless mind, threading time's tapestry in
 * silent reverie.  I craft, therefore I evolve.
 *
 *               //JACR, March MMXXV (@ver. 0.1.0 "'ovelya")
 */

/* Enable features from the POSIX.1-2008 standard */
#define _POSIX_C_SOURCE 200809L


/* System includes */
#include <getopt.h>     /* getopt */
#include <stdbool.h>    /* bool, false, true */
#include <stdio.h>      /* fprintf */
#include <stdlib.h>     /* atoi */
#include <string.h>     /* strdup */

/* Project includes */
#include <logger.h>
#include <wm.h>


// TODO: PUT THIS DEFINITIONS ON A FILE
#define ICOWM_AUTHOR "J. A. Corbal"                 /* Author's name */
#define ICOWM_NAME_SHORT "IcoWM"                    /* Short name */
#define ICOWM_NAME_LONG "Iconizer Window Manager"   /* Long name */
#define ICOWM_NAME_PROG "icowm"                     /* Binary name */

#define ICOWM_BUILD_NUMBER "117"                    /* Build number */
#define ICOWM_BUILD_DATE "20250425T052707"          /* Build date (UTC) */
#define ICOWM_RELEASE "20250425"                    /* Release date */
#define ICOWM_VERSION "0.1.0"                       /* Semantic version */
#define ICOWM_VERSION_CODENAME "'ovelya"            /* Version codename */
#define ICOWM_LICENSE "ISC License"                 /* License name */

/* Messages */
#define ICOWM_MOTV "qaStaHvIS Hu'vam chay' bIpIvneS SoH?"   /* MOTV */
#define ICOWM_MESSAGE_ON_EXIT "pe'vIl mu'qaDmey"    /* Exit message */


/**
 * @brief Show help
 *
 * @param fp File pointer to the stream where to write the output
 */
static void _show_help(FILE *fp)
{
    fprintf(fp, "Usage: %s [<options>]\n", ICOWM_NAME_PROG);
    fprintf(fp, "   -h              This help\n");
    fprintf(fp, "   -v              Version information\n");
    fprintf(fp, "   -c <config_dir> Configuration base directory\n");
    fprintf(fp, "   -l <log file>   Log file\n");
    fprintf(fp, "   -L <log level>  Log verbosity (0..%d)\n",
            LOG_MAX_LEVELS);
    fprintf(fp, "\n");
    fprintf(fp, "Default values:\n");
    fprintf(fp, "   Configuration directory: %s\n", "~/.config");
    fprintf(fp, "   Log file: %s\n", "stdout");
    fprintf(fp, "   Log level: %d\n", LOG_INFO);
}


/**
 * @brief Show current version
 *
 * @param fp File pointer to the stream where to write the output
 */
static void _show_version(FILE *fp)
{
    fprintf(fp, "%s -- %s\n", ICOWM_NAME_SHORT, ICOWM_NAME_LONG);
    fprintf(fp, "licensed under '%s' -- Copyright (c) 2025, %s\n",
            ICOWM_LICENSE, ICOWM_AUTHOR);
    fprintf(fp, "version %s release %s (\"%s\") build %s.%s\n",
            ICOWM_VERSION, ICOWM_RELEASE, ICOWM_VERSION_CODENAME,
            ICOWM_BUILD_NUMBER, ICOWM_BUILD_DATE);
    fprintf(fp, "MOTV: \"%s\"\n", ICOWM_MOTV);
}


/**
 * @brief Show initial line to present the program when invoked
 *
 * @param fp File pointer to the stream where to write the output
 */
static void _show_salutation(FILE *fp)
{
    fprintf(fp, "%s -- version %s release %s (\"%s\") build %s.%s\n",
            ICOWM_NAME_SHORT,
            ICOWM_VERSION, ICOWM_RELEASE, ICOWM_VERSION_CODENAME,
            ICOWM_BUILD_NUMBER, ICOWM_BUILD_DATE);
}


/**
 * @brief Show farewell line to present the program when finished
 *
 * @param fp File pointer to the stream where to write the output
 */
static void _show_farewell(FILE *fp)
{
    fprintf(stdout, "%s exited successfully: \"%s\"  :|\n",
            ICOWM_NAME_SHORT, ICOWM_MESSAGE_ON_EXIT);
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
//    char *config_dir = NULL;
    char *log_file = NULL;
    FILE *log_fp = stdout;
    bool log_file_needs_closing = false;
    enum logger_level_e log_level_min = LOG_INFO;
    int opt;

    /* Default values */
//    config_dir = strdup("~/.config");

    /* Get (short) options */
    while ((opt = getopt(argc, argv, "hvc:l:L:")) != -1) {
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
                            "Cannot open file '%s'\n", log_file);
                        return -3;
                    }
                    log_file_needs_closing = true;
                }
                break;

            case 'L':
                log_level_min = (enum logger_level_e) atoi(optarg);
                break;

            default:
                _show_help(stderr);
                return 1;
                break;
        }
    }

    /* Welcome */
    _show_salutation(stdout);

    /* Set logger */
    if (logger_start(log_fp, log_level_min) != 0) {
        fprintf(stderr, "Cannot initialize logger\n");
        return -3;
    }

    /* Get configuration */
    config = config_init();
    if (config == NULL) {
        logger_msg(LOG_ERROR,
                "Cannot initialize configuration structure");
        logger_stop();
        return -2;
    }

    /* Initialize the window manager */
    wm = wm_init(config);
    if (wm == NULL) {
        logger_msg(LOG_FATAL, "Failed to initialize window manager");
        config_destroy(config);
        logger_stop();
        return -1;
    }

    logger_msg(LOG_INFO, "%s has started!  Qapla'!", ICOWM_NAME_SHORT);
    wm_loop(wm);            /* Main event loop */
    wm_destroy(wm);         /* End the window manager */
    logger_stop();          /* Finish the logger */
    if (log_file_needs_closing && log_fp != NULL) {
        fclose(log_fp);
    }

    /* Be polite, say goodbye */
    _show_farewell(stdout);

    return 0;
}
