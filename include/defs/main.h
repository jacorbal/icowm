/**
 * @file main.h
 *
 * @brief Main global definitions for the program
 */

#ifndef DEFS_MAIN_H
#define DEFS_MAIN_H


/* Project includes */
#include <logger.h>     /* 'logger_level_e' values */


/* Use 'notice' as default log level unless 'DEBUG' is on, in that case
 * use 'trace' log level */
#ifndef DEBUG
#define ICOWM_DEFAULT_LOGGER_LEVEL_MIN (LOG_NOTICE)
#else
#define ICOWM_DEFAULT_LOGGER_LEVEL_MIN (LOG_TRACE)
#endif  /* ! DEBUG */

/* Set the logger standard behavior */
#define ICOWM_DEFAULT_LOGGER_BEHAVIOR "DEFAULT"

/* Messages I should understand due many decades of 'Star Trek', until
 * the franchise was destroyed, like a phaser set to kill vaporizing my
 * poor human heart!  Those petaQpu'!  ghuy'cha'!  D'kar tel G'denna!
 * Now, everything looks like a starless night of boundless black... */
#define ICOWM_MSG_ON_INIT "Qapla'!"
#define ICOWM_MSG_ON_EXIT "pe'vIl mu'qaDmey tIbach"

#define ICOWM_DESCRIPTION \
    __PROJECT_NAME_SHORT \
    " is an austere, ascetic, minimal, and" \
    " unembellished window manager for X11"


#endif  /* ! DEFS_MAIN_H */
