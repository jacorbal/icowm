/**
 * @file main.h
 *
 * @brief Main global definitions for the program
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_MAIN_H
#define DEFS_MAIN_H


/* Project includes */
#include <logger.h>     /* 'logger_level_e' values */


/* Use 'notice' as default log level unless 'DEBUG' is on, in that case
 * use 'trace' log level */
#ifdef DEBUG
#define ICOWM_DEFAULT_LOGGER_LEVEL_MIN (LOG_TRACE)
#else
#define ICOWM_DEFAULT_LOGGER_LEVEL_MIN (LOG_NOTICE)
#endif  /* ! DEBUG */

/* Set the logger standard behavior */
#define ICOWM_DEFAULT_LOGGER_BEHAVIOR "DEFAULT"

/* Messages I should understand due to many decades of 'Star Trek',
 * until the franchise was destroyed like a phaser set to kill,
 * vaporizing my poor human heart!  petaQpu'!  Heghlu'meH QaQ jajvam!
 * bIjatlh 'e' yImev!  Now, everything looks like a starless night of
 * boundless black... */
#define ICOWM_MSG_ON_INIT "Qapla'!"
#define ICOWM_MSG_ON_EXIT "pe'vIl mu'qaDmey tIbach"

#define ICOWM_DESCRIPTION \
    "is an austere, ascetic, minimal, and" \
    " unembellished window manager for X11"


#endif  /* ! DEFS_MAIN_H */
