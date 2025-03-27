/**
 * @file main.h
 *
 * @brief Main global definitions for the program
 */

#ifndef DEFS_MAIN_H
#define DEFS_MAIN_H


/* Main values */
#define ICOWM_AUTHOR "J. A. Corbal"
#define ICOWM_NAME_LONG "Iconifying Window Manager"
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

/* Messages I should understand due many decades of 'Star Trek', until
 * the franchise was destroyed, like a phaser set to kill vaporizing my
 * poor human heart!  Those petaQpu'!  ghuy'cha'!  D'kar tel G'denna!
 * Now, everything looks like a starless night of boundless black... */
#define ICOWM_MSG_ON_INIT "Qapla'!"
#define ICOWM_MSG_ON_EXIT "pe'vIl mu'qaDmey tIbach"


#endif  /* ! DEFS_MAIN_H */
