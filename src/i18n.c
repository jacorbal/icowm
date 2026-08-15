/**
 * @file i18n.c
 *
 * @brief GUI text translation setup implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <libintl.h>    /* bindtextdomain, textdomain */
#include <locale.h>     /* setlocale */

/* Local includes */
#include <i18n.h>


/* Set up GUI text translation for the rest of this process's own
 * lifetime */
void i18n_init(void)
{
    (void) setlocale(LC_ALL, "");
    (void) bindtextdomain(I18N_DOMAIN, I18N_LOCALE_DIR);
    (void) textdomain(I18N_DOMAIN);
}
